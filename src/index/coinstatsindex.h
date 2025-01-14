// Copyright (c) 2020-2022 The Bitcoin Core developers
// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <amount.h>
#include <coinstats.h>
#include <index/base.h>
#include <serialize.h>
#include <sync.h>
#include <uint256.h>

#include <memory>
#include <optional>

static constexpr bool DEFAULT_COINSTATSINDEX = false;

/**
 * CoinStatsIndex maintains statistics on the UTXO set.
 */
class CoinStatsIndex final : public BaseIndex {
    struct DB;
    const std::unique_ptr<DB> m_db;

    Mutex m_cs_stats;
    struct BestBlockStats;
    std::unique_ptr<BestBlockStats> m_best_block_stats GUARDED_BY(m_cs_stats) {nullptr};

protected:
    bool WriteBlock(const CBlock &block, const CBlockIndex *pindex) override;

    BaseIndex::DB &GetDB() const override;

public:
    /// Constructs the index, which becomes available to be queried.
    explicit CoinStatsIndex(size_t n_cache_size, bool f_memory = false, bool f_wipe = false);

    // Destructor is declared because this class contains a unique_ptr to an incomplete type.
    virtual ~CoinStatsIndex() override;

    /// Look up by block hash.
    ///
    /// @param[in]   blockHash  The block hash to look up UTXO set stats for. Need not be a block in the active chain.
    /// @return  valid optional if stats for this blockHash are found, invalid optional otherwise.
    std::optional<CoinStats> GetStatsForHash(const BlockHash &blockHash, CoinStatsHashType hash_type) const;

    /// Look up by block height in the active chain.
    ///
    /// @param[in]   height  The height of the block to be returned.
    /// @return  valid optional if stats for this height are found, invalid optional otherwise.
    std::optional<CoinStats> GetStatsForHeight(int height, CoinStatsHashType hash_type) const;
};

/// The global UTXO set stats index object. May be null.
extern std::unique_ptr<CoinStatsIndex> g_coin_stats_index;
