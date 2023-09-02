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

template<typename T, size_t R, size_t C>
struct Matrix
{
    using type = T;
    static constexpr size_t rows {R};
    static constexpr size_t cols {C};

    Matrix() {}

    Matrix(const Matrix<T, R, C>& m)
    {
        *this = m;
    }

    Matrix& operator = (const Matrix<T, R, C>& m)
    {
        for(int i = 0; i < R; i++)
        {
            for(int j = 0; j < C; j++){
                data[i][j] = m[i][j];
            }
        }
        return *this;
    }

    const T* operator[](size_t i) const
    {
        return data[i];
    }

    T* operator[](size_t i)
    {
        return data[i];
    }

    T data[R][C] {{}};
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
