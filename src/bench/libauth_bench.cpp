// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <chainparams.h>
#include <coins.h>
#include <config.h>
#include <script/interpreter.h>
#include <test/libauth_testing_setup.h>
#include <tinyformat.h>
#include <util/string.h>
#include <validation.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace {

inline constexpr auto testPackName = "2023"; // we run the 2023 pack
inline constexpr size_t NITERS = 1000; // Number of iterations for the "[baseline]" benchmark
                                       // We scale other benchmark iters. up or down according to txSize

class Registerer {
    static void registerBenches();
public:
    Registerer() {
        static std::once_flag flag;
        std::call_once(flag, registerBenches);
    }
};

using TestVector = LibauthTestingSetup::TestVector;
using Test = TestVector::Test;
using TxStandard = LibauthTestingSetup::TxStandard;
using State = benchmark::State;
using Printer = benchmark::Printer;

void RunBench(State &state, const Test &test, TxStandard txStd, bool checkSigs = true);
void BenchCompleted(const State &state, Printer &printer, const Test &test);

/* static */
void Registerer::registerBenches() {
    // The below detects benchmarks from the LibAuth JSON and adds them to the static BenchRunner::benchmarks() map.
    LibauthTestingSetup::LoadAllTestPacks(0);
    auto *pack = LibauthTestingSetup::GetTestPack(testPackName);
    assert(pack != nullptr);
    assert(pack->type != LibauthTestingSetup::TestPack::FEATURE);
    if (!pack->benchmarkVectors.empty()) {
        // count how many benches total
        size_t numBenches = 0;
        for (const size_t idx : pack->benchmarkVectors)
            numBenches += pack->testVectors.at(idx).benchmarks.size();
        assert(numBenches > 0u);
        // add baseline first
        const LibauthTestingSetup::TestVector::Test *alreadyAdded = nullptr;
        size_t addCt = 0;
        auto MkName = [&addCt, numBenches](const std::string &ident) {
            const int padding = std::log10(numBenches) + 1; // dynamically determine how many 0's to pad with
            return strprintf("%0*d_%s", padding, addCt, ident);
        };
        size_t baselineTxSize = 366; // not critical that this matches, since it is set below; but pre-set it anyway.
        if (const auto &baseline = pack->baselineBenchmark) {
            auto &testVec = pack->testVectors.at(baseline->first);
            auto &test = testVec.vec.at(baseline->second);
            assert(test.benchmark && test.baselineBench);
            benchmark::BenchRunner( // implicitly adds to benchmarks map
                "LibAuth_" + MkName(test.ident), // We must ensure this sorts first!
                [test, txStd = testVec.standardness](State &state){ RunBench(state, test, txStd); },
                NITERS,
                [test](const State &s, Printer &p) { BenchCompleted(s, p, test); }
            );
            baselineTxSize = test.txSize;
            alreadyAdded = &test;
            ++addCt;
        }
        // next add everything but baseline
        for (const size_t idx : pack->benchmarkVectors) {
            const auto &testVec = pack->testVectors.at(idx);
            for (const size_t tidx : testVec.benchmarks) {
                const auto &test = testVec.vec.at(tidx);
                if (&test == alreadyAdded) continue;
                assert(test.benchmark);
                static_assert(NITERS >= 10, "Below code assumes NITERS is at least 10");
                benchmark::BenchRunner( // implicitly adds to benchmarks map
                    "LibAuth_" + MkName(test.ident),
                    [test, txStd = testVec.standardness](State &state){ RunBench(state, test, txStd);},
                    // number of iterations is based off the txSize, clamped to the range [10, NITERS]
                    std::clamp<size_t>(NITERS * (baselineTxSize / double(test.txSize)), 10, NITERS),
                    [test](const State &s, Printer &p) { BenchCompleted(s, p, test); }
                );
                ++addCt;
            }
        }
    }
}

// This object causes the above registerBenches() to run once at app startup, which then adds LibAuth benches at
// runtime based on compiled-in JSON from src/test/data/libauth_test_vectors/bch_vmb_tests*.json
const Registerer registerer;

