#include "video/renderer.h"
#include "entities/player.h"
#include "entities/enemy.h"
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
static bool  g_WireframeMode = false;

// Framebuffer (512x272 32-bit ABGR)
static std::vector<u32> g_DebugBuffer(PSP_SCREEN_BUFFER_WIDTH * PSP_SCREEN_HEIGHT, 0xFF14121A);

// Depth buffer for 3D rasterization
static std::vector<float> g_DepthBuffer(PSP_SCREEN_BUFFER_WIDTH * PSP_SCREEN_HEIGHT, 1e9f);

void HAL_SetWireframeMode(bool enabled) {
    g_WireframeMode = enabled;
}

bool HAL_GetWireframeMode() {
    return g_WireframeMode;
}

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
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                return false;
            }
            if (event.key.keysym.sym == SDLK_v) {
                g_WireframeMode = !g_WireframeMode;
                printf("[Renderer] Wireframe mode: %s\n", g_WireframeMode ? "ENABLED" : "DISABLED (Solid Shading)");
            }
        }
    }
    return true;
}

void HAL_Clear3D(u32 clearColor) {
    std::fill(g_DepthBuffer.begin(), g_DepthBuffer.end(), 1e9f);
    // Atmospheric Canyon Sunset / Dusk Sky Gradient
    for (int y = 0; y < PSP_SCREEN_HEIGHT; ++y) {
        u32 col;
        if (y < 165) {
            float t = (float)y / 165.0f;
            // Dusk sky gradient: Deep Navy/Indigo (25, 20, 50) -> Warm Canyon Twilight (80, 42, 35)
            u8 r = (u8)(25.0f + 55.0f * t);
            u8 g = (u8)(20.0f + 22.0f * t);
            u8 b = (u8)(50.0f - 15.0f * t);
            col = 0xFF000000 | (b << 16) | (g << 8) | r;
        } else {
            col = clearColor;
        }
        int rowIdx = y * PSP_SCREEN_BUFFER_WIDTH;
        for (int x = 0; x < PSP_SCREEN_WIDTH; ++x) {
            g_DebugBuffer[rowIdx + x] = col;
        }
    }
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
            if (z < g_DepthBuffer[idx] + 0.15f) {
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

struct ScreenVertex {
    float sx = 0.0f;
    float sy = 0.0f;
    float sz = 0.0f;
    bool visible = false;
};

// Solid 3D Barycentric Triangle Rasterizer with Depth Buffer
static void RasterizeTriangle3D(const ScreenVertex& sv0, const ScreenVertex& sv1, const ScreenVertex& sv2, u32 color) {
    if (!sv0.visible || !sv1.visible || !sv2.visible) return;

    int minX = std::max(0, static_cast<int>(std::floor(std::min({sv0.sx, sv1.sx, sv2.sx}))));
    int maxX = std::min(PSP_SCREEN_WIDTH - 1, static_cast<int>(std::ceil(std::max({sv0.sx, sv1.sx, sv2.sx}))));
    int minY = std::max(0, static_cast<int>(std::floor(std::min({sv0.sy, sv1.sy, sv2.sy}))));
    int maxY = std::min(PSP_SCREEN_HEIGHT - 1, static_cast<int>(std::ceil(std::max({sv0.sy, sv1.sy, sv2.sy}))));

    if (minX > maxX || minY > maxY) return;

    float x0 = sv0.sx, y0 = sv0.sy, z0 = sv0.sz;
    float x1 = sv1.sx, y1 = sv1.sy, z1 = sv1.sz;
    float x2 = sv2.sx, y2 = sv2.sy, z2 = sv2.sz;

    float area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    if (std::abs(area) < 0.0001f) return;

    if (area < 0.0f) {
        std::swap(x1, x2);
        std::swap(y1, y2);
        std::swap(z1, z2);
        area = -area;
    }

    float invArea = 1.0f / area;
    float invZ0 = 1.0f / z0;
    float invZ1 = 1.0f / z1;
    float invZ2 = 1.0f / z2;

    for (int py = minY; py <= maxY; ++py) {
        float py_f = py + 0.5f;
        int rowIdx = py * PSP_SCREEN_BUFFER_WIDTH;

        for (int px = minX; px <= maxX; ++px) {
            float px_f = px + 0.5f;

            float w0 = (x2 - x1) * (py_f - y1) - (y2 - y1) * (px_f - x1);
            float w1 = (x0 - x2) * (py_f - y2) - (y0 - y2) * (px_f - x2);
            float w2 = (x1 - x0) * (py_f - y0) - (y1 - y0) * (px_f - x0);

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                float l0 = w0 * invArea;
                float l1 = w1 * invArea;
                float l2 = w2 * invArea;
                float z = 1.0f / (l0 * invZ0 + l1 * invZ1 + l2 * invZ2);

                int idx = rowIdx + px;
                if (z > 0.5f && z < g_DepthBuffer[idx]) {
                    g_DepthBuffer[idx] = z;
                    g_DebugBuffer[idx] = color;
                }
            }
        }
    }
}

// 5x7 bitmap font
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
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A'); // Normalize lowercase to uppercase font
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

// 3D Perspective Projection & Rendering for Map
void HAL_Render3DMap(const Quantum::LevelMap& map, const Camera3D& camera, bool wireframe) {
    float cosYaw = std::cos(camera.yaw);
    float sinYaw = std::sin(camera.yaw);
    float cosPitch = std::cos(camera.pitch);
    float sinPitch = std::sin(camera.pitch);

    float fovRad = camera.fov * (3.14159265f / 180.0f);
    float focalLength = (PSP_SCREEN_WIDTH * 0.5f) / std::tan(fovRad * 0.5f);
    float centerX = PSP_SCREEN_WIDTH * 0.5f;
    float centerY = PSP_SCREEN_HEIGHT * 0.5f;

    // Directional light (from upper right sky)
    Quantum::Vector3 lightDir = {0.4f, 0.8f, -0.45f};
    float lightLen = std::sqrt(lightDir.x * lightDir.x + lightDir.y * lightDir.y + lightDir.z * lightDir.z);
    lightDir.x /= lightLen; lightDir.y /= lightLen; lightDir.z /= lightLen;

    bool renderAsWireframe = wireframe || g_WireframeMode;

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

            if (cz > 0.5f) {
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

            const auto& sv0 = screenVerts[tri.v0];
            const auto& sv1 = screenVerts[tri.v1];
            const auto& sv2 = screenVerts[tri.v2];

            if (!sv0.visible || !sv1.visible || !sv2.visible) continue;

            if (renderAsWireframe) {
                u32 edgeColor = mesh.meshColor;
                DrawLine3D((int)sv0.sx, (int)sv0.sy, sv0.sz, (int)sv1.sx, (int)sv1.sy, sv1.sz, edgeColor);
                DrawLine3D((int)sv1.sx, (int)sv1.sy, sv1.sz, (int)sv2.sx, (int)sv2.sy, sv2.sz, edgeColor);
                DrawLine3D((int)sv2.sx, (int)sv2.sy, sv2.sz, (int)sv0.sx, (int)sv0.sy, sv0.sz, edgeColor);
            } else {
                // Compute face normal & Lambertian directional lighting
                const auto& p0 = mesh.vertices[tri.v0].pos;
                const auto& p1 = mesh.vertices[tri.v1].pos;
                const auto& p2 = mesh.vertices[tri.v2].pos;

                Quantum::Vector3 e1 = p1 - p0;
                Quantum::Vector3 e2 = p2 - p0;
                Quantum::Vector3 norm = {
                    e1.y * e2.z - e1.z * e2.y,
                    e1.z * e2.x - e1.x * e2.z,
                    e1.x * e2.y - e1.y * e2.x
                };
                float nlen = std::sqrt(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z);
                if (nlen > 0.0001f) {
                    norm.x /= nlen; norm.y /= nlen; norm.z /= nlen;
                } else {
                    norm = {0.0f, 1.0f, 0.0f};
                }

                float dot = std::abs(norm.x * lightDir.x + norm.y * lightDir.y + norm.z * lightDir.z);
                float intensity = std::clamp(0.40f + 0.60f * dot, 0.25f, 1.0f);

                u32 baseColor = mesh.meshColor;
                u8 a = (baseColor >> 24) & 0xFF;
                u8 b = static_cast<u8>(std::clamp(((baseColor >> 16) & 0xFF) * intensity, 0.0f, 255.0f));
                u8 g = static_cast<u8>(std::clamp(((baseColor >> 8) & 0xFF) * intensity, 0.0f, 255.0f));
                u8 r = static_cast<u8>(std::clamp(((baseColor) & 0xFF) * intensity, 0.0f, 255.0f));
                u32 shadedColor = (a << 24) | (b << 16) | (g << 8) | r;

                RasterizeTriangle3D(sv0, sv1, sv2, shadedColor);

                // Subtle dark edge wireframe for depth clarity
                u32 contourColor = 0xFF181818;
                DrawLine3D((int)sv0.sx, (int)sv0.sy, sv0.sz, (int)sv1.sx, (int)sv1.sy, sv1.sz, contourColor);
                DrawLine3D((int)sv1.sx, (int)sv1.sy, sv1.sz, (int)sv2.sx, (int)sv2.sy, sv2.sz, contourColor);
                DrawLine3D((int)sv2.sx, (int)sv2.sy, sv2.sz, (int)sv0.sx, (int)sv0.sy, sv0.sz, contourColor);
            }
        }
    }
}

