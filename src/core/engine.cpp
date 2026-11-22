#include "core/engine.h"
#include "psp/ctrl.h"
#include "psp/display.h"
#include "psp/kernel.h"
#include "video/renderer.h"
#include "quantum/streamer.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>

Engine& Engine::Instance() {
    static Engine s_Instance;
    return s_Instance;
}

bool Engine::Init(int argc, char* argv[]) {
    printf("==================================================\n");
    printf("  Ben 10: Protector of Earth - PC Port (Quantum)  \n");
    printf("  High Voltage Software / Sony PSP HAL Wrapper   \n");
    printf("==================================================\n");

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--test") == 0) {
            m_TestMode = true;
            printf("[Engine] Running in automated test mode (120 frames @ 60 FPS).\n");
        }
        if (strcmp(argv[i], "--level") == 0 && i + 1 < argc) {
            m_InitialLevel = argv[++i];
        }
    }

    // Initialize Video & SDL2 Subsystems (2x scale = 960x544)
    if (!HAL_InitVideo("Ben 10: Protector of Earth (PC Port)", 2)) {
        return false;
    }

    // Initialize Quantum Engine Volatile Memory Pool
    Engine_VolatileMemInit();

    // Set Controller Sampling Mode
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    // Load Level 11_01 via CStreamManager (0x00081F5C)
    printf("[Engine] Loading starting level: %s\n", m_InitialLevel.c_str());
    Quantum::StreamManager::Instance().LoadMapAndStrats(m_InitialLevel);

    // Load Strat WAD (Entities, Spawners, Triggers)
    std::string stratPath = "assets/levels/" + m_InitialLevel + "/" + m_InitialLevel + ".r4s";
    m_StratWad.Load(stratPath);

    // Spawn Ben 10 at Strat Start Point
    Quantum::Vector3 spawnPos;
    m_StratWad.GetPlayerSpawn(spawnPos);
    m_Player.Reset(spawnPos);
    printf("[Engine] Spawned Ben 10 at (%.2f, %.2f, %.2f)\n", spawnPos.x, spawnPos.y, spawnPos.z);

    // Spawn Enemies from Strat WAD
    auto enemySpawns = m_StratWad.GetEnemies();
    m_Enemies.clear();
    for (const auto& spawn : enemySpawns) {
        m_Enemies.emplace_back(spawn.position, Game::EnemyType::VILGAX_DRONE);
    }

    // Ensure active Vilgax Drones are present in Canyon Lookout arena
    if (m_Enemies.size() < 3) {
        m_Enemies.emplace_back(Quantum::Vector3{-85.0f, -39.94f, -40.0f});
        m_Enemies.emplace_back(Quantum::Vector3{-135.0f, -39.94f, 47.0f});
        m_Enemies.emplace_back(Quantum::Vector3{-40.0f, -39.94f, -90.0f});
    }
    printf("[Engine] Spawned %zu Vilgax Drones in arena\n", m_Enemies.size());

    // Setup Third-Person Camera
    m_Camera.yaw = 0.0f;
    m_Camera.pitch = 0.25f;
    m_Camera.fov = 60.0f;
    m_Camera.x = spawnPos.x;
    m_Camera.y = spawnPos.y + 25.0f;
    m_Camera.z = spawnPos.z - 50.0f;

    m_Running = true;
    m_FrameCounter = 0;
    return true;
}

void Engine::Shutdown() {
    printf("[Engine] Shutting down...\n");
    HAL_ShutdownVideo();
    m_Running = false;
}

void Engine::Run() {
    MainLoop();
}

// Reconstructed main loop lifecycle based on 0x0008E298
void Engine::MainLoop() {
    while (m_Running) {
        // 1. Check Window / OS Events
        if (!HAL_PollWindowEvents()) {
            m_Running = false;
            break;
        }

        // 2. Poll Input (Input_PollController @ 0x0007141C)
        Input_PollController();

        // 3. Engine Tick / Simulation
        float dt = HAL_GetDeltaTime();
        Tick(dt);

        // 4. Render Frame
        Render();

        // 5. Present Frame to Display
        HAL_PresentFrame();

        // 6. Synchronize with VBlank (60 FPS limiter @ 0x0008E4D0)
        Engine_WaitVblank(1);

        m_FrameCounter++;
    }
}

