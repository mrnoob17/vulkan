#pragma once

template<size_t S, typename T>
constexpr size_t array_size(const T(&)[S]){
    return S;
}