// Helper to project a 3D point
static bool ProjectPoint(float wx, float wy, float wz, const Camera3D& camera,
                         float cosYaw, float sinYaw, float cosPitch, float sinPitch,
                         float focalLength, float centerX, float centerY,
                         ScreenVertex& out) {
    float tx = wx - camera.x;
    float ty = wy - camera.y;
    float tz = wz - camera.z;

    float rx = tx * cosYaw - tz * sinYaw;
    float rz = tx * sinYaw + tz * cosYaw;
    float ry = ty;

    float cy = ry * cosPitch - rz * sinPitch;
    float cz = ry * sinPitch + rz * cosPitch;
    float cx = rx;

    if (cz <= 0.5f) {
        out.visible = false;
        return false;
    }

    out.sz = cz;
    out.sx = centerX + cx * focalLength / cz;
    out.sy = centerY - cy * focalLength / cz;
    out.visible = true;
    return true;
}

// Helper to rasterize a solid 3D oriented cuboid
static void RasterizeBox3D(float cx, float cy, float cz,
                           float sizeX, float sizeY, float sizeZ,
                           float yaw, u32 baseColor,
                           const Camera3D& camera,
                           float cosYaw, float sinYaw, float cosPitch, float sinPitch,
                           float focalLength, float centerX, float centerY) {
    float hx = sizeX * 0.5f;
    float hy = sizeY * 0.5f;
    float hz = sizeZ * 0.5f;

    float pCos = std::cos(yaw);
    float pSin = std::sin(yaw);

    float offsets[8][3] = {
        {-hx, -hy, -hz}, {hx, -hy, -hz}, {hx, -hy, hz}, {-hx, -hy, hz},
        {-hx,  hy, -hz}, {hx,  hy, -hz}, {hx,  hy, hz}, {-hx,  hy, hz}
    };

    ScreenVertex sv[8];
    for (int i = 0; i < 8; ++i) {
        float lx = offsets[i][0];
        float ly = offsets[i][1];
        float lz = offsets[i][2];

        float rx = lx * pCos - lz * pSin;
        float rz = lx * pSin + lz * pCos;

        ProjectPoint(cx + rx, cy + ly, cz + rz, camera,
                     cosYaw, sinYaw, cosPitch, sinPitch,
                     focalLength, centerX, centerY, sv[i]);
    }

    // 6 Faces: 2 triangles each with basic directional tone modulation
    auto shade = [](u32 col, float f) -> u32 {
        u8 a = (col >> 24) & 0xFF;
        u8 b = static_cast<u8>(std::clamp(((col >> 16) & 0xFF) * f, 0.0f, 255.0f));
        u8 g = static_cast<u8>(std::clamp(((col >> 8) & 0xFF) * f, 0.0f, 255.0f));
        u8 r = static_cast<u8>(std::clamp((col & 0xFF) * f, 0.0f, 255.0f));
        return (a << 24) | (b << 16) | (g << 8) | r;
    };

    auto drawQuad = [&](int i0, int i1, int i2, int i3, float factor) {
        u32 c = shade(baseColor, factor);
        RasterizeTriangle3D(sv[i0], sv[i1], sv[i2], c);
        RasterizeTriangle3D(sv[i0], sv[i2], sv[i3], c);
    };

    drawQuad(4, 5, 6, 7, 1.25f); // Top
    drawQuad(0, 3, 2, 1, 0.60f); // Bottom
    drawQuad(7, 6, 2, 3, 1.05f); // Front (+Z)
    drawQuad(5, 4, 0, 1, 0.75f); // Back (-Z)
    drawQuad(4, 7, 3, 0, 0.90f); // Left (-X)
    drawQuad(6, 5, 1, 2, 0.90f); // Right (+X)
}

