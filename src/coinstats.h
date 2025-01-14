// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <amount.h>
#include <crypto/muhash.h>
#include <ec_multiset.h>
#include <primitives/blockhash.h>
#include <primitives/transaction.h>

#include <functional>
#include <optional>
#include <variant>

class Coin;
class CCoinsView;

enum class CoinStatsHashType {
    NONE,
    /// sha256d simple non-multiset hash of current utxo set (Core compatible)
    HASH_SERIALIZED_3,
    /// MUHASH for Core compat.
    MUHASH_TESTING,
    /// BCH-specific ECMultiSet hasher
    ECMH,
};

struct CoinStatsBase {
    int nHeight{};
    uint64_t nTransactionOutputs{};
    uint64_t nBogoSize{};

    //! The total amount, or nullopt if an overflow occurred calculating it
    std::optional<Amount> nTotalAmount{Amount::zero()};

    // Following values are only available from coinstats index

    //! Total cumulative amount of block subsidies up to and including this block
    Amount total_subsidy;
    //! Total cumulative amount of unspendable coins up to and including this block
    Amount total_unspendable_amount;
    //! Total cumulative amount of prevouts spent up to and including this block
    Amount total_prevout_spent_amount;
    //! Total cumulative amount of outputs created up to and including this block
    Amount total_new_outputs_ex_coinbase_amount;
    //! Total cumulative amount of coinbase outputs up to and including this block
    Amount total_coinbase_amount;
    //! The unspendable coinbase amount from the genesis block
    Amount total_unspendables_genesis_block;
    //! The two unspendable coinbase outputs total amount caused by BIP30
    Amount total_unspendables_bip30;
    //! Total cumulative amount of outputs sent to unspendable scripts (OP_RETURN for example) up to and including this block
    Amount total_unspendables_scripts;
    //! Total cumulative amount of coins lost due to unclaimed miner rewards up to and including this block
    Amount total_unspendables_unclaimed_rewards;

    CoinStatsBase() noexcept = default;

    //! Adds `amount` to `this->nTotalAmount` (if valid). On overflow, invalidates `this->nTotalAmount`
    void safeAddToTotalAmount(const Amount &amount);
    //! Subtracts `amount` from `this->nTotalAmount` (if valid). On overflow, invalidates `this->nTotalAmount`
    void safeSubFromTotalAmount(const Amount &amount);

protected:
    explicit CoinStatsBase(int height) noexcept : nHeight{height} {}
};

struct CoinStats : CoinStatsBase {
    BlockHash hashBlock;
    uint256 hashSerialized; /* May either be a sha256d hash of all the current utxos or the ECMultiSet.GetHash(), depending on caller */
    uint64_t nTransactions{}; /* Not available if using coinstatsindex */
    uint64_t nDiskSize{}; /* Not available if using coinstatsindex */

    //! Signals if the coinstatsindex was used to retrieve the statistics.
    bool indexUsed{};

    //! May be either monostate for non-multiset hasher used, or the multiset hasher state for this block (coinstats index only)
    //! Currently, will be non-monostate only for: CoinStatsHashType::ECMH and CoinStatsHashType::MUHASH.
    std::variant<std::monostate, MuHash3072, ECMultiSet> multiSet;

    CoinStats() noexcept = default;
    CoinStats(int block_height, const BlockHash &block_hash) noexcept
        : CoinStatsBase(block_height), hashBlock{block_hash} {}
};

/// Database-independent metric for a particular UTXO's size
size_t GetBogoSize(const CTxOut &txout);

/// Adds a coin to the multiset `ms`. Pass-in an optional `scratchBuf` to reuse (to avoid repetitive reallocations)
void AddCoinToMultiSet(ECMultiSet &ms, const COutPoint &outpoint, const Coin &coin, std::vector<uint8_t> *scratchBuf = nullptr);
/// Removes a coin from the multiset `ms`. Pass-in an optional `scratchBuf` to reuse (to avoid repetitive reallocations)
void RemoveCoinFromMultiSet(ECMultiSet &ms, const COutPoint &outpoint, const Coin &coin, std::vector<uint8_t> *scratchBuf = nullptr);

/// Adds a coin to the muhash `mh`. Pass-in an optional `scratchBuf` to reuse (to avoid repetitive reallocations)
void AddCoinToMuHash(MuHash3072 &mh, const COutPoint &outpoint, const Coin &coin, std::vector<uint8_t> *scratchBuf = nullptr);
/// Removes a coin from the muhash `mh`. Pass-in an optional `scratchBuf` to reuse (to avoid repetitive reallocations)
void RemoveCoinFromMuHash(MuHash3072 &mh, const COutPoint &outpoint, const Coin &coin, std::vector<uint8_t> *scratchBuf = nullptr);

/// Calculate statistics about the unspent transaction output set
std::optional<CoinStats> ComputeUTXOStats(CCoinsView *view, CoinStatsHashType hash_type,
                                          const std::function<void()> &interruption_point);
