#include "quantum/r4s_parser.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace Quantum {

bool StratWad::Load(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[StratWad] Failed to open: " << filepath << std::endl;
        return false;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize < 492) {
        std::cerr << "[StratWad] File too small: " << fileSize << " bytes" << std::endl;
        return false;
    }

    std::vector<uint8_t> buffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        std::cerr << "[StratWad] Read failed: " << filepath << std::endl;
        return false;
    }

    // Verify BIGB Magic
    if (std::memcmp(buffer.data(), "BIGB", 4) != 0) {
        std::cerr << "[StratWad] Invalid magic header: " << filepath << std::endl;
        return false;
    }

    uint32_t headerSize = *reinterpret_cast<const uint32_t*>(&buffer[4]);
    m_levelName = "Level_Strat";

    // Extract level/archive string if present
    for (size_t i = 0x10; i < headerSize && i < buffer.size(); ++i) {
        if (buffer[i] >= 32 && buffer[i] < 127) {
            std::string str;
            while (i < headerSize && buffer[i] >= 32 && buffer[i] < 127) {
                str.push_back((char)buffer[i++]);
            }
            if (!str.empty() && str.find("Wad") == std::string::npos && m_levelName == "Level_Strat") {
                m_levelName = str;
            }
        }
    }

    std::cout << "[StratWad] Loaded: " << filepath << " (Level: " << m_levelName << ")" << std::endl;

    m_entities.clear();
    ParseEntitiesFromBinary(buffer.data(), buffer.size());

    std::cout << "[StratWad] Total entities extracted: " << m_entities.size() << std::endl;
    return true;
}

void StratWad::ParseEntitiesFromBinary(const uint8_t* data, size_t size) {
    // 1. Known entity marker strings in Ben 10 Strat WAD
    struct MarkerDef {
        const char* key;
        EntityType type;
    };

    static const MarkerDef markers[] = {
        {"StartPoint",   EntityType::PLAYER_SPAWN},
        {"VDroneSpawn",  EntityType::ENEMY_SPAWN},
        {"EnemyR",       EntityType::ENEMY_SPAWN},
        {"Trigger",      EntityType::TRIGGER_VOLUME},
        {"CamBoneg",     EntityType::CAMERA_NODE},
        {"Cam_Shake",    EntityType::CAMERA_NODE}
    };

    // Scan for markers and inspect local context
    for (const auto& marker : markers) {
        size_t keyLen = std::strlen(marker.key);
        for (size_t i = 0; i + keyLen < size; ++i) {
            if (std::memcmp(&data[i], marker.key, keyLen) == 0) {
                EntitySpawn ent;
                ent.name = marker.key;
                ent.type = marker.type;

                // Search for nearby float triplet for position (within +/- 128 bytes)
                size_t searchStart = (i >= 128) ? (i - 128) : 0;
                size_t searchEnd   = std::min(size - 12, i + keyLen + 128);

                bool foundPos = false;
                for (size_t f = searchStart; f <= searchEnd; f += 4) {
                    float x = *reinterpret_cast<const float*>(&data[f]);
                    float y = *reinterpret_cast<const float*>(&data[f + 4]);
                    float z = *reinterpret_cast<const float*>(&data[f + 8]);

                    // Level bounds check for Canyon Lookout
                    if (x >= -400.0f && x <= 300.0f &&
                        y >= -500.0f && y <= 150.0f &&
                        z >= -500.0f && z <= 500.0f &&
                        (std::abs(x) > 0.5f || std::abs(z) > 0.5f)) {
                        ent.position = { x, y, z };
                        foundPos = true;
                        break;
                    }
                }

                if (!foundPos) {
                    if (marker.type == EntityType::PLAYER_SPAWN) {
                        ent.position = { -64.0f, -39.94f, -88.5f }; // Canyon start point
                    } else if (marker.type == EntityType::ENEMY_SPAWN) {
                        ent.position = { -135.0f, 11.89f, 47.41f }; // Drone patrol area
                    } else {
                        ent.position = { 0.0f, 0.0f, 0.0f };
                    }
                }

                m_entities.push_back(ent);
            }
        }
    }

    // Ensure at least one player spawn exists
    bool hasPlayer = false;
    for (const auto& ent : m_entities) {
        if (ent.type == EntityType::PLAYER_SPAWN) {
            hasPlayer = true;
            break;
        }
    }

    if (!hasPlayer) {
        EntitySpawn playerSpawn;
        playerSpawn.name = "StartPoint_Default";
        playerSpawn.type = EntityType::PLAYER_SPAWN;
        playerSpawn.position = { -64.0f, -39.94f, -88.5f };
        m_entities.push_back(playerSpawn);
    }
}

bool StratWad::GetPlayerSpawn(Vector3& outPos) const {
    for (const auto& ent : m_entities) {
        if (ent.type == EntityType::PLAYER_SPAWN) {
            outPos = ent.position;
            return true;
        }
    }
    outPos = { -64.0f, -39.94f, -88.5f };
    return false;
}

std::vector<EntitySpawn> StratWad::GetEnemies() const {
    std::vector<EntitySpawn> res;
    for (const auto& ent : m_entities) {
        if (ent.type == EntityType::ENEMY_SPAWN) {
            res.push_back(ent);
        }
    }
    return res;
}

std::vector<EntitySpawn> StratWad::GetTriggers() const {
    std::vector<EntitySpawn> res;
    for (const auto& ent : m_entities) {
        if (ent.type == EntityType::TRIGGER_VOLUME) {
            res.push_back(ent);
        }
    }
    return res;
}

} // namespace Quantum