void HAL_RenderPlayer(const Game::Player& player, const Camera3D& camera) {
    const auto& pos = player.GetPosition();
    float yaw = player.GetYaw();
    u32 color = player.GetFormPrimaryColor();

    float cosYaw = std::cos(camera.yaw);
    float sinYaw = std::sin(camera.yaw);
    float cosPitch = std::cos(camera.pitch);
    float sinPitch = std::sin(camera.pitch);

    float fovRad = camera.fov * (3.14159265f / 180.0f);
    float focalLength = (PSP_SCREEN_WIDTH * 0.5f) / std::tan(fovRad * 0.5f);
    float centerX = PSP_SCREEN_WIDTH * 0.5f;
    float centerY = PSP_SCREEN_HEIGHT * 0.5f;

    float pCos = std::cos(yaw);
    float pSin = std::sin(yaw);

    // Form-based dimensions
    float scale = 1.0f;
    if (player.GetCurrentForm() == Game::AlienForm::FOUR_ARMS) {
        scale = 1.45f;
    } else if (player.GetCurrentForm() == Game::AlienForm::XLR8) {
        scale = 1.15f;
    } else if (player.GetCurrentForm() == Game::AlienForm::CANNONBOLT) {
        scale = 1.35f;
    }

    // 1. Torso
    float torsoY = pos.y + 4.5f * scale;
    RasterizeBox3D(pos.x, torsoY, pos.z, 3.2f * scale, 4.2f * scale, 2.0f * scale,
                   yaw, color, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);

    // 2. Head
    float headY = pos.y + 7.4f * scale;
    RasterizeBox3D(pos.x, headY, pos.z, 2.0f * scale, 2.0f * scale, 2.0f * scale,
                   yaw, 0xFFE0D0C0, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);

    // 3. Legs
    float legY = pos.y + 1.5f * scale;
    float legOffset = 0.9f * scale;
    float legL_X = pos.x - pCos * legOffset;
    float legL_Z = pos.z + pSin * legOffset;
    float legR_X = pos.x + pCos * legOffset;
    float legR_Z = pos.z - pSin * legOffset;

    u32 pantsColor = 0xFF202020; // Black pants / dark alien armor
    RasterizeBox3D(legL_X, legY, legL_Z, 1.2f * scale, 3.0f * scale, 1.3f * scale,
                   yaw, pantsColor, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);
    RasterizeBox3D(legR_X, legY, legR_Z, 1.2f * scale, 3.0f * scale, 1.3f * scale,
                   yaw, pantsColor, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);

    // 4. Arms
    float armY = pos.y + 4.8f * scale;
    float armOffset = 2.2f * scale;
    float armL_X = pos.x - pCos * armOffset;
    float armL_Z = pos.z + pSin * armOffset;
    float armR_X = pos.x + pCos * armOffset;
    float armR_Z = pos.z - pSin * armOffset;

    RasterizeBox3D(armL_X, armY, armL_Z, 1.1f * scale, 3.2f * scale, 1.2f * scale,
                   yaw, color, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);
    RasterizeBox3D(armR_X, armY, armR_Z, 1.1f * scale, 3.2f * scale, 1.2f * scale,
                   yaw, color, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);

    // Four Arms Extra Pair of Arms
    if (player.GetCurrentForm() == Game::AlienForm::FOUR_ARMS) {
        float arm2Y = pos.y + 3.2f * scale;
        float arm2Offset = 2.4f * scale;
        float arm2L_X = pos.x - pCos * arm2Offset;
        float arm2L_Z = pos.z + pSin * arm2Offset;
        float arm2R_X = pos.x + pCos * arm2Offset;
        float arm2R_Z = pos.z - pSin * arm2Offset;

        RasterizeBox3D(arm2L_X, arm2Y, arm2L_Z, 1.0f * scale, 2.8f * scale, 1.1f * scale,
                       yaw, color, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);
        RasterizeBox3D(arm2R_X, arm2Y, arm2R_Z, 1.0f * scale, 2.8f * scale, 1.1f * scale,
                       yaw, color, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);
    }

    // 5. Omnitrix Emblem on Chest (Glowing Neon Green Quad)
    float chestX = pos.x - pSin * (1.1f * scale);
    float chestZ = pos.z + pCos * (1.1f * scale);
    float chestY = pos.y + 5.2f * scale;
    RasterizeBox3D(chestX, chestY, chestZ, 0.8f * scale, 0.8f * scale, 0.2f * scale,
                   yaw, 0xFF00FF00, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);

    // 6. Attack Visual Effect (Energy Slash Arc)
    if (player.IsAttacking()) {
        float slashDist = 3.5f * scale;
        float slashX = pos.x - pSin * slashDist;
        float slashZ = pos.z + pCos * slashDist;
        float slashY = pos.y + 4.0f * scale;

        u32 vfxColor = (player.GetState() == Game::PlayerState::ATTACK_HEAVY) ? 0xFF00FFFF : 0xFFFFFFFF;
        RasterizeBox3D(slashX, slashY, slashZ, 4.5f * scale, 0.4f * scale, 2.0f * scale,
                       yaw + 0.4f, vfxColor, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);
    }
}

