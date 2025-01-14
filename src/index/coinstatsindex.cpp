// Copyright (c) 2020-2022 The Bitcoin Core developers
// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <index/coinstatsindex.h>

#include <chain.h>
#include <chainparams.h>
#include <logging.h>
#include <node/blockstorage.h>
#include <undo.h>
#include <util/check.h>
#include <validation.h>

namespace {

inline constexpr uint8_t DB_BLOCK_HASH{'h'};

struct DBHashKey {
    BlockHash hash;

    explicit DBHashKey(const BlockHash &hash_in) : hash(hash_in) {}

    SERIALIZE_METHODS(DBHashKey, obj) {
        uint8_t prefix;
        SER_WRITE(obj, prefix = DB_BLOCK_HASH);
        READWRITE(prefix);
        if (prefix != DB_BLOCK_HASH) {
            throw std::ios_base::failure("Invalid format for coinstatsindex DB hash key");
        }

        READWRITE(obj.hash);
    }
};

struct DBVal : CoinStatsBase {
    ECMultiSet ec_multiset;
    MuHash3072 muhash;

    SERIALIZE_METHODS(DBVal, obj) {
        uint8_t version = 1u;
        READWRITE(version);
        if (version != 1u) {
            // Refuse to proceed on unexpected version number
            throw std::ios_base::failure("Unknown version for coinstatsindex DB value");
        }
        READWRITE(obj.ec_multiset);
        READWRITE(obj.muhash);

        // Below are fields from CoinStatsBase
        READWRITE(obj.nHeight);
        READWRITE(obj.nTransactionOutputs);
        READWRITE(obj.nBogoSize);
        READWRITE(obj.nTotalAmount);
        READWRITE(obj.total_subsidy);
        READWRITE(obj.total_unspendable_amount);
        READWRITE(obj.total_prevout_spent_amount);
        READWRITE(obj.total_new_outputs_ex_coinbase_amount);
        READWRITE(obj.total_coinbase_amount);
        READWRITE(obj.total_unspendables_genesis_block);
        READWRITE(obj.total_unspendables_bip30);
        READWRITE(obj.total_unspendables_scripts);
        READWRITE(obj.total_unspendables_unclaimed_rewards);
    }

    [[nodiscard]]
    CoinStats ToCoinStats(const BlockHash &blockHash, const CoinStatsHashType ht) const {
        CoinStats ret;
        static_cast<CoinStatsBase &>(ret) = *this;
        ret.hashBlock = blockHash;// we save CoinStatsBase to DB which lacks this field, so update it now
        ret.indexUsed = true;
        ret.nDiskSize = 0;
        switch (ht) {
            case CoinStatsHashType::NONE:
                break;
            case CoinStatsHashType::ECMH:
                ret.multiSet = ec_multiset;
                ret.hashSerialized = ec_multiset.GetHash();
                break;
            case CoinStatsHashType::MUHASH_TESTING:
                ret.multiSet = muhash;
                MuHash3072(muhash).Finalize(ret.hashSerialized);
                break;
            case CoinStatsHashType::HASH_SERIALIZED_3:
                LogPrintf("ERROR: Invalid CoinStatsHashType specified to CoinStatsIndex::GetStatsForHash: %i\n",
                          static_cast<int>(ht));
                break;
        }
        return ret;
    }
};

} // namespace

std::unique_ptr<CoinStatsIndex> g_coin_stats_index;

/**
 * Access to the coin stats database (indexes/coinstatsindex/)
 *
 * The database stores coin stats by block hash, for all blocks ever connected
 * to the main chain ever (even reorged blocks).
 */
struct CoinStatsIndex::DB : BaseIndex::DB {
    explicit DB(size_t n_cache_size, bool f_memory = false, bool f_wipe = false);

    /// Read the UTXO set stats for the given block hash.
    /// Returns false if the block hash is not indexed.
    bool ReadStats(const BlockHash &blockHash, DBVal &stats) const;

    /// Write stats for a given block hash to the DB.
    bool WriteStats(const BlockHash &blockHash, const DBVal &stats, bool fSync = false);
};

struct CoinStatsIndex::BestBlockStats : DBVal {
    BlockHash blockHash;

