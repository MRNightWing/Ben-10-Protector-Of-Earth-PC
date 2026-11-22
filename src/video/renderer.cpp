#include "video/renderer.h"
#include "psp/ctrl.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>

static SDL_Window*   g_Window   = nullptr;
static SDL_Renderer* g_Renderer = nullptr;
static SDL_Texture*  g_Texture  = nullptr;

static void* g_EngineFramebuffer = nullptr;
static int   g_BufferWidth = PSP_SCREEN_BUFFER_WIDTH;
static int   g_PixelFormat = PSP_DISPLAY_PIXEL_FORMAT_8888;

// Fallback / debug 512x272 32-bit pixel buffer
static std::vector<u32> g_DebugBuffer(PSP_SCREEN_BUFFER_WIDTH * PSP_SCREEN_HEIGHT, 0xFF181818);

bool HAL_InitVideo(const char* window_title, int scale) {
    if (scale < 1) scale = 2; // Default 2x integer scale = 960x544

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "[Video] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    int window_w = PSP_SCREEN_WIDTH * scale;
    int window_h = PSP_SCREEN_HEIGHT * scale;

    g_Window = SDL_CreateWindow(
        window_title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        window_w,
        window_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!g_Window) {
        fprintf(stderr, "[Video] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    g_Renderer = SDL_CreateRenderer(g_Window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_Renderer) {
        fprintf(stderr, "[Video] Warning: Accelerated renderer failed, falling back to software: %s\n", SDL_GetError());
        g_Renderer = SDL_CreateRenderer(g_Window, -1, SDL_RENDERER_SOFTWARE);
    }

    // Set logical presentation size so aspect ratio (16:9) is always preserved with letterboxing
    SDL_RenderSetLogicalSize(g_Renderer, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);

    // PSP texture: 512x272 ABGR8888 streaming texture
    g_Texture = SDL_CreateTexture(
        g_Renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        PSP_SCREEN_BUFFER_WIDTH,
        PSP_SCREEN_HEIGHT
    );

    if (!g_Texture) {
        fprintf(stderr, "[Video] SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }

    printf("[Video] HAL Video initialized successfully (%dx%d -> %dx%d)\n",
           PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, window_w, window_h);
    return true;
}

void HAL_ShutdownVideo() {
    if (g_Texture)  { SDL_DestroyTexture(g_Texture); g_Texture = nullptr; }
    if (g_Renderer) { SDL_DestroyRenderer(g_Renderer); g_Renderer = nullptr; }
    if (g_Window)   { SDL_DestroyWindow(g_Window); g_Window = nullptr; }
    SDL_Quit();
}

void HAL_SetCurrentFramebuffer(void* buffer, int bufferwidth, int pixelformat) {
    g_EngineFramebuffer = buffer;
    g_BufferWidth = (bufferwidth > 0) ? bufferwidth : PSP_SCREEN_BUFFER_WIDTH;
    g_PixelFormat = pixelformat;
}

u32* HAL_GetDebugFramebuffer() {
    return g_DebugBuffer.data();
}

void HAL_PresentFrame() {
    if (!g_Renderer || !g_Texture) return;

    void* pixels_to_render = g_EngineFramebuffer ? g_EngineFramebuffer : g_DebugBuffer.data();
    int pitch = g_BufferWidth * sizeof(u32);

    // Update GPU texture from framebuffer
    SDL_UpdateTexture(g_Texture, nullptr, pixels_to_render, pitch);

    SDL_SetRenderDrawColor(g_Renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_Renderer);

    // Source rect is PSP screen area (480x272), ignoring the 512 stride padding
    SDL_Rect src_rect = { 0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT };
    SDL_Rect dst_rect = { 0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT };
    SDL_RenderCopy(g_Renderer, g_Texture, &src_rect, &dst_rect);

    SDL_RenderPresent(g_Renderer);
}

bool HAL_PollWindowEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_F11) {
                // Toggle Fullscreen
                Uint32 flags = SDL_GetWindowFlags(g_Window);
                bool is_fullscreen = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP);
                SDL_SetWindowFullscreen(g_Window, is_fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
        }
    }
    return true;
}
