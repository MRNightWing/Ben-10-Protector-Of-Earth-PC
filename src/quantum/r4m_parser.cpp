#include "quantum/r4m_parser.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace Quantum {

static bool IsValidFloat(float val, float minV = -1000.0f, float maxV = 1000.0f) {
    if (std::isnan(val) || std::isinf(val)) return false;
    return (val >= minV && val <= maxV);
}

bool ParseR4M(const std::string& filepath, LevelMap& outMap) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[R4M Parser] Failed to open: " << filepath << std::endl;
        return false;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize < 492) {
        std::cerr << "[R4M Parser] File too small: " << fileSize << " bytes" << std::endl;
        return false;
    }

    std::vector<uint8_t> buffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        std::cerr << "[R4M Parser] Read failed." << std::endl;
        return false;
    }

    // Check BIGB Magic
    if (std::memcmp(buffer.data(), "BIGB", 4) != 0) {
        std::cerr << "[R4M Parser] Invalid magic (expected 'BIGB')" << std::endl;
        return false;
    }

    uint32_t headerSize = *reinterpret_cast<const uint32_t*>(&buffer[4]);
    outMap.version      = *reinterpret_cast<const uint32_t*>(&buffer[8]);
    
    if (headerSize >= 0x1EC) {
        outMap.soundHeapSize = *reinterpret_cast<const uint32_t*>(&buffer[0x1E8]);
    }

    // Parse metadata strings from header (0x10 to headerSize)
    std::vector<std::string> metaStrings;
    std::string cur;
    for (size_t i = 0x10; i < headerSize && i < buffer.size(); ++i) {
        uint8_t b = buffer[i];
        if (b == 0) {
            if (!cur.empty()) {
                metaStrings.push_back(cur);
                cur.clear();
            }
        } else if (b >= 32 && b < 127) {
            cur.push_back((char)b);
        }
    }

    outMap.levelName  = metaStrings.size() > 0 ? metaStrings[0] : "Unknown";
    outMap.author     = metaStrings.size() > 1 ? metaStrings[1] : "Unknown";
    outMap.buildFlags = metaStrings.size() > 2 ? metaStrings[2] : "";

    std::cout << "[R4M Parser] Loaded WAD: " << outMap.levelName 
              << " (" << filepath << ")" << std::endl;
    std::cout << "[R4M Parser] Author: " << outMap.author 
              << ", Ver: " << outMap.version 
              << ", Sound Heap: " << outMap.soundHeapSize << " bytes" << std::endl;

    const uint8_t* payload = buffer.data() + headerSize;
    size_t payloadSize = buffer.size() - headerSize;

    // Scan for submesh nodes in payload
    // Known node name prefixes in Quantum engine maps
    const char* prefixes[] = {
        "GBrocks", "ABox", "BBox", "AMesh", "BMesh", "Rock", "Ground", "Cliff"
    };

    struct FoundNode {
        std::string name;
        size_t offset;
    };
    std::vector<FoundNode> nodes;

    for (size_t i = 0; i < payloadSize - 8; ++i) {
        for (const char* pre : prefixes) {
            size_t preLen = std::strlen(pre);
            if (i + preLen < payloadSize && std::memcmp(payload + i, pre, preLen) == 0) {
                // Read full ascii node name
                std::string name;
                size_t j = i;
                while (j < payloadSize && payload[j] >= 33 && payload[j] <= 126 && name.size() < 32) {
                    name.push_back((char)payload[j++]);
                }
                if (name.size() >= preLen) {
                    nodes.push_back({name, i});
                    i = j; // skip ahead
                    break;
                }
            }
        }
    }

    // Unique colors for meshes to distinguish geometry
    const u32 palette[] = {
        0xFF88CC44, // Forest / Omnitrix Green
        0xFFD28842, // Canyon Orange/Brown
        0xFFE0C060, // Sandstone Yellow
        0xFF5090C0, // Slate Blue
        0xFFB06040, // Terracotta
        0xFF70B080, // Moss Green
        0xFFA0A0A0, // Grey Rock
    };
    int palIdx = 0;

    // Extract geometry for each node
    for (size_t idx = 0; idx < nodes.size(); ++idx) {
        Mesh mesh;
        mesh.name = nodes[idx].name;
        mesh.meshColor = palette[palIdx % 7];
        palIdx++;

        size_t startOff = nodes[idx].offset;
        size_t endOff = (idx + 1 < nodes.size()) ? nodes[idx + 1].offset : payloadSize;
        if (endOff > startOff + 8192) endOff = startOff + 8192; // Clamp chunk search window

        // Look for 3D coordinate stream in this chunk
        for (size_t p = startOff + mesh.name.size(); p + 12 <= endOff; p += 4) {
            float x = *reinterpret_cast<const float*>(payload + p);
            float y = *reinterpret_cast<const float*>(payload + p + 4);
            float z = *reinterpret_cast<const float*>(payload + p + 8);

            // Level coordinates check
            if (IsValidFloat(x, -500.0f, 500.0f) &&
                IsValidFloat(y, -250.0f, 250.0f) &&
                IsValidFloat(z, -500.0f, 500.0f)) 
            {
                // Must have non-trivial spatial spread
                if (std::abs(x) > 0.05f || std::abs(y) > 0.05f || std::abs(z) > 0.05f) {
                    Vertex v;
                    v.pos = {x, y, z};
                    v.color = mesh.meshColor;

                    // Update local bounds
                    mesh.minBounds.x = std::min(mesh.minBounds.x, x);
                    mesh.minBounds.y = std::min(mesh.minBounds.y, y);
                    mesh.minBounds.z = std::min(mesh.minBounds.z, z);
                    mesh.maxBounds.x = std::max(mesh.maxBounds.x, x);
                    mesh.maxBounds.y = std::max(mesh.maxBounds.y, y);
                    mesh.maxBounds.z = std::max(mesh.maxBounds.z, z);

                    mesh.vertices.push_back(v);
                    p += 8; // advance past Y, Z
                }
            }
        }

        // Generate triangles if we have at least 3 vertices
        if (mesh.vertices.size() >= 3) {
            mesh.center = (mesh.minBounds + mesh.maxBounds) * 0.5f;

            // Generate triangle mesh strips/faces
            for (size_t t = 0; t + 2 < mesh.vertices.size(); t += 3) {
                Triangle tri;
                tri.v0 = (u32)t;
                tri.v1 = (u32)(t + 1);
                tri.v2 = (u32)(t + 2);

                // Compute normal for lighting
                Vector3 edge1 = mesh.vertices[tri.v1].pos - mesh.vertices[tri.v0].pos;
                Vector3 edge2 = mesh.vertices[tri.v2].pos - mesh.vertices[tri.v0].pos;
                Vector3 norm = {
                    edge1.y * edge2.z - edge1.z * edge2.y,
                    edge1.z * edge2.x - edge1.x * edge2.z,
                    edge1.x * edge2.y - edge1.y * edge2.x
                };
                float len = std::sqrt(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z);
                if (len > 1e-4f) {
                    norm.x /= len; norm.y /= len; norm.z /= len;
                }
                mesh.vertices[tri.v0].normal = norm;
                mesh.vertices[tri.v1].normal = norm;
                mesh.vertices[tri.v2].normal = norm;

                mesh.triangles.push_back(tri);
            }

            outMap.totalVertices += mesh.vertices.size();
            outMap.totalTriangles += mesh.triangles.size();

            // Update map-wide bounds
            outMap.boundsMin.x = std::min(outMap.boundsMin.x, mesh.minBounds.x);
            outMap.boundsMin.y = std::min(outMap.boundsMin.y, mesh.minBounds.y);
            outMap.boundsMin.z = std::min(outMap.boundsMin.z, mesh.minBounds.z);
            outMap.boundsMax.x = std::max(outMap.boundsMax.x, mesh.maxBounds.x);
            outMap.boundsMax.y = std::max(outMap.boundsMax.y, mesh.maxBounds.y);
            outMap.boundsMax.z = std::max(outMap.boundsMax.z, mesh.maxBounds.z);

            outMap.meshes.push_back(mesh);
        }
    }

    // Also scan remaining geometry in payload to assemble full terrain
    Mesh terrain;
    terrain.name = "Canyon_Terrain_Base";
    terrain.meshColor = 0xFF4A7D3C; // Earthy canyon base

    for (size_t p = 0; p + 12 <= payloadSize; p += 16) {
        float x = *reinterpret_cast<const float*>(payload + p);
        float y = *reinterpret_cast<const float*>(payload + p + 4);
        float z = *reinterpret_cast<const float*>(payload + p + 8);

        if (IsValidFloat(x, -300.0f, 300.0f) &&
            IsValidFloat(y, -150.0f, 150.0f) &&
            IsValidFloat(z, -400.0f, 400.0f)) 
        {
            if (std::abs(x) > 0.1f && std::abs(y) > 0.1f && std::abs(z) > 0.1f) {
                Vertex v;
                v.pos = {x, y, z};
                v.color = terrain.meshColor;
                terrain.vertices.push_back(v);
            }
        }
    }

    if (terrain.vertices.size() >= 3) {
        for (size_t t = 0; t + 2 < terrain.vertices.size(); t += 3) {
            terrain.triangles.push_back({(u32)t, (u32)(t + 1), (u32)(t + 2)});
        }
        outMap.totalVertices += terrain.vertices.size();
        outMap.totalTriangles += terrain.triangles.size();
        outMap.meshes.push_back(terrain);
    }

    outMap.center = (outMap.boundsMin + outMap.boundsMax) * 0.5f;

    std::cout << "[R4M Parser] Parsed " << outMap.meshes.size() << " meshes, "
              << outMap.totalVertices << " vertices, " 
              << outMap.totalTriangles << " triangles." << std::endl;
    std::cout << "[R4M Parser] Bounds: [" 
              << outMap.boundsMin.x << ", " << outMap.boundsMin.y << ", " << outMap.boundsMin.z << "] to ["
              << outMap.boundsMax.x << ", " << outMap.boundsMax.y << ", " << outMap.boundsMax.z << "]" << std::endl;

    return true;
}

} // namespace Quantum
