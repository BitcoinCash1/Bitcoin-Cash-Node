// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2020-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/standard.h>

#include <crypto/sha256.h>
#include <pubkey.h>
#include <script/script.h>
#include <util/strencodings.h>
#include <util/system.h>

using valtype = std::vector<uint8_t>;

uint32_t nMaxDatacarrierBytes = MAX_OP_RETURN_RELAY;

ScriptID::ScriptID(const CScript &in, bool is32)
    : var(is32 ? var_t{Hash(in)} : var_t{Hash160(in)}) {}

const char *GetTxnOutputType(txnouttype t) {
    switch (t) {
        case TX_NONSTANDARD:
            return "nonstandard";
        case TX_PUBKEY:
            return "pubkey";
        case TX_PUBKEYHASH:
            return "pubkeyhash";
        case TX_SCRIPTHASH:
            return "scripthash";
        case TX_MULTISIG:
            return "multisig";
        case TX_NULL_DATA:
            return "nulldata";
    }
    return nullptr;
}

static bool MatchPayToPubkey(const CScript &script, valtype &pubkey) {
    if (script.size() == CPubKey::PUBLIC_KEY_SIZE + 2 &&
        script[0] == CPubKey::PUBLIC_KEY_SIZE && script.back() == OP_CHECKSIG) {
        pubkey = valtype(script.begin() + 1,
                         script.begin() + CPubKey::PUBLIC_KEY_SIZE + 1);
        return CPubKey::ValidSize(pubkey);
    }
    if (script.size() == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE + 2 &&
        script[0] == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE &&
        script.back() == OP_CHECKSIG) {
        pubkey =
            valtype(script.begin() + 1,
                    script.begin() + CPubKey::COMPRESSED_PUBLIC_KEY_SIZE + 1);
        return CPubKey::ValidSize(pubkey);
    }
    return false;
}

static bool MatchPayToPubkeyHash(const CScript &script, valtype &pubkeyhash) {
    if (script.size() == 25 && script[0] == OP_DUP && script[1] == OP_HASH160 &&
        script[2] == 20 && script[23] == OP_EQUALVERIFY &&
        script[24] == OP_CHECKSIG) {
        pubkeyhash = valtype(script.begin() + 3, script.begin() + 23);
        return true;
    }
    return false;
}

/** Test for "small positive integer" script opcodes - OP_1 through OP_16. */
static constexpr bool IsSmallInteger(opcodetype opcode) {
    return opcode >= OP_1 && opcode <= OP_16;
}

static bool MatchMultisig(const CScript &script, unsigned int &required,
                          std::vector<valtype> &pubkeys) {
    opcodetype opcode;
    valtype data;
    CScript::const_iterator it = script.begin();
    if (script.size() < 1 || script.back() != OP_CHECKMULTISIG) {
        return false;
    }

    if (!script.GetOp(it, opcode, data) || !IsSmallInteger(opcode)) {
        return false;
    }
    required = CScript::DecodeOP_N(opcode);
    while (script.GetOp(it, opcode, data) && CPubKey::ValidSize(data)) {
        if (opcode < 0 || opcode > OP_PUSHDATA4 ||
            !CheckMinimalPush(data, opcode)) {
            return false;
        }
        pubkeys.emplace_back(std::move(data));
    }
    if (!IsSmallInteger(opcode)) {
        return false;
    }
    unsigned int keys = CScript::DecodeOP_N(opcode);
    if (pubkeys.size() != keys || keys < required) {
        return false;
    }
    return (it + 1 == script.end());
}

txnouttype Solver(const CScript &scriptPubKey, std::vector<std::vector<uint8_t>> &vSolutionsRet, uint32_t flags) {
    vSolutionsRet.clear();

    // Shortcut for pay-to-script-hash, which are more constrained than the
    // other types:
    // Before p2sh_32 activation in `flags`, it is always:    OP_HASH160 20 [20 byte hash] OP_EQUAL
    // *After* p2sh_32 activation in `flags`, it may also be: OP_HASH256 32 [32 byte hash] OP_EQUAL
    if (valtype hashBytes; scriptPubKey.IsPayToScriptHash(flags, &hashBytes)) {
        vSolutionsRet.push_back(std::move(hashBytes));
        return TX_SCRIPTHASH;
    }

    // Provably prunable, data-carrying output
    //
    // So long as script passes the IsUnspendable() test and all but the first
    // byte passes the IsPushOnly() test we don't care what exactly is in the
    // script.
    if (scriptPubKey.size() >= 1 && scriptPubKey[0] == OP_RETURN &&
        scriptPubKey.IsPushOnly(scriptPubKey.begin() + 1)) {
        return TX_NULL_DATA;
    }

    std::vector<uint8_t> data;
    if (MatchPayToPubkey(scriptPubKey, data)) {
        vSolutionsRet.push_back(std::move(data));
        return TX_PUBKEY;
    }

    if (MatchPayToPubkeyHash(scriptPubKey, data)) {
        vSolutionsRet.push_back(std::move(data));
        return TX_PUBKEYHASH;
    }

    unsigned int required;
    std::vector<std::vector<uint8_t>> keys;
    if (MatchMultisig(scriptPubKey, required, keys)) {
        // safe as required is in range 1..16
        vSolutionsRet.push_back({static_cast<uint8_t>(required)});
        vSolutionsRet.insert(vSolutionsRet.end(), keys.begin(), keys.end());
        // safe as size is in range 1..16
        vSolutionsRet.push_back({static_cast<uint8_t>(keys.size())});
        return TX_MULTISIG;
    }

    vSolutionsRet.clear();
    return TX_NONSTANDARD;
}

