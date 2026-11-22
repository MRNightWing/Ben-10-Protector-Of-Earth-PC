#include "core/engine.h"
#include "psp/ctrl.h"
#include "psp/display.h"
#include "psp/kernel.h"
#include "video/renderer.h"
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
            printf("[Engine] Running in automated test mode (will run 120 frames @ 60 FPS then exit).\n");
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
    // Check for Exit shortcut: Select + Start or 120 frames in test mode
    if (m_TestMode && m_FrameCounter >= 120) {
        printf("[Engine] Test run reached %u frames successfully. Exiting.\n", m_FrameCounter);
        m_Running = false;
    }

    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);
    if ((pad.Buttons & PSP_CTRL_SELECT) && (pad.Buttons & PSP_CTRL_START)) {
        printf("[Engine] Select + Start pressed. Exiting.\n");
        m_Running = false;
    }
}

void Engine::Render() {
    RenderDebugOverlay();
}

// Procedural visual test pattern to verify framebuffer output & input responsiveness
void Engine::RenderDebugOverlay() {
    u32* fb = HAL_GetDebugFramebuffer();
    if (!fb) return;

    // Check for Exit shortcut: Select + Start or 120 frames in test mode
    if (m_TestMode && m_FrameCounter >= 120) {
        printf("[Engine] Test run reached %u frames successfully. Exiting.\n", m_FrameCounter);
        m_Running = false;
    }

    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad, 1);

    // Dynamic background gradient animated with frame counter
    float time = m_FrameCounter * 0.03f;
    for (int y = 0; y < PSP_SCREEN_HEIGHT; ++y) {
        for (int x = 0; x < PSP_SCREEN_WIDTH; ++x) {
            // Subtle dark grid pattern
            u8 r = (u8)(16 + 12 * sinf(time + x * 0.015f));
            u8 g = (u8)(24 + 16 * cosf(time + y * 0.015f));
            u8 b = (u8)(32 + 20 * sinf(time + (x + y) * 0.01f));

            // Omnitrix Green tint
            g = (u8)(g > 220 ? 220 : g + 25);

            // Grid lines every 32 pixels
            if ((x % 32 == 0) || (y % 32 == 0)) {
                r += 20; g += 30; b += 20;
            }

            fb[y * PSP_SCREEN_BUFFER_WIDTH + x] = 0xFF000000 | (b << 16) | (g << 8) | r;
        }
    }

    // Draw Analog Stick Indicator (circle cursor in bottom-left corner)
    int center_x = 70;
    int center_y = PSP_SCREEN_HEIGHT - 70;
    int stick_off_x = (int)((pad.Lx - 128) * 0.35f);
    int stick_off_y = (int)((pad.Ly - 128) * 0.35f);

    for (int dy = -25; dy <= 25; ++dy) {
        for (int dx = -25; dx <= 25; ++dx) {
            int dist_sq = dx * dx + dy * dy;
            if (dist_sq <= 25 * 25) {
                int px = center_x + dx;
                int py = center_y + dy;
                if (px >= 0 && px < PSP_SCREEN_WIDTH && py >= 0 && py < PSP_SCREEN_HEIGHT) {
                    fb[py * PSP_SCREEN_BUFFER_WIDTH + px] = 0xFF303030; // base circle
                }
            }
        }
    }

    // Stick thumb
    for (int dy = -8; dy <= 8; ++dy) {
        for (int dx = -8; dx <= 8; ++dx) {
            if (dx * dx + dy * dy <= 8 * 8) {
                int px = center_x + stick_off_x + dx;
                int py = center_y + stick_off_y + dy;
                if (px >= 0 && px < PSP_SCREEN_WIDTH && py >= 0 && py < PSP_SCREEN_HEIGHT) {
                    // Bright green omnitrix cursor
                    fb[py * PSP_SCREEN_BUFFER_WIDTH + px] = 0xFF00FF44;
                }
            }
        }
    }

    // Draw Button Status Boxes (top-right)
    auto draw_indicator = [&](int x, int y, int w, int h, bool active, u32 active_color) {
        u32 color = active ? active_color : 0xFF404040;
        for (int iy = 0; iy < h; ++iy) {
            for (int ix = 0; ix < w; ++ix) {
                int px = x + ix;
                int py = y + iy;
                if (px >= 0 && px < PSP_SCREEN_WIDTH && py >= 0 && py < PSP_SCREEN_HEIGHT) {
                    fb[py * PSP_SCREEN_BUFFER_WIDTH + px] = color;
                }
            }
        }
    };

    // Cross (A / K / Space) -> Green
    draw_indicator(430, 180, 14, 14, (pad.Buttons & PSP_CTRL_CROSS) != 0, 0xFF00E060);
    // Circle (B / L) -> Red
    draw_indicator(450, 160, 14, 14, (pad.Buttons & PSP_CTRL_CIRCLE) != 0, 0xFF4040E0);
    // Square (X / J) -> Pink
    draw_indicator(410, 160, 14, 14, (pad.Buttons & PSP_CTRL_SQUARE) != 0, 0xFFE040E0);
    // Triangle (Y / I) -> Teal
    draw_indicator(430, 140, 14, 14, (pad.Buttons & PSP_CTRL_TRIANGLE) != 0, 0xFF00D0D0);

    // L / R Triggers (Q / E)
    draw_indicator(30, 30, 40, 12, (pad.Buttons & PSP_CTRL_LTRIGGER) != 0, 0xFFE0A020);
    draw_indicator(410, 30, 40, 12, (pad.Buttons & PSP_CTRL_RTRIGGER) != 0, 0xFFE0A020);
}
