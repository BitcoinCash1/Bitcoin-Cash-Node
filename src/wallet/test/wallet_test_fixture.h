// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <interfaces/chain.h>
#include <interfaces/wallet.h>
#include <wallet/wallet.h>

#include <test/setup_common.h>

#include <memory>

/**
 * Testing setup and teardown for wallet.
 */
struct WalletTestingSetup : public TestingSetup {
    explicit WalletTestingSetup(
        const std::string &chainName = CBaseChainParams::MAIN);
    ~WalletTestingSetup();

    std::unique_ptr<interfaces::Chain> m_chain = interfaces::MakeChain();
    CWallet m_wallet;
};
