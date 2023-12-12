// Copyright (c) 2022-2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <config.h>
#include <consensus/activation.h>
#include <util/system.h>
#include <validation.h>

#include <test/chip_testing_setup.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstdlib>
#include <map>
#include <string>

namespace {

/// Test fixture that:
/// - tracks if we set the upgrade 9 activation height override, and resets it on test end
struct TokenTransactionTestingSetup : ChipTestingSetup {
    std::optional<int32_t> upgrade9OriginalOverride;
    bool touchedUpgrade9{};

    TokenTransactionTestingSetup() {
        upgrade9OriginalOverride = g_Upgrade9HeightOverride;
    }

    ~TokenTransactionTestingSetup() override {
        if (touchedUpgrade9) {
            g_Upgrade9HeightOverride = upgrade9OriginalOverride;
        }
    }

    /// Activates or deactivates upgrade 9 by setting the activation time in the past or future respectively
    void SetUpgrade9Active(bool active) {
        const auto currentHeight = []{
            LOCK(cs_main);
            return ::ChainActive().Tip()->nHeight;
        }();
        auto activationHeight = active ? currentHeight - 1 : currentHeight + 1;
        g_Upgrade9HeightOverride = activationHeight;
        touchedUpgrade9 = true;
    }

protected:
    /// Concrete implementation of abstract base pure virtual method
    void ActivateChip(bool active) override { SetUpgrade9Active(active); }
};

} // namespace


BOOST_AUTO_TEST_SUITE(token_transaction_tests)

BOOST_FIXTURE_TEST_CASE(test_chips, TokenTransactionTestingSetup) {
    RunTestsForChip("cashtokens");
}

BOOST_AUTO_TEST_SUITE_END()
