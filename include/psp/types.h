#pragma once
#include <cstdint>
#include <cstddef>

// PSP Primitive Types
typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;

typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;

typedef float     f32;
typedef double    f64;

// PSP Screen Dimensions
constexpr int PSP_SCREEN_WIDTH  = 480;
constexpr int PSP_SCREEN_HEIGHT = 272;
constexpr int PSP_SCREEN_BUFFER_WIDTH = 512; // PSP VRAM pitch is 512 pixels
