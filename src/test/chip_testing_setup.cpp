// Copyright (c) 2022-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/chip_testing_setup.h>

#include <config.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <fs.h>
#include <streams.h>
#include <txmempool.h>
#include <util/defer.h>
#include <validation.h>

#include <test/data/chip_test_vectors.json.h>
#include <test/data/expected_test_fail_reasons.json.h>
#include <test/jsonutil.h>

#include <boost/test/unit_test.hpp>

#include <cstdlib>
#include <map>
#include <string>

/* static */
std::map<std::string, std::vector<ChipTestingSetup::TestVector>> ChipTestingSetup::allChipsVectors = {};
ChipTestingSetup::AllChipsReasonsDict ChipTestingSetup::allLibauthReasons = {};
ChipTestingSetup::AllChipsReasonsDict ChipTestingSetup::bchnProducedReasons = {};
UniValue::Object ChipTestingSetup::reasonsLookupTable = {};

/* static */
void ChipTestingSetup::LoadChipsVectors() {
    if (!allChipsVectors.empty()) return;

    static_assert(sizeof(json_tests::chip_test_vectors[0]) == 1 && sizeof(json_tests::expected_test_fail_reasons[0]) == 1,
                  "Assumption is that the test vectors are byte blobs of json data");

    const auto allChipsTests = read_json({ reinterpret_cast<const char *>(json_tests::chip_test_vectors),
                                           std::size(json_tests::chip_test_vectors) });
    const auto expectedReasons = read_json({ reinterpret_cast<const char *>(json_tests::expected_test_fail_reasons),
                                         std::size(json_tests::expected_test_fail_reasons) });

    // Load in the Libauth -> BCHN error message lookup table
    BOOST_CHECK( ! expectedReasons.empty());
    for (const auto &outerWrap : expectedReasons) {
        BOOST_CHECK(outerWrap.isObject());
        if (outerWrap.isObject()) {
            reasonsLookupTable = outerWrap.get_obj();
        }
    }
    // Load the CHIP test vectors, and Libauth suggested failure reasons
    BOOST_CHECK( ! allChipsTests.empty());
    unsigned coinHeights = []{
        LOCK(cs_main);
        return ::ChainActive().Tip()->nHeight;
    }();
    for (auto &chip : allChipsTests) {
        BOOST_CHECK(chip.isObject());
        if (chip.isObject()) {
            auto &chipObj = chip.get_obj();
            auto *nameVal = chipObj.locate("name");
            BOOST_CHECK(nameVal != nullptr);
            if (nameVal) {
                auto chipName = nameVal->get_str();
                std::vector<TestVector> chipVec;
                for (const auto &uv : chipObj.at("tests").get_array()) {
                    BOOST_CHECK(uv.isObject());
                    if (uv.isObject()) {
                        auto &uvObj = uv.get_obj();
                        auto *testNameVal = uvObj.locate("name");
                        BOOST_CHECK(testNameVal != nullptr);
                        if (testNameVal) {
                            std::string testName = testNameVal->get_str();
                            std::string preactivePrefix = "preactivation_";
                            bool chipActive = testName.rfind(preactivePrefix, 0) != 0;
                            std::string standardnessStr = chipActive ? testName
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
                            std::string descActiveString = chipActive ? "Post-Activation" : "Pre-Activation";
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
                            testVec.chipActive = chipActive;
                            testVec.standardness = testStandardness;

                            const auto &libauthReasons = uvObj.at("reasons");
                            if (libauthReasons.isObject()) { // may be null
                                for (const auto & [ident, obj] : libauthReasons.get_obj()) {
                                    if (obj.isStr()) {
                                        // Invalid tests should produce errors under both standard and nonstandard validation
                                        // Nonstandard tests should produce errors only under standard validation
                                        if (testStandardness == INVALID || testStandardness == NONSTANDARD) {
                                            allLibauthReasons[chipName][chipActive][true][ident] = obj.get_str();
                                            if (testStandardness == INVALID) {
                                                allLibauthReasons[chipName][chipActive][false][ident] = obj.get_str();
                                            }
                                        }
                                    }
                                }
                            }
                            for (const auto &t : uvObj.at("tests").get_array()) {
                                const UniValue::Array &vec = t.get_array();
                                BOOST_CHECK_GE(vec.size(), 6);
                                TestVector::Test test;
                                test.ident = vec.at(0).get_str();
                                test.description = vec.at(1).get_str();
                                test.stackAsm = vec.at(2).get_str();
                                test.scriptAsm = vec.at(3).get_str();

                                CMutableTransaction mtx;
                                BOOST_CHECK(DecodeHexTx(mtx, vec.at(4).get_str()));
                                test.tx = MakeTransactionRef(std::move(mtx));
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
                            chipVec.push_back(std::move(testVec));
                        }
                    }
                }
                // Assign Libauth's suggested failure reasons and BCHN expected failure reasons to each test
                for (auto &tv : chipVec) {
                    for (auto &test : tv.vec) {
                        if (tv.standardness == INVALID || tv.standardness == NONSTANDARD) {
                            test.libauthStandardReason = allLibauthReasons[chipName][tv.chipActive][true][test.ident];
                            test.standardReason = LookupReason(test.libauthStandardReason, test.ident, chipName,
                                                               tv.chipActive, true);
                            if (tv.standardness == INVALID) {
                                test.libauthNonstandardReason = allLibauthReasons[chipName][tv.chipActive][false][test.ident];
                                test.nonstandardReason = LookupReason(test.libauthNonstandardReason, test.ident,
                                                                      chipName, tv.chipActive, false);
                            }
                        }
                    }
                }
                allChipsVectors[chipName] = std::move(chipVec);
            }
        }
    }
    BOOST_CHECK( ! allChipsVectors.empty());
}

/* static */
void ChipTestingSetup::RunTestVector(const TestVector &test, const std::string &chipName) {
    std::string activeStr = test.chipActive ? "postactivation" : "preactivation";
    const bool expectStd = test.standardness == STANDARD;
    const bool expectNonStd = test.standardness == STANDARD || test.standardness == NONSTANDARD;
    BOOST_TEST_MESSAGE(strprintf("Running test vectors \"%s\", description: \"%s\" ...", test.name, test.description));

    size_t num = 0;
    for (const auto &tv : test.vec) {
        ++num;
        BOOST_TEST_MESSAGE(strprintf("Executing \"%s\" test %i \"%s\": \"%s\", tx-size: %i, nInputs: %i ...\n",
                                     test.name, num, tv.ident, tv.description, ::GetSerializeSize(*tv.tx),
                                     tv.inputCoins.size()));
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
        bool const ok1 = AcceptToMemoryPool(GetConfig(), g_mempool, state, tv.tx, &missingInputs,
                                            false          /* bypass_limits */,
                                            Amount::zero() /* nAbsurdFee    */,
                                            false          /* test_accept   */);
        std::string standardReason = state.GetRejectReason();
        std::string nonstandardReason{""};
        if (standardReason.empty() && !ok1 && missingInputs) standardReason = "Missing inputs";
        BOOST_CHECK_MESSAGE(ok1 == expectStd, strprintf("(%s standard) %s Wrong result. %s.", activeStr, tv.ident,
                                                        expectStd ? "Pass expected, test failed." :
                                                                    "Fail expected, test passed."));
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
            ok2 = AcceptToMemoryPool(GetConfig(), g_mempool, state, tv.tx, &missingInputs,
                                     true           /* bypass_limits */,
                                     Amount::zero() /* nAbsurdFee    */,
                                     false          /* test_accept   */);
            nonstandardReason = state.GetRejectReason();
            if (nonstandardReason.empty() && !ok2 && missingInputs) nonstandardReason = "Missing inputs";
            BOOST_CHECK_MESSAGE(ok2 == expectNonStd,
                                strprintf("(%s nonstandard) %s Wrong result. %s.", activeStr, tv.ident,
                                          expectNonStd ? "Pass expected, test failed."
                                                       : "Fail expected, test passed."));
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
            bchnProducedReasons[chipName][test.chipActive][true][tv.ident] = standardReason;
        }
        if (!ok2) {
            bchnProducedReasons[chipName][test.chipActive][false][tv.ident] = nonstandardReason;
        }
    }
}

