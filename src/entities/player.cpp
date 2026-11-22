#include "entities/player.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace Game {

Player::Player()
    : m_position{ 0.0f, 0.0f, 0.0f }
    , m_velocity{ 0.0f, 0.0f, 0.0f }
    , m_yaw(0.0f)
    , m_currentForm(AlienForm::BEN_TENNYSON)
    , m_state(PlayerState::IDLE)
    , m_health(100.0f)
    , m_maxHealth(100.0f)
    , m_omnitrixCharge(100.0f)
    , m_isGrounded(true)
    , m_stateTimer(0.0f)
    , m_lastButtons(0)
{
}

void Player::Reset(const Quantum::Vector3& spawnPos) {
    m_position = spawnPos;
    m_velocity = { 0.0f, 0.0f, 0.0f };
    m_yaw = 0.0f;
    m_currentForm = AlienForm::BEN_TENNYSON;
    m_state = PlayerState::IDLE;
    m_health = 100.0f;
    m_omnitrixCharge = 100.0f;
    m_isGrounded = true;
    m_stateTimer = 0.0f;
    m_lastButtons = 0;
}

float Player::GetMoveSpeed() const {
    switch (m_currentForm) {
        case AlienForm::XLR8:         return 120.0f; // Fast sprinter
        case AlienForm::FOUR_ARMS:    return 40.0f;  // Heavy bruiser
        case AlienForm::HEATBLAST:    return 55.0f;
        case AlienForm::CANNONBOLT:   return 70.0f;
        case AlienForm::WILDVINE:     return 50.0f;
        case AlienForm::BEN_TENNYSON:
        default:                      return 48.0f;  // Standard kid run
    }
}

float Player::GetJumpForce() const {
    switch (m_currentForm) {
        case AlienForm::FOUR_ARMS:    return 22.0f;
        case AlienForm::XLR8:         return 18.0f;
        case AlienForm::CANNONBOLT:   return 14.0f;
        case AlienForm::BEN_TENNYSON:
        default:                      return 16.0f;
    }
}

const char* Player::GetFormName() const {
    switch (m_currentForm) {
        case AlienForm::BEN_TENNYSON: return "Ben Tennyson";
        case AlienForm::FOUR_ARMS:    return "Four Arms";
        case AlienForm::HEATBLAST:    return "Heatblast";
        case AlienForm::XLR8:         return "XLR8";
        case AlienForm::CANNONBOLT:   return "Cannonbolt";
        case AlienForm::WILDVINE:     return "Wildvine";
        default:                      return "Unknown";
    }
}

uint32_t Player::GetFormPrimaryColor() const {
    switch (m_currentForm) {
        case AlienForm::BEN_TENNYSON: return 0xFF00FF7F; // Spring Green / Omnitrix Green
        case AlienForm::FOUR_ARMS:    return 0xFFDC143C; // Crimson Red
        case AlienForm::HEATBLAST:    return 0xFFFF8C00; // Dark Orange / Fire
        case AlienForm::XLR8:         return 0xFF00BFFF; // Deep Sky Blue / Black
        case AlienForm::CANNONBOLT:   return 0xFFFFF8DC; // Cornsilk / Yellowish-White
        case AlienForm::WILDVINE:     return 0xFF228B22; // Forest Green
        default:                      return 0xFFFFFFFF;
    }
}

void Player::TransformTo(AlienForm form) {
    if (m_currentForm == form) return;
    m_currentForm = form;
    m_state = PlayerState::TRANSFORMING;
    m_stateTimer = 0.5f; // half-second transformation pause
    std::cout << "[Player] Omnitrix transformed into: " << GetFormName() << std::endl;
}

void Player::CycleAlien(int direction) {
    int current = static_cast<int>(m_currentForm);
    int total = static_cast<int>(AlienForm::COUNT);
    int next = (current + direction) % total;
    if (next < 0) next += total;
    TransformTo(static_cast<AlienForm>(next));
}

void Player::TakeDamage(float amount) {
    m_health = std::max(0.0f, m_health - amount);
}

void Player::Heal(float amount) {
    m_health = std::min(m_maxHealth, m_health + amount);
}

