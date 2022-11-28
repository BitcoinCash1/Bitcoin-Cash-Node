// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2020-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <attributes.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/script_execution_context.h>
#include <script/sign.h>

#include <optional>

// Magic bytes
static constexpr uint8_t PSBT_MAGIC_BYTES[5] = {'p', 's', 'b', 't', 0xff};

// Global types
static constexpr uint8_t PSBT_GLOBAL_UNSIGNED_TX = 0x00;

// Input types
static constexpr uint8_t PSBT_IN_UTXO = 0x00;
static constexpr uint8_t PSBT_IN_PARTIAL_SIG = 0x02;
static constexpr uint8_t PSBT_IN_SIGHASH = 0x03;
static constexpr uint8_t PSBT_IN_REDEEMSCRIPT = 0x04;
static constexpr uint8_t PSBT_IN_BIP32_DERIVATION = 0x06;
static constexpr uint8_t PSBT_IN_SCRIPTSIG = 0x07;

// Output types
static constexpr uint8_t PSBT_OUT_REDEEMSCRIPT = 0x00;
static constexpr uint8_t PSBT_OUT_BIP32_DERIVATION = 0x02;

// The separator is 0x00. Reading this in means that the unserializer can
// interpret it as a 0 length key which indicates that this is the separator.
// The separator has no value.
static constexpr uint8_t PSBT_SEPARATOR = 0x00;

/** A structure for PSBTs which contain per-input information */
struct PSBTInput {
    CTxOut utxo;
    CScript redeem_script;
    CScript final_script_sig;
    std::map<CPubKey, KeyOriginInfo> hd_keypaths;
    std::map<CKeyID, SigPair> partial_sigs;
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> unknown;
    SigHashType sighash_type = SigHashType(0);

    bool IsNull() const;
    void FillSignatureData(SignatureData &sigdata) const;
    void FromSignatureData(const SignatureData &sigdata);
    void Merge(const PSBTInput &input);
    bool IsSane() const;
    PSBTInput() {}

    template <typename Stream> inline void Serialize(Stream &s) const {
        // Write the utxo
        if (!utxo.IsNull()) {
            SerializeToVector(s, PSBT_IN_UTXO);
            SerializeToVector(s, utxo);
        }

        if (final_script_sig.empty()) {
            // Write any partial signatures
            for (auto sig_pair : partial_sigs) {
                SerializeToVector(s, PSBT_IN_PARTIAL_SIG,
                                  Span{sig_pair.second.first});
                s << sig_pair.second.second;
            }

            // Write the sighash type
            if (sighash_type.getRawSigHashType() != 0) {
                SerializeToVector(s, PSBT_IN_SIGHASH);
                SerializeToVector(s, sighash_type);
            }

            // Write the redeem script
            if (!redeem_script.empty()) {
                SerializeToVector(s, PSBT_IN_REDEEMSCRIPT);
                s << redeem_script;
            }

            // Write any hd keypaths
            SerializeHDKeypaths(s, hd_keypaths, PSBT_IN_BIP32_DERIVATION);
        }

        // Write script sig
        if (!final_script_sig.empty()) {
            SerializeToVector(s, PSBT_IN_SCRIPTSIG);
            s << final_script_sig;
        }

        // Write unknown things
        for (auto &entry : unknown) {
            s << entry.first;
            s << entry.second;
        }

        s << PSBT_SEPARATOR;
    }