void Engine::Tick(float dt) {
    if (m_TestMode && m_FrameCounter >= 120) {
        printf("[Engine] Test run reached %u frames successfully. Exiting.\n", m_FrameCounter);
        m_Running = false;
    }

    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);

    // Exit shortcut: Select + Start
    if ((pad.Buttons & PSP_CTRL_SELECT) && (pad.Buttons & PSP_CTRL_START)) {
        printf("[Engine] Select + Start pressed. Exiting.\n");
        m_Running = false;
    }

    // Update Player (Ben 10) Physics, Input, and Omnitrix State
    m_Player.Update(dt, pad);

    // Update Enemies (Vilgax Drones AI, Combat, Lasers)
    for (auto& enemy : m_Enemies) {
        enemy.Update(dt, m_Player);
    }

    // Smooth Third-Person Chase Camera
    const auto& pPos = m_Player.GetPosition();
    float pYaw = m_Player.GetYaw();

    const float chaseDist = 55.0f;
    const float chaseHeight = 28.0f;
    float targetCamX = pPos.x - std::sin(pYaw) * chaseDist;
    float targetCamZ = pPos.z - std::cos(pYaw) * chaseDist;
    float targetCamY = pPos.y + chaseHeight;

    // Smooth camera lag
    float followSpeed = 6.0f;
    m_Camera.x += (targetCamX - m_Camera.x) * followSpeed * dt;
    m_Camera.y += (targetCamY - m_Camera.y) * followSpeed * dt;
    m_Camera.z += (targetCamZ - m_Camera.z) * followSpeed * dt;

    // Camera look angle (faces Ben)
    float dx = pPos.x - m_Camera.x;
    float dz = pPos.z - m_Camera.z;
    m_Camera.yaw = std::atan2(dx, dz);
    m_Camera.pitch = 0.22f;

    // Test mode simulated movement
    if (m_TestMode) {
        // Automatically cycle alien form during test mode to verify all states
        if (m_FrameCounter == 30)  m_Player.CycleAlien(1);  // Four Arms
        if (m_FrameCounter == 60)  m_Player.CycleAlien(1);  // Heatblast
        if (m_FrameCounter == 90)  m_Player.CycleAlien(1);  // XLR8
    }
}

void Engine::Render() {
    // 1. Clear Framebuffer to Canyon Night / Atmosphere Color (ABGR)
    HAL_Clear3D(0xFF1B1418);

    // 2. Render 3D Map Geometry (Solid Shaded with Depth Buffer & Lighting)
    const Quantum::LevelMap* map = Quantum::StreamManager::Instance().GetCurrentMap();
    if (map) {
        HAL_Render3DMap(*map, m_Camera, m_WireframeMode);
    }

    // 3. Render Enemies (Vilgax Drones + Laser Bolts + 3D Health Bars)
    HAL_RenderEnemies(m_Enemies, m_Camera);

    // 4. Render Player (Ben 10 / Active Alien) in Solid 3D
    HAL_RenderPlayer(m_Player, m_Camera);

    // 5. Render Game HUD (Ben 10 HP, Omnitrix, Alien Indicator)
    HAL_RenderHUD(m_Player);

    // 6. Render Diagnostics Overlay
    RenderDebugOverlay();
}

void Engine::RenderDebugOverlay() {
    u32* fb = HAL_GetDebugFramebuffer();
    if (!fb) return;

    // Header HUD
    HAL_DrawString(120, 10, "BEN 10: PROTECTOR OF EARTH [QUANTUM ENGINE PC]", 0xFF00FF88);

    const Quantum::LevelMap* map = Quantum::StreamManager::Instance().GetCurrentMap();
    if (map) {
        int aliveDrones = 0;
        for (const auto& e : m_Enemies) if (!e.IsDead()) aliveDrones++;

        char buf[128];
        snprintf(buf, sizeof(buf), "LEVEL: %s | DRONES: %d ALIVE | SHADING: %s",
                 map->levelName.c_str(),
                 aliveDrones,
                 HAL_GetWireframeMode() ? "WIREFRAME" : "SOLID SHADED (Z-BUFFER)");
        HAL_DrawString(120, 22, buf, 0xFFFFFFFF);
    }

    // Player position stats
    const auto& pos = m_Player.GetPosition();
    char pBuf[128];
    snprintf(pBuf, sizeof(pBuf), "POS: [%.1f, %.1f, %.1f] | YAW: %.2f | FORM: %s",
             pos.x, pos.y, pos.z, m_Player.GetYaw(), m_Player.GetFormName());
    HAL_DrawString(120, 34, pBuf, 0xFF88CCFF);

    // Analog Stick Input Visualizer (Bottom-Right radar)
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);

    int center_x = PSP_SCREEN_WIDTH - 25;
    int center_y = PSP_SCREEN_HEIGHT - 25;
    int stick_off_x = (int)((pad.Lx - 128) * 0.12f);
    int stick_off_y = (int)((pad.Ly - 128) * 0.12f);

    // Draw circular boundary
    for (int dy = -15; dy <= 15; ++dy) {
        for (int dx = -15; dx <= 15; ++dx) {
            int dsq = dx * dx + dy * dy;
            if (dsq >= 13 * 13 && dsq <= 15 * 15) {
                int px = center_x + dx;
                int py = center_y + dy;
                if (px >= 0 && px < PSP_SCREEN_WIDTH && py >= 0 && py < PSP_SCREEN_HEIGHT) {
                    fb[py * PSP_SCREEN_BUFFER_WIDTH + px] = 0xFF448844;
                }
            }
        }
    }

    // Draw cursor
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            int px = center_x + stick_off_x + dx;
            int py = center_y + stick_off_y + dy;
            if (px >= 0 && px < PSP_SCREEN_WIDTH && py >= 0 && py < PSP_SCREEN_HEIGHT) {
                fb[py * PSP_SCREEN_BUFFER_WIDTH + px] = 0xFF00FF00;
            }
        }
    }
}
