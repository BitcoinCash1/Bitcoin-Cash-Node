#pragma once
/**
 * Chain params constants for each tracked chain.
 * @generated by contrib/devtools/chainparams/generate_chainparams_constants.py
 */

#include <primitives/blockhash.h>
#include <uint256.h>

namespace ChainParamsConstants {
    const BlockHash MAINNET_DEFAULT_ASSUME_VALID = BlockHash::fromHex("000000000000000003a7669e4401eeaa204becf0f6702e73b488646ceb17de1a");
    const uint256 MAINNET_MINIMUM_CHAIN_WORK = uint256S("000000000000000000000000000000000000000001ba459ca1852b2acb2ac172");

    const BlockHash TESTNET_DEFAULT_ASSUME_VALID = BlockHash::fromHex("000000000237ef45f98f93e45784ea794be6bfcc799aa790775fda11a0d816ad");
    const uint256 TESTNET_MINIMUM_CHAIN_WORK = uint256S("0000000000000000000000000000000000000000000000dbaaa2edc74c8a23c1");

    const BlockHash TESTNET4_DEFAULT_ASSUME_VALID = BlockHash::fromHex("0000000002f47e63a2f5aae7546c20a8ce7826ba037575e3b14394bb646ccb16");
    const uint256 TESTNET4_MINIMUM_CHAIN_WORK = uint256S("00000000000000000000000000000000000000000000000001c80f74b1c7f747");

    // Scalenet re-organizes above height 10,000 - use block 9,999 hash here.
    const BlockHash SCALENET_DEFAULT_ASSUME_VALID = BlockHash::fromHex("000000007fb3362740efd1435aa414f54171993483799782f83c61bc7bf1b1be");
    const uint256 SCALENET_MINIMUM_CHAIN_WORK = uint256S("00000000000000000000000000000000000000000000000003a54dce8032552f");

    const BlockHash CHIPNET_DEFAULT_ASSUME_VALID = BlockHash::fromHex("000000000793d3c9723910cb9d6ddb9b2b236dac6b02bb32ad1c554498c672fe");
    const uint256 CHIPNET_MINIMUM_CHAIN_WORK = uint256S("0000000000000000000000000000000000000000000000000162f2b9f9209ad5");
} // namespace ChainParamsConstants
