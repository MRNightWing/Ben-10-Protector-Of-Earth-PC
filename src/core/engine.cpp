#include "core/engine.h"
#include "psp/ctrl.h"
#include "psp/display.h"
#include "psp/kernel.h"
#include "video/renderer.h"
#include "quantum/streamer.h"
#include <cstdio>
#include <cmath>
#include <cstring>

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

    // Initial Camera Setup for Canyon Lookout
    m_Camera.x = 60.0f;
    m_Camera.y = 80.0f;
    m_Camera.z = -220.0f;
    m_Camera.yaw = 0.0f;
    m_Camera.pitch = 0.25f;
    m_Camera.fov = 60.0f;

    // Load Level 11_01 via CStreamManager (0x00081F5C)
    printf("[Engine] Loading starting level: %s\n", m_InitialLevel.c_str());
    Quantum::StreamManager::Instance().LoadMapAndStrats(m_InitialLevel);

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

    // Camera Rotation via Analog Stick
    float stickX = (pad.Lx - 128) / 128.0f;
    float stickY = (pad.Ly - 128) / 128.0f;

    if (std::abs(stickX) > 0.15f) {
        m_Camera.yaw += stickX * 2.0f * dt;
    }
    if (std::abs(stickY) > 0.15f) {
        m_Camera.pitch += stickY * 2.0f * dt;
        if (m_Camera.pitch > 1.4f) m_Camera.pitch = 1.4f;
        if (m_Camera.pitch < -1.4f) m_Camera.pitch = -1.4f;
    }

    // Camera Movement via D-Pad
    float moveSpeed = 120.0f * dt;
    float sinY = std::sin(m_Camera.yaw);
    float cosY = std::cos(m_Camera.yaw);

    if (pad.Buttons & PSP_CTRL_UP) {
        m_Camera.x += sinY * moveSpeed;
        m_Camera.z += cosY * moveSpeed;
    }
    if (pad.Buttons & PSP_CTRL_DOWN) {
        m_Camera.x -= sinY * moveSpeed;
        m_Camera.z -= cosY * moveSpeed;
    }
    if (pad.Buttons & PSP_CTRL_LEFT) {
        m_Camera.x -= cosY * moveSpeed;
        m_Camera.z += sinY * moveSpeed;
    }
    if (pad.Buttons & PSP_CTRL_RIGHT) {
        m_Camera.x += cosY * moveSpeed;
        m_Camera.z -= sinY * moveSpeed;
    }

    // Height adjustment via Shoulder Triggers (L / R)
    if (pad.Buttons & PSP_CTRL_LTRIGGER) m_Camera.y += moveSpeed;
    if (pad.Buttons & PSP_CTRL_RTRIGGER) m_Camera.y -= moveSpeed;

    // Slow turntable auto-rotation in test mode to show off 3D geometry
    if (m_TestMode) {
        m_Camera.yaw += 0.02f;
    }
}

void Engine::Render() {
    // 1. Clear Framebuffer to Canyon Night / Atmosphere Color (ABGR)
    HAL_Clear3D(0xFF1B1418);

    // 2. Render 3D Map Geometry if loaded
    const Quantum::LevelMap* map = Quantum::StreamManager::Instance().GetCurrentMap();
    if (map) {
        HAL_Render3DMap(*map, m_Camera, true);
    }

    // 3. Render HUD and Diagnostics
    RenderDebugOverlay();
}

void Engine::RenderDebugOverlay() {
    u32* fb = HAL_GetDebugFramebuffer();
    if (!fb) return;

    // Header HUD
    HAL_DrawString(8, 8, "BEN 10: PROTECTOR OF EARTH - PC PORT (C++20/SDL2)", 0xFF00FF88);

    const Quantum::LevelMap* map = Quantum::StreamManager::Instance().GetCurrentMap();
    if (map) {
        char buf[128];
        snprintf(buf, sizeof(buf), "LEVEL: %s (%s)", map->levelName.c_str(), Quantum::StreamManager::Instance().GetCurrentLevelId().c_str());
        HAL_DrawString(8, 20, buf, 0xFFFFFFFF);

        snprintf(buf, sizeof(buf), "MESHES: %zu  VERTS: %zu  TRIS: %zu", map->meshes.size(), map->totalVertices, map->totalTriangles);
        HAL_DrawString(8, 30, buf, 0xFF88CCFF);
    }

    // Camera Stats
    char camBuf[128];
    snprintf(camBuf, sizeof(camBuf), "CAM: [%.1f, %.1f, %.1f] YAW: %.2f PITCH: %.2f",
             m_Camera.x, m_Camera.y, m_Camera.z, m_Camera.yaw, m_Camera.pitch);
    HAL_DrawString(8, 42, camBuf, 0xFFAAAAAA);

    // Controls Helper
    HAL_DrawString(8, PSP_SCREEN_HEIGHT - 16, "STICK: ROTATE | DPAD: MOVE | L/R: ELEVATION | SEL+START: EXIT", 0xFF66EE66);

    // Analog Stick Input Visualizer (Bottom-Right radar)
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);

    int center_x = PSP_SCREEN_WIDTH - 30;
    int center_y = PSP_SCREEN_HEIGHT - 30;
    int stick_off_x = (int)((pad.Lx - 128) * 0.15f);
    int stick_off_y = (int)((pad.Ly - 128) * 0.15f);

    // Draw circular boundary
    for (int dy = -18; dy <= 18; ++dy) {
        for (int dx = -18; dx <= 18; ++dx) {
            int dsq = dx * dx + dy * dy;
            if (dsq >= 16 * 16 && dsq <= 18 * 18) {
                int px = center_x + dx;
                int py = center_y + dy;
                if (px >= 0 && px < PSP_SCREEN_WIDTH && py >= 0 && py < PSP_SCREEN_HEIGHT) {
                    fb[py * PSP_SCREEN_BUFFER_WIDTH + px] = 0xFF448844;
                }
            }
        }
    }

    // Draw cursor
    for (int dy = -3; dy <= 3; ++dy) {
        for (int dx = -3; dx <= 3; ++dx) {
            int px = center_x + stick_off_x + dx;
            int py = center_y + stick_off_y + dy;
            if (px >= 0 && px < PSP_SCREEN_WIDTH && py >= 0 && py < PSP_SCREEN_HEIGHT) {
                fb[py * PSP_SCREEN_BUFFER_WIDTH + px] = 0xFF00FF00;
            }
        }
    }
}