void HAL_RenderEnemies(const std::vector<Game::EnemyDrone>& enemies, const Camera3D& camera) {
    float cosYaw = std::cos(camera.yaw);
    float sinYaw = std::sin(camera.yaw);
    float cosPitch = std::cos(camera.pitch);
    float sinPitch = std::sin(camera.pitch);

    float fovRad = camera.fov * (3.14159265f / 180.0f);
    float focalLength = (PSP_SCREEN_WIDTH * 0.5f) / std::tan(fovRad * 0.5f);
    float centerX = PSP_SCREEN_WIDTH * 0.5f;
    float centerY = PSP_SCREEN_HEIGHT * 0.5f;

    for (const auto& enemy : enemies) {
        if (enemy.IsDead()) continue;

        const auto& pos = enemy.GetPosition();
        float yaw = enemy.GetYaw();
        float hoverY = pos.y + 6.0f + enemy.GetHoverOffset();

        u32 droneColor = enemy.IsFlashing() ? 0xFFFFFFFF : 0xFF4A4240; // Steel grey
        u32 eyeColor   = enemy.IsFlashing() ? 0xFFFFFFFF : 0xFF0000FF; // Vilgax Red eye

        // Drone Chassis (Central saucer body)
        RasterizeBox3D(pos.x, hoverY, pos.z, 3.5f, 1.8f, 3.5f,
                       yaw, droneColor, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);

        // Glowing Vilgax Red Eye / Sensor Dome
        float pCos = std::cos(yaw);
        float pSin = std::sin(yaw);
        float eyeX = pos.x - pSin * 1.9f;
        float eyeZ = pos.z + pCos * 1.9f;
        RasterizeBox3D(eyeX, hoverY, eyeZ, 1.2f, 0.9f, 0.6f,
                       yaw, eyeColor, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);

        // Side Weapon Pods
        float podL_X = pos.x - pCos * 2.3f;
        float podL_Z = pos.z + pSin * 2.3f;
        float podR_X = pos.x + pCos * 2.3f;
        float podR_Z = pos.z - pSin * 2.3f;
        RasterizeBox3D(podL_X, hoverY - 0.4f, podL_Z, 0.8f, 0.8f, 2.2f,
                       yaw, 0xFF2A2220, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);
        RasterizeBox3D(podR_X, hoverY - 0.4f, podR_Z, 0.8f, 0.8f, 2.2f,
                       yaw, 0xFF2A2220, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);

        // Overhead Health Bar
        ScreenVertex hpV;
        if (ProjectPoint(pos.x, hoverY + 3.5f, pos.z, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY, hpV)) {
            int barW = 32;
            int barH = 4;
            int bx = static_cast<int>(hpV.sx - barW * 0.5f);
            int by = static_cast<int>(hpV.sy);

            if (bx >= 0 && bx + barW < PSP_SCREEN_WIDTH && by >= 0 && by + barH < PSP_SCREEN_HEIGHT) {
                float hpRatio = enemy.GetHealth() / enemy.GetMaxHealth();
                int fillW = static_cast<int>(barW * hpRatio);

                for (int y = by; y < by + barH; ++y) {
                    for (int x = bx; x < bx + barW; ++x) {
                        int idx = y * PSP_SCREEN_BUFFER_WIDTH + x;
                        if (x < bx + fillW) {
                            g_DebugBuffer[idx] = 0xFF0000FF; // Red HP
                        } else {
                            g_DebugBuffer[idx] = 0xFF222222; // Dark background
                        }
                    }
                }
            }
        }

        // Render Active Laser Bolts
        for (const auto& bolt : enemy.GetLaserBolts()) {
            if (!bolt.active) continue;
            RasterizeBox3D(bolt.pos.x, bolt.pos.y, bolt.pos.z, 0.8f, 0.8f, 2.0f,
                           yaw, 0xFF0000FF, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);
        }
    }
}

