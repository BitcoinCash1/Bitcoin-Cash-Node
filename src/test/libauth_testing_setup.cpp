// Copyright (c) 2022-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/libauth_testing_setup.h>

#include <chainparams.h>
#include <config.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <fs.h>
#include <policy/policy.h>
#include <script/interpreter.h>
#include <script/script_error.h>
#include <streams.h>
#include <txmempool.h>
#include <util/defer.h>
#include <validation.h>

#include <test/data/libauth_test_vectors.json.h>
#include <test/data/libauth_expected_test_fail_reasons.json.h>
#include <test/jsonutil.h>

#include <boost/test/unit_test.hpp>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>

/* static */
std::map<std::string, LibauthTestingSetup::TestPack> LibauthTestingSetup::allTestPacks;
LibauthTestingSetup::AllReasonsDict LibauthTestingSetup::allLibauthReasons;
LibauthTestingSetup::AllReasonsDict LibauthTestingSetup::bchnProducedReasons;
UniValue::Object LibauthTestingSetup::reasonsLookupTable;

/* static */
void LibauthTestingSetup::LoadAllTestPacks() {
    if (!allTestPacks.empty()) return;

    static_assert(sizeof(json_tests::libauth_test_vectors[0]) == 1 && sizeof(json_tests::libauth_expected_test_fail_reasons[0]) == 1,
                  "Assumption is that the test vectors are byte blobs of json data");

    const auto testPacksUV = read_json({ reinterpret_cast<const char *>(json_tests::libauth_test_vectors.data()),
                                         json_tests::libauth_test_vectors.size() });
    const auto expectedReasons = read_json({ reinterpret_cast<const char *>(json_tests::libauth_expected_test_fail_reasons),
                                             std::size(json_tests::libauth_expected_test_fail_reasons) });

    // Load in the Libauth -> BCHN error message lookup table
    BOOST_CHECK( ! expectedReasons.empty());
    for (const auto &outerWrap : expectedReasons) {
        BOOST_CHECK(outerWrap.isObject());
        if (outerWrap.isObject()) {
            reasonsLookupTable = outerWrap.get_obj();
        }
    }
    // Load the test pack vectors, and Libauth suggested failure reasons
    BOOST_CHECK( ! testPacksUV.empty());
    unsigned coinHeights = []{
        LOCK(cs_main);
        return ::ChainActive().Tip()->nHeight;
    }();
    for (auto &pack : testPacksUV) {
        BOOST_CHECK(pack.isObject());
        if (pack.isObject()) {
            auto &packObj = pack.get_obj();
            auto *nameVal = packObj.locate("name");
            auto *typeVal = packObj.locate("type");
            BOOST_CHECK(nameVal != nullptr);
            if (nameVal) {
                TestPack testPack;
                const auto &packName = testPack.name = nameVal->get_str();
                BOOST_REQUIRE(typeVal != nullptr);
                if (const auto s = typeVal->get_str(); s == "feature") {
                    testPack.type = TestPack::FEATURE;
                } else if (s == "other") {
                    testPack.type = TestPack::OTHER;
                } else {
                    BOOST_ERROR(strprintf("Unknown test pack type: %s", s));
                }
                std::vector<TestVector> &packVec = testPack.testVectors; // append to this member variable of testPack
                for (const auto &uv : packObj.at("tests").get_array()) {
                    BOOST_CHECK(uv.isObject());
                    if (uv.isObject()) {
                        auto &uvObj = uv.get_obj();
                        auto *testNameVal = uvObj.locate("name");
                        BOOST_CHECK(testNameVal != nullptr);
                        if (testNameVal) {
                            std::string testName = testNameVal->get_str();
                            std::string preactivePrefix = "preactivation_";
                            bool featureActive = testName.rfind(preactivePrefix, 0) != 0;
                            std::string standardnessStr = featureActive ? testName
                                                                        : testName.substr(preactivePrefix.size());
                            BOOST_CHECK(standardnessStr == "invalid" ||
                                        standardnessStr == "nonstandard" ||
                                        standardnessStr == "standard" );
                            TxStandard testStandardness = INVALID;
                            if (standardnessStr == "nonstandard") {
                                testStandardness = NONSTANDARD;
                            } else if (standardnessStr == "standard") {
                                testStandardness = STANDARD;
                            }
                            std::string descActiveString = featureActive ? "Post-Activation" : "Pre-Activation";
                            std::string descStdString = "fail validation in both nonstandard and standard mode";
                            if (testStandardness == NONSTANDARD) {
                                descStdString = "fail validation in standard mode but pass validation in nonstandard mode";
                            } else if (testStandardness == STANDARD) {
                                descStdString = "pass validation in both standard and nonstandard mode";
                            }
                            std::string testDescription = descActiveString + ": Test vectors that must " + descStdString;
                            TestVector testVec;
                            testVec.name = testName;
                            testVec.description = testDescription;
                            testVec.featureActive = featureActive;
                            testVec.standardness = testStandardness;

                            const auto &libauthReasons = uvObj.at("reasons");
                            if (libauthReasons.isObject()) { // may be null
                                for (const auto & [ident, obj] : libauthReasons.get_obj()) {
                                    if (obj.isStr()) {
                                        // Invalid tests should produce errors under both standard and nonstandard validation
                                        // Nonstandard tests should produce errors only under standard validation
                                        if (testStandardness == INVALID || testStandardness == NONSTANDARD) {
                                            allLibauthReasons[packName][featureActive][true][ident] = obj.get_str();
                                            if (testStandardness == INVALID) {
                                                allLibauthReasons[packName][featureActive][false][ident] = obj.get_str();
                                            }
                                        }
                                    }
                                }
                            }
                            // Read the "scriptonly" list and put into a set (may be empty)
                            std::set<std::string> scriptOnlyOverrides;
                            for (const auto &identUV : uvObj.at("scriptonly").get_array()) {
                                scriptOnlyOverrides.insert(identUV.get_str());
                            }
                            for (const auto &t : uvObj.at("tests").get_array()) {
                                const UniValue::Array &vec = t.get_array();
                                BOOST_CHECK_GE(vec.size(), 6);
                                TestVector::Test test;
                                test.ident = vec.at(0).get_str();
                                test.description = vec.at(1).get_str();
                                test.stackAsm = vec.at(2).get_str();
                                test.scriptAsm = vec.at(3).get_str();
                                test.scriptOnly = scriptOnlyOverrides.count(test.ident);
                                if (vec.size() >= 7) {
                                    test.inputNum = vec.at(6).get_int();
                                }

                                CMutableTransaction mtx;
                                BOOST_CHECK(DecodeHexTx(mtx, vec.at(4).get_str()));
                                test.tx = MakeTransactionRef(std::move(mtx));
                                BOOST_REQUIRE(test.inputNum < test.tx->vin.size());
                                const auto serinputs = ParseHex(vec.at(5).get_str());
                                std::vector<CTxOut> utxos;
                                {
                                    VectorReader vr(SER_NETWORK, INIT_PROTO_VERSION, serinputs, 0);
                                    vr >> utxos;
                                    BOOST_CHECK(vr.empty());
                                }
                                BOOST_CHECK_EQUAL(utxos.size(), test.tx->vin.size());
                                std::string skipReason;
                                for (size_t i = 0; i < utxos.size(); ++i) {
                                    auto [it, inserted] = test.inputCoins.emplace(std::piecewise_construct,
                                                                                  std::forward_as_tuple(test.tx->vin[i].prevout),
                                                                                  std::forward_as_tuple(Coin(utxos[i],
                                                                                                             coinHeights, false)));
                                    it->second.flags = CCoinsCacheEntry::FRESH;
                                    if (!inserted) {
                                        skipReason += strprintf("\n- Skipping bad tx due to dupe input Input[%i]: %s, Coin1: %s,"
                                                                " Coin2: %s\n%s",
                                                                i, it->first.ToString(true),
                                                                it->second.coin.GetTxOut().ToString(true),
                                                                utxos[i].ToString(true), test.tx->ToString(true));
                                    }
                                    BOOST_CHECK(!it->second.coin.IsSpent());
                                }
                                test.txSize = ::GetSerializeSize(*test.tx);
                                if ( ! skipReason.empty()) {
                                    BOOST_WARN_MESSAGE(false, strprintf("Skipping test \"%s\": %s", test.ident, skipReason));
                                } else {
                                    testVec.vec.push_back(std::move(test));
                                }
                            }
                            packVec.push_back(std::move(testVec));
                        }
                    }
                }
                // Assign Libauth's suggested failure reasons and BCHN expected failure reasons to each test
                for (auto &tv : packVec) {
                    for (auto &test : tv.vec) {
                        if (tv.standardness == INVALID || tv.standardness == NONSTANDARD) {
                            test.libauthStandardReason = allLibauthReasons[packName][tv.featureActive][true][test.ident];
                            test.standardReason = LookupReason(test.libauthStandardReason, test.ident, packName,
                                                               tv.featureActive, true);
                            if (tv.standardness == INVALID) {
                                test.libauthNonstandardReason = allLibauthReasons[packName][tv.featureActive][false][test.ident];
                                test.nonstandardReason = LookupReason(test.libauthNonstandardReason, test.ident,
                                                                      packName, tv.featureActive, false);
                            }
                        }
                    }
                }
                allTestPacks[testPack.name] = std::move(testPack);
            }
        }
    }
    BOOST_CHECK( ! allTestPacks.empty());
}

