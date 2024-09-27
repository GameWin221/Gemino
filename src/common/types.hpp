#ifndef GEMINO_TYPES_HPP
#define GEMINO_TYPES_HPP

#include <cinttypes>
#include <cmath>
#include <memory>

using usize = size_t;
using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;

using i64 = int64_t;
using i32 = int32_t;
using i16 = int16_t;
using i8 = int8_t;

using f32 = float;
using f64 = double;

#define INVALID_HANDLE UINT32_MAX

using Proxy = void*;

template <typename T>
using Unique = std::unique_ptr<T>;

template<typename T, typename ... Args>
Unique<T> MakeUnique(Args&& ... args)
{
    return std::make_unique<T>(std::forward<Args>(args)...);
}

#endif