// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <amount.h>
#include <feerate.h>
#include <primitives/token.h> // Token & NFT support
#include <primitives/txid.h>
#include <script/script.h>
#include <serialize.h>

#include <algorithm>
#include <utility> // for std::move

static const int SERIALIZE_TRANSACTION = 0x00;

/**
 * An outpoint - a combination of a transaction hash and an index n into its
 * vout.
 */
class COutPoint {
private:
    TxId txid;
    uint32_t n;

public:
    static constexpr uint32_t NULL_INDEX = std::numeric_limits<uint32_t>::max();

    COutPoint() : txid(), n(NULL_INDEX) {}
    COutPoint(TxId txidIn, uint32_t nIn) : txid(txidIn), n(nIn) {}

    SERIALIZE_METHODS(COutPoint, obj) { READWRITE(obj.txid, obj.n); }

    bool IsNull() const { return txid.IsNull() && n == NULL_INDEX; }

    const TxId &GetTxId() const { return txid; }
    uint32_t GetN() const { return n; }

    friend bool operator<(const COutPoint &a, const COutPoint &b) {
        int cmp = a.txid.Compare(b.txid);
        return cmp < 0 || (cmp == 0 && a.n < b.n);
    }

    friend bool operator==(const COutPoint &a, const COutPoint &b) {
        return (a.txid == b.txid && a.n == b.n);
    }

    friend bool operator!=(const COutPoint &a, const COutPoint &b) {
        return !(a == b);
    }

    std::string ToString(bool fVerbose = false) const;
};

/**
 * An input of a transaction. It contains the location of the previous
 * transaction's output that it claims and a signature that matches the output's
 * public key.
 */
class CTxIn {
public:
    COutPoint prevout;
    CScript scriptSig;
    uint32_t nSequence;

    /**
     * Setting nSequence to this value for every input in a transaction disables
     * nLockTime.
     */
    static const uint32_t SEQUENCE_FINAL = 0xffffffff;

    /* Below flags apply in the context of BIP 68*/
    /**
     * If this flag set, CTxIn::nSequence is NOT interpreted as a relative
     * lock-time.
     */
    static const uint32_t SEQUENCE_LOCKTIME_DISABLE_FLAG = (1U << 31);

    /**
     * If CTxIn::nSequence encodes a relative lock-time and this flag is set,
     * the relative lock-time has units of 512 seconds, otherwise it specifies
     * blocks with a granularity of 1.
     */
    static const uint32_t SEQUENCE_LOCKTIME_TYPE_FLAG = (1 << 22);

    /**
     * If CTxIn::nSequence encodes a relative lock-time, this mask is applied to
     * extract that lock-time from the sequence field.
     */
    static const uint32_t SEQUENCE_LOCKTIME_MASK = 0x0000ffff;

    /**
     * In order to use the same number of bits to encode roughly the same
     * wall-clock duration, and because blocks are naturally limited to occur
     * every 600s on average, the minimum granularity for time-based relative
     * lock-time is fixed at 512 seconds. Converting from CTxIn::nSequence to
     * seconds is performed by multiplying by 512 = 2^9, or equivalently
     * shifting up by 9 bits.
     */
    static const int SEQUENCE_LOCKTIME_GRANULARITY = 9;

    CTxIn() { nSequence = SEQUENCE_FINAL; }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn = CScript(),
                   uint32_t nSequenceIn = SEQUENCE_FINAL)
        : prevout(prevoutIn), scriptSig(scriptSigIn), nSequence(nSequenceIn) {}
    CTxIn(TxId prevTxId, uint32_t nOut, CScript scriptSigIn = CScript(),
          uint32_t nSequenceIn = SEQUENCE_FINAL)
        : CTxIn(COutPoint(prevTxId, nOut), scriptSigIn, nSequenceIn) {}

    SERIALIZE_METHODS(CTxIn, obj) { READWRITE(obj.prevout, obj.scriptSig, obj.nSequence); }

    friend bool operator==(const CTxIn &a, const CTxIn &b) {
        return (a.prevout == b.prevout && a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn &a, const CTxIn &b) { return !(a == b); }

    std::string ToString(bool fVerbose = false) const;
};

