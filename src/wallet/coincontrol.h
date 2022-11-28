// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <primitives/transaction.h>
#include <wallet/wallet.h>

#include <optional>

/** Coin Control Features. */
class CCoinControl {
public:
    CTxDestination destChange;
    //! Override the default change type if set, ignored if destChange is set
    std::optional<OutputType> m_change_type;
    //! If false, only safe (confirmed) inputs will be used
    bool m_include_unsafe_inputs = DEFAULT_INCLUDE_UNSAFE_INPUTS;
    //! If false, allows unselected inputs, but requires all selected inputs be
    //! used
    bool fAllowOtherInputs;
    //! Includes watch only addresses which are solvable
    bool fAllowWatchOnly;
    //! Override automatic min/max checks on fee, m_feerate must be set if true
    bool fOverrideFeeRate;
    //! Override the wallet's m_pay_tx_fee if set
    std::optional<CFeeRate> m_feerate;
    //! Override the default confirmation target if set
    std::optional<unsigned int> m_confirm_target;
    //! Avoid partial use of funds sent to a given address
    bool m_avoid_partial_spends;
    //! Allow spending of coins that have tokens on them
    bool m_allow_tokens;
    //! Only select coins that have tokens on them (requires m_allow_tokens == true)
    bool m_tokens_only;

    CCoinControl() { SetNull(); }

    void SetNull();

    bool HasSelected() const { return (setSelected.size() > 0); }

    bool IsSelected(const COutPoint &output) const {
        return (setSelected.count(output) > 0);
    }

    void Select(const COutPoint &output) { setSelected.insert(output); }

    void UnSelect(const COutPoint &output) { setSelected.erase(output); }

    void UnSelectAll() { setSelected.clear(); }

    void ListSelected(std::vector<COutPoint> &vOutpoints) const {
        vOutpoints.assign(setSelected.begin(), setSelected.end());
    }

private:
    std::set<COutPoint> setSelected;
};
