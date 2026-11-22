#pragma once
#include "quantum/r4m_parser.h"
#include <string>
#include <vector>

namespace Game {

class Player;

enum class EnemyType {
    VILGAX_DRONE,
    ROBOT_MINION
};

enum class EnemyState {
    IDLE,
    PATROL,
    CHASE,
    ATTACK_PREP,
    ATTACK_FIRE,
    HITSTUN,
    DEAD
};

struct LaserBolt {
    Quantum::Vector3 pos;
    Quantum::Vector3 vel;
    float life = 2.0f;
    bool active = true;
};

class EnemyDrone {
public:
    EnemyDrone(const Quantum::Vector3& spawnPos, EnemyType type = EnemyType::VILGAX_DRONE);
    ~EnemyDrone() = default;

    void Update(float dt, Player& player);
    void TakeDamage(float amount, const Quantum::Vector3& knockbackDir);

    // Getters
    const Quantum::Vector3& GetPosition() const { return m_position; }
    float GetYaw() const { return m_yaw; }
    float GetHealth() const { return m_health; }
    float GetMaxHealth() const { return m_maxHealth; }
    EnemyState GetState() const { return m_state; }
    EnemyType GetType() const { return m_type; }
    bool IsDead() const { return m_state == EnemyState::DEAD; }
    float GetHoverOffset() const { return m_hoverOffset; }
    bool IsFlashing() const { return m_flashTimer > 0.0f; }

    const std::vector<LaserBolt>& GetLaserBolts() const { return m_laserBolts; }

private:
    EnemyType m_type;
    EnemyState m_state;
    Quantum::Vector3 m_position;
    Quantum::Vector3 m_spawnPos;
    Quantum::Vector3 m_velocity;
    float m_yaw;

    float m_health;
    float m_maxHealth;
    float m_stateTimer;
    float m_attackCooldown;
    float m_flashTimer;
    float m_animTimer;
    float m_hoverOffset;

    std::vector<LaserBolt> m_laserBolts;

    void FireLaser(const Quantum::Vector3& targetPos);
};

} // namespace Game
