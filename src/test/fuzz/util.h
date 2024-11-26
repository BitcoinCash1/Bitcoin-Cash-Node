// Copyright (c) 2009-2020 The Bitcoin Core developers
// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <amount.h>
#include <serialize.h>
#include <streams.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <version.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

[[nodiscard]]
inline std::vector<uint8_t> ConsumeRandomLengthByteVector(FuzzedDataProvider& fuzzed_data_provider,
                                                          const size_t max_length = 4096) noexcept {
    const std::string s = fuzzed_data_provider.ConsumeRandomLengthString(max_length);
    return {s.begin(), s.end()};
}

template <typename T>
[[nodiscard]]
inline std::optional<T> ConsumeDeserializable(FuzzedDataProvider& fuzzed_data_provider,
                                              const size_t max_length = 4096) noexcept {
    const std::vector<uint8_t> buffer = ConsumeRandomLengthByteVector(fuzzed_data_provider, max_length);
    VectorReader vr(SER_NETWORK, INIT_PROTO_VERSION, buffer, 0);
    T obj;
    try {
        vr >> obj;
    } catch (const std::ios_base::failure&) {
        return std::nullopt;
    }
    return obj;
}

[[nodiscard]]
inline Amount ConsumeMoney(FuzzedDataProvider& fuzzed_data_provider,
                           const std::optional<Amount> &max = std::nullopt) noexcept {
    auto val = fuzzed_data_provider.ConsumeIntegralInRange<int64_t>(0, max.value_or(MAX_MONEY) / SATOSHI);
    return val * SATOSHI;
}
