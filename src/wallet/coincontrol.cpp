// Copyright (c) 2018 The Bitcoin Core developers
// Copyright (c) 2019-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/coincontrol.h>

#include <util/system.h>

void CCoinControl::SetNull() {
    destChange = CNoDestination();
    m_change_type.reset();
    fAllowOtherInputs = false;
    fAllowWatchOnly = false;
    m_avoid_partial_spends =
        gArgs.GetBoolArg("-avoidpartialspends", DEFAULT_AVOIDPARTIALSPENDS);
    setSelected.clear();
    m_feerate.reset();
    fOverrideFeeRate = false;
    m_confirm_target.reset();
    m_allow_tokens = false;
    m_tokens_only = false;
}