/* static */
bool LibauthTestingSetup::RunScriptOnlyTest(const TestVector::Test &tv, bool standard, CValidationState &state) {
    AssertLockHeld(cs_main);
    const uint32_t flags = [&] {
        uint32_t blockFlags{};
        uint32_t stdFlags = GetMemPoolScriptFlags(GetConfig().GetChainParams().GetConsensus(), ChainActive().Tip(),
                                                  &blockFlags);
        return standard ? stdFlags : blockFlags;
    }();
    state = {};
    if (standard) {
        // If caller wants standardness, do rudimentary checks anyway even in "scriptonly" mode
        if (std::string reason; !IsStandardTx(*tv.tx, reason, flags)) {
            return state.Invalid(false, REJECT_NONSTANDARD, reason);
        }
        if (!AreInputsStandard(*tv.tx, *pcoinsTip, flags)) {
            return state.Invalid(false, REJECT_NONSTANDARD, "bad-txns-nonstandard-inputs");
        }
    }
    auto const contexts = ScriptExecutionContext::createForAllInputs(*tv.tx, *pcoinsTip);
    auto const &context = contexts.at(tv.inputNum);
    const PrecomputedTransactionData txdata{context};
    const TransactionSignatureChecker checker(context, txdata);
    ScriptExecutionMetrics metrics;
    ScriptError serror{};
    const bool ret = VerifyScript(context.scriptSig(), context.coinScriptPubKey(), flags, checker, metrics, &serror);
    if (!ret) {
        state.Invalid(false, REJECT_INVALID, ScriptErrorString(serror));
    }
    BOOST_TEST_MESSAGE(strprintf("\"%s\" *scriptonly* eval input number: %i, nSigChecks: %i, result: %i, error: \"%s\"",
                                 tv.ident, tv.inputNum, metrics.nSigChecks, ret, state.GetRejectReason()));
    return ret;
}

