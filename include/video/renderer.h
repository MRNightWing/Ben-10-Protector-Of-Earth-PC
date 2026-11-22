#pragma once
#include "psp/types.h"
#include "psp/display.h"

// Renderer API
bool HAL_InitVideo(const char* window_title, int scale);
void HAL_ShutdownVideo();
void HAL_SetCurrentFramebuffer(void* buffer, int bufferwidth, int pixelformat);
void HAL_PresentFrame();
bool HAL_PollWindowEvents();

// Access internal test framebuffer for software rasterizer / debug UI
u32* HAL_GetDebugFramebuffer();
