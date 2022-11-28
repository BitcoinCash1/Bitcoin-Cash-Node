// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdlib>
#include <string>

void SetMiscWarning(const std::string &strWarning);
void SetfLargeWorkForkFound(bool flag);
bool GetfLargeWorkForkFound();
void SetfLargeWorkInvalidChainFound(bool flag);
/**
 * Format a string that describes several potential problems detected by the
 * core.
 * strFor can have three values:
 * - "statusbar": get all warnings
 * - "gui": get all warnings, translated (where possible) for GUI
 * This function only returns the highest priority warning of the set selected
 * by strFor.
 */
std::string GetWarnings(const std::string &strFor);