/* static */
void LibauthTestingSetup::RunTestVector(const TestVector &test, const std::string &packName) {
    std::string activeStr = test.featureActive ? "postactivation" : "preactivation";
    const bool expectStd = test.standardness == STANDARD;
    const bool expectNonStd = test.standardness == STANDARD || test.standardness == NONSTANDARD;
    BOOST_TEST_MESSAGE(strprintf("Running test vectors \"%s\", description: \"%s\" ...", test.name, test.description));

    size_t num = 0;
    for (const auto &tv : test.vec) {
        ++num;
        const std::string scriptOnlyBlurb = tv.scriptOnly ? strprintf(" (scriptonly, input number %i)", tv.inputNum) : std::string{};
        BOOST_TEST_MESSAGE(strprintf("Executing \"%s\" test %i \"%s\": \"%s\", tx-size: %i, nInputs: %i%s ...\n",
                                     test.name, num, tv.ident, tv.description, ::GetSerializeSize(*tv.tx),
                                     tv.inputCoins.size(), scriptOnlyBlurb));
        Defer cleanup([&]{
            LOCK(cs_main);
            g_mempool.clear();
            for (auto & [outpt, _] : tv.inputCoins) {
                // clear utxo set of the temp coins we added for this tx
                pcoinsTip->SpendCoin(outpt);
            }
        });
        LOCK(cs_main);
        for (const auto &[outpt, entry] : tv.inputCoins) {
            // add each coin that the tx spends to the utxo set
            pcoinsTip->AddCoin(outpt, entry.coin, false);;
        }
        // First, do "standard" test; result should match `expectStd`
        ::fRequireStandard = true;
        CValidationState state;
        bool missingInputs{};
        bool ok1;
        if (tv.scriptOnly) {
            ok1 = RunScriptOnlyTest(tv, ::fRequireStandard, state);
        } else {
            ok1 = AcceptToMemoryPool(GetConfig(), g_mempool, state, tv.tx, &missingInputs,
                                     false          /* bypass_limits */,
                                     Amount::zero() /* nAbsurdFee    */,
                                     false          /* test_accept   */);
        }
        std::string standardReason = state.GetRejectReason();
        std::string nonstandardReason{""};
        if (standardReason.empty() && !ok1 && missingInputs) standardReason = "Missing inputs";
        BOOST_CHECK_MESSAGE(ok1 == expectStd, strprintf("(%s standard) %s Wrong result. %s.", activeStr, tv.ident,
                                                        expectStd ? strprintf("Pass expected, test failed (%s)", standardReason)
                                                                  : "Fail expected, test passed"));
        bool goodStandardReason = expectStd || tv.standardReason == standardReason;
        bool goodNonstandardReason = true;
        BOOST_CHECK_MESSAGE(goodStandardReason,
                            strprintf("(%s standard) %s Unexpected reject reason. Expected \"%s\", got \"%s\". "
                                      "Libauth's reason: \"%s\".", activeStr, tv.ident, tv.standardReason,
                                      standardReason, tv.libauthStandardReason));
        bool ok2 = expectNonStd;
        if (!expectStd) {
            // Next, do "nonstandard" test but only if `!expectStd`; result should match `expectNonStd`
            state = {};
            missingInputs = false;
            ::fRequireStandard = false;
            if (tv.scriptOnly) {
                ok2 = RunScriptOnlyTest(tv, ::fRequireStandard, state);
            } else {
                ok2 = AcceptToMemoryPool(GetConfig(), g_mempool, state, tv.tx, &missingInputs,
                                         true           /* bypass_limits */,
                                         Amount::zero() /* nAbsurdFee    */,
                                         false          /* test_accept   */);
            }
            nonstandardReason = state.GetRejectReason();
            if (nonstandardReason.empty() && !ok2 && missingInputs) nonstandardReason = "Missing inputs";
            BOOST_CHECK_MESSAGE(ok2 == expectNonStd,
                                strprintf("(%s nonstandard) %s Wrong result. %s.", activeStr, tv.ident,
                                          expectNonStd ? strprintf("Pass expected, test failed (%s)", nonstandardReason)
                                                       : "Fail expected, test passed"));
            goodNonstandardReason = expectNonStd || tv.nonstandardReason == nonstandardReason;
            BOOST_CHECK_MESSAGE(goodNonstandardReason,
                                strprintf("(%s nonstandard) %s Unexpected reject reason. Expected \"%s\", got \"%s\". "
                                          "Libauth's reason: \"%s\".", activeStr, tv.ident, tv.nonstandardReason,
                                          nonstandardReason, tv.libauthNonstandardReason));
        }
        if (ok1 != expectStd || ok2 != expectNonStd || !goodStandardReason || !goodNonstandardReason) {
            auto &tx = tv.tx;
            BOOST_TEST_MESSAGE(strprintf("TxId %s for test \"%s\" details:", tx->GetId().ToString(), tv.ident));
            size_t i = 0;
            for (auto &inp : tx->vin) {
                const CTxOut &txout = pcoinsTip->AccessCoin(inp.prevout).GetTxOut();
                BOOST_TEST_MESSAGE(strprintf("Input %i: %s, coin = %s",
                                             i, inp.prevout.ToString(true), txout.ToString(true)));
                ++i;
            }
            i = 0;
            for (auto &outp : tx->vout) {
                BOOST_TEST_MESSAGE(strprintf("Output %i: %s", i, outp.ToString(true)));
                ++i;
            }
        }
        if (!ok1) {
            bchnProducedReasons[packName][test.featureActive][true][tv.ident] = standardReason;
        }
        if (!ok2) {
            bchnProducedReasons[packName][test.featureActive][false][tv.ident] = nonstandardReason;
        }
    }
}

