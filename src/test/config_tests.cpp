// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <config.h>

#include <chainparams.h>
#include <consensus/consensus.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(config_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(max_block_size) {
    GlobalConfig config;

    // Too small.
    BOOST_CHECK(!config.SetExcessiveBlockSize(0));
    BOOST_CHECK(!config.SetExcessiveBlockSize(12345));
    BOOST_CHECK(!config.SetExcessiveBlockSize(LEGACY_MAX_BLOCK_SIZE - 1));
    BOOST_CHECK(!config.SetExcessiveBlockSize(LEGACY_MAX_BLOCK_SIZE));

    // LEGACY_MAX_BLOCK_SIZE + 1
    BOOST_CHECK(config.SetExcessiveBlockSize(LEGACY_MAX_BLOCK_SIZE + 1));
    BOOST_CHECK_EQUAL(config.GetExcessiveBlockSize(), LEGACY_MAX_BLOCK_SIZE + 1);

    // 2MB
    BOOST_CHECK(config.SetExcessiveBlockSize(2 * ONE_MEGABYTE));
    BOOST_CHECK_EQUAL(config.GetExcessiveBlockSize(), 2 * ONE_MEGABYTE);

    // 8MB
    BOOST_CHECK(config.SetExcessiveBlockSize(8 * ONE_MEGABYTE));
    BOOST_CHECK_EQUAL(config.GetExcessiveBlockSize(), 8 * ONE_MEGABYTE);

    // Invalid size keep config.
    BOOST_CHECK(!config.SetExcessiveBlockSize(54321));
    BOOST_CHECK_EQUAL(config.GetExcessiveBlockSize(), 8 * ONE_MEGABYTE);

    // Setting it back down
    BOOST_CHECK(config.SetExcessiveBlockSize(7 * ONE_MEGABYTE));
    BOOST_CHECK_EQUAL(config.GetExcessiveBlockSize(), 7 * ONE_MEGABYTE);
    BOOST_CHECK(config.SetExcessiveBlockSize(ONE_MEGABYTE + 1));
    BOOST_CHECK_EQUAL(config.GetExcessiveBlockSize(), ONE_MEGABYTE + 1);
}

BOOST_AUTO_TEST_CASE(chain_params) {
    GlobalConfig config;

    // Global config is consistent with params.
    SelectParams(CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());

    SelectParams(CBaseChainParams::TESTNET);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());

    SelectParams(CBaseChainParams::TESTNET4);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());

    SelectParams(CBaseChainParams::REGTEST);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());

    SelectParams(CBaseChainParams::SCALENET);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());

    SelectParams(CBaseChainParams::CHIPNET);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());
}

BOOST_AUTO_TEST_CASE(generated_block_size_percent) {
    GlobalConfig config;

    // Default constructed should be at the default consensus blocksize
    BOOST_CHECK_EQUAL(config.GetExcessiveBlockSize(), DEFAULT_EXCESSIVE_BLOCK_SIZE);

    // Default to equal the max block size.
    BOOST_CHECK_EQUAL(config.GetExcessiveBlockSize(), config.GetGeneratedBlockSize());

    // Out of range
    BOOST_CHECK(!config.SetGeneratedBlockSizePercent(-0.01));
    BOOST_CHECK_EQUAL(config.GetGeneratedBlockSize(), config.GetExcessiveBlockSize());
    BOOST_CHECK(!config.SetGeneratedBlockSizePercent(100.1));
    BOOST_CHECK_EQUAL(config.GetGeneratedBlockSize(), config.GetExcessiveBlockSize());

    BOOST_CHECK(config.SetGeneratedBlockSizePercent(0.0));
    BOOST_CHECK_EQUAL(config.GetGeneratedBlockSize(), 0);

    BOOST_CHECK(config.SetGeneratedBlockSizePercent(100.0));
    BOOST_CHECK_EQUAL(config.GetGeneratedBlockSize(), config.GetExcessiveBlockSize());

    // try various percentages and they should be what we expect
    for (double percent = 0.0; percent <= 100.0; percent += 0.1) {
        const uint64_t expected = config.GetExcessiveBlockSize() * (percent / 100.0);
        BOOST_CHECK(config.SetGeneratedBlockSizePercent(percent));
        BOOST_CHECK_EQUAL(config.GetGeneratedBlockSize(), expected);
    }
}

BOOST_AUTO_TEST_SUITE_END()
