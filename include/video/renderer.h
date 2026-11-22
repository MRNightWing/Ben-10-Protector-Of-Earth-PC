#pragma once
#include "psp/types.h"
#include "psp/display.h"
#include "quantum/r4m_parser.h"

namespace Game {
    class Player;
    class EnemyDrone;
}

// Renderer API
bool HAL_InitVideo(const char* window_title, int scale);
void HAL_ShutdownVideo();
void HAL_SetCurrentFramebuffer(void* buffer, int bufferwidth, int pixelformat);
void HAL_PresentFrame();
bool HAL_PollWindowEvents();

void HAL_SetWireframeMode(bool enabled);
bool HAL_GetWireframeMode();

// Access internal test framebuffer
u32* HAL_GetDebugFramebuffer();

// 3D Level Rendering Pipeline
struct Camera3D {
    float x = 0.0f;
    float y = 50.0f;
    float z = -150.0f;
    float yaw = 0.0f;    // Radians
    float pitch = 0.2f;  // Radians
    float fov = 60.0f;   // Degrees
};

void HAL_Clear3D(u32 clearColor);
void HAL_Render3DMap(const Quantum::LevelMap& map, const Camera3D& camera, bool wireframe);
void HAL_RenderPlayer(const Game::Player& player, const Camera3D& camera);
void HAL_RenderEnemies(const std::vector<Game::EnemyDrone>& enemies, const Camera3D& camera);
void HAL_RenderHUD(const Game::Player& player);
void HAL_DrawString(int x, int y, const char* str, u32 color);