LibauthTestingSetup::LibauthTestingSetup()
    : saved_fRequireStandard{::fRequireStandard}
{}

LibauthTestingSetup::~LibauthTestingSetup() {
    // restore original fRequireStandard flag since the testing setup definitely touched this flag
    ::fRequireStandard = saved_fRequireStandard;
}

void LibauthTestingSetup::RunTestPack(const std::string &packName) {
    LoadAllTestPacks();
    const auto it = std::as_const(allTestPacks).find(packName);
    if (it != allTestPacks.cend()) {
        const TestPack &pack = it->second;
        BOOST_CHECK_EQUAL(packName, pack.name); // paranoia, should always match
        BOOST_TEST_MESSAGE(strprintf("----- Running '%s' tests -----", packName));
        for (const TestVector &testVector : pack.testVectors) {
            if (pack.type == TestPack::FEATURE) {
                ActivateFeature(testVector.featureActive);
            }
            RunTestVector(testVector, packName);
        }
    } else {
        // fail if test vectors for `packName` are not found
        BOOST_CHECK_MESSAGE(false, strprintf("No tests found for '%s'!", packName));
    }
}

/* static */
bool LibauthTestingSetup::ProcessReasonsLookupTable() {
    // Gather all the reasons/errors information
    ReasonsMapTree reasonsTree{};
    // Optimize the structure to minimize the number of rules/overrides
    reasonsTree.Prune();
    // Resolve the data into a JSON table structure ready for exporting
    UniValue::Object lookupTable = reasonsTree.GetLookupTable();

    auto stringToFile = [](const std::string &str, const std::string &path) {
        fs::path filePath{path};
        FILE *file = fsbridge::fopen(filePath, "w");
        if (file) {
            std::fwrite(str.data(), 1, str.size(), file);
            std::fclose(file);
        } else {
            BOOST_WARN_MESSAGE(false, "Can't open output file: " + path);
        }
    };

    // If the produced lookup table differs from the table we initially loaded in, then write it out to file
    bool tablesMatch = lookupTable == reasonsLookupTable;
    if (!tablesMatch) {
        // The `[]` wrapper is needed since `json_read` expects an array at the top level
        std::string path{"./libauth_expected_test_fail_reasons.json"};
        std::string jsonOut = "[" + UniValue::stringify(lookupTable, 2) + "]\n";
        BOOST_WARN_MESSAGE(false, "Saving Libauth -> BCHN error message lookup table to: " + path);
        stringToFile(jsonOut, path);
        // Also output a human-readable checklist
        path = "./libauth_expected_reasons_checklist.csv";
        std::string csvOut = reasonsTree.GetReasonsLookupChecklist(lookupTable);
        BOOST_WARN_MESSAGE(false, "Saving Libauth -> BCHN error message lookup table checklist to: " + path);
        stringToFile(csvOut, path);
    }
    return tablesMatch;
}