    [[nodiscard]]
    static BestBlockStats FromDBVal(DBVal && v, const BlockHash &bh) {
        return BestBlockStats{std::move(v), bh};
    }
};

CoinStatsIndex::DB::DB(size_t n_cache_size, bool f_memory, bool f_wipe)
    : BaseIndex::DB(GetDataDir() / "indexes" / "coinstatsindex", n_cache_size, f_memory, f_wipe) {}

bool CoinStatsIndex::DB::ReadStats(const BlockHash &blockHash, DBVal &stats) const {
    return Read(DBHashKey(blockHash), stats);
}

bool CoinStatsIndex::DB::WriteStats(const BlockHash &blockHash, const DBVal &stats, bool fSync) {
    return Write(DBHashKey(blockHash), stats, fSync);
}


CoinStatsIndex::CoinStatsIndex(size_t n_cache_size, bool f_memory, bool f_wipe)
    : BaseIndex("coinstatsindex"), m_db(std::make_unique<CoinStatsIndex::DB>(n_cache_size, f_memory, f_wipe))
{}

CoinStatsIndex::~CoinStatsIndex() {}

std::optional<CoinStats> CoinStatsIndex::GetStatsForHeight(int height, CoinStatsHashType ht) const {
    const CBlockIndex *pindex;
    WITH_LOCK(cs_main, pindex = ::ChainActive()[height]);
    std::optional<CoinStats> ret;
    if (pindex) {
        ret = GetStatsForHash(pindex->GetBlockHash(), ht);
    }
    return ret;
}

std::optional<CoinStats> CoinStatsIndex::GetStatsForHash(const BlockHash &blockHash, const CoinStatsHashType ht) const {
    if (DBVal v; m_db->ReadStats(blockHash, v)) {
        return v.ToCoinStats(blockHash, ht);
    }
    return std::nullopt;
}

BaseIndex::DB &CoinStatsIndex::GetDB() const {
    return *m_db;
}

