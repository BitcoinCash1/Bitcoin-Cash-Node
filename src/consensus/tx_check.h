// Copyright (c) 2017-2018 The Bitcoin Core developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

/**
 * Context-independent transaction checking code that can be called outside the
 * bitcoin server and doesn't depend on chain or mempool state. Transaction
 * verification code that does call server functions or depend on server state
 * belongs in tx_verify.h/cpp instead.
 */

class CTransaction;
class CValidationState;

/**
 * Context-independent validity checks for coinbase and non-coinbase
 * transactions.
 */

bool CheckRegularTransaction(const CTransaction &tx, CValidationState &state);
bool CheckCoinbase(const CTransaction &tx, CValidationState &state);