/* static */
std::string LibauthTestingSetup::LookupReason(const std::string &libauthReason, const std::string &ident,
                                              const std::string &packName, const bool featureActive,
                                              const bool standardValidation, const UniValue::Object &table) {
    // Return matches in order most specific to least specific:
    // - First use any specific test overrides if found.
    // - Next consult specific-situation libauth to bchn error message rules, that is, rules that should be
    //   applied for this test pack, feature activation, and validation standardness.
    // - Finally try progressively less specific rules, ultimately using the most general context-free rules.
    std::string activeStr = featureActive ? "postactivation" : "preactivation";
    std::string standardStr = standardValidation ? "standard" : "nonstandard";
    const UniValue &packEntry = table["testpacks"][packName];
    for (const UniValue *reason : { packEntry[activeStr][standardStr]["overrides"].locate(ident),
                                    packEntry[activeStr][standardStr]["mappings"].locate(libauthReason),
                                    packEntry[activeStr]["mappings"].locate(libauthReason),
                                    packEntry["mappings"].locate(libauthReason),
                                    table["mappings"].locate(libauthReason) }) {
        if (reason) {
            return reason->getValStr();
        }
    }
    BOOST_ERROR(strprintf("No rule or override found for test \"%s\" with Libauth suggeted reason \"%s\" for test pack \"%s\"",
                          ident, libauthReason, packName));
    return "";
}

LibauthTestingSetup::ReasonsMapTree::ReasonsMapTree() {
    for (const auto & [packName, m1] : bchnProducedReasons) {
        for (const auto & [active, m2] : m1) {
            for (const auto & [standard, testVec] : m2) {
                for (const auto & [ident, bchnReason] : testVec) {
                    // If there is a Libauth suggested reason for this test, assign a mapping
                    try {
                        const std::string &libauthReason =
                                allLibauthReasons.at(packName).at(active).at(standard).at(ident);
                        entries[packName].entries[active].entries[standard]
                                .mappings[libauthReason][bchnReason].insert(ident);
                    } catch (const std::out_of_range &e) {
                        std::string desc = std::string(active ? "post" : "pre") + "activation-"
                                           + (standard ? "" : "non") + "standard";
                        BOOST_ERROR(strprintf("Missing Libauth suggested failure reason for %s test \"%s\" for test pack \"%s\"",
                                              desc, ident, packName));
                    }
                }
            }
        }
    }
}

