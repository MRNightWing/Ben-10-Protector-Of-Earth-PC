#pragma once
#include "quantum/r4m_parser.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Quantum {

enum class EntityType {
    PLAYER_SPAWN,
    ENEMY_SPAWN,
    TRIGGER_VOLUME,
    WAYPOINT,
    INTERACTIVE_OBJECT,
    CAMERA_NODE,
    UNKNOWN
};

struct EntitySpawn {
    std::string name;
    EntityType type = EntityType::UNKNOWN;
    Vector3 position;
    Vector3 rotation;
    Vector3 boundsMin;
    Vector3 boundsMax;
    uint32_t flags = 0;
};

class StratWad {
public:
    StratWad() = default;
    ~StratWad() = default;

    bool Load(const std::string& filepath);

    const std::vector<EntitySpawn>& GetEntities() const { return m_entities; }
    bool GetPlayerSpawn(Vector3& outPos) const;
    std::vector<EntitySpawn> GetEnemies() const;
    std::vector<EntitySpawn> GetTriggers() const;

    size_t GetEntityCount() const { return m_entities.size(); }
    const std::string& GetLevelName() const { return m_levelName; }

private:
    std::string m_levelName;
    std::vector<EntitySpawn> m_entities;
    void ParseEntitiesFromBinary(const uint8_t* data, size_t size);
};

} // namespace Quantum