    template <typename Stream> inline void Unserialize(Stream &s) {
        // Read loop
        while (!s.empty()) {
            // Read
            std::vector<uint8_t> key;
            s >> key;

            // the key is empty if that was actually a separator byte
            // This is a special case for key lengths 0 as those are not allowed
            // (except for separator)
            if (key.empty()) {
                return;
            }

            // First byte of key is the type
            uint8_t type = key[0];

            // Do stuff based on type
            switch (type) {
                case PSBT_IN_UTXO:
                    if (!utxo.IsNull()) {
                        throw std::ios_base::failure(
                            "Duplicate Key, input utxo already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure(
                            "utxo key is more than one byte type");
                    }
                    UnserializeFromVector(s, utxo);
                    break;
                case PSBT_IN_PARTIAL_SIG: {
                    // Make sure that the key is the size of pubkey + 1
                    if (key.size() != CPubKey::PUBLIC_KEY_SIZE + 1 &&
                        key.size() != CPubKey::COMPRESSED_PUBLIC_KEY_SIZE + 1) {
                        throw std::ios_base::failure(
                            "Size of key was not the expected size for the "
                            "type partial signature pubkey");
                    }
                    // Read in the pubkey from key
                    CPubKey pubkey(key.begin() + 1, key.end());
                    if (!pubkey.IsFullyValid()) {
                        throw std::ios_base::failure("Invalid pubkey");
                    }
                    if (partial_sigs.count(pubkey.GetID()) > 0) {
                        throw std::ios_base::failure(
                            "Duplicate Key, input partial signature for pubkey "
                            "already provided");
                    }

                    // Read in the signature from value
                    std::vector<uint8_t> sig;
                    s >> sig;

                    // Add to list
                    partial_sigs.emplace(pubkey.GetID(),
                                         SigPair(pubkey, std::move(sig)));
                    break;
                }
                case PSBT_IN_SIGHASH:
                    if (sighash_type.getRawSigHashType() != 0) {
                        throw std::ios_base::failure(
                            "Duplicate Key, input sighash type already "
                            "provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure(
                            "Sighash type key is more than one byte type");
                    }
                    UnserializeFromVector(s, sighash_type);
                    break;
                case PSBT_IN_REDEEMSCRIPT: {
                    if (!redeem_script.empty()) {
                        throw std::ios_base::failure(
                            "Duplicate Key, input redeemScript already "
                            "provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure(
                            "Input redeemScript key is more than one byte "
                            "type");
                    }
                    s >> redeem_script;
                    break;
                }
                case PSBT_IN_BIP32_DERIVATION: {
                    DeserializeHDKeypaths(s, key, hd_keypaths);
                    break;
                }
                case PSBT_IN_SCRIPTSIG: {
                    if (!final_script_sig.empty()) {
                        throw std::ios_base::failure(
                            "Duplicate Key, input final scriptSig already "
                            "provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure(
                            "Final scriptSig key is more than one byte type");
                    }
                    s >> final_script_sig;
                    break;
                }
                // Unknown stuff
                default:
                    if (unknown.count(key) > 0) {
                        throw std::ios_base::failure(
                            "Duplicate Key, key for unknown value already "
                            "provided");
                    }
                    // Read in the value
                    std::vector<uint8_t> val_bytes;
                    s >> val_bytes;
                    unknown.emplace(std::move(key), std::move(val_bytes));
                    break;
            }
        }
    }

    template <typename Stream> PSBTInput(deserialize_type, Stream &s) {
        Unserialize(s);
    }
};

/** A structure for PSBTs which contains per output information */
struct PSBTOutput {
    CScript redeem_script;
    std::map<CPubKey, KeyOriginInfo> hd_keypaths;
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> unknown;

    bool IsNull() const;
    void FillSignatureData(SignatureData &sigdata) const;
    void FromSignatureData(const SignatureData &sigdata);
    void Merge(const PSBTOutput &output);
    bool IsSane() const;
    PSBTOutput() {}

    template <typename Stream> inline void Serialize(Stream &s) const {
        // Write the redeem script
        if (!redeem_script.empty()) {
            SerializeToVector(s, PSBT_OUT_REDEEMSCRIPT);
            s << redeem_script;
        }

        // Write any hd keypaths
        SerializeHDKeypaths(s, hd_keypaths, PSBT_OUT_BIP32_DERIVATION);

        // Write unknown things
        for (auto &entry : unknown) {
            s << entry.first;
            s << entry.second;
        }

        s << PSBT_SEPARATOR;
    }

    template <typename Stream> inline void Unserialize(Stream &s) {
        // Read loop
        while (!s.empty()) {
            // Read
            std::vector<uint8_t> key;
            s >> key;

            // the key is empty if that was actually a separator byte
            // This is a special case for key lengths 0 as those are not allowed
            // (except for separator)
            if (key.empty()) {
                return;
            }

            // First byte of key is the type
            uint8_t type = key[0];

            // Do stuff based on type
            switch (type) {
                case PSBT_OUT_REDEEMSCRIPT: {
                    if (!redeem_script.empty()) {
                        throw std::ios_base::failure(
                            "Duplicate Key, output redeemScript already "
                            "provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure(
                            "Output redeemScript key is more than one byte "
                            "type");
                    }
                    s >> redeem_script;
                    break;
                }
                case PSBT_OUT_BIP32_DERIVATION: {
                    DeserializeHDKeypaths(s, key, hd_keypaths);
                    break;
                }
                // Unknown stuff
                default: {
                    if (unknown.count(key) > 0) {
                        throw std::ios_base::failure(
                            "Duplicate Key, key for unknown value already "
                            "provided");
                    }
                    // Read in the value
                    std::vector<uint8_t> val_bytes;
                    s >> val_bytes;
                    unknown.emplace(std::move(key), std::move(val_bytes));
                    break;
                }
            }
        }
    }

    template <typename Stream> PSBTOutput(deserialize_type, Stream &s) {
        Unserialize(s);
    }
};

/**
 * A version of CTransaction with the PSBT format.
 */
struct PartiallySignedTransaction {
    std::optional<CMutableTransaction> tx;
    std::vector<PSBTInput> inputs;
    std::vector<PSBTOutput> outputs;
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> unknown;

    bool IsNull() const;

