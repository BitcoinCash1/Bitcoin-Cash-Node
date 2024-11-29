// Copyright (c) 2019-2022 The Bitcoin Core developers
// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <stdexcept>
#include <string_view>
#include <utility>

std::string StrFormatInternalBug(std::string_view msg, std::string_view file, int line, std::string_view func);

struct NonFatalCheckError : std::runtime_error {
    NonFatalCheckError(std::string_view msg, std::string_view file, int line, std::string_view func);
};

/** Helper for CHECK_NONFATAL() */
template <typename T>
T&& inline_check_non_fatal(T&& val, const char* file, int line, const char* func, const char* assertion) {
    if (!val) {
        throw NonFatalCheckError{assertion, file, line, func};
    }
    return std::forward<T>(val);
}

// All macros may use __func__ inside a lambda, so put them under nolint.
// NOLINTBEGIN(bugprone-lambda-function-name)

#define STR_INTERNAL_BUG(msg) StrFormatInternalBug((msg), __FILE__, __LINE__, __func__)

/**
 * Identity function. Throw a NonFatalCheckError when the condition evaluates to false
 *
 * This should only be used
 * - where the condition is assumed to be true, not for error handling or validating user input
 * - where a failure to fulfill the condition is recoverable and does not abort the program
 *
 * For example in RPC code, where it is undesirable to crash the whole program, this can be generally used to replace
 * asserts or recoverable logic errors. A NonFatalCheckError in RPC code is caught and passed as a string to the RPC
 * caller, which can then report the issue to the developers.
 */
#define CHECK_NONFATAL(condition) \
    inline_check_non_fatal(condition, __FILE__, __LINE__, __func__, #condition)