bool ExtractDestination(const CScript &scriptPubKey, CTxDestination &addressRet, uint32_t flags) {
    std::vector<valtype> vSolutions;
    txnouttype whichType = Solver(scriptPubKey, vSolutions, flags);

    if (whichType == TX_PUBKEY) {
        CPubKey pubKey(vSolutions[0]);
        if (!pubKey.IsValid()) {
            return false;
        }

        addressRet = pubKey.GetID();
        return true;
    }
    if (whichType == TX_PUBKEYHASH) {
        addressRet = CKeyID(uint160(vSolutions[0]));
        return true;
    }
    if (whichType == TX_SCRIPTHASH) {
        const auto sol_size = vSolutions[0].size();
        if (sol_size == uint160::size()) {
            // legacy p2sh
            addressRet = ScriptID(uint160(vSolutions[0]));
        } else if (sol_size == uint256::size()) {
            // newer p2sh_32
            addressRet = ScriptID(uint256(vSolutions[0]));
        } else {
            assert(!"Expected solution size to either be 20 or 32 bytes!");
            return false; // not reached
        }
        return true;
    }
    // Multisig txns have more than one address...
    return false;
}

bool ExtractDestinations(const CScript &scriptPubKey, txnouttype &typeRet,
                         std::vector<CTxDestination> &addressRet,
                         int &nRequiredRet, uint32_t flags) {
    addressRet.clear();
    std::vector<valtype> vSolutions;
    typeRet = Solver(scriptPubKey, vSolutions, flags);
    if (typeRet == TX_NONSTANDARD) {
        return false;
    } else if (typeRet == TX_NULL_DATA) {
        // This is data, not addresses
        return false;
    }

    if (typeRet == TX_MULTISIG) {
        nRequiredRet = vSolutions.front()[0];
        for (size_t i = 1; i < vSolutions.size() - 1; i++) {
            CPubKey pubKey(vSolutions[i]);
            if (!pubKey.IsValid()) {
                continue;
            }

            CTxDestination address = pubKey.GetID();
            addressRet.push_back(address);
        }

        if (addressRet.empty()) {
            return false;
        }
    } else {
        nRequiredRet = 1;
        CTxDestination address;
        if (!ExtractDestination(scriptPubKey, address, flags)) {
            return false;
        }
        addressRet.push_back(address);
    }

    return true;
}

namespace {
class CScriptVisitor : public boost::static_visitor<bool> {
private:
    CScript *script;

public:
    explicit CScriptVisitor(CScript *scriptin) { script = scriptin; }

    bool operator()(const CNoDestination &) const {
        script->clear();
        return false;
    }

    bool operator()(const CKeyID &keyID) const {
        script->clear();
        *script << OP_DUP << OP_HASH160 << ToByteVector(keyID) << OP_EQUALVERIFY
                << OP_CHECKSIG;
        return true;
    }

    bool operator()(const ScriptID &scriptID) const {
        script->clear();
        if (scriptID.IsP2SH_20()) {
            *script << OP_HASH160 << ToByteVector(scriptID) << OP_EQUAL;
        } else if (scriptID.IsP2SH_32()) {
            *script << OP_HASH256 << ToByteVector(scriptID) << OP_EQUAL;
        } else {
            assert(!"Unexpected state in class ScriptID");
            return false; // not reached
        }
        return true;
    }
};
} // namespace

CScript GetScriptForDestination(const CTxDestination &dest) {
    CScript script;

    boost::apply_visitor(CScriptVisitor(&script), dest);
    return script;
}

CScript GetScriptForRawPubKey(const CPubKey &pubKey) {
    return CScript() << std::vector<uint8_t>(pubKey.begin(), pubKey.end())
                     << OP_CHECKSIG;
}

CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey> &keys) {
    CScript script;

    script << CScript::EncodeOP_N(nRequired);
    for (const CPubKey &key : keys) {
        script << ToByteVector(key);
    }
    script << CScript::EncodeOP_N(keys.size()) << OP_CHECKMULTISIG;
    return script;
}

bool IsValidDestination(const CTxDestination &dest) {
    return dest.which() != 0;
}
