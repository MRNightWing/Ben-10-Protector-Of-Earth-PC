#pragma once
#include "quantum/r4s_parser.h"
#include "psp/ctrl.h"
#include <string>

namespace Game {

enum class AlienForm {
    BEN_TENNYSON,
    FOUR_ARMS,
    HEATBLAST,
    XLR8,
    CANNONBOLT,
    WILDVINE,
    COUNT
};

enum class PlayerState {
    IDLE,
    RUNNING,
    JUMPING,
    FALLING,
    ATTACK_LIGHT,
    ATTACK_HEAVY,
    TRANSFORMING
};

class Player {
public:
    Player();
    ~Player() = default;

    void Reset(const Quantum::Vector3& spawnPos);
    void Update(float dt, const SceCtrlData& pad);

    // Getters
    const Quantum::Vector3& GetPosition() const { return m_position; }
    const Quantum::Vector3& GetVelocity() const { return m_velocity; }
    float GetYaw() const { return m_yaw; }
    AlienForm GetCurrentForm() const { return m_currentForm; }
    PlayerState GetState() const { return m_state; }
    float GetHealth() const { return m_health; }
    float GetMaxHealth() const { return m_maxHealth; }
    float GetOmnitrixCharge() const { return m_omnitrixCharge; }
    bool IsGrounded() const { return m_isGrounded; }
    bool IsAttacking() const { return m_state == PlayerState::ATTACK_LIGHT || m_state == PlayerState::ATTACK_HEAVY; }
    float GetAttackDamage() const;
    float GetAttackRadius() const;

    const char* GetFormName() const;
    uint32_t GetFormPrimaryColor() const;

    // Actions
    void TransformTo(AlienForm form);
    void CycleAlien(int direction);
    void TakeDamage(float amount);
    void Heal(float amount);

    void SetPosition(const Quantum::Vector3& pos) { m_position = pos; }

private:
    Quantum::Vector3 m_position;
    Quantum::Vector3 m_velocity;
    float m_yaw; // in radians

    AlienForm m_currentForm;
    PlayerState m_state;

    float m_health;
    float m_maxHealth;
    float m_omnitrixCharge;
    bool m_isGrounded;

    float m_stateTimer;
    uint32_t m_lastButtons;

    float GetMoveSpeed() const;
    float GetJumpForce() const;
};

} // namespace Game