void HAL_RenderExitPortal(const Quantum::Vector3& pos, float time, const Camera3D& camera, bool active) {
    float cosYaw = std::cos(camera.yaw);
    float sinYaw = std::sin(camera.yaw);
    float cosPitch = std::cos(camera.pitch);
    float sinPitch = std::sin(camera.pitch);

    float fovRad = camera.fov * (3.14159265f / 180.0f);
    float focalLength = (PSP_SCREEN_WIDTH * 0.5f) / std::tan(fovRad * 0.5f);
    float centerX = PSP_SCREEN_WIDTH * 0.5f;
    float centerY = PSP_SCREEN_HEIGHT * 0.5f;

    // Outer beacon ring (12 segments)
    const int segments = 12;
    float radius = 10.0f;
    u32 ringColor = active ? 0xFF00FF7F : 0xFF555555; // Neon green when active, dark gray when inactive
    u32 coreColor = active ? 0xFF00FF00 : 0xFF333333;

    float pulse = std::sin(time * 3.0f) * 0.2f + 1.0f;
    radius *= pulse;

    ScreenVertex pts[segments];
    bool valid = true;
    for (int i = 0; i < segments; ++i) {
        float angle = (float)i / segments * 6.2831853f + (active ? time * 1.5f : 0.0f);
        float px = pos.x + std::cos(angle) * radius;
        float pz = pos.z + std::sin(angle) * radius;
        if (!ProjectPoint(px, pos.y + 0.5f, pz, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY, pts[i])) {
            valid = false;
        }
    }

    if (valid) {
        for (int i = 0; i < segments; ++i) {
            int next = (i + 1) % segments;
            DrawLine3D((int)pts[i].sx, (int)pts[i].sy, pts[i].sz,
                       (int)pts[next].sx, (int)pts[next].sy, pts[next].sz, ringColor);
        }
    }

    // Floating Omnitrix Beacon Icon
    if (active) {
        float floatY = pos.y + 6.0f + std::sin(time * 4.0f) * 1.0f;
        RasterizeBox3D(pos.x, floatY, pos.z, 2.5f, 2.5f, 2.5f,
                       time * 2.0f, coreColor, camera, cosYaw, sinYaw, cosPitch, sinPitch, focalLength, centerX, centerY);
    }
}

