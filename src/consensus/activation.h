// Copyright (c) 2018-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>

class CBlockIndex;

namespace Consensus {
struct Params;
}

/** Check if UAHF has activated. */
bool IsUAHFenabled(const Consensus::Params &params,
                   const CBlockIndex *pindexPrev);

/** Check if DAA HF has activated. */
bool IsDAAEnabled(const Consensus::Params &params,
                  const CBlockIndex *pindexPrev);

/** Check if Nov 15, 2018 HF has activated using block height. */
bool IsMagneticAnomalyEnabled(const Consensus::Params &params, int32_t nHeight);
/** Check if Nov 15, 2018 HF has activated using previous block index. */
bool IsMagneticAnomalyEnabled(const Consensus::Params &params,
                              const CBlockIndex *pindexPrev);

/** Check if Nov 15th, 2019 protocol upgrade has activated. */
bool IsGravitonEnabled(const Consensus::Params &params,
                       const CBlockIndex *pindexPrev);

/** Check if May 15th, 2020 protocol upgrade has activated. */
bool IsPhononEnabled(const Consensus::Params &params,
                     const CBlockIndex *pindexPrev);

/** Check if November 15th, 2020 protocol upgrade has activated. */
bool IsAxionEnabled(const Consensus::Params &params,
                    const CBlockIndex *pindexPrev);

/** Note: May 15th, 2021 protocol upgrade was relay-only, and has no on-chain rules.
 *  The function "IsTachyonEnabled" that used to live here has been removed. */

/** Check if May 15th, 2022 protocol upgrade has activated. */
bool IsUpgrade8Enabled(const Consensus::Params &params, const CBlockIndex *pindexPrev);

/** Check if May 15th, 2023 protocol upgrade has activated. */
bool IsUpgrade9Enabled(const Consensus::Params &params, const int64_t nMedianTimePast);
bool IsUpgrade9Enabled(const Consensus::Params &params, const CBlockIndex *pindexPrev);

/** Check if May 15th, 2024 protocol upgrade has activated. */
bool IsUpgrade10Enabled(const Consensus::Params &params, const CBlockIndex *pindexPrev);
