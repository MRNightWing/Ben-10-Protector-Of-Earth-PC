#pragma once
#include "psp/types.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Quantum {

struct Vector2 {
    float u = 0.0f;
    float v = 0.0f;
};

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default;
    Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vector3 operator+(const Vector3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vector3 operator-(const Vector3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }
};

struct Vertex {
    Vector3 pos;
    Vector3 normal;
    Vector2 uv;
    u32 color = 0xFFFFFFFF;
};

struct Triangle {
    u32 v0 = 0;
    u32 v1 = 0;
    u32 v2 = 0;
};

struct Mesh {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
    Vector3 minBounds{1e9f, 1e9f, 1e9f};
    Vector3 maxBounds{-1e9f, -1e9f, -1e9f};
    Vector3 center{0.0f, 0.0f, 0.0f};
    u32 meshColor = 0xFF55FF55; // Default Omnitrix green tint
};

struct LevelMap {
    std::string levelName;
    std::string author;
    std::string buildFlags;
    u32 version = 0;
    u32 soundHeapSize = 0;
    std::vector<Mesh> meshes;

    Vector3 boundsMin{1e9f, 1e9f, 1e9f};
    Vector3 boundsMax{-1e9f, -1e9f, -1e9f};
    Vector3 center{0.0f, 0.0f, 0.0f};
    size_t totalVertices = 0;
    size_t totalTriangles = 0;
};

// Parser API
bool ParseR4M(const std::string& filepath, LevelMap& outMap);

} // namespace Quantum