void LibauthTestingSetup::ReasonsMapTree::Prune() {
    // Moves the most common conflicting rules to become overrides
    auto setCommonOverrides = [](Mappings &mappings_, Overrides &overrides) {
        for (auto & [libauthReason, bchnReasons] : mappings_) {
            // Identify the most common bchnReason for this libauthReason
            std::set<std::string> bchnReasonsToOverride{};
            size_t mostCommonNumber{0};
            std::string mostCommonReason{""};
            for (const auto & [bchnReason, idents] : bchnReasons) {
                if (size_t num = idents.size(); num > mostCommonNumber) {
                    mostCommonReason = bchnReason;
                    mostCommonNumber = num;
                }
            }
            // Add overrides
            for (const auto & [bchnReason, idents] : bchnReasons) {
                if (bchnReason != mostCommonReason) {
                    bchnReasonsToOverride.insert(bchnReason);
                    for (const auto &ident : idents) {
                        overrides[ident] = bchnReason;
                    }
                }
            }
            // Remove redundant mappings entries
            for (const auto &bchnReason : bchnReasonsToOverride) {
                bchnReasons.erase(bchnReason);
            }
        }
    };

    // Moves all rules with the specified libauthReason key to become overrides
    auto setSpecificOverrides = [](const std::string &libauthReason, Mappings &mappings_, Overrides &overrides) {
        auto &bchnReasons = mappings_[libauthReason];
        // Add overrides
        std::set<std::string> bchnReasonsToOverride{};
        for (const auto & [bchnReason, idents] : bchnReasons) {
            bchnReasonsToOverride.insert(bchnReason);
            for (const auto &ident : idents) {
                overrides[ident] = bchnReason;
            }
        }
        // Remove redundant mappings entries
        mappings_.erase(libauthReason);
    };

    // To help make the rules lookup table as succinct as possible, move uniformly duplicated rules to their
    // common denominator node, leaving any other mappings where they are
    auto promoteDuplicateRules = [&setSpecificOverrides](Mappings &common, std::vector<ReasonsMapLeaf*> &descendants) {
        // First combine all the rules across branches to get a set of all libauthReasons
        std::set<std::string> descendantsLibauthReasons{};
        for (const auto *child : descendants) {
            for (const auto &rule : child->mappings) {
                descendantsLibauthReasons.insert(rule.first);
            }
        }
        // Determine which mappings should be promoted
        Mappings mappingsToPromote{};
        for (const auto &libauthReason : descendantsLibauthReasons) {
            // Gather stats about how often each bchnReason is mapped from this libauthReason among the descendants
            std::map<std::string, std::set<std::string>> promotionShortlist{};
            std::map<std::string, size_t> numOriginalReasons{};
            std::map<std::string, size_t> numChildOccurrances{};
            for (const auto *child : descendants) {
                if (child->mappings.count(libauthReason)) {
                    auto &bchnReasons = child->mappings.at(libauthReason);
                    for (const auto & [bchnReason, idents] : bchnReasons) {
                        for (const auto &ident : idents) {
                            promotionShortlist[bchnReason].insert(ident);
                        }
                        numOriginalReasons[bchnReason] += idents.size();
                        ++numChildOccurrances[bchnReason];
                    }

                }
            }
            // Get the candidate with the most duplication between branches
            std::string mostDuplicatedReason{""};
            size_t greatestNumReduction{0};
            for (const auto & [bchnReason, idents] : promotionShortlist) {
                // Only consider candidates that are on all leaf nodes
                if (numChildOccurrances[bchnReason] == descendants.size()) {
                    size_t numReduction = numOriginalReasons[bchnReason] - idents.size();
                    if (numReduction > greatestNumReduction) {
                        greatestNumReduction = numReduction;
                        mostDuplicatedReason = bchnReason;
                    }
                }
            }
            if (greatestNumReduction > 0) {
                mappingsToPromote[libauthReason][mostDuplicatedReason] = promotionShortlist[mostDuplicatedReason];
            }
        }
        // Add the mappings to the common denominator level
        for (const auto & [libauthReason, bchnReasons] : mappingsToPromote) {
            for (const auto & [bchnReason, idents] : bchnReasons) {
                for (const auto &ident : idents) {
                    common[libauthReason][bchnReason].insert(ident);
                }
            }
        }
        // Remove the now-redundant descendant mappings
        for (auto *child : descendants) {
            for (const auto & [libauthReason, bchnReasons] : mappingsToPromote) {
                for (const auto & [bchnReason, _] : bchnReasons) {
                    child->mappings[libauthReason].erase(bchnReason);
                    if (child->mappings[libauthReason].size() == 0) {
                        child->mappings.erase(libauthReason);
                    }
                }
                setSpecificOverrides(libauthReason, child->mappings, child->overrides);
            }
        }
    };

    // Deduplicate rules between CHIP branches if there is more than one CHIP
    if (entries.size() > 1) {
        std::vector<ReasonsMapLeaf*> treeLeaves{};
        for (auto & [packName, packEntries] : entries) {
            for (auto & [featureActive, activationEntries] : packEntries.entries) {
                for (auto & [_, standardnessEntries] : activationEntries.entries) {
                    treeLeaves.push_back(&standardnessEntries);
                }
            }
        }
        promoteDuplicateRules(mappings, treeLeaves);
    }
    // Deduplicate rules between activation branches
    for (auto & [packName, packEntries] : entries) {
        std::vector<ReasonsMapLeaf*> treeLeaves{};
        for (auto & [featureActive, activationEntries] : packEntries.entries) {
            std::vector<Mappings*> thisBranchDescendants{};
            for (auto & [_, standardnessEntries] : activationEntries.entries) {
                treeLeaves.push_back(&standardnessEntries);
            }
        }
        promoteDuplicateRules(packEntries.mappings, treeLeaves);
    }
    // Deduplicate rules between standardness branches
    for (auto & [packName, packEntries] : entries) {
        for (auto & [featureActive, activationEntries] : packEntries.entries) {
            std::vector<ReasonsMapLeaf*> treeLeaves{};
            for (auto & [_, standardnessEntries] : activationEntries.entries) {
                treeLeaves.push_back(&standardnessEntries);
            }
            promoteDuplicateRules(activationEntries.mappings, treeLeaves);
        }
    }
    // At each leaf node, for each libauthReason, move every mapping that is not the most common mapping to
    // become an override instead of a general rule
    for (auto & [packName, packEntries] : entries) {
        for (auto & [featurective, activeEntries] : packEntries.entries) {
            for (auto & [standard, standardnessEntries] : activeEntries.entries) {
                setCommonOverrides(standardnessEntries.mappings, standardnessEntries.overrides);
            }
        }
    }
}

