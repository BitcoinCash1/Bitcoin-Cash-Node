// Copyright (c) 2022-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <coins.h>
#include <primitives/transaction.h>
#include <test/setup_common.h>
#include <univalue.h>

#include <map>
#include <set>
#include <string>

class CValidationState;

/// Testing setup that:
/// - loads all of the json data for all of the libauth tests into a static structure (lazy load, upon first use)
/// - tracks if we overrode ::fRequireStandard, and resets it on test end
/// For FEATURE tests, subclasses must reimplement "ActivateFeature()" (see libauth_tests.cpp for examples that use
/// this setup)
class LibauthTestingSetup : public TestChain100Setup {

    enum TxStandard { INVALID, NONSTANDARD, STANDARD };

    // A structure to hold all failure reason messages for all tests for all test packs
    // packName: {featureActive: {standardValidation: {ident: "reason"}}}
    using AllReasonsDict = std::map<std::string, std::map<bool, std::map<bool, std::map<std::string, std::string>>>>;

    // libauthReason: {bchnReason: [idents]}
    using Mappings = std::map<std::string, std::map<std::string, std::set<std::string>>>;
    // ident: bchnReason
    using Overrides = std::map<std::string, std::string>;

    // A workspace to help produce the optimized libauth -> bchn failure message lookup table
    struct ReasonsMapTree {
        struct TestPackEntries {
            struct ActivationEntries {
                struct StandardnessEntries {
                    Mappings mappings;
                    Overrides overrides;
                };
                std::map<bool, StandardnessEntries> entries;
                Mappings mappings;
            };
            std::map<bool, ActivationEntries> entries;
            Mappings mappings;
        };
        std::map<std::string, TestPackEntries> entries;
        Mappings mappings;

        // Constructs the tree with all information from `allLibauthReasons` and `bchnProducedErrors`
        ReasonsMapTree();
        // Optimize the tree structure
        void Prune();
        // Get JSON representation of the lookup table ready to be exported to file
        UniValue::Object GetLookupTable() const;
        // Get a human readable checklist to help manually confirm the failure message lookup table
        std::string GetReasonsLookupChecklist(const UniValue::Object &newLookup) const;
    };
    using ReasonsMapLeaf = ReasonsMapTree::TestPackEntries::ActivationEntries::StandardnessEntries;

    struct TestVector {
        std::string name;
        std::string description;
        bool featureActive = true; // Only pack.type == FEATURE; activate/deactivate the consensus rule in question
        TxStandard standardness{}; // Which validation standard this test should meet

        struct Test {
            std::string ident;
            std::string description;
            std::string stackAsm;
            std::string scriptAsm;
            CTransactionRef tx;
            size_t txSize{};
            CCoinsMap inputCoins;
            std::string standardReason; //! Expected failure reason when validated in standard mode
            std::string nonstandardReason; //! Expected failure reason when validated in nonstandard mode
            std::string libauthStandardReason; //! Libauth suggested failure reason when validated in standard mode
            std::string libauthNonstandardReason; //! Libauth suggested failure reason when validated in nonstandard mode
            bool scriptOnly = false; //< If true, this test vector should not test against AcceptToMemoryPool() for the
                                     //< whole txn, but should just evaluate the script for input `inputNum`.
            unsigned inputNum = 0;   //< The input number to test. Comes from the optional 7th column of the JSON array
                                     //< for this test, defaults to 0 if unspecified. Only used if scriptOnly == true.
        };

        std::vector<Test> vec;
    };

    // Container for a group of test vectors. Corresponds to either an individual "CHIP" test pack or a regression or
    // other named package of tests imported from libauth.
    struct TestPack {
        std::string name; //! Test pack name, same as the key in the `allTestPacks` map below
        std::vector<TestVector> testVectors;

        enum Type {
            FEATURE, //! Feature-specific test pack. May toggle the "featureActive" bool to test pre and post activation
            OTHER,   //! Non-feature-specific or general regression test pack. The "featureActive" bool is not toggled.
        };

        Type type = OTHER;

        TestPack() = default;
    };

    static std::map<std::string, TestPack> allTestPacks;  //! key: testPack.name, value: testPack

    // A lookup table that can be used to find a single expected failure test message given information about the
    // particular Libauth test and the testing context
    static UniValue::Object reasonsLookupTable;
    static std::string LookupReason(const std::string &libauthReason, const std::string &ident,
                                    const std::string &packName, const bool featureActive, const bool standardValidation,
                                    const UniValue::Object &table=reasonsLookupTable);

    // These dictionaries are populated per-pack by running `RunTestPack`
    static AllReasonsDict allLibauthReasons; // All error messages suggested by Libauth
    static AllReasonsDict bchnProducedReasons; // All error messages actually produced

    static void LoadAllTestPacks();
    static void RunTestVector(const TestVector &test, const std::string &packName);
    static bool RunScriptOnlyTest(const TestVector::Test &tv, bool standard, CValidationState &state);

    const bool saved_fRequireStandard;

protected:
    /// Reimplement this in subclasses to turn on/off the consensus rule or feature in question. Only called for
    /// test packs of type TestPack::FEATURE.
    virtual void ActivateFeature(bool) {}

public:
    LibauthTestingSetup();
    ~LibauthTestingSetup() override;

    /// Run all tests for the test pack named `name`.
    void RunTestPack(const std::string &name);

    /// Generate the reasons lookup table and compare it against the currently loaded table. Returns false and outputs
    /// the corrected version to a file if it differs, and includes a human-readable checklist file to help with manual
    /// confirmation. This should be called exactly once after all of the test pack tests have executed.
    static bool ProcessReasonsLookupTable();
};