/**
 * An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut {
public:
    Amount nValue;
    CScript scriptPubKey;
    token::OutputDataPtr tokenDataPtr; ///< may be null (indicates no token data for this output)

    CTxOut() { SetNull(); }

    CTxOut(Amount nValueIn, const CScript &scriptPubKeyIn, const token::OutputDataPtr &tokenDataIn = {})
        : nValue(nValueIn), scriptPubKey(scriptPubKeyIn), tokenDataPtr(tokenDataIn) {}

    CTxOut(Amount nValueIn, const CScript &scriptPubKeyIn, token::OutputDataPtr &&tokenDataIn)
        : nValue(nValueIn), scriptPubKey(scriptPubKeyIn), tokenDataPtr(std::move(tokenDataIn)) {}

    SERIALIZE_METHODS(CTxOut, obj) {
        READWRITE(obj.nValue);
        if (!ser_action.ForRead() && !obj.tokenDataPtr) {
            // fast-path for writing with no token data, just write out the scriptPubKey directly
            READWRITE(obj.scriptPubKey);
        } else {
            token::WrappedScriptPubKey wspk;
            SER_WRITE(obj, token::WrapScriptPubKey(wspk, obj.tokenDataPtr, obj.scriptPubKey, s.GetVersion()));
            READWRITE(wspk);
            SER_READ(obj, token::UnwrapScriptPubKey(wspk, obj.tokenDataPtr, obj.scriptPubKey, s.GetVersion()));
        }
    }

    void SetNull() {
        nValue = -SATOSHI;
        scriptPubKey.clear();
        tokenDataPtr.reset();
    }

    bool IsNull() const { return nValue == -SATOSHI; }

    bool HasUnparseableTokenData() const {
        return !tokenDataPtr && !scriptPubKey.empty() && scriptPubKey[0] == token::PREFIX_BYTE;
    }

    friend bool operator==(const CTxOut &a, const CTxOut &b) {
        return a.nValue == b.nValue && a.scriptPubKey == b.scriptPubKey && a.tokenDataPtr == b.tokenDataPtr;
    }

    friend bool operator!=(const CTxOut &a, const CTxOut &b) {
        return !(a == b);
    }

    std::string ToString(bool fVerbose = false) const;
};

class CMutableTransaction;

/**
 * Basic transaction serialization format:
 * - int32_t nVersion
 * - std::vector<CTxIn> vin
 * - std::vector<CTxOut> vout
 * - uint32_t nLockTime
 */
template <typename Stream, typename TxType>
inline void UnserializeTransaction(TxType &tx, Stream &s) {
    s >> tx.nVersion;
    tx.vin.clear();
    tx.vout.clear();
    /* Try to read the vin. In case the dummy is there, this will be read as an
     * empty vector. */
    s >> tx.vin;
    /* We read a non-empty vin. Assume a normal vout follows. */
    s >> tx.vout;
    s >> tx.nLockTime;
}

template <typename Stream, typename TxType>
inline void SerializeTransaction(const TxType &tx, Stream &s) {
    s << tx.nVersion;
    s << tx.vin;
    s << tx.vout;
    s << tx.nLockTime;
}

class CTransaction;
using CTransactionRef = std::shared_ptr<const CTransaction>;

/**
 * The basic transaction that is broadcasted on the network and contained in
 * blocks. A transaction can contain multiple inputs and outputs.
 */
class CTransaction final {
public:
    // Default transaction version.
    static constexpr int32_t CURRENT_VERSION = 2;

    // Note: These two values are used until Upgrade9 activates (May 2023),
    // after which time they will no longer be relevant since version
    // enforcement will be done by the consensus layer.
    static constexpr int32_t MIN_STANDARD_VERSION = 1, MAX_STANDARD_VERSION = 2;