UniValue::Object LibauthTestingSetup::ReasonsMapTree::GetLookupTable() const {
    auto getMappingsJson = [](const Mappings &mappings_) -> UniValue::Object {
        UniValue::Object json;
        for (const auto & [libauthReason, bchnReasons] : mappings_) {
            if (bchnReasons.size()) {
                // Ignore idents when outputting to JSON
                json.emplace_back(libauthReason, bchnReasons.begin()->first);
            }
        }
        return json;
    };

    auto getOverridesJson = [](const Overrides &overrides) -> UniValue::Object {
        UniValue::Object json;
        for (const auto & [ident, bchnReason] : overrides) {
            json.emplace_back(ident, bchnReason);
        }
        return json;
    };

    UniValue::Object table;
    table.reserve(2);
    table.emplace_back("mappings", getMappingsJson(mappings));
    UniValue::Object testPacks;
    testPacks.reserve(entries.size());
    for (const auto & [packName, packEntries] : entries) {
        UniValue::Object packObj;
        packObj.reserve(1 + packEntries.entries.size());
        packObj.emplace_back("mappings", getMappingsJson(packEntries.mappings));
        for (const auto & [featureActive, activationEntries] : packEntries.entries) {
            std::string activationStr = featureActive ? "postactivation" : "preactivation";
            UniValue::Object activationObj;
            activationObj.reserve(2);
            activationObj.emplace_back("mappings", getMappingsJson(activationEntries.mappings));
            for (const auto & [standard, standardnessEntries] : activationEntries.entries) {
                std::string standardStr = standard ? "standard" : "nonstandard";
                UniValue::Object standardObj;
                standardObj.emplace_back("mappings", getMappingsJson(standardnessEntries.mappings));
                standardObj.emplace_back("overrides", getOverridesJson(standardnessEntries.overrides));
                activationObj.emplace_back(standardStr, std::move(standardObj));
            }
            packObj.emplace_back(activationStr, std::move(activationObj));
        }
        testPacks.emplace_back(packName, std::move(packObj));
    }
    table.emplace_back("testpacks", std::move(testPacks));
    return table;
}

