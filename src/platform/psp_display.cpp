#include "psp/display.h"
#include "video/renderer.h"
#include <SDL2/SDL.h>

static u32 g_Vcount = 0;
static Uint64 g_LastFramePerf = 0;
static Uint64 g_PerfFrequency = 0;
static float g_CurrentFPS = 60.0f;
static float g_DeltaTime = 1.0f / 60.0f;

extern "C" {

int sceDisplaySetFrameBuf(void* topaddr, int bufferwidth, int pixelformat, int sync) {
    HAL_SetCurrentFramebuffer(topaddr, bufferwidth, pixelformat);
    return 0;
}

int sceDisplayWaitVblankStart() {
    Engine_WaitVblank(1);
    return 0;
}

u32 sceDisplayGetVcount() {
    return g_Vcount;
}

void Engine_WaitVblank(int count) {
    if (g_PerfFrequency == 0) {
        g_PerfFrequency = SDL_GetPerformanceFrequency();
        g_LastFramePerf = SDL_GetPerformanceCounter();
    }

    // Target frame time for 60 FPS = 1000.0 / 60.0 = 16.6666 ms
    const double target_frame_sec = (1.0 / 60.0) * (count > 0 ? count : 1);
    const Uint64 target_ticks = (Uint64)(target_frame_sec * (double)g_PerfFrequency);

    Uint64 current_perf = SDL_GetPerformanceCounter();
    Uint64 elapsed_ticks = current_perf - g_LastFramePerf;

    if (elapsed_ticks < target_ticks) {
        Uint64 remain_ticks = target_ticks - elapsed_ticks;
        double remain_ms = ((double)remain_ticks * 1000.0) / (double)g_PerfFrequency;
        if (remain_ms > 1.5) {
            SDL_Delay((Uint32)(remain_ms - 1.0));
        }
        // Busy wait fine precision for remainder
        while ((SDL_GetPerformanceCounter() - g_LastFramePerf) < target_ticks) {
            // spin
        }
    }

    current_perf = SDL_GetPerformanceCounter();
    Uint64 total_frame_ticks = current_perf - g_LastFramePerf;
    if (total_frame_ticks > 0) {
        g_DeltaTime = (float)((double)total_frame_ticks / (double)g_PerfFrequency);
        g_CurrentFPS = 1.0f / g_DeltaTime;
    }
    g_LastFramePerf = current_perf;
    g_Vcount += (count > 0 ? count : 1);
}

float HAL_GetFPS() {
    return g_CurrentFPS;
}

float HAL_GetDeltaTime() {
    return g_DeltaTime;
}

} // extern "C"