void HAL_RenderHUD(const Game::Player& player) {
    // 1. Health Bar (Top Left)
    int hpX = 10;
    int hpY = 10;
    int barW = 100;
    int barH = 8;

    // Background border
    for (int y = hpY - 1; y <= hpY + barH; ++y) {
        for (int x = hpX - 1; x <= hpX + barW; ++x) {
            if (x >= 0 && x < PSP_SCREEN_WIDTH && y >= 0 && y < PSP_SCREEN_HEIGHT) {
                if (x == hpX - 1 || x == hpX + barW || y == hpY - 1 || y == hpY + barH) {
                    g_DebugBuffer[y * PSP_SCREEN_BUFFER_WIDTH + x] = 0xFFFFFFFF; // White border
                } else {
                    g_DebugBuffer[y * PSP_SCREEN_BUFFER_WIDTH + x] = 0xFF330000; // Dark red empty
                }
            }
        }
    }

    // Filled health
    int fillW = static_cast<int>((player.GetHealth() / player.GetMaxHealth()) * barW);
    fillW = std::clamp(fillW, 0, barW);
    for (int y = hpY; y < hpY + barH; ++y) {
        for (int x = hpX; x < hpX + fillW; ++x) {
            if (x >= 0 && x < PSP_SCREEN_WIDTH && y >= 0 && y < PSP_SCREEN_HEIGHT) {
                g_DebugBuffer[y * PSP_SCREEN_BUFFER_WIDTH + x] = 0xFF00D000; // Bright green HP
            }
        }
    }

    HAL_DrawString(hpX, hpY + 11, "BEN 10 HP", 0xFF00FF00);

    // 2. Omnitrix Energy Bar
    int omX = 10;
    int omY = 32;
    int omFillW = static_cast<int>((player.GetOmnitrixCharge() / 100.0f) * barW);
    omFillW = std::clamp(omFillW, 0, barW);

    for (int y = omY - 1; y <= omY + barH; ++y) {
        for (int x = omX - 1; x <= omX + barW; ++x) {
            if (x >= 0 && x < PSP_SCREEN_WIDTH && y >= 0 && y < PSP_SCREEN_HEIGHT) {
                if (x == omX - 1 || x == omX + barW || y == omY - 1 || y == omY + barH) {
                    g_DebugBuffer[y * PSP_SCREEN_BUFFER_WIDTH + x] = 0xFFFFFFFF;
                } else {
                    g_DebugBuffer[y * PSP_SCREEN_BUFFER_WIDTH + x] = 0xFF002222;
                }
            }
        }
    }

    for (int y = omY; y < omY + barH; ++y) {
        for (int x = omX; x < omX + omFillW; ++x) {
            if (x >= 0 && x < PSP_SCREEN_WIDTH && y >= 0 && y < PSP_SCREEN_HEIGHT) {
                g_DebugBuffer[y * PSP_SCREEN_BUFFER_WIDTH + x] = 0xFF00FF7F; // Omnitrix Neon Green
            }
        }
    }

    // Active Alien Name & State
    char statusBuf[64];
    std::snprintf(statusBuf, sizeof(statusBuf), "FORM: %s", player.GetFormName());
    HAL_DrawString(omX, omY + 11, statusBuf, player.GetFormPrimaryColor());

    // 3. Controls Legend (Bottom)
    HAL_DrawString(10, PSP_SCREEN_HEIGHT - 12, "MOVE:STICK | X:JUMP | []:LIGHT | ^:HEAVY | L/R:OMNITRIX | V:SHADING", 0xFFAAAAAA);
}
