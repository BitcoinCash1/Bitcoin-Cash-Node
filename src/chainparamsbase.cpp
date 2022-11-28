// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparamsbase.h>

#include <tinyformat.h>
#include <util/system.h>

#include <cassert>
#include <memory>

const std::string CBaseChainParams::MAIN = "main";
const std::string CBaseChainParams::TESTNET = "test";
const std::string CBaseChainParams::TESTNET4 = "test4";
const std::string CBaseChainParams::SCALENET = "scale";
const std::string CBaseChainParams::CHIPNET = "chip";
const std::string CBaseChainParams::REGTEST = "regtest";

void SetupChainParamsBaseOptions() {
    gArgs.AddArg("-regtest",
                 "Enter regression test mode, which uses a special chain in "
                 "which blocks can be solved instantly. This is intended for "
                 "regression testing tools and app development.",
                 ArgsManager::ALLOW_ANY, OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-testnet", "Use the test chain", ArgsManager::ALLOW_ANY,
                 OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-testnet4", "Use the test4 chain", ArgsManager::ALLOW_ANY,
                 OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-scalenet", "Use the scaling test chain", ArgsManager::ALLOW_ANY,
                 OptionsCategory::CHAINPARAMS);
    gArgs.AddArg("-chipnet", "Use the upcoming upgrade activation chain", ArgsManager::ALLOW_ANY,
                 OptionsCategory::CHAINPARAMS);
}

static std::unique_ptr<CBaseChainParams> globalChainBaseParams;

const CBaseChainParams &BaseParams() {
    assert(globalChainBaseParams);
    return *globalChainBaseParams;
}

/**
 * Port numbers for incoming Tor connections (8334, 18334, 28334, 38334, 18445) have been chosen arbitrarily to keep
 * ranges of used ports tight.
 */
std::unique_ptr<CBaseChainParams> CreateBaseChainParams(const std::string &chain) {
    if (chain == CBaseChainParams::MAIN) {
        return std::make_unique<CBaseChainParams>("", 8332, 8334);
    }

    if (chain == CBaseChainParams::TESTNET) {
        return std::make_unique<CBaseChainParams>("testnet3", 18332, 18334);
    }

    if (chain == CBaseChainParams::TESTNET4) {
        return std::make_unique<CBaseChainParams>("testnet4", 28332, 28334);
    }

    if (chain == CBaseChainParams::SCALENET) {
        return std::make_unique<CBaseChainParams>("scalenet", 38332, 38334);
    }

    if (chain == CBaseChainParams::CHIPNET) {
        return std::make_unique<CBaseChainParams>("chipnet", 48332, 48334);
    }

    if (chain == CBaseChainParams::REGTEST) {
        return std::make_unique<CBaseChainParams>("regtest", 18443, 18445);
    }

    throw std::runtime_error(
        strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectBaseParams(const std::string &chain) {
    globalChainBaseParams = CreateBaseChainParams(chain);
    gArgs.SelectConfigNetwork(chain);
}
