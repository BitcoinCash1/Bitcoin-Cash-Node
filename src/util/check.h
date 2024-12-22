// Copyright (c) 2019 The Bitcoin Core developers
// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <clientversion.h>
#include <tinyformat.h>

#include <stdexcept>

struct NonFatalCheckError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
/**
 * Throw a NonFatalCheckError when the condition evaluates to false
 *
 * This should only be used
 * - where the condition is assumed to be true, not for error handling or validating user input
 * - where a failure to fulfill the condition is recoverable and does not abort the program
 *
 * For example in RPC code, where it is undersirable to crash the whole program, this can be generally used to replace
 * asserts or recoverable logic errors. A NonFatalCheckError in RPC code is caught and passed as a string to the RPC
 * caller, which can then report the issue to the developers.
 */
#define CHECK_NONFATAL(condition)                                             \
    do {                                                                      \
        if (!(condition)) {                                                   \
            throw NonFatalCheckError(                                         \
                strprintf("Internal bug detected: %s\n%s:%d (%s)\n%s %s\n"    \
                          "Please report this issue to the developers\n",     \
                          (#condition), __FILE__, __LINE__, __func__,         \
                          CLIENT_NAME, FormatFullVersion()));                 \
        }                                                                     \
    } while (false)