    /**
     * Merge psbt into this. The two psbts must have the same underlying
     * CTransaction (i.e. the same actual Bitcoin transaction.) Returns true if
     * the merge succeeded, false otherwise.
     */
    [[nodiscard]] bool Merge(const PartiallySignedTransaction &psbt);
    bool IsSane() const;
    PartiallySignedTransaction() {}
    PartiallySignedTransaction(const PartiallySignedTransaction &psbt_in)
        : tx(psbt_in.tx), inputs(psbt_in.inputs), outputs(psbt_in.outputs),
          unknown(psbt_in.unknown) {}
    explicit PartiallySignedTransaction(const CTransaction &txIn);

    template <typename Stream> inline void Serialize(Stream &s) const {
        // magic bytes
        s << PSBT_MAGIC_BYTES;

        // unsigned tx flag
        SerializeToVector(s, PSBT_GLOBAL_UNSIGNED_TX);

        // Write serialized tx to a stream
        SerializeToVector(s, *tx);

        // Write the unknown things
        for (auto &entry : unknown) {
            s << entry.first;
            s << entry.second;
        }

        // Separator
        s << PSBT_SEPARATOR;

        // Write inputs
        for (const PSBTInput &input : inputs) {
            s << input;
        }

        // Write outputs
        for (const PSBTOutput &output : outputs) {
            s << output;
        }
    }

    template <typename Stream> inline void Unserialize(Stream &s) {
        // Read the magic bytes
        uint8_t magic[5];
        s >> magic;
        if (!std::equal(magic, magic + 5, PSBT_MAGIC_BYTES)) {
            throw std::ios_base::failure("Invalid PSBT magic bytes");
        }

        // Read global data
        while (!s.empty()) {
            // Read
            std::vector<uint8_t> key;
            s >> key;

            // the key is empty if that was actually a separator byte
            // This is a special case for key lengths 0 as those are not allowed
            // (except for separator)
            if (key.empty()) {
                break;
            }

            // First byte of key is the type
            uint8_t type = key[0];

            // Do stuff based on type
            switch (type) {
                case PSBT_GLOBAL_UNSIGNED_TX: {
                    if (tx) {
                        throw std::ios_base::failure(
                            "Duplicate Key, unsigned tx already provided");
                    } else if (key.size() != 1) {
                        throw std::ios_base::failure(
                            "Global unsigned tx key is more than one byte "
                            "type");
                    }
                    CMutableTransaction mtx;
                    UnserializeFromVector(s, mtx);
                    tx = std::move(mtx);
                    // Make sure that all scriptSigs are empty.
                    for (const CTxIn &txin : tx->vin) {
                        if (!txin.scriptSig.empty()) {
                            throw std::ios_base::failure(
                                "Unsigned tx does not have empty scriptSigs.");
                        }
                    }
                    break;
                }
                // Unknown stuff
                default: {
                    if (unknown.count(key) > 0) {
                        throw std::ios_base::failure(
                            "Duplicate Key, key for unknown value already "
                            "provided");
                    }
                    // Read in the value
                    std::vector<uint8_t> val_bytes;
                    s >> val_bytes;
                    unknown.emplace(std::move(key), std::move(val_bytes));
                }
            }
        }

        // Make sure that we got an unsigned tx
        if (!tx) {
            throw std::ios_base::failure(
                "No unsigned transcation was provided");
        }

        // Read input data
        size_t i = 0;
        while (!s.empty() && i < tx->vin.size()) {
            PSBTInput input;
            s >> input;
            inputs.push_back(input);
            ++i;
        }
        // Make sure that the number of inputs matches the number of inputs in
        // the transaction
        if (inputs.size() != tx->vin.size()) {
            throw std::ios_base::failure("Inputs provided does not match the "
                                         "number of inputs in transaction.");
        }

        // Read output data
        i = 0;
        while (!s.empty() && i < tx->vout.size()) {
            PSBTOutput output;
            s >> output;
            outputs.push_back(output);
            ++i;
        }
        // Make sure that the number of outputs matches the number of outputs in
        // the transaction
        if (outputs.size() != tx->vout.size()) {
            throw std::ios_base::failure("Outputs provided does not match the "
                                         "number of outputs in transaction.");
        }
        // Sanity check
        if (!IsSane()) {
            throw std::ios_base::failure("PSBT is not sane.");
        }
    }

    template <typename Stream>
    PartiallySignedTransaction(deserialize_type, Stream &s) {
        Unserialize(s);
    }
};

/** Checks whether a PSBTInput is already signed. */
bool PSBTInputSigned(PSBTInput &input);

/**
 * Signs a PSBTInput, verifying that all provided data matches what is being
 * signed.
 */
bool SignPSBTInput(const SigningProvider &provider,
                   PartiallySignedTransaction &psbt, int index,
                   uint32_t scriptFlags,
                   SigHashType sighash = SigHashType(),
                   const ScriptExecutionContextOpt &context = {});