ChipTestingSetup::ChipTestingSetup()
    : saved_fRequireStandard{::fRequireStandard}
{}

ChipTestingSetup::~ChipTestingSetup() {
    // restore original fRequireStandard flag since the testing setup definitely touched this flag
    ::fRequireStandard = saved_fRequireStandard;
}

void ChipTestingSetup::RunTestsForChip(const std::string &chipName) {
    LoadChipsVectors();
    const auto it = allChipsVectors.find(chipName);
    if (it != allChipsVectors.end()) {
        BOOST_TEST_MESSAGE(strprintf("----- Running '%s' CHIP tests -----", chipName));
        for (const TestVector &testVector : it->second) {
            ActivateChip(testVector.chipActive);
            RunTestVector(testVector, chipName);
        }
    } else {
        // fail if test vectors for `chipName` are not found
        BOOST_CHECK_MESSAGE(false, strprintf("No tests found for '%s' CHIP!", chipName));
    }
}

/* static */
bool ChipTestingSetup::ProcessReasonsLookupTable() {
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
            fwrite(str.data(), 1, str.size(), file);
        } else {
            BOOST_WARN_MESSAGE(false, "Can't open output file: " + path);
        }
    };

    // If the produced lookup table differs from the table we initially loaded in, then write it out to file
    bool tablesMatch = lookupTable == reasonsLookupTable;
    if (!tablesMatch) {
        // The `[]` wrapper is needed since `json_read` expects an array at the top level
        std::string path{"./expected_test_fail_reasons.json"};
        std::string jsonOut = "[" + UniValue::stringify(lookupTable, 2) + "]\n";
        BOOST_WARN_MESSAGE(false, "Saving Libauth -> BCHN error message lookup table to: " + path);
        stringToFile(jsonOut, path);
        // Also output a human-readable checklist
        path = "./expected_reasons_checklist.csv";
        std::string csvOut = reasonsTree.GetReasonsLookupChecklist(lookupTable);
        BOOST_WARN_MESSAGE(false, "Saving Libauth -> BCHN error message lookup table checklist to: " + path);
        stringToFile(csvOut, path);
    }
    return tablesMatch;
}