// This gets called once for each benchmark evaluation.
void RunBench(State &state, const Test &test, const TxStandard txStd, const bool checkSigs) {
    // We cache the setup for each evaluation to save cycles in the overall runtime of the benchmark(s)
    struct CacheData {
        CCoinsView coinsDummy;
        CCoinsViewCache coinsCache{&coinsDummy};
        std::vector<ScriptExecutionContext> contexts;
        std::optional<PrecomputedTransactionData> precomputedTxDataOpt; // used only if really checking sigs
        std::vector<std::unique_ptr<BaseSignatureChecker>> txSigCheckers; // either fake or real signature checker, 1 per txin
    };

    using CacheDataOpt = std::optional<CacheData>;

    using MapKey = std::tuple<std::string, bool>;
    static std::map<MapKey, CacheDataOpt> cache;

    const CTransaction &txn = *test.tx;
    auto &cdata = cache[{test.ident, checkSigs}];

    if (!cdata) {
        // Save coin data
        cdata.emplace();
        CCoinsViewCache &coinsCache = cdata->coinsCache;
        for (const auto & [outpt, entry] : test.inputCoins) {
            coinsCache.AddCoin(outpt, entry.coin, false);
        }

        // Set up the signature checkers ahead of time to save cycles during the actual benchmarks
        struct FakeSignaureChecker final : ContextOptSignatureChecker {
            bool VerifySignature(const std::vector<uint8_t> &, const CPubKey &, const uint256 &) const override { return true; }
            bool CheckSig(const std::vector<uint8_t> &, const std::vector<uint8_t> &, const CScript &,
                          uint32_t, size_t *) const override { return true; }
            bool CheckLockTime(const CScriptNum &) const override { return true; }
            bool CheckSequence(const CScriptNum &) const override { return true; }
            using ContextOptSignatureChecker::ContextOptSignatureChecker;
        };
        auto &contexts = cdata->contexts = ScriptExecutionContext::createForAllInputs(txn, coinsCache);
        auto &precomputedTxDataOpt = cdata->precomputedTxDataOpt; // used only if really checking sigs
        auto &txSigCheckers = cdata->txSigCheckers; // either fake or real signature checker, 1 per txin
        if (checkSigs) {
            precomputedTxDataOpt.emplace(contexts.at(0));
            for (const auto &ctx : contexts) {
                txSigCheckers.push_back(std::make_unique<TransactionSignatureChecker>(ctx, *precomputedTxDataOpt));
            }
        } else {
            for (const auto &ctx : contexts) {
                txSigCheckers.push_back(std::make_unique<FakeSignaureChecker>(ctx));
            }
        }
    }
    assert(txn.vin.size() == test.inputCoins.size());
    assert(txn.vin.size() == cdata->txSigCheckers.size());
    assert(txn.vin.size() == cdata->contexts.size());
    const bool expectOk = txStd != TxStandard::INVALID;
    const uint32_t scriptFlags = [&] {
        const CBlockIndex *tip = WITH_LOCK(cs_main, return ::ChainActive().Tip());
        const bool requireStandard = txStd == TxStandard::STANDARD;
        uint32_t blockFlags{};
        const uint32_t standardFlags = GetMemPoolScriptFlags(::GetConfig().GetChainParams().GetConsensus(), tip, &blockFlags);
        return requireStandard ? standardFlags : blockFlags;
    }();

    // Finally, after everything is set up ahead of time, run the benchmark loop
    BENCHMARK_LOOP {
        for (size_t size = txn.vin.size(), inputNum = 0; inputNum < size; ++inputNum) {
            const ScriptExecutionContext &context = cdata->contexts[inputNum];
            const BaseSignatureChecker &checker = *cdata->txSigCheckers[inputNum];
            ScriptError serror{};
            const bool ok = VerifyScript(context.scriptSig(), context.coinScriptPubKey(), scriptFlags, checker, &serror);
            if (ok != expectOk) {
                throw std::runtime_error(
                    strprintf("Wrong result; expected: %i, got: %i. [test: %s, inputNum: %i, checkSigs: %i, std: %i,"
                              " serror: \"%s\"]", int(expectOk), int(ok), test.ident, inputNum, int(checkSigs),
                              int(txStd), ScriptErrorString(serror)));
            }
        }
    }
}

// Completion function called once after each LibAuth bench completes all evaluations; pushes supplemental stats to be
// printed in a table at the end.
void BenchCompleted(const State &state, Printer &printer, const Test &test) {
    struct Cost {
        size_t txSize;
        double perIter;
    };
    static std::optional<Cost> baseline;
    assert(test.benchmark);
    assert(state.m_num_iters); // cannot proceed if no iters!
    assert(state.GetResults().size()); // cannot proceed if no results!
    const Cost cost{test.txSize, state.GetMedian()};
    if (test.baselineBench) baseline = cost; // save baseline (should be first one seen)
    if (!baseline) return; // cannot do anything without the baseline result!
    const double relCost = cost.perIter / baseline->perIter;
    auto MkDesc = [](std::string s) {
        // Keep everything after the first ':' char, trim the resulting string, then wrap the result in quotes.
        std::vector<std::string> parts;
        Split(parts, s, ":", true);
        if (!parts.empty()) {
            parts.erase(parts.begin());
            s = Join(parts, ":");
        }
        return "\"" + TrimString(s) + "\"";
    };
    printer.appendExtraDataForCategory("LibAuth", {{
        {"ID", test.ident},
        {"TxByteLen", strprintf("%i", cost.txSize)},
        {"Hz", strprintf("%1.1f", 1.0 / cost.perIter)},
        {"Cost", strprintf("%1.3f", relCost)},
        {"CostPerByte", strprintf("%1.6f", relCost * baseline->txSize / test.txSize)},
        {"Description", MkDesc(test.description)},
    }});
}

} // namespace
