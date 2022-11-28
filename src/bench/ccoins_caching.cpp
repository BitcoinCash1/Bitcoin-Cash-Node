// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <coins.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <policy/policy.h>
#include <wallet/crypter.h>

#include <vector>

// FIXME: Dedup with SetupDummyInputs in test/transaction_tests.cpp.
//
// Helper: create two dummy transactions, each with
// two outputs.  The first has 11 and 50 COIN outputs
// paid to a TX_PUBKEY, the second 21 and 22 COIN outputs
// paid to a TX_PUBKEYHASH.
//
static std::vector<CMutableTransaction>
SetupDummyInputs(CBasicKeyStore &keystoreRet, CCoinsViewCache &coinsRet) {
    std::vector<CMutableTransaction> dummyTransactions;
    dummyTransactions.resize(2);

    // Add some keys to the keystore:
    CKey key[4];
    for (int i = 0; i < 4; i++) {
        key[i].MakeNewKey(i % 2);
        keystoreRet.AddKey(key[i]);
    }

    // Create some dummy input transactions
    dummyTransactions[0].vout.resize(2);
    dummyTransactions[0].vout[0].nValue = 11 * COIN;
    dummyTransactions[0].vout[0].scriptPubKey
        << ToByteVector(key[0].GetPubKey()) << OP_CHECKSIG;
    dummyTransactions[0].vout[1].nValue = 50 * COIN;
    dummyTransactions[0].vout[1].scriptPubKey
        << ToByteVector(key[1].GetPubKey()) << OP_CHECKSIG;
    AddCoins(coinsRet, CTransaction(dummyTransactions[0]), 0);

    dummyTransactions[1].vout.resize(2);
    dummyTransactions[1].vout[0].nValue = 21 * COIN;
    dummyTransactions[1].vout[0].scriptPubKey =
        GetScriptForDestination(key[2].GetPubKey().GetID());
    dummyTransactions[1].vout[1].nValue = 22 * COIN;
    dummyTransactions[1].vout[1].scriptPubKey =
        GetScriptForDestination(key[3].GetPubKey().GetID());
    AddCoins(coinsRet, CTransaction(dummyTransactions[1]), 0);

    return dummyTransactions;
}

// Microbenchmark for simple accesses to a CCoinsViewCache database. Note from
// laanwj, "replicating the actual usage patterns of the client is hard though,
// many times micro-benchmarks of the database showed completely different
// characteristics than e.g. reindex timings. But that's not a requirement of
// every benchmark."
// (https://github.com/bitcoin/bitcoin/issues/7883#issuecomment-224807484)
static void CCoinsCaching(benchmark::State &state) {
    CBasicKeyStore keystore;
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    std::vector<CMutableTransaction> dummyTransactions =
        SetupDummyInputs(keystore, coins);

    CMutableTransaction t1;
    t1.vin.resize(3);
    t1.vin[0].prevout = COutPoint(dummyTransactions[0].GetId(), 1);
    t1.vin[0].scriptSig << std::vector<uint8_t>(65, 0);
    t1.vin[1].prevout = COutPoint(dummyTransactions[1].GetId(), 0);
    t1.vin[1].scriptSig << std::vector<uint8_t>(65, 0)
                        << std::vector<uint8_t>(33, 4);
    t1.vin[2].prevout = COutPoint(dummyTransactions[1].GetId(), 1);
    t1.vin[2].scriptSig << std::vector<uint8_t>(65, 0)
                        << std::vector<uint8_t>(33, 4);
    t1.vout.resize(2);
    t1.vout[0].nValue = 90 * COIN;
    t1.vout[0].scriptPubKey << OP_1;

    // Benchmark.
    BENCHMARK_LOOP {
        CTransaction t(t1);
        bool success =
            AreInputsStandard(t, coins, STANDARD_SCRIPT_VERIFY_FLAGS);
        assert(success);
        Amount value = coins.GetValueIn(t);
        assert(value == (50 + 21 + 22) * COIN);
    }
}

static void CheckTxInputs(benchmark::State &state) {
    CBasicKeyStore keystore;
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    constexpr size_t nTx = 3072;
    std::vector<CTransactionRef> transactions;
    transactions.reserve(nTx);

    for (size_t i = 0; i < nTx; ++i) {
        const auto dummyTransactions = SetupDummyInputs(keystore, coins);
        CMutableTransaction tx;
        tx.vin.resize(3);
        tx.vin[0].prevout = COutPoint(dummyTransactions[0].GetId(), 1);
        tx.vin[0].scriptSig << std::vector<uint8_t>(65, 0);
        tx.vin[1].prevout = COutPoint(dummyTransactions[1].GetId(), 0);
        tx.vin[1].scriptSig << std::vector<uint8_t>(65, 0)
                            << std::vector<uint8_t>(33, 4);
        tx.vin[2].prevout = COutPoint(dummyTransactions[1].GetId(), 1);
        tx.vin[2].scriptSig << std::vector<uint8_t>(65, 0)
                            << std::vector<uint8_t>(33, 4);
        tx.vout.resize(1);
        tx.vout[0].nValue = 20 * COIN;
        tx.vout[0].scriptPubKey << OP_1;
        transactions.push_back(MakeTransactionRef(tx));
    }


    // Benchmark.
    BENCHMARK_LOOP {
        for (const CTransactionRef &tx : transactions) {
            CValidationState valstate;
            Amount txfee;
            const bool success = Consensus::CheckTxInputs(*tx, valstate, coins, 0, txfee);
            assert(success);
        }
    }
}

BENCHMARK(CCoinsCaching, 170 * 1000);
BENCHMARK(CheckTxInputs, 1000);
