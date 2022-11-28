// Copyright (c) 2017 - The Bitcoin Developers
// Copyright (c) 2017-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <array>
#include <cstdint>

class CTransaction;

/**
 * The script cache is a map using a key/value element, that caches the
 * success of executing a specific transaction's input scripts under a
 * specific set of flags, along with any associated information learned
 * during execution.
 *
 * The key is slightly shorter than a power-of-two size to make room for
 * the value.
 */
class ScriptCacheKey {
    std::array<uint8_t, 28> data;

public:
    ScriptCacheKey() = default;
    ScriptCacheKey(const ScriptCacheKey &rhs) = default;
    ScriptCacheKey(const CTransaction &tx, uint32_t flags);

    bool operator==(const ScriptCacheKey &rhs) const {
        return rhs.data == data;
    }
    ScriptCacheKey &operator=(const ScriptCacheKey &) noexcept = default; // prevent -Wdepecated-copy

    friend class ScriptCacheHasher;
};

// DoS prevention: limit cache size to 32MB (over 1000000 entries on 64-bit
// systems). Due to how we count cache size, actual memory usage is slightly
// more (~32.25 MB)
static constexpr int64_t DEFAULT_MAX_SCRIPT_CACHE_SIZE = 32;
// Maximum sig cache size allowed
static constexpr int64_t MAX_MAX_SCRIPT_CACHE_SIZE = 16384;

/** Initializes the script-execution cache */
void InitScriptExecutionCache();

/**
 * Check if a given key is in the cache, and if so, return its values.
 * (if not found, nSigChecks may or may not be set to an arbitrary value)
 */
bool IsKeyInScriptCache(ScriptCacheKey key, bool erase, int &nSigChecksOut);

/**
 * Add an entry in the cache.
 */
void AddKeyInScriptCache(ScriptCacheKey key, int nSigChecks);
