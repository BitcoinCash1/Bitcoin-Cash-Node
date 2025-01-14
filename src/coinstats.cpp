// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coinstats.h>

#include <chain.h> // for LookupBlockIndex
#include <coins.h>
#include <primitives/token.h>
#include <primitives/transaction.h>
#include <streams.h>
#include <util/overflow.h>
#include <util/system.h>
#include <utxosync/primitives.h>
#include <validation.h> // for cs_main
#include <version.h>

namespace {

// Helper used by AddCoinToMultiSet() and RemoveCoinFromMultiSet() below
template <bool add>
void serUTXOAndAddOrRemoveFromECMS(ECMultiSet &ms, const utxosync::UTXOShallowCRef &u, std::vector<uint8_t> *buf) {
    std::optional<std::vector<uint8_t>> tmp;
    if (!buf) {
        buf = &tmp.emplace();
    } else {
        buf->clear();
    }
    CVectorWriter(SER_NETWORK, PROTOCOL_VERSION, *buf, 0) << u;
    if constexpr (add) {
        ms.Add(*buf);
    } else {
        ms.Remove(*buf);
    }
}

// Helper used by the hash_serialized_3 code
template <typename Stream>
void basicUTXOSer(Stream &ss, const COutPoint &outpoint, const Coin &coin) {
    ss << outpoint;
    ss << static_cast<uint32_t>((coin.GetHeight() << 1) + coin.IsCoinBase());
    ss << coin.GetTxOut();
}


template <bool add>
void safeAddOrSub(std::optional<Amount> &rop, const Amount &val) {
    if (!rop.has_value()) {
        return;
    }
    const int64_t a = *rop / SATOSHI;
    const int64_t b = val / SATOSHI;
    std::optional<int64_t> res;
    if constexpr (add) {
        res = CheckedAdd(a, b);
    } else {
        res = CheckedSub(a, b);
    }
    if (res) {
        rop = *res * SATOSHI;
    } else {
        rop.reset();
    }
}

template <bool add>
void serUTXOAndAddOrRemoveFromMuHash(MuHash3072 &mh, const COutPoint &o, const Coin &c, std::vector<uint8_t> *buf) {
    std::optional<std::vector<uint8_t>> tmp;
    if (!buf) {
        buf = &tmp.emplace();
    } else {
        buf->clear();
    }
    CVectorWriter vw(SER_NETWORK, PROTOCOL_VERSION, *buf, 0);
    basicUTXOSer(vw, o, c);
    if constexpr (add) {
        mh.Insert(*buf);
    } else {
        mh.Remove(*buf);
    }
}

} // namespace


size_t GetBogoSize(const CTxOut &txout) {
    size_t ret =  32u /* txid */
                 + 4u /* vout index */
                 + 4u /* height + coinbase */
                 + 8u /* amount */ +
                 + 2u /* scriptPubKey len */;
    if (txout.tokenDataPtr) {
        ret += 1u /* prefix byte */ + GetSerializeSize(*txout.tokenDataPtr) /* token data */;
    }
    ret += txout.scriptPubKey.size();
    return ret;
}

void AddCoinToMultiSet(ECMultiSet &ms, const COutPoint &outpoint, const Coin &coin, std::vector<uint8_t> *scratchBuf) {
    serUTXOAndAddOrRemoveFromECMS<true>(ms, {outpoint, coin}, scratchBuf);
}

void RemoveCoinFromMultiSet(ECMultiSet &ms, const COutPoint &outpoint, const Coin &coin, std::vector<uint8_t> *scratchBuf) {
    serUTXOAndAddOrRemoveFromECMS<false>(ms, {outpoint, coin}, scratchBuf);
}

void AddCoinToMuHash(MuHash3072 &mh, const COutPoint &outpoint, const Coin &coin, std::vector<uint8_t> *scratchBuf) {
    serUTXOAndAddOrRemoveFromMuHash<true>(mh, outpoint, coin, scratchBuf);
}

void RemoveCoinFromMuHash(MuHash3072 &mh, const COutPoint &outpoint, const Coin &coin, std::vector<uint8_t> *scratchBuf) {
    serUTXOAndAddOrRemoveFromMuHash<false>(mh, outpoint, coin, scratchBuf);
}

void CoinStatsBase::safeAddToTotalAmount(const Amount &amount) {
    safeAddOrSub<true>(nTotalAmount, amount);
}

void CoinStatsBase::safeSubFromTotalAmount(const Amount &amount) {
    safeAddOrSub<false>(nTotalAmount, amount);
}

static void ApplyStats(CoinStats &stats, const TxId &, const std::map<uint32_t, Coin> &outputs) {
    assert(!outputs.empty());
    ++stats.nTransactions;
    for (const auto & [outN, coin] : outputs) {
        ++stats.nTransactionOutputs;
        stats.safeAddToTotalAmount(coin.GetTxOut().nValue);
        stats.nBogoSize += GetBogoSize(coin.GetTxOut());
    }
}

