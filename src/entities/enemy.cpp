#include "entities/enemy.h"
#include "entities/player.h"
#include "audio/audio.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace Game {

EnemyDrone::EnemyDrone(const Quantum::Vector3& spawnPos, EnemyType type)
    : m_type(type)
    , m_state(EnemyState::PATROL)
    , m_position(spawnPos)
    , m_spawnPos(spawnPos)
    , m_velocity{0.0f, 0.0f, 0.0f}
    , m_yaw(0.0f)
    , m_health(60.0f)
    , m_maxHealth(60.0f)
    , m_stateTimer(0.0f)
    , m_attackCooldown(1.5f)
    , m_flashTimer(0.0f)
    , m_animTimer(0.0f)
    , m_hoverOffset(0.0f)
{
}

void EnemyDrone::FireLaser(const Quantum::Vector3& targetPos) {
    Quantum::Vector3 dir = targetPos - m_position;
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 0.001f) {
        dir.x /= len;
        dir.y /= len;
        dir.z /= len;
    }

    LaserBolt bolt;
    bolt.pos = m_position + Quantum::Vector3{0.0f, 2.0f, 0.0f};
    const float laserSpeed = 65.0f;
    bolt.vel = dir * laserSpeed;
    bolt.life = 2.5f;
    bolt.active = true;

    m_laserBolts.push_back(bolt);
    Audio::AudioEngine::Instance().PlaySFX(Audio::SFX_DRONE_LASER);
}

void EnemyDrone::TakeDamage(float amount, const Quantum::Vector3& knockbackDir) {
    if (m_state == EnemyState::DEAD) return;

    m_health -= amount;
    m_flashTimer = 0.2f;

    if (m_health <= 0.0f) {
        m_health = 0.0f;
        m_state = EnemyState::DEAD;
        m_stateTimer = 1.0f;
        Audio::AudioEngine::Instance().PlaySFX(Audio::SFX_DRONE_EXPLODE);
        std::cout << "[Enemy] Vilgax Drone destroyed!" << std::endl;
    } else {
        m_state = EnemyState::HITSTUN;
        m_stateTimer = 0.35f;
        m_velocity = knockbackDir * 20.0f;
        Audio::AudioEngine::Instance().PlaySFX(Audio::SFX_DRONE_HIT);
    }
}

void EnemyDrone::Update(float dt, Player& player) {
    m_animTimer += dt;
    m_hoverOffset = std::sin(m_animTimer * 4.0f) * 1.5f;

    if (m_flashTimer > 0.0f) {
        m_flashTimer -= dt;
    }

    // Update active laser projectiles
    const auto& playerPos = player.GetPosition();
    for (auto& bolt : m_laserBolts) {
        if (!bolt.active) continue;

        bolt.pos.x += bolt.vel.x * dt;
        bolt.pos.y += bolt.vel.y * dt;
        bolt.pos.z += bolt.vel.z * dt;
        bolt.life -= dt;

        if (bolt.life <= 0.0f) {
            bolt.active = false;
            continue;
        }

        // Hit check against Ben 10
        float bdx = bolt.pos.x - playerPos.x;
        float bdy = bolt.pos.y - (playerPos.y + 4.0f);
        float bdz = bolt.pos.z - playerPos.z;
        float bDistSq = bdx * bdx + bdy * bdy + bdz * bdz;

        if (bDistSq < 16.0f) { // 4.0 unit radius hit
            player.TakeDamage(10.0f);
            bolt.active = false;
        }
    }

    // Remove inactive bolts
    m_laserBolts.erase(
        std::remove_if(m_laserBolts.begin(), m_laserBolts.end(), [](const LaserBolt& b) { return !b.active; }),
        m_laserBolts.end()
    );

    if (m_state == EnemyState::DEAD) {
        // Drone falls and disintegrates
        m_position.y -= 25.0f * dt;
        return;
    }

    // Timer countdowns
    if (m_attackCooldown > 0.0f) m_attackCooldown -= dt;

    if (m_state == EnemyState::HITSTUN) {
        m_position.x += m_velocity.x * dt;
        m_position.z += m_velocity.z * dt;
        m_velocity.x *= 0.85f;
        m_velocity.z *= 0.85f;

        m_stateTimer -= dt;
        if (m_stateTimer <= 0.0f) {
            m_state = EnemyState::CHASE;
        }
        return;
    }

    // Calculate distance and vector to Player (Ben 10)
    float dx = playerPos.x - m_position.x;
    float dz = playerPos.z - m_position.z;
    float dist = std::sqrt(dx * dx + dz * dz);

    // Drone AI State Machine
    const float detectionRange = 90.0f;
    const float attackRange = 35.0f;
    const float moveSpeed = 16.0f;

    if (dist < detectionRange) {
        // Face towards player
        m_yaw = std::atan2(dx, dz);

        if (dist > attackRange) {
            // Chase Ben
            m_state = EnemyState::CHASE;
            float dirX = dx / dist;
            float dirZ = dz / dist;
            m_position.x += dirX * moveSpeed * dt;
            m_position.z += dirZ * moveSpeed * dt;
        } else {
            // In attack range
            if (m_attackCooldown <= 0.0f) {
                m_state = EnemyState::ATTACK_FIRE;
                FireLaser(playerPos + Quantum::Vector3{0.0f, 4.0f, 0.0f});
                m_attackCooldown = 1.8f;
            } else {
                m_state = EnemyState::IDLE;
            }
        }
    } else {
        // Patrol around spawn origin
        m_state = EnemyState::PATROL;
        float patrolRadius = 15.0f;
        float targetX = m_spawnPos.x + std::sin(m_animTimer * 0.8f) * patrolRadius;
        float targetZ = m_spawnPos.z + std::cos(m_animTimer * 0.8f) * patrolRadius;
        float pdx = targetX - m_position.x;
        float pdz = targetZ - m_position.z;
        float pdist = std::sqrt(pdx * pdx + pdz * pdz);
        if (pdist > 0.5f) {
            m_yaw = std::atan2(pdx, pdz);
            m_position.x += (pdx / pdist) * (moveSpeed * 0.5f) * dt;
            m_position.z += (pdz / pdist) * (moveSpeed * 0.5f) * dt;
        }
    }

    // Check if Ben hits this drone with melee attack
    if (player.IsAttacking()) {
        float atkRadius = player.GetAttackRadius();
        if (dist <= atkRadius) {
            // Check if player is facing drone (within ~110 degree cone)
            float pYaw = player.GetYaw();
            float pForwardX = std::sin(pYaw);
            float pForwardZ = std::cos(pYaw);

            float toEnemyX = dx / dist;
            float toEnemyZ = dz / dist;
            float dot = -toEnemyX * pForwardX + -toEnemyZ * pForwardZ; // Facing dot

            if (dot > -0.6f) { // In front of player
                Quantum::Vector3 knockDir = {-pForwardX, 0.2f, pForwardZ};
                TakeDamage(player.GetAttackDamage(), knockDir);
            }
        }
    }
}

} // namespace Game