/* static */
std::string ChipTestingSetup::LookupReason(const std::string &libauthReason, const std::string &ident,
                                           const std::string &chipName, const bool chipActive,
                                           const bool standardValidation, const UniValue::Object &table) {
    // Return matches in order most specific to least specific:
    // - First use any specific test overrides if found.
    // - Next consult specific-situation libauth to bchn error message rules, that is, rules that should be
    //   applied for this CHIP, CHIP activation, and validation standardness.
    // - Finally try progressively less specific rules, ultimately using the most general context-free rules.
    std::string activeStr = chipActive ? "postactivation" : "preactivation";
    std::string standardStr = standardValidation ? "standard" : "nonstandard";
    const UniValue &chipEntry = table["chips"][chipName];
    for (const UniValue *reason : { chipEntry[activeStr][standardStr]["overrides"].locate(ident),
                                    chipEntry[activeStr][standardStr]["mappings"].locate(libauthReason),
                                    chipEntry[activeStr]["mappings"].locate(libauthReason),
                                    chipEntry["mappings"].locate(libauthReason),
                                    table["mappings"].locate(libauthReason) }) {
        if (reason) {
            return reason->getValStr();
        }
    }
    BOOST_ERROR(strprintf("No rule or override found for test \"%s\" with Libauth suggeted reason \"%s\"",
                          ident, libauthReason));
    return "";
}

