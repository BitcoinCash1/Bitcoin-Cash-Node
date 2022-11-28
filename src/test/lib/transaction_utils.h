// Copyright (c) 2019 The Bitcoin Core developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <primitives/transaction.h>

// create crediting transaction
// [1 coinbase input => 1 output with given scriptPubkey and value]
CMutableTransaction BuildCreditingTransaction(const CScript &scriptPubKey,
                                              const Amount nValue);

// create spending transaction
// [1 input with referenced transaction outpoint, scriptSig =>
//  1 output with empty scriptPubKey, full value of referenced transaction]
CMutableTransaction BuildSpendingTransaction(const CScript &scriptSig,
                                             const CTransaction &txCredit);