static void ApplyCoinHash(std::nullptr_t, const COutPoint &, const Coin &) {}
static void ApplyCoinHash(HashWriter &ss, const COutPoint &o, const Coin &c) { basicUTXOSer(ss, o, c); }
static void ApplyCoinHash(ECMultiSet &ms, const COutPoint &o, const Coin &c) { AddCoinToMultiSet(ms, o, c); }
static void ApplyCoinHash(MuHash3072 &mh, const COutPoint &o, const Coin &c) { AddCoinToMuHash(mh, o, c); }

//! Warning: be very careful when changing this! assumeutxo and UTXO snapshot
//! validation commitments are reliant on the hash constructed by this
//! function.
//!
//! If the construction of this hash is changed, it will invalidate
//! existing UTXO snapshots. This will not result in any kind of consensus
//! failure, but it will force clients that were expecting to make use of
//! assumeutxo to do traditional IBD instead.
//!
//! It is also possible, though very unlikely, that a change in this
//! construction could cause a previously invalid (and potentially malicious)
//! UTXO snapshot to be considered valid.
template <typename HashObj>
static void ApplyHash(HashObj &hash_obj, const TxId &txid, const std::map<uint32_t, Coin> &outputs) {
    for (const auto & [outN, coin] : outputs) {
        COutPoint outpoint = COutPoint(txid, outN);
        ApplyCoinHash(hash_obj, outpoint, coin);
    }
}

static void FinalizeHash(HashWriter &ss, CoinStats &stats) { stats.hashSerialized = ss.GetHash(); }
static void FinalizeHash(std::nullptr_t, CoinStats &) {}
static void FinalizeHash(ECMultiSet &ms, CoinStats &stats) {
    stats.hashSerialized = ms.GetHash();
    stats.multiSet = ms;
}
static void FinalizeHash(MuHash3072 &mh, CoinStats &stats) {
    mh.Finalize(stats.hashSerialized);
    stats.multiSet = mh;
}

template <typename HashObj>
static bool ComputeUTXOStats(CCoinsView &view, CCoinsViewCursor &cursor, CoinStats &stats, HashObj hash_obj,
                             const std::function<void()> &interruption_point) {
    TxId prevkey;
    std::map<uint32_t, Coin> outputs;
    while (cursor.Valid()) {
        if (interruption_point) {
            interruption_point();
        }
        COutPoint key;
        Coin coin;
        if (cursor.GetKey(key) && cursor.GetValue(coin)) {
            if (!outputs.empty() && key.GetTxId() != prevkey) {
                ApplyStats(stats, prevkey, outputs);
                ApplyHash(hash_obj, prevkey, outputs);
                outputs.clear();
            }
            prevkey = key.GetTxId();
            outputs[key.GetN()] = std::move(coin);
        } else {
            return error("%s: unable to read value\n", __func__);
        }
        cursor.Next();
    }
    if (!outputs.empty()) {
        ApplyStats(stats, prevkey, outputs);
        ApplyHash(hash_obj, prevkey, outputs);
    }

    FinalizeHash(hash_obj, stats);

    stats.nDiskSize = view.EstimateSize();

    return true;
}

std::optional<CoinStats> ComputeUTXOStats(CCoinsView *view, const CoinStatsHashType hash_type,
                                          const std::function<void()> &interruption_point) {
    std::optional<CoinStats> ret;

    const bool success = [&]() -> bool {
        assert(view);
        std::unique_ptr<CCoinsViewCursor> pcursor(view->Cursor());
        assert(pcursor);

        const CBlockIndex* pindex = WITH_LOCK(::cs_main, return ::LookupBlockIndex(view->GetBestBlock()));
        assert(pindex);

        CoinStats &stats = ret.emplace(pindex->nHeight, pindex->GetBlockHash());

        switch (hash_type) {
            // legacy serialization, Core compatible (not used by coinstatsindex)
            case CoinStatsHashType::HASH_SERIALIZED_3: {
                return ComputeUTXOStats(*view, *pcursor, stats, HashWriter{}, interruption_point);
            }

            case CoinStatsHashType::MUHASH_TESTING: {
                return ComputeUTXOStats(*view, *pcursor, stats, MuHash3072{}, interruption_point);
            }

            case CoinStatsHashType::ECMH: {
                return ComputeUTXOStats(*view, *pcursor, stats, ECMultiSet{}, interruption_point);
            }

            case CoinStatsHashType::NONE: {
                return ComputeUTXOStats(*view, *pcursor, stats, nullptr, interruption_point);
            }
        } // no default case, so the compiler can warn about missing cases
        assert(false);
    }();

    if (!success) {
        ret.reset();
    }

    return ret;
}