    // Changing the default transaction version requires a two step process:
    // First adapting relay policy by bumping MAX_CONSENSUS_VERSION, and then
    // later date bumping the default CURRENT_VERSION at which point both
    // CURRENT_VERSION and MAX_CONSENSUS_VERSION will be equal.
    //
    // Note: These values are ignored until Upgrade9 (May 2023) is activated,
    // after which time versions outside the range [MIN_CONSENSUS_VERSION,
    // MAX_CONSENSUS_VERSION] are rejected by consensus.
    static constexpr int32_t MIN_CONSENSUS_VERSION = 1, MAX_CONSENSUS_VERSION = 2;

    // The local variables are made const to prevent unintended modification
    // without updating the cached hash value. However, CTransaction is not
    // actually immutable; deserialization and assignment are implemented,
    // and bypass the constness. This is safe, as they update the entire
    // structure, including the hash.
    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;
    const int32_t nVersion;
    const uint32_t nLockTime;

private:
    /** Memory only. */
    const uint256 hash;

    uint256 ComputeHash() const;

    /** Construct a CTransaction that qualifies as IsNull() */
    CTransaction();

public:
    /** Default-constructed CTransaction that qualifies as IsNull() */
    static const CTransaction null;
    //! Points to null (with a no-op deleter)
    static const CTransactionRef sharedNull;

    /** Convert a CMutableTransaction into a CTransaction. */
    explicit CTransaction(const CMutableTransaction &tx);
    explicit CTransaction(CMutableTransaction &&tx);

    /**
     * We prevent copy assignment & construction to enforce use of
     * CTransactionRef, as well as prevent new code from inadvertently copying
     * around these potentially very heavy objects.
     */
    CTransaction(const CTransaction &) = delete;
    CTransaction &operator=(const CTransaction &) = delete;

    template <typename Stream> inline void Serialize(Stream &s) const {
        SerializeTransaction(*this, s);
    }

    /**
     * This deserializing constructor is provided instead of an Unserialize
     * method. Unserialize is not possible, since it would require overwriting
     * const fields.
     */
    template <typename Stream>
    CTransaction(deserialize_type, Stream &s)
        : CTransaction(CMutableTransaction(deserialize, s)) {}

    bool IsNull() const { return vin.empty() && vout.empty(); }

    const TxId GetId() const { return TxId(hash); }
    const TxHash GetHash() const { return TxHash(hash); }

    // Return sum of txouts.
    Amount GetValueOut() const;
    // GetValueIn() is a method on CCoinsViewCache, because
    // inputs must be known to compute value in.

    /**
     * Get the total transaction size in bytes.
     * @return Total transaction size in bytes
     */
    unsigned int GetTotalSize() const;

    bool IsCoinBase() const {
        return (vin.size() == 1 && vin[0].prevout.IsNull());
    }

    /// @return true if this transaction has any vouts with non-null token::OutputData
    bool HasTokenOutputs() const {
        return std::any_of(vout.begin(), vout.end(), [](const CTxOut &out){ return bool(out.tokenDataPtr); });
    }

    /// @return true if any vouts have scriptPubKey[0] == token::PREFIX_BYTE,
    /// and if the vout has tokenDataPtr == nullptr.  This indicates badly
    /// formatted and/or unparseable token data embedded in the scriptPubKey.
    /// Before token activation we allow such scriptPubKeys to appear in
    /// vouts, but after activation of native tokens such txns are rejected by
    /// consensus (see: CheckTxTokens() in consensus/tokens.cpp).
    bool HasOutputsWithUnparseableTokenData() const {
        return std::any_of(vout.begin(), vout.end(), [](const CTxOut &out){ return out.HasUnparseableTokenData(); });
    }

    friend bool operator==(const CTransaction &a, const CTransaction &b) {
        return a.GetHash() == b.GetHash();
    }

    friend bool operator!=(const CTransaction &a, const CTransaction &b) {
        return !(a == b);
    }

