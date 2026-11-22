#include "video/renderer.h"
#include "psp/ctrl.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#include <algorithm>

static SDL_Window*   g_Window   = nullptr;
static SDL_Renderer* g_Renderer = nullptr;
static SDL_Texture*  g_Texture  = nullptr;

static void* g_EngineFramebuffer = nullptr;
static int   g_BufferWidth = PSP_SCREEN_BUFFER_WIDTH;
static int   g_PixelFormat = PSP_DISPLAY_PIXEL_FORMAT_8888;

// Framebuffer (512x272 32-bit ABGR)
static std::vector<u32> g_DebugBuffer(PSP_SCREEN_BUFFER_WIDTH * PSP_SCREEN_HEIGHT, 0xFF14121A);

// Depth buffer for 3D rasterization
static std::vector<float> g_DepthBuffer(PSP_SCREEN_BUFFER_WIDTH * PSP_SCREEN_HEIGHT, 1e9f);

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

    SDL_RenderSetLogicalSize(g_Renderer, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);

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

    SDL_UpdateTexture(g_Texture, nullptr, pixels_to_render, pitch);

    SDL_SetRenderDrawColor(g_Renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_Renderer);

    SDL_Rect srcRect = { 0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT };
    SDL_Rect dstRect = { 0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT };
    SDL_RenderCopy(g_Renderer, g_Texture, &srcRect, &dstRect);

    SDL_RenderPresent(g_Renderer);
}

bool HAL_PollWindowEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            return false;
        }
    }
    return true;
}

void HAL_Clear3D(u32 clearColor) {
    std::fill(g_DebugBuffer.begin(), g_DebugBuffer.end(), clearColor);
    std::fill(g_DepthBuffer.begin(), g_DepthBuffer.end(), 1e9f);
}

// Draw a line using Bresenham's algorithm with depth testing
static void DrawLine3D(int x0, int y0, float z0, int x1, int y1, float z1, u32 color) {
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    int totalSteps = std::max(dx, dy);
    if (totalSteps == 0) totalSteps = 1;
    int step = 0;

    while (true) {
        if (x0 >= 0 && x0 < PSP_SCREEN_WIDTH && y0 >= 0 && y0 < PSP_SCREEN_HEIGHT) {
            float t = (float)step / totalSteps;
            float z = z0 + (z1 - z0) * t;
            int idx = y0 * PSP_SCREEN_BUFFER_WIDTH + x0;
            if (z < g_DepthBuffer[idx] + 0.1f) {
                g_DebugBuffer[idx] = color;
            }
        }

        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
        step++;
    }
}

// Simple 5x7 bitmap font for on-screen debug display
static const u8 FONT_5X7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // 32 ' '
    {0x00,0x00,0x5F,0x00,0x00}, // 33 '!'
    {0x00,0x07,0x00,0x07,0x00}, // 34 '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // 35 '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // 36 '$'
    {0x23,0x13,0x08,0x64,0x62}, // 37 '%'
    {0x36,0x49,0x55,0x22,0x50}, // 38 '&'
    {0x00,0x05,0x03,0x00,0x00}, // 39 '''
    {0x00,0x1C,0x22,0x41,0x00}, // 40 '('
    {0x00,0x41,0x22,0x1C,0x00}, // 41 ')'
    {0x14,0x08,0x3E,0x08,0x14}, // 42 '*'
    {0x08,0x08,0x3E,0x08,0x08}, // 43 '+'
    {0x00,0x50,0x30,0x00,0x00}, // 44 ','
    {0x08,0x08,0x08,0x08,0x08}, // 45 '-'
    {0x00,0x60,0x60,0x00,0x00}, // 46 '.'
    {0x20,0x10,0x08,0x04,0x02}, // 47 '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // 48 '0'
    {0x00,0x42,0x7F,0x40,0x00}, // 49 '1'
    {0x42,0x61,0x51,0x49,0x46}, // 50 '2'
    {0x21,0x41,0x45,0x4B,0x31}, // 51 '3'
    {0x18,0x14,0x12,0x7F,0x10}, // 52 '4'
    {0x27,0x45,0x45,0x45,0x39}, // 53 '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // 54 '6'
    {0x01,0x71,0x09,0x05,0x03}, // 55 '7'
    {0x36,0x49,0x49,0x49,0x36}, // 56 '8'
    {0x06,0x49,0x49,0x29,0x1E}, // 57 '9'
    {0x00,0x36,0x36,0x00,0x00}, // 58 ':'
    {0x00,0x56,0x36,0x00,0x00}, // 59 ';'
    {0x08,0x14,0x22,0x41,0x00}, // 60 '<'
    {0x14,0x14,0x14,0x14,0x14}, // 61 '='
    {0x00,0x41,0x22,0x14,0x08}, // 62 '>'
    {0x02,0x01,0x51,0x09,0x06}, // 63 '?'
    {0x32,0x49,0x79,0x41,0x3E}, // 64 '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 65 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 66 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 67 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 68 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 69 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 70 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 71 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 72 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 73 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 74 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 75 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 76 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 77 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 78 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 79 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 80 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 81 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 82 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 83 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 84 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 85 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 86 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 87 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 88 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 89 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 90 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // 91 '['
    {0x02,0x04,0x08,0x10,0x20}, // 92 '\'
    {0x00,0x41,0x41,0x7F,0x00}, // 93 ']'
    {0x04,0x02,0x01,0x02,0x04}, // 94 '^'
    {0x40,0x40,0x40,0x40,0x40}, // 95 '_'
};

