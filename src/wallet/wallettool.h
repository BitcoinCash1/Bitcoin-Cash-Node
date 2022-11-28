// Copyright (c) 2016-2018 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <script/ismine.h>
#include <wallet/wallet.h>

namespace WalletTool {

std::shared_ptr<CWallet> CreateWallet(const std::string &name,
                                      const fs::path &path);
std::shared_ptr<CWallet> LoadWallet(const std::string &name,
                                    const fs::path &path);
void WalletShowInfo(CWallet *wallet_instance);
bool ExecuteWalletToolFunc(const std::string &command, const std::string &file);

} // namespace WalletTool
