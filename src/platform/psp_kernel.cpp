#include "psp/kernel.h"
#include <SDL2/SDL.h>
#include <cstdlib>
#include <cstdio>

static size_t g_TotalAllocated = 0;

extern "C" {

u32 sceKernelGetSystemTimeLow() {
    return (u32)(SDL_GetTicks() * 1000); // return microseconds
}

u64 sceKernelGetSystemTimeWide() {
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 count = SDL_GetPerformanceCounter();
    return (count * 1000000ULL) / freq;
}

void sceKernelDelayThread(u32 usec) {
    if (usec >= 1000) {
        SDL_Delay(usec / 1000);
    }
}

void* Engine_MemAlloc(u32 size) {
    if (size == 0) return nullptr;
    // Align allocations to 16 bytes (standard PSP cacheline / MIPS requirement)
    size_t aligned_size = (size + 15) & ~15;
    void* ptr = malloc(aligned_size);
    if (!ptr) {
        fprintf(stderr, "[Memory] FAILED to allocate %u bytes (Total: %zu)\n", size, g_TotalAllocated);
        return nullptr;
    }
    g_TotalAllocated += aligned_size;
    return ptr;
}

void Engine_MemFree(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

int Engine_VolatileMemInit() {
    printf("[Kernel] Engine_VolatileMemInit: PSP volatile memory pool initialized (PC Mock).\n");
    return 0;
}

} // extern "C"