ChipTestingSetup::ReasonsMapTree::ReasonsMapTree() {
    for (const auto & [chipName, chipVec] : bchnProducedReasons) {
        for (const auto & [active, chipTests] : chipVec) {
            for (const auto & [standard, testVec] : chipTests) {
                for (const auto & [ident, bchnReason] : testVec) {
                    // If there is a Libauth suggested reason for this test, assign a mapping
                    try {
                        const std::string &libauthReason =
                                allLibauthReasons.at(chipName).at(active).at(standard).at(ident);
                        entries[chipName].entries[active].entries[standard]
                                .mappings[libauthReason][bchnReason].insert(ident);
                    } catch (const std::out_of_range &e) {
                        std::string desc = std::string(active ? "post" : "pre") + "activation-"
                                           + (standard ? "" : "non") + "standard";
                        BOOST_ERROR(strprintf("Missing Libauth suggested failure reason for %s test \"%s\"",
                                              desc, ident));
                    }
                }
            }
        }
    }
}

void ChipTestingSetup::ReasonsMapTree::Prune() {
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
        for (auto & [chipName, chipEntries] : entries) {
            for (auto & [chipActive, activationEntries] : chipEntries.entries) {
                for (auto & [_, standardnessEntries] : activationEntries.entries) {
                    treeLeaves.push_back(&standardnessEntries);
                }
            }
        }
        promoteDuplicateRules(mappings, treeLeaves);
    }
    // Deduplicate rules between activation branches
    for (auto & [chipName, chipEntries] : entries) {
        std::vector<ReasonsMapLeaf*> treeLeaves{};
        for (auto & [chipActive, activationEntries] : chipEntries.entries) {
            std::vector<Mappings*> thisBranchDescendants{};
            for (auto & [_, standardnessEntries] : activationEntries.entries) {
                treeLeaves.push_back(&standardnessEntries);
            }
        }
        promoteDuplicateRules(chipEntries.mappings, treeLeaves);
    }
    // Deduplicate rules between standardness branches
    for (auto & [chipName, chipEntries] : entries) {
        for (auto & [chipActive, activationEntries] : chipEntries.entries) {
            std::vector<ReasonsMapLeaf*> treeLeaves{};
            for (auto & [_, standardnessEntries] : activationEntries.entries) {
                treeLeaves.push_back(&standardnessEntries);
            }
            promoteDuplicateRules(activationEntries.mappings, treeLeaves);
        }
    }
    // At each leaf node, for each libauthReason, move every mapping that is not the most common mapping to
    // become an override instead of a general rule
    for (auto & [chipName, chipEntries] : entries) {
        for (auto & [chipActive, activeEntries] : chipEntries.entries) {
            for (auto & [standard, standardnessEntries] : activeEntries.entries) {
                setCommonOverrides(standardnessEntries.mappings, standardnessEntries.overrides);
            }
        }
    }
}