    std::string ToString(bool fVerbose = false) const;
};
#if defined(__x86_64__)
static_assert(sizeof(CTransaction) == 88,
              "sizeof CTransaction is expected to be 88 bytes");
#endif

/**
 * A mutable version of CTransaction.
 */
class CMutableTransaction {
public:
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    int32_t nVersion;
    uint32_t nLockTime;

    CMutableTransaction();
    explicit CMutableTransaction(const CTransaction &tx);

    template <typename Stream> inline void Serialize(Stream &s) const {
        SerializeTransaction(*this, s);
    }

    template <typename Stream> inline void Unserialize(Stream &s) {
        UnserializeTransaction(*this, s);
    }

    template <typename Stream>
    CMutableTransaction(deserialize_type, Stream &s) {
        Unserialize(s);
    }

    /**
     * Compute the id and hash of this CMutableTransaction. This is computed on
     * the fly, as opposed to GetId() and GetHash() in CTransaction, which uses
     * a cached result.
     */
    TxId GetId() const;
    TxHash GetHash() const;

    friend bool operator==(const CMutableTransaction &a,
                           const CMutableTransaction &b) {
        return a.GetHash() == b.GetHash();
    }

    /// Mutates this txn. Sorts the inputs according to BIP-69
    void SortInputsBip69();
    /// Mutates this txn. Sorts the outputs according to BIP-69
    void SortOutputsBip69();
    /// Convenience: Calls the above two functions.
    void SortBip69() { SortInputsBip69(); SortOutputsBip69(); }
};
#if defined(__x86_64__)
static_assert(sizeof(CMutableTransaction) == 56,
              "sizeof CMutableTransaction is expected to be 56 bytes");
#endif

static inline CTransactionRef MakeTransactionRef() { return CTransaction::sharedNull; }

template <typename Tx>
static inline CTransactionRef MakeTransactionRef(Tx &&txIn) {
    return std::make_shared<const CTransaction>(std::forward<Tx>(txIn));
}

/// A class that wraps a pointer to either a CTransaction or a
/// CMutableTransaction and presents a uniform view of the minimal
/// intersection of both classes' exposed data.
///
/// This is used by the native introspection code to make it possible for
/// mutable txs as well constant txs to be treated uniformly for the purposes
/// of the native introspection opcodes.
///
/// Contract is: The wrapped tx or mtx pointer must have a lifetime at least
///              as long as an instance of this class.
class CTransactionView {
    const CTransaction *tx{};
    const CMutableTransaction *mtx{};
public:
    CTransactionView(const CTransaction &txIn) noexcept : tx(&txIn) {}
    CTransactionView(const CMutableTransaction &mtxIn) noexcept : mtx(&mtxIn) {}

    bool isMutableTx() const noexcept { return mtx; }

    const std::vector<CTxIn> &vin() const noexcept { return mtx ? mtx->vin : tx->vin; }
    const std::vector<CTxOut> &vout() const noexcept { return mtx ? mtx->vout : tx->vout; }
    const int32_t &nVersion() const noexcept { return mtx ? mtx->nVersion : tx->nVersion; }
    const uint32_t &nLockTime() const noexcept { return mtx ? mtx->nLockTime : tx->nLockTime; }

    TxId GetId() const { return mtx ? mtx->GetId() : tx->GetId(); }
    TxHash GetHash() const { return mtx ? mtx->GetHash() : tx->GetHash(); }

    bool operator==(const CTransactionView &o) const noexcept {
        return isMutableTx() == o.isMutableTx() && (mtx ? *mtx == *o.mtx : *tx == *o.tx);
    }
    bool operator!=(const CTransactionView &o) const noexcept { return !operator==(o); }

    /// Get a pointer to the underlying constant transaction, if such a thing exists.
    /// This is used by the validation engine which is always passed a CTransaction.
    /// Returned pointer will be nullptr if this->isMutableTx()
    const CTransaction *constantTx() const { return tx; }
};