std::string LibauthTestingSetup::ReasonsMapTree::GetReasonsLookupChecklist(const UniValue::Object& newLookup) const {
    // [ident, description]
    using TestsDetails = std::set<std::pair<std::string, std::string>>;
    // libauthReason: {bchnReason: [ident, description]}
    using DetailedOverrides = std::map<std::string, std::map<std::string, TestsDetails>>;
    // packName: { featureActive: { standard: { libauthReason: {bchnReason: [ident, description]}}}}
    using AllDetailedOverrides = std::map<std::string, std::map<std::string, std::map<std::string, DetailedOverrides>>>;

    // Get the description and suggested failure reason for a given test
    const auto getTestDetails = [](const std::string &ident, const std::string &packName, const bool featureActive,
                                   const bool standardValidation) {
        std::pair<std::string, std::string> out{};
        if (auto it = std::as_const(allTestPacks).find(packName); it != allTestPacks.cend()) {
            const auto &testPack = it->second;
            for (const TestVector &testVector : testPack.testVectors) {
                if (testVector.featureActive == featureActive) {
                    for (const auto &test : testVector.vec) {
                        if (test.ident == ident) {
                            out.first = test.description;
                            if (standardValidation)  {
                                out.second = test.libauthStandardReason;
                            } else {
                                out.second = test.libauthNonstandardReason;
                            }
                            break;
                        }
                    }
                }
            }
        }
        return out;
    };

    // Gather extra information about all overrides so they can be inserted immediately after the rules
    // that they override
    AllDetailedOverrides allOverrides{};
    for (const auto & [packName, packEntries] : entries) {
        for (const auto & [featureActive, activationEntries] : packEntries.entries) {
            std::string activationStr = featureActive ? "postactivation" : "preactivation";
            for (const auto & [standard, standardnessEntries] : activationEntries.entries) {
                std::string standardStr = standard ? "standard" : "nonstandard";
                for (const auto & [ident, bchnReason] : standardnessEntries.overrides) {
                    const auto [description, suggestedReason] = getTestDetails(ident, packName, featureActive, standard);
                    allOverrides[packName][activationStr][standardStr][suggestedReason][bchnReason]
                            .emplace(ident, description);
                }
            }
        }
    }

    // Returns whether or not the specified lookup would have produced a different result using the originally
    // loaded in reasons lookup table
    auto ruleChanged = [&](const std::string &packName, const std::string &featureActive, const std::string &standard,
                           const std::string &libauthReason, const std::string &ident) -> bool {
        // If the rule to check applies to a specific TestPack, feature activation state and validation standard, then
        // we need only check that the same expected bchnReason is produced by the same lookup.  However if there are
        // placeholders, such as "--both--" for the activation state, then we need to confirm that the original
        // lookup would have produce the same expected result for each state
        for (const auto & [packName_, _] : reasonsLookupTable["testpacks"].get_obj()) {
            if (packName == "--all--" || packName_ == packName) {
                for (const bool featureActive_ : {true, false}) {
                    if (featureActive == "--both--" || featureActive_ == (featureActive == "postactivation")) {
                        for (const bool standard_ : {true, false}) {
                            if (standard == "--both--" || standard_ == (standard == "standard")) {
                                std::string origReason = LookupReason(libauthReason, ident, packName, featureActive_,
                                                                      standard_);
                                std::string newReason = LookupReason(libauthReason, ident, packName, featureActive_,
                                                                     standard_, newLookup);
                                if (newReason != origReason) {
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
        return false;
    };

    int numToCheck{0}, numTests{0};

    // Construct one line of content for the spreadsheet
    auto getEntry = [&](const bool &override, const std::string &bchnReason,
                        const std::string &packName, const std::string &featureActive, const std::string &standard,
                        const std::string &suggestedReason, const TestsDetails &tests) -> std::string {
        std::ostringstream ss{};
        std::string newRule = "";
        for (const auto & [ident, _] : tests) {
            if (ruleChanged(packName, featureActive, standard, suggestedReason, ident)) {
                newRule = "NEW";
                ++numToCheck;
                break;
            }
        }
        std::string ruleOrOverride = override ? "override" : "rule";
        ss << "\"" << newRule << "\",\"" << ruleOrOverride << "\",\"" << tests.size() << "\",\"" << bchnReason;
        ss << "\",\"" << suggestedReason << "\",\"" << packName << "\",\"" << featureActive << "\",\"" << standard << "\"";
        for (const auto & [ident, description] : tests) {
            ss << ",\"" << ident << "\",\"" << description << "\"";
            ++numTests;
        }
        ss << std::endl;
        return ss.str();
    };

    // Construct all the lines of content for the specified mapping rules and the overrides that apply to them
    auto getEntriesForRules = [&](const Mappings &mappings_, const std::string &packName = "--all--",
                                  const std::string &featureActive = "--both--",
                                  const std::string &standard = "--both--") -> std::string {
        std::ostringstream ss{};
        for (const auto & [libauthReason, bchnReasons] : mappings_) {
            for (const auto & [bchnReason, idents] : bchnReasons) {
                TestsDetails tests{};
                for (const auto &ident : idents) {
                    const auto [description, _] = getTestDetails(ident, packName, featureActive == "postactivation",
                                                                 standard == "standard");
                    tests.emplace(ident, description);
                }
                ss << getEntry(false, bchnReason, packName, featureActive, standard, libauthReason, tests);
            }
            DetailedOverrides &detailedOverrides = allOverrides[packName][featureActive][standard];
            if (detailedOverrides.count(libauthReason)) {
                for (const auto & [bchnReason, tests] : detailedOverrides[libauthReason]) {
                    ss << getEntry(true, bchnReason, packName, featureActive, standard, libauthReason, tests);
                }
            }
        }
        return ss.str();
    };

    // Construct the contents of the checklist spreadsheet
    std::ostringstream ss{};
    ss << "\"New?\",\"Type\",\"Uses\",\"BCHN error message\",";
    ss << "\"Libauth suggested reason\",\"TestPack name\",\"Feature activation\",";
    ss << "\"Validation standard\",\"Test ID\",\"Test description (columns repeat when multiple tests fit a rule)\"";
    ss << std::endl;
    ss << getEntriesForRules(mappings);
    for (const auto & [packName, packEntries] : entries) {
        ss << getEntriesForRules(packEntries.mappings, packName);
        for (const auto & [featureActive, activationEntries] : packEntries.entries) {
            std::string activationStr = featureActive ? "postactivation" : "preactivation";
            ss << getEntriesForRules(activationEntries.mappings, packName, activationStr);
            for (const auto & [standard, standardnessEntries] : activationEntries.entries) {
                std::string standardStr = standard ? "standard" : "nonstandard";
                ss << getEntriesForRules(standardnessEntries.mappings, packName, activationStr, standardStr);
            }
        }
    }
    BOOST_TEST_MESSAGE(strprintf("Total number of modified checklist rules: %d", numToCheck));
    BOOST_TEST_MESSAGE(strprintf("Total number of tests: %d", numTests));
    BOOST_TEST_MESSAGE(strprintf("Manual check effort reduced to: %d%%", (numToCheck*100.0)/numTests));
    return ss.str();
}
