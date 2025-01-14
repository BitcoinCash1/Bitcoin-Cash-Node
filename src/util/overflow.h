// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <optional>
#include <type_traits>

//! Overflow-checking addition.
//! @return `a + b` inside a valid optional if `a + b` does not overflow, otherwise returns `std::nullopt`.
template <typename T>
[[nodiscard]] std::optional<T> CheckedAdd(const T a, const T b) noexcept {
    static_assert(std::is_integral_v<T>, "CheckedAdd only works with integral types");
    T res;
    // Ok for us to use the __builtin_add_overflow() because the only compilers we support (clang & gcc) both offer it.
    if (__builtin_add_overflow(a, b, &res)) {
        return std::nullopt;
    }
    return res;
}

//! Overflow-checking subtraction.
//! @return `a - b` inside a valid optional if `a - b` does not overflow, otherwise returns `std::nullopt`.
template <typename T>
[[nodiscard]] std::optional<T> CheckedSub(const T a, const T b) noexcept {
    static_assert(std::is_integral_v<T>, "CheckedSub only works with integral types");
    T res;
    if (__builtin_sub_overflow(a, b, &res)) {
        return std::nullopt;
    }
    return res;
}

//! Overflow-checking multiplication.
//! @return `a * b` inside a valid optional if `a * b` does not overflow, otherwise returns `std::nullopt`.
template <typename T>
[[nodiscard]] std::optional<T> CheckedMul(const T a, const T b) noexcept {
    static_assert(std::is_integral_v<T>, "CheckedMul only works with integral types");
    T res;
    if (__builtin_mul_overflow(a, b, &res)) {
        return std::nullopt;
    }
    return res;
}
