#pragma once
#include <cstdint>
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

#include <vector>
template<typename T>
using Array = std::vector<T>;

#include <string>
using String = std::string;

struct V2
{
    float x {0};
    float y {0};
};

struct V4 {
    union {
        float c[4];
        struct {
            union { float x, r; };
            union { float y, g; };
            union { float z, b; };
            union { float w, a; };
        };
    };
};

using RGBA = V4;