void HAL_DrawString(int x, int y, const char* str, u32 color) {
    if (!str) return;
    int curX = x;
    while (*str) {
        char c = *str++;
        if (c < 32 || c > 126) c = ' ';
        const u8* col = FONT_5X7[c - 32];
        for (int i = 0; i < 5; ++i) {
            u8 b = col[i];
            for (int j = 0; j < 7; ++j) {
                if (b & (1 << j)) {
                    int px = curX + i;
                    int py = y + j;
                    if (px >= 0 && px < PSP_SCREEN_WIDTH && py >= 0 && py < PSP_SCREEN_HEIGHT) {
                        g_DebugBuffer[py * PSP_SCREEN_BUFFER_WIDTH + px] = color;
                    }
                }
            }
        }
        curX += 6;
    }
}

// 3D Perspective Projection & Rendering
void HAL_Render3DMap(const Quantum::LevelMap& map, const Camera3D& camera, bool wireframe) {
    float cosYaw = std::cos(camera.yaw);
    float sinYaw = std::sin(camera.yaw);
    float cosPitch = std::cos(camera.pitch);
    float sinPitch = std::sin(camera.pitch);

    float fovRad = camera.fov * (3.14159265f / 180.0f);
    float focalLength = (PSP_SCREEN_WIDTH * 0.5f) / std::tan(fovRad * 0.5f);
    float centerX = PSP_SCREEN_WIDTH * 0.5f;
    float centerY = PSP_SCREEN_HEIGHT * 0.5f;

    // Directional light (from upper right)
    Quantum::Vector3 lightDir = {0.577f, 0.577f, -0.577f};

    struct ScreenVertex {
        float sx, sy, sz;
        bool visible;
    };

    for (const auto& mesh : map.meshes) {
        std::vector<ScreenVertex> screenVerts(mesh.vertices.size());

        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            const auto& v = mesh.vertices[i];

            // 1. World to Camera Translation
            float tx = v.pos.x - camera.x;
            float ty = v.pos.y - camera.y;
            float tz = v.pos.z - camera.z;

            // 2. Yaw Rotation (around Y axis)
            float rx = tx * cosYaw - tz * sinYaw;
            float rz = tx * sinYaw + tz * cosYaw;
            float ry = ty;

            // 3. Pitch Rotation (around X axis)
            float cy = ry * cosPitch - rz * sinPitch;
            float cz = ry * sinPitch + rz * cosPitch;
            float cx = rx;

            if (cz > 0.5f) { // Near plane clip
                float invZ = 1.0f / cz;
                screenVerts[i].sx = centerX + cx * focalLength * invZ;
                screenVerts[i].sy = centerY - cy * focalLength * invZ;
                screenVerts[i].sz = cz;
                screenVerts[i].visible = true;
            } else {
                screenVerts[i].visible = false;
            }
        }

        // Render Triangles
        for (const auto& tri : mesh.triangles) {
            if (tri.v0 >= screenVerts.size() || tri.v1 >= screenVerts.size() || tri.v2 >= screenVerts.size())
                continue;

            const auto& v0 = screenVerts[tri.v0];
            const auto& v1 = screenVerts[tri.v1];
            const auto& v2 = screenVerts[tri.v2];

            if (!v0.visible || !v1.visible || !v2.visible) continue;

            // Draw Wireframe edges
            u32 edgeColor = mesh.meshColor;
            DrawLine3D((int)v0.sx, (int)v0.sy, v0.sz, (int)v1.sx, (int)v1.sy, v1.sz, edgeColor);
            DrawLine3D((int)v1.sx, (int)v1.sy, v1.sz, (int)v2.sx, (int)v2.sy, v2.sz, edgeColor);
            DrawLine3D((int)v2.sx, (int)v2.sy, v2.sz, (int)v0.sx, (int)v0.sy, v0.sz, edgeColor);
        }
    }
}
