#pragma once
#include "psp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Kernel System Time
u32  sceKernelGetSystemTimeLow();
u64  sceKernelGetSystemTimeWide();
void sceKernelDelayThread(u32 usec);

// Memory Management (decompiled from Engine_MemAlloc @ 0x000573C8)
void* Engine_MemAlloc(u32 size);
void  Engine_MemFree(void* ptr);

// High Voltage Software Quantum Engine Volatile Memory (0x000924A0)
int  Engine_VolatileMemInit();

#ifdef __cplusplus
}
#endif
