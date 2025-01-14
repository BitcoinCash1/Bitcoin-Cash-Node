// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <coins.h>
#include <primitives/token.h>
#include <primitives/transaction.h>
#include <serialize.h>

#include <cstdint>
#include <limits>
#include <utility>

namespace utxosync {

/// CRTP Base of UTXO and UTXOShallowCRef, encapsulates the special serialization format we use for utxo sets.
template<typename Derived>
struct UTXOSerBase {
    // Serialization of a utxo set entry according to the spec.
    // See: https://github.com/SoftwareVerde/bitcoin-verde/blob/master/specification/utxo-fastsync-chip-20210625.md#utxo-commitment-format
    SERIALIZE_METHODS(UTXOSerBase, obj) {
        // 1. txhash (32 bytes uint256 LE)
        // 2. outputIndex (1-5 bytes uint32 LE, compactsize)
        READWRITE(Using<CompactSizeCOutPointFormatter>(obj.derived().outPoint));
        // 3. height, isCoinbase (4 bytes uint32 LE)
        //    - The least significant bit is set if the UTXO is a coinbase output. The remaining 31 bits represent the
        //      block height (right-shifted by 1 bit, of course).
        // The rest of the fields follow from the a modified CTxOut serialization:
        // 4. value (1-9 bytes int64 LE, compactsize)
        // 5. locking script compactsize (1-3 bytes)
        // 6. locking script bytes (variable, depending on (5) above)
        READWRITE(Using<CompactSizeCoinFormatter>(obj.derived().coin));
    }

protected:

    Derived &derived() { return static_cast<Derived &>(*this); }
    const Derived &derived() const { return static_cast<const Derived &>(*this); }

    /** Formatter for serializing COutPoint that provides a more compact serialization for obj.GetN() (uses CompactSize). */
    struct CompactSizeCOutPointFormatter {
        FORMATTER_METHODS(COutPoint, obj) {
            TxId txid{TxId::Uninitialized};
            uint32_t n;
            SER_WRITE(obj, (txid = obj.GetTxId(), n = obj.GetN()));
            READWRITE(txid, Using<CompactSizeFormatter<false>>(n));
            SER_READ(obj, obj = COutPoint(txid, n));
        }
    };

    /** Formatter for serializing Coin that provides a more compact serialization (uses CompactSize for some parts). */
    struct CompactSizeCoinFormatter {
        FORMATTER_METHODS(Coin, obj) {
            uint32_t heightAndIsCb;
            SER_WRITE(obj, heightAndIsCb = obj.GetHeight() << 0x1u | (obj.IsCoinBase() ? 0x1u : 0x0u));
            READWRITE(heightAndIsCb); // Not a CompactSize since there is little benefit to it in the average case
            if constexpr (ser_action.ForRead()) {
                CTxOut tmp;
                READWRITE(Using<CompactSizeTxOutFormatter>(tmp));
                const uint32_t height = heightAndIsCb >> 0x1u;
                const bool isCb = heightAndIsCb & 0x1u;
                obj = Coin(std::move(tmp), height, isCb);
            } else {
                READWRITE(Using<CompactSizeTxOutFormatter>(obj.GetTxOut()));
            }
        }

        /** Formatter for serializing CTxOut that provides a more compact serialization for nValue (uses CompactSize). */
        struct CompactSizeTxOutFormatter {
            FORMATTER_METHODS(CTxOut, obj) {
                // 1. nValue as a compactsize, guarding against negatives to avoid UB
                uint64_t amt;
                if constexpr (!ser_action.ForRead()) {
                    if (obj.nValue < Amount::zero()) {
                        throw std::ios_base::failure("Attempt to serialize a negative amount; this is unsupported");
                    }
                }
                SER_WRITE(obj, amt = obj.nValue / SATOSHI);
                READWRITE(Using<CompactSizeFormatter<false>>(amt));
                if constexpr (ser_action.ForRead()) {
                    if (amt > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                        throw std::ios_base::failure("Deserialized amount is negative");
                    }
                    obj.nValue = static_cast<int64_t>(amt) * SATOSHI;
                }
                // 2. scriptPubKey and optional tokenDataPtr
                if (!ser_action.ForRead() && !obj.tokenDataPtr) {
                    // Faster path when writing without token data, just do this as it's faster
                    READWRITE(obj.scriptPubKey);
                } else {
                    // Slower path, juggle the optional tokenData and pack/unpack it into the WrappedScriptPubKey
                    token::WrappedScriptPubKey wspk;
                    SER_WRITE(obj, token::WrapScriptPubKey(wspk, obj.tokenDataPtr, obj.scriptPubKey, s.GetVersion()));
                    READWRITE(wspk);
                    SER_READ(obj, token::UnwrapScriptPubKey(wspk, obj.tokenDataPtr, obj.scriptPubKey, s.GetVersion()));
                }
            }
        };
    };
};

/// Encapsulate an individual utxo set entry in the network and disk format, and for inclusion in ECMultiSets
struct UTXO : UTXOSerBase<UTXO> {
    COutPoint outPoint;
    Coin coin;

    UTXO() = default;
    UTXO(const COutPoint &o, Coin &&c) : outPoint{o}, coin{std::move(c)} {}
    UTXO(const COutPoint &o, const Coin &c) : outPoint{o}, coin{c} {}

    // Note: this type is (un)serializable via UTXOSerBase
};

/// For serializing from const refs (to avoid excess value copy)
struct UTXOShallowCRef : UTXOSerBase<UTXOShallowCRef> {
    const COutPoint &outPoint;
    const Coin &coin;

    // NB: lifetime of `o` and `c` must be at least as long as the lifetime of this instance.
    UTXOShallowCRef(const COutPoint &o, const Coin &c) : outPoint(o), coin(c) {}

    // Note: this type is serializable via UTXOSerBase
};

} // namespace utxosync