void Player::Update(float dt, const SceCtrlData& pad) {
    uint32_t pressed = pad.Buttons & ~m_lastButtons;
    m_lastButtons = pad.Buttons;

    if (m_stateTimer > 0.0f) {
        m_stateTimer -= dt;
        if (m_stateTimer <= 0.0f) {
            m_state = PlayerState::IDLE;
        }
    }

    // Omnitrix Selection / Transformation controls (L / R Triggers)
    if (pressed & PSP_CTRL_RTRIGGER) {
        CycleAlien(1);
    } else if (pressed & PSP_CTRL_LTRIGGER) {
        CycleAlien(-1);
    }

    // Omnitrix energy recharge
    if (m_currentForm == AlienForm::BEN_TENNYSON) {
        m_omnitrixCharge = std::min(100.0f, m_omnitrixCharge + dt * 15.0f);
    } else {
        // Alien form drains charge slowly
        m_omnitrixCharge = std::max(0.0f, m_omnitrixCharge - dt * 2.5f);
        if (m_omnitrixCharge <= 0.0f) {
            TransformTo(AlienForm::BEN_TENNYSON); // Revert to human when empty
        }
    }

    // Input Movement Vector (Analog Stick + D-Pad)
    float inputX = 0.0f;
    float inputZ = 0.0f;

    // Analog Stick (Center: 128, range 0-255)
    float stickX = (static_cast<float>(pad.Lx) - 128.0f) / 128.0f;
    float stickY = (static_cast<float>(pad.Ly) - 128.0f) / 128.0f;

    const float deadzone = 0.2f;
    if (std::abs(stickX) > deadzone) inputX = stickX;
    if (std::abs(stickY) > deadzone) inputZ = stickY;

    // D-Pad override
    if (pad.Buttons & PSP_CTRL_LEFT)  inputX = -1.0f;
    if (pad.Buttons & PSP_CTRL_RIGHT) inputX = 1.0f;
    if (pad.Buttons & PSP_CTRL_UP)    inputZ = -1.0f;
    if (pad.Buttons & PSP_CTRL_DOWN)  inputZ = 1.0f;

    float inputLen = std::sqrt(inputX * inputX + inputZ * inputZ);
    if (inputLen > 1.0f) {
        inputX /= inputLen;
        inputZ /= inputLen;
        inputLen = 1.0f;
    }

    // Movement calculation
    float speed = GetMoveSpeed();
    if (inputLen > 0.05f) {
        m_velocity.x = inputX * speed;
        m_velocity.z = inputZ * speed;
        m_yaw = std::atan2(inputX, inputZ); // Face movement direction

        if (m_isGrounded && m_state != PlayerState::ATTACK_LIGHT && m_state != PlayerState::ATTACK_HEAVY) {
            m_state = PlayerState::RUNNING;
        }
    } else {
        // Friction dampening
        m_velocity.x *= 0.75f;
        m_velocity.z *= 0.75f;
        if (std::abs(m_velocity.x) < 0.1f) m_velocity.x = 0.0f;
        if (std::abs(m_velocity.z) < 0.1f) m_velocity.z = 0.0f;

        if (m_isGrounded && m_state == PlayerState::RUNNING) {
            m_state = PlayerState::IDLE;
        }
    }

    // Jump (CROSS / X Button)
    if ((pressed & PSP_CTRL_CROSS) && m_isGrounded) {
        m_velocity.y = GetJumpForce();
        m_isGrounded = false;
        m_state = PlayerState::JUMPING;
    }

    // Attack (SQUARE: Light Attack, TRIANGLE: Heavy Attack)
    if ((pressed & PSP_CTRL_SQUARE) && m_state != PlayerState::ATTACK_LIGHT) {
        m_state = PlayerState::ATTACK_LIGHT;
        m_stateTimer = 0.35f;
    } else if ((pressed & PSP_CTRL_TRIANGLE) && m_state != PlayerState::ATTACK_HEAVY) {
        m_state = PlayerState::ATTACK_HEAVY;
        m_stateTimer = 0.6f;
    }

    // Gravity
    const float gravity = -45.0f;
    if (!m_isGrounded) {
        m_velocity.y += gravity * dt;
        if (m_velocity.y < 0.0f && m_state == PlayerState::JUMPING) {
            m_state = PlayerState::FALLING;
        }
    }

    // Update position
    m_position.x += m_velocity.x * dt;
    m_position.y += m_velocity.y * dt;
    m_position.z += m_velocity.z * dt;

    // Floor collision clamp (Canyon Lookout terrain ground level ~ -39.94f)
    const float groundHeight = -39.94f;
    if (m_position.y <= groundHeight) {
        m_position.y = groundHeight;
        m_velocity.y = 0.0f;
        m_isGrounded = true;
        if (m_state == PlayerState::FALLING) {
            m_state = (inputLen > 0.05f) ? PlayerState::RUNNING : PlayerState::IDLE;
        }
    }

    // Canyon Lookout boundary clamp
    m_position.x = std::clamp(m_position.x, -380.0f, 225.0f);
    m_position.z = std::clamp(m_position.z, -430.0f, 430.0f);
}

} // namespace Game
