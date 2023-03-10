#pragma once
#include <cstdint>

#define null nullptr

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s32 = int32_t;
using f32 = float;

struct vec2 {
    f32 x, y;
};

struct vec3 {
    f32 x, y, z;
};

struct uvec2 {
    u32 x;
    u32 y;
};

#define NAMESPACE_BEGIN namespace docscanner {
#define NAMESPACE_END }