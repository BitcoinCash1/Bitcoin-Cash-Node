// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <bench/data.h>
#include <streams.h>
#include <txmempool.h>
#include <version.h>       // For PROTOCOL_VERSION

static void DisconnectPoolAddForBlock(benchmark::State &state) {
    const auto &data = benchmark::data::Get_block877227();
    VectorReader stream(SER_NETWORK, PROTOCOL_VERSION, data, 0);

    CBlock block;
    stream >> block;

    BENCHMARK_LOOP {
        DisconnectedBlockTransactions pool;
        pool.addForBlock(block.vtx);
        pool.clear(); // class invariant.. need to clear pool before destruction
    }
}

BENCHMARK(DisconnectPoolAddForBlock, 5)
