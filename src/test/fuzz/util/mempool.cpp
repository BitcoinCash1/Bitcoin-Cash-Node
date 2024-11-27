// Copyright (c) 2022 The Bitcoin Core developers
// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <consensus/consensus.h>
#include <primitives/transaction.h>
#include <policy/policy.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/util.h>
#include <test/fuzz/util/mempool.h>

#include <cassert>
#include <cstdint>
#include <limits>

CTxMemPoolEntry ConsumeTxMemPoolEntry(FuzzedDataProvider& fuzzed_data_provider, const CTransaction& tx) noexcept {
    // Avoid:
    // policy/feerate.cpp:28:34: runtime error: signed integer overflow: 34873208148477500 * 1000 cannot be represented in type 'long'
    //
    // Reproduce using CFeeRate(348732081484775, 10).GetFeePerK()
    const Amount fee = ConsumeMoney(fuzzed_data_provider, /*max=*/(std::numeric_limits<int64_t>::max() / int64_t{100'000}) * SATOSHI);
    assert(MoneyRange(fee));
    const int64_t time = fuzzed_data_provider.ConsumeIntegral<int64_t>();
    const bool spends_coinbase = fuzzed_data_provider.ConsumeBool();
    const int64_t sig_checks = fuzzed_data_provider.ConsumeIntegralInRange<int64_t>(0, MAX_TX_SIGCHECKS);
    return CTxMemPoolEntry{MakeTransactionRef(CMutableTransaction{tx}), fee, time, spends_coinbase, sig_checks, {}};
}