UniValue::Object ChipTestingSetup::ReasonsMapTree::GetLookupTable() const {
    auto getMappingsJson = [](const Mappings &mappings_) -> UniValue::Object {
        UniValue::Object json;
        for (const auto & [libauthReason, bchnReasons] : mappings_) {
            if (bchnReasons.size()) {
                // Ignore idents when outputting to JSON
                json.emplace_back(libauthReason, UniValue(bchnReasons.begin()->first));
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
    table.emplace_back("mappings", getMappingsJson(mappings));
    UniValue::Object chips;
    for (const auto & [chipName, chipEntries] : entries) {
        UniValue::Object chipObj;
        UniValue::Object chipMappings = getMappingsJson(chipEntries.mappings);
        chipObj.emplace_back("mappings", chipMappings);
        for (const auto & [chipActive, activationEntries] : chipEntries.entries) {
            std::string activationStr = chipActive ? "postactivation" : "preactivation";
            UniValue::Object activationObj;
            UniValue::Object activationMappings = getMappingsJson(activationEntries.mappings);
            activationObj.emplace_back("mappings", activationMappings);
            for (const auto & [standard, standardnessEntries] : activationEntries.entries) {
                std::string standardStr = standard ? "standard" : "nonstandard";
                UniValue::Object standardObj;
                UniValue::Object standardMappings = getMappingsJson(standardnessEntries.mappings);
                UniValue::Object standardOverrides = getOverridesJson(standardnessEntries.overrides);
                standardObj.emplace_back("mappings", standardMappings);
                standardObj.emplace_back("overrides", standardOverrides);
                activationObj.emplace_back(standardStr, standardObj);
            }
            chipObj.emplace_back(activationStr, activationObj);
        }
        chips.emplace_back(chipName, chipObj);
    }
    table.emplace_back("chips", chips);
    return table;
}

std::string ChipTestingSetup::ReasonsMapTree::GetReasonsLookupChecklist(const UniValue::Object& newLookup) const {
    // [ident, description]
    using TestsDetails = std::set<std::pair<std::string, std::string>>;
    // libauthReason: {bchnReason: [ident, description]}
    using DetailedOverrides = std::map<std::string, std::map<std::string, TestsDetails>>;
    // chipName: { chipActive: { standard: { libauthReason: {bchnReason: [ident, description]}}}}
    using AllDetailedOverrides = std::map<std::string, std::map<std::string, std::map<std::string, DetailedOverrides>>>;

    // Get the description and suggested failure reason for a given test
    const auto getTestDetails = [](const std::string &ident, const std::string &chipName, const bool &chipActive,
                                   const bool &standardValidation) {
        std::pair<std::string, std::string> out{};
        for (const auto & [chipName_, chipVec] : allChipsVectors) {
            if (chipName == chipName_) {
                for (const TestVector &testVector : chipVec) {
                    if (testVector.chipActive == chipActive) {
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
        }
        return out;
    };

    // Gather extra information about all overrides so they can be inserted immediately after the rules
    // that they override
    AllDetailedOverrides allOverrides{};
    for (const auto & [chipName, chipEntries] : entries) {
        for (const auto & [chipActive, activationEntries] : chipEntries.entries) {
            std::string activationStr = chipActive ? "postactivation" : "preactivation";
            for (const auto & [standard, standardnessEntries] : activationEntries.entries) {
                std::string standardStr = standard ? "standard" : "nonstandard";
                for (const auto & [ident, bchnReason] : standardnessEntries.overrides) {
                    const auto [description, suggestedReason] = getTestDetails(ident, chipName, chipActive, standard);
                    allOverrides[chipName][activationStr][standardStr][suggestedReason][bchnReason]
                            .insert({ident, description});
                }
            }
        }
    }

    // Returns whether or not the specified lookup would have produced a different result using the originally
    // loaded in reasons lookup table
    auto ruleChanged = [&](const std::string &chipName, const std::string &chipActive, const std::string &standard,
            const std::string &libauthReason, const std::string &ident) -> bool {
        // If the rule to check applies to a specific CHIP, CHIP activation state and validation standard, then we
        // need only check that the same expected bchnReason is produced by the same lookup.  However if there are
        // placeholders, such as "--both--" for the activation state, then we need to confirm that the original
        // lookup would have produce the same expected result for each state
        for (const auto & [chipName_, _] : reasonsLookupTable["chips"].get_obj()) {
            if (chipName == "--all--" || chipName_ == chipName) {
                for (bool chipActive_ : {true, false}) {
                    if (chipActive == "--both--" || chipActive_ == (chipActive == "postactivation")) {
                        for (bool standard_ : {true, false}) {
                            if (standard == "--both--" || standard_ == (standard == "standard")) {
                                std::string origReason = LookupReason(libauthReason, ident, chipName, chipActive_,
                                                                      standard_);
                                std::string newReason = LookupReason(libauthReason, ident, chipName, chipActive_,
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
            const std::string &chipName, const std::string &chipActive, const std::string &standard,
            const std::string &suggestedReason, const TestsDetails &tests)
            -> std::string {
        std::ostringstream ss{};
        std::string newRule = "";
        for (const auto & [ident, _] : tests) {
            if (ruleChanged(chipName, chipActive, standard, suggestedReason, ident)) {
                newRule = "NEW";
                ++numToCheck;
                break;
            }
        }
        std::string ruleOrOverride = override ? "override" : "rule";
        ss << "\"" << newRule << "\",\"" << ruleOrOverride << "\",\"" << tests.size() << "\",\"" << bchnReason;
        ss << "\",\"" << suggestedReason << "\",\"" << chipName << "\",\"" << chipActive << "\",\"" << standard << "\"";
        for (const auto & [ident, description] : tests) {
            ss << ",\"" << ident << "\",\"" << description << "\"";
            ++numTests;
        }
        ss << std::endl;
        return ss.str();
    };

    // Construct all the lines of content for the specified mapping rules and the overrides that apply to them
    auto getEntriesForRules = [&](const Mappings &mappings_, const std::string &chipName="--all--",
            const std::string &chipActive="--both--", const std::string &standard="--both--") -> std::string {
        std::ostringstream ss{};
        for (const auto & [libauthReason, bchnReasons] : mappings_) {
            for (const auto & [bchnReason, idents] : bchnReasons) {
                TestsDetails tests{};
                for (const auto &ident : idents) {
                    const auto [description, _] = getTestDetails(ident, chipName, chipActive == "postactivation",
                                                                 standard == "standard");
                    tests.insert({ident, description});
                }
                ss << getEntry(false, bchnReason, chipName, chipActive, standard, libauthReason, tests);
            }
            DetailedOverrides &detailedOverrides = allOverrides[chipName][chipActive][standard];
            if (detailedOverrides.count(libauthReason)) {
                for (const auto & [bchnReason, tests] : detailedOverrides[libauthReason]) {
                    ss << getEntry(true, bchnReason, chipName, chipActive, standard, libauthReason, tests);
                }
            }
        }
        return ss.str();
    };

    // Construct the contents of the checklist spreadsheet
    std::ostringstream ss{};
    ss << "\"New?\",\"Type\",\"Uses\",\"BCHN error message\",";
    ss << "\"Libauth suggested reason\",\"CHIP name\",\"CHIP activation\",";
    ss << "\"Validation standard\",\"Test ID\",\"Test description (columns repeat when multiple tests fit a rule)\"";
    ss << std::endl;
    ss << getEntriesForRules(mappings);
    for (const auto & [chipName, chipEntries] : entries) {
        ss << getEntriesForRules(chipEntries.mappings, chipName);
        for (const auto & [chipActive, activationEntries] : chipEntries.entries) {
            std::string activationStr = chipActive ? "postactivation" : "preactivation";
            ss << getEntriesForRules(activationEntries.mappings, chipName, activationStr);
            for (const auto & [standard, standardnessEntries] : activationEntries.entries) {
                std::string standardStr = standard ? "standard" : "nonstandard";
                ss << getEntriesForRules(standardnessEntries.mappings, chipName, activationStr, standardStr);
            }
        }
    }
    BOOST_TEST_MESSAGE(strprintf("Total number of modified checklist rules: %d", numToCheck));
    BOOST_TEST_MESSAGE(strprintf("Total number of tests: %d", numTests));
    BOOST_TEST_MESSAGE(strprintf("Manual check effort reduced to: %d%%", (numToCheck*100.0)/numTests));
    return ss.str();
}
