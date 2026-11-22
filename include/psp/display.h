#pragma once
#include "psp/types.h"

enum PspDisplayPixelFormat : int {
    PSP_DISPLAY_PIXEL_FORMAT_565   = 0,
    PSP_DISPLAY_PIXEL_FORMAT_5551  = 1,
    PSP_DISPLAY_PIXEL_FORMAT_4444  = 2,
    PSP_DISPLAY_PIXEL_FORMAT_8888  = 3,
};

enum PspDisplaySetBufSync : int {
    PSP_DISPLAY_SETBUF_IMMEDIATE = 0,
    PSP_DISPLAY_SETBUF_NEXTFRAME = 1,
};

#ifdef __cplusplus
extern "C" {
#endif

// PSP Display API
int sceDisplaySetFrameBuf(void* topaddr, int bufferwidth, int pixelformat, int sync);
int sceDisplayWaitVblankStart();
u32 sceDisplayGetVcount();

// Engine timing / VBlank synchronizer (decompiled from 0x0008E4D0)
void Engine_WaitVblank(int count);

// Frame timing statistics
float HAL_GetFPS();
float HAL_GetDeltaTime();

#ifdef __cplusplus
}
#endif
