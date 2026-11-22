#pragma once
#include "psp/types.h"

// PSP Control Button Bitmasks
enum PspCtrlButtons : u32 {
    PSP_CTRL_SELECT     = 0x00000001,
    PSP_CTRL_START      = 0x00000008,
    PSP_CTRL_UP         = 0x00000010,
    PSP_CTRL_RIGHT      = 0x00000020,
    PSP_CTRL_DOWN       = 0x00000040,
    PSP_CTRL_LEFT       = 0x00000080,
    PSP_CTRL_LTRIGGER   = 0x00000100,
    PSP_CTRL_RTRIGGER   = 0x00000200,
    PSP_CTRL_TRIANGLE   = 0x00001000,
    PSP_CTRL_CIRCLE     = 0x00002000,
    PSP_CTRL_CROSS      = 0x00004000,
    PSP_CTRL_SQUARE     = 0x00008000,
    PSP_CTRL_HOME       = 0x00010000,
    PSP_CTRL_HOLD       = 0x00020000,
};

enum PspCtrlMode : int {
    PSP_CTRL_MODE_DIGITAL = 0,
    PSP_CTRL_MODE_ANALOG  = 1,
};

struct SceCtrlData {
    u32 TimeStamp;
    u32 Buttons;
    u8  Lx; // 0..255 (128 centered)
    u8  Ly; // 0..255 (128 centered)
    u8  Rsrv[6];
};

#ifdef __cplusplus
extern "C" {
#endif

// PSP SDK C API Mocks
int sceCtrlSetSamplingCycle(int cycle);
int sceCtrlSetSamplingMode(int mode);
int sceCtrlReadBufferPositive(SceCtrlData* pad_data, int count);

// Ben 10 Engine specific controller polling (decompiled from 0x0007141C)
void Input_PollController();

// Internal HAL helpers
void HAL_UpdateInputState();

#ifdef __cplusplus
}
#endif