bool CoinStatsIndex::WriteBlock(const CBlock &block, const CBlockIndex *pindex) {
    std::optional<BestBlockStats> stats = [pindex, this]() -> std::optional<BestBlockStats> {
        if (!pindex->pprev) {
            // return default-constructed value for block 0
            return BestBlockStats();
        }
        const auto prev_bh = pindex->pprev->GetBlockHash();
        if (LOCK(m_cs_stats);
            m_best_block_stats && m_best_block_stats->blockHash == prev_bh) {
            // return the previous block's state to build off of
            return *m_best_block_stats;
        }
        // If we get here, perhaps this block is on a different chain than m_best_block, or somesuch.
        // Attempt to read previous block's state and return it if successful.
        DBVal v;
        if (!m_db->ReadStats(prev_bh, v)) {
            // error, not found, etc
            return std::nullopt;
        }
        return BestBlockStats::FromDBVal(std::move(v), prev_bh);
    }();

    // if missing previous stats, cannot proceed
    if (!stats) {
        return false;
    }

    // update stats, accumulate for this block
    CBlockUndo undo;
    const auto block_subsidy = GetBlockSubsidy(pindex->nHeight, ::Params().GetConsensus());
    stats->nHeight = pindex->nHeight;
    stats->blockHash = pindex->GetBlockHash();
    stats->total_subsidy += block_subsidy;

    // Ignore genesis block
    if (pindex->nHeight > 0) {
        if (!UndoReadFromDisk(undo, pindex)) {
            // error reading undo...
            return false;
        }

        const bool isBIP30Unspendable = IsBIP30Unspendable(*pindex);

        std::vector<uint8_t> scratchBuf; /* to avoid repetitive allocations, we reuse scratch space in the below loop */

        for (size_t i = 0u; i < block.vtx.size(); ++i) {
            const auto &tx = block.vtx[i];

            if (isBIP30Unspendable && tx->IsCoinBase()) {
                const auto valueOut = tx->GetValueOut();
                stats->total_unspendable_amount += valueOut;
                stats->total_unspendables_bip30 += valueOut;
                continue;
            }

            for (size_t j = 0u; j < tx->vout.size(); ++j) {
                const Coin coin(tx->vout[j], pindex->nHeight, tx->IsCoinBase());
                const Amount &nValue = coin.GetTxOut().nValue;
                const COutPoint outpoint(tx->GetId(), j);

                // Skip unspendable coins
                if (coin.GetTxOut().scriptPubKey.IsUnspendable()) {
                    stats->total_unspendable_amount += nValue;
                    stats->total_unspendables_scripts += nValue;
                    continue;
                }

                AddCoinToMuHash(stats->muhash, outpoint, coin, &scratchBuf);
                AddCoinToMultiSet(stats->ec_multiset, outpoint, coin, &scratchBuf);

                if (tx->IsCoinBase()) {
                    stats->total_coinbase_amount += nValue;
                } else {
                    stats->total_new_outputs_ex_coinbase_amount += nValue;
                }

                ++stats->nTransactionOutputs;
                stats->safeAddToTotalAmount(nValue);
                stats->nBogoSize += GetBogoSize(coin.GetTxOut());
            }

            // The coinbase tx has no undo data since no former output is spent
            if (!tx->IsCoinBase()) {
                const auto &tx_undo = undo.vtxundo.at(i - 1u);

                for (size_t j = 0; j < tx_undo.vprevout.size(); ++j) {
                    const Coin &coin = tx_undo.vprevout[j];
                    const COutPoint &outpoint = tx->vin[j].prevout;
                    const Amount &nValue = coin.GetTxOut().nValue;

                    stats->total_prevout_spent_amount += nValue;

                    if (coin.GetTxOut().scriptPubKey.IsUnspendable()) {
                        // Undo "unspendable" coin being spent (should never happen).
                        //
                        // This branch should never be taken. We log here if it is to detect bugs.
                        //
                        // Why do we have this branch? In case future upgrades tighten the criteria on what is
                        // considered "unspendable"... which means past-block "spendables" become present-day
                        // "unspendables" -- and this could be buggy if not implemented correctly to account for
                        // scriptFlags; so we detect the situation here as a belt-and-suspenders check.
                        //
                        // Future code that might make IsUnspendable() depend on scriptFlags could change the logic here
                        // and this warning can be removed in that case.
                        LogPrintf("%s\n", STR_INTERNAL_BUG(strprintf("\"unspendable\" coin %s was spent in tx %s",
                                                                     outpoint.ToString(true), tx->GetId().ToString())));
                        stats->total_unspendable_amount -= nValue;
                        stats->total_unspendables_scripts -= nValue;
                        continue;
                    }

                    RemoveCoinFromMuHash(stats->muhash, outpoint, coin, &scratchBuf);
                    RemoveCoinFromMultiSet(stats->ec_multiset, outpoint, coin, &scratchBuf);

                    --stats->nTransactionOutputs;
                    stats->safeSubFromTotalAmount(nValue);
                    stats->nBogoSize -= GetBogoSize(coin.GetTxOut());
                }
            }
        }

    } else {
        // genesis block
        stats->total_unspendable_amount += block_subsidy;
        stats->total_unspendables_genesis_block += block_subsidy;
    }

    // If spent prevouts + block subsidy are still a higher amount than
    // new outputs + coinbase + current unspendable amount this means
    // the miner did not claim the full block reward. Unclaimed block
    // rewards are also unspendable.
    const Amount unclaimed_rewards = (stats->total_prevout_spent_amount + stats->total_subsidy)
                                     - (stats->total_new_outputs_ex_coinbase_amount + stats->total_coinbase_amount
                                        + stats->total_unspendable_amount);
    if (MoneyRange(unclaimed_rewards)) {
        stats->total_unspendable_amount += unclaimed_rewards;
        stats->total_unspendables_unclaimed_rewards += unclaimed_rewards;
    } else {
        // This should never happen but we warn here for belt-and-suspenders check to inform of bugs.
        LogPrintf("%s\n", STR_INTERNAL_BUG(strprintf("\"unclaimed rewards\" for block %s is %s",
                                                     pindex->GetBlockHash().ToString(), unclaimed_rewards.ToString())));
    }


    // save stats to db
    if (!m_db->WriteStats(stats->blockHash, *stats)) {
        // error...
        return false;
    }

    // cache this for next call to WriteBlock()
    LOCK(m_cs_stats);
    if (!m_best_block_stats) {
        m_best_block_stats = std::make_unique<BestBlockStats>(*stats);
    } else {
        *m_best_block_stats = *stats;
    }
    return true;
}
