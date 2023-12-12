// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2017-2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/test/wallet_test_fixture.h>

#include <chainparams.h>
#include <rpc/server.h>
#include <wallet/db.h>
#include <wallet/rpcdump.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>

WalletTestingSetup::WalletTestingSetup(const std::string &chainName)
    : TestingSetup(chainName), m_wallet(Params(), *m_chain, WalletLocation(),
                                        WalletDatabase::CreateMock()) {
    bool fFirstRun;
    m_wallet.LoadWallet(fFirstRun);
    RegisterValidationInterface(&m_wallet);

    RegisterWalletRPCCommands(tableRPC);
    RegisterDumpRPCCommands(tableRPC);
}

WalletTestingSetup::~WalletTestingSetup() {
    // Ensure the TestingSetup scheduler is stopped before we delete our temporary wallet to avoid defunct delivery of
    // signals to a now-deleted wallet.
    StopScheduler();
    UnregisterValidationInterface(&m_wallet);
}
