// Copyright (c) 2022-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <validation.h>
#include <coins.h>
#include <primitives/transaction.h>
#include <univalue.h>

#include <test/setup_common.h>

#include <map>
#include <string>

/// Testing setup that:
/// - loads all of the json data for all of the "chip" tests into a static structure (lazy load, upon first use)
/// - tracks if we overrode ::fRequireStandard, and resets it on test end
/// Subclasses must reimplement "ActivateChip()" (see libauth_chip_tests.cpp for examples that use this setup)
class ChipTestingSetup : public TestChain100Setup {

    enum TxStandard { INVALID, NONSTANDARD, STANDARD };

    // A structure to hold all failure reason messages for all tests for all CHIPs
    // chipName: {chipActive: {standardValidation: {ident: "reason"}}}
    using AllChipsReasonsDict = std::map<std::string, std::map<bool, std::map<bool, std::map<std::string, std::string>>>>;

    // libauthReason: {bchnReason: [idents]}
    using Mappings = std::map<std::string, std::map<std::string, std::set<std::string>>>;
    // ident: bchnReason
    using Overrides = std::map<std::string, std::string>;

    // A workspace to help produce the optimized libauth -> bchn failure message lookup table
    struct ReasonsMapTree {
        struct ChipEntries {
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
        std::map<std::string, ChipEntries> entries;
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
    using ReasonsMapLeaf = ReasonsMapTree::ChipEntries::ActivationEntries::StandardnessEntries;

    struct TestVector {
        std::string name;
        std::string description;
        bool chipActive; // Whether or not the chip should be activated for this test
        TxStandard standardness; // Which validation standard this test should meet

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
        };

        std::vector<Test> vec;
    };

    static std::map<std::string, std::vector<TestVector>> allChipsVectors;

    // A lookup table that can be used to find a single expected failure test message given information about the
    // particular Libauth test and the testing context
    static UniValue::Object reasonsLookupTable;
    static std::string LookupReason(const std::string &libauthReason, const std::string &ident,
                                    const std::string &chipName, const bool chipActive, const bool standardValidation,
                                    const UniValue::Object &table=reasonsLookupTable);

    // These dictionaries are populated per-chip by running `RunTestsForChip`
    static AllChipsReasonsDict allLibauthReasons; // All error messages suggested by Libauth
    static AllChipsReasonsDict bchnProducedReasons; // All error messages actually produced

    static void LoadChipsVectors();
    static void RunTestVector(const TestVector &test, const std::string &chipName);

    const bool saved_fRequireStandard;

protected:
    /// Reimplement this in subclasses to turn on/off the chip in question.
    virtual void ActivateChip(bool active) = 0;

public:
    ChipTestingSetup();
    ~ChipTestingSetup() override;

    /// Run all CHIP tests for the `chipName` CHIP
    void RunTestsForChip(const std::string &chipName);

    /// Generate the reasons lookup table and compare it against the currently loaded table. Returns false and outputs
    /// the corrected version to a file if it differs, and includes a human-readable checklist file to help with manual
    /// confirmation
    static bool ProcessReasonsLookupTable();
};
