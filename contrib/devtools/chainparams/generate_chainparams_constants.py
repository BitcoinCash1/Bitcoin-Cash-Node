#!/usr/bin/env python3
# Copyright (c) 2019-2022 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''
Script to generate list of chainparams constants (ie. assumevalid and
minimum chainwork).

This script expects a text file for each chain in the directory that is passed
as an argument:

    chainparams_main.txt
    chainparams_test.txt
    chainparams_testnet4.txt
    chainparams_scalenet.txt
    chainparams_chipnet.txt

These files must consist of lines in the format

    <assumevalid hash>
    <minimum chainwork>

The outputted constants should be pasted into `src/chainparamsconstants.h`.
'''

import sys
import os


def process_constants(indir, file_name):
    with open(os.path.join(indir, file_name), 'r', encoding="utf8") as f:
        constants = f.readlines()

    # Ensure only two lines are read from the file.
    assert len(constants) == 2

    return [line.rstrip() for line in constants]


def main():
    if len(sys.argv) != 2:
        print('Usage: {} <dir_with_chainparams_txt>'.format(
            sys.argv[0]), file=sys.stderr)
        sys.exit(1)

    indir = sys.argv[1]

    print('''\
#pragma once
/**
 * Chain params constants for each tracked chain.
 * @{} by contrib/devtools/chainparams/generate_chainparams_constants.py
 */

#include <primitives/blockhash.h>
#include <uint256.h>

namespace ChainParamsConstants {{
    const BlockHash MAINNET_DEFAULT_ASSUME_VALID = BlockHash::fromHex("{}");
    const uint256 MAINNET_MINIMUM_CHAIN_WORK = uint256S("{}");

    const BlockHash TESTNET_DEFAULT_ASSUME_VALID = BlockHash::fromHex("{}");
    const uint256 TESTNET_MINIMUM_CHAIN_WORK = uint256S("{}");

    const BlockHash TESTNET4_DEFAULT_ASSUME_VALID = BlockHash::fromHex("{}");
    const uint256 TESTNET4_MINIMUM_CHAIN_WORK = uint256S("{}");

    // Scalenet re-organizes above height 10,000 - use block 9,999 hash here.
    const BlockHash SCALENET_DEFAULT_ASSUME_VALID = BlockHash::fromHex("{}");
    const uint256 SCALENET_MINIMUM_CHAIN_WORK = uint256S("{}");

    const BlockHash CHIPNET_DEFAULT_ASSUME_VALID = BlockHash::fromHex("{}");
    const uint256 CHIPNET_MINIMUM_CHAIN_WORK = uint256S("{}");
}} // namespace ChainParamsConstants\
'''.format(
        # 'generated' is split out so this file is not identified as generated.
        "generated",
        *process_constants(indir, 'chainparams_main.txt'),
        *process_constants(indir, 'chainparams_test.txt'),
        *process_constants(indir, 'chainparams_testnet4.txt'),
        *process_constants(indir, 'chainparams_scalenet.txt'),
        *process_constants(indir, 'chainparams_chipnet.txt'))
    )


if __name__ == '__main__':
    main()
