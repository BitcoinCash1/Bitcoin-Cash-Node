// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <primitives/transaction.h>
#include <script/script_error.h>
#include <script/script_flags.h>
#include <script/script_execution_context.h>
#include <script/script_metrics.h>
#include <script/sighashtype.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

class CPubKey;

using StackT = std::vector<std::vector<uint8_t>>;

/** Precompute sighash midstate to avoid quadratic hashing */
struct PrecomputedTransactionData {
    uint256 hashPrevouts, hashSequence, hashOutputs;
    /// `hashUtxos` will not contain a value if the ScriptExecutionContext passed-into c'tor was a "limited" context
    std::optional<uint256> hashUtxos;
    bool populated = false;

    PrecomputedTransactionData() = default;

    explicit PrecomputedTransactionData(const ScriptExecutionContext &context)
        : hashPrevouts(uint256::Uninitialized), hashSequence(uint256::Uninitialized),
          hashOutputs(uint256::Uninitialized) {
        PopulateFromContext(context);
    }

    void PopulateFromContext(const ScriptExecutionContext &context);
};

/// Exception thrown by SignatureHash below if sigHashType requests SIGHASH_UTXOS and context.isLimited() and/or
/// !cache->hashUtxos.
struct SignatureHashMissingUtxoDataError : std::runtime_error {
    using std::runtime_error::runtime_error;
    virtual ~SignatureHashMissingUtxoDataError() override = default;
};

/// Returns the transaction input hash digest for signature creation and/or verification
/// @throw std::ios_base::failure or SignatureHashMissingUtxoDataError (in tests only)
uint256 SignatureHash(const CScript &scriptCode, const ScriptExecutionContext &context, SigHashType sigHashType,
                      const PrecomputedTransactionData *cache /* null ok */, uint32_t flags);

class BaseSignatureChecker {
public:
    virtual bool VerifySignature(const std::vector<uint8_t> &vchSig, const CPubKey &vchPubKey,
                                 const uint256 &sighash) const;

    virtual bool CheckSig(const std::vector<uint8_t> &vchSigIn, const std::vector<uint8_t> &vchPubKey,
                          const CScript &scriptCode, uint32_t flags) const {
        return false;
    }

    virtual bool CheckLockTime(const CScriptNum &nLockTime) const {
        return false;
    }

    virtual bool CheckSequence(const CScriptNum &nSequence) const {
        return false;
    }

    virtual const ScriptExecutionContext *GetContext() const { return nullptr; }

    virtual ~BaseSignatureChecker() {}
};

struct ContextOptSignatureChecker : BaseSignatureChecker {
    const ScriptExecutionContextOpt contextOpt;

    ContextOptSignatureChecker(const ScriptExecutionContextOpt &contextIn) : contextOpt(contextIn) {}
    ~ContextOptSignatureChecker() override;

    const ScriptExecutionContext *GetContext() const override { return contextOpt ? &*contextOpt : nullptr; }
};



class TransactionSignatureChecker : public BaseSignatureChecker {
    const ScriptExecutionContext &context;
    const PrecomputedTransactionData *txdata = nullptr;

public:
    // Note: Both `contextIn` and `txDataIn` must have a lifetime as long or longer than this instance (we keep
    //       references to them).
    explicit TransactionSignatureChecker(const ScriptExecutionContext &contextIn) : context(contextIn) {}
    TransactionSignatureChecker(const ScriptExecutionContext &contextIn, const PrecomputedTransactionData &txdataIn)
        : context(contextIn), txdata(&txdataIn) {}

    // The overridden functions are now final.
    bool CheckSig(const std::vector<uint8_t> &vchSigIn, const std::vector<uint8_t> &vchPubKey,
                  const CScript &scriptCode, uint32_t flags) const final override;
    bool CheckLockTime(const CScriptNum &nLockTime) const final override;
    bool CheckSequence(const CScriptNum &nSequence) const final override;
    const ScriptExecutionContext *GetContext() const final override { return &context; }
};

bool EvalScript(StackT& stack, const CScript &script,
                uint32_t flags, const BaseSignatureChecker &checker,
                ScriptExecutionMetrics &metrics, ScriptError *error = nullptr);

inline
bool EvalScript(StackT& stack, const CScript &script, uint32_t flags,
                const BaseSignatureChecker &checker, ScriptError *error = nullptr) {
    ScriptExecutionMetrics dummymetrics;
    return EvalScript(stack, script, flags, checker, dummymetrics, error);
}

/**
 * Execute an unlocking and locking script together.
 *
 * Upon success, metrics will hold the accumulated script metrics.
 * (upon failure, the results should not be relied on)
 */
bool VerifyScript(const CScript &scriptSig, const CScript &scriptPubKey, uint32_t flags, const BaseSignatureChecker &checker,
                  ScriptExecutionMetrics &metricsOut, ScriptError *serror = nullptr);

inline
bool VerifyScript(const CScript &scriptSig, const CScript &scriptPubKey, uint32_t flags, const BaseSignatureChecker &checker,
                  ScriptError *serror = nullptr) {
    ScriptExecutionMetrics dummymetrics;
    return VerifyScript(scriptSig, scriptPubKey, flags, checker, dummymetrics, serror);
}

int FindAndDelete(CScript &script, const CScript &b);
