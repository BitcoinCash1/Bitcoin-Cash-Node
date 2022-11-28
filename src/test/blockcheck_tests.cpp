// Copyright (c) 2013-2015 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <clientversion.h>
#include <config.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <span.h>
#include <streams.h>
#include <validation.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(blockcheck_tests, BasicTestingSetup)

static void RunCheckOnBlockImpl(const GlobalConfig &config, const CBlock &block,
                                CValidationState &state, bool expected) {
    block.fChecked = false;
    bool fValid = CheckBlock(
        block, state, config.GetChainParams().GetConsensus(),
        BlockValidationOptions(config).withCheckPoW(false).withCheckMerkleRoot(
            false));

    BOOST_CHECK_EQUAL(fValid, expected);
    BOOST_CHECK_EQUAL(fValid, state.IsValid());
}

static void RunCheckOnBlock(const GlobalConfig &config, const CBlock &block) {
    CValidationState state;
    RunCheckOnBlockImpl(config, block, state, true);
}

static void RunCheckOnBlock(const GlobalConfig &config, const CBlock &block,
                            const std::string &reason) {
    CValidationState state;
    RunCheckOnBlockImpl(config, block, state, false);

    BOOST_CHECK_EQUAL(state.GetRejectCode(), REJECT_INVALID);
    BOOST_CHECK_EQUAL(state.GetRejectReason(), reason);
}

static COutPoint InsecureRandOutPoint() {
    return COutPoint(TxId(InsecureRand256()), 0);
}

BOOST_AUTO_TEST_CASE(blockfail) {
    SelectParams(CBaseChainParams::MAIN);

    // Set max blocksize to default in case other tests left it dirty
    GlobalConfig config;
    config.SetExcessiveBlockSize(DEFAULT_EXCESSIVE_BLOCK_SIZE);

    CBlock block;
    RunCheckOnBlock(config, block, "bad-cb-missing");

    CMutableTransaction tx;

    // Coinbase only.
    tx.vin.resize(1);
    tx.vin[0].scriptSig.resize(10);
    tx.vout.resize(1);
    tx.vout[0].nValue = 42 * SATOSHI;
    const CTransaction coinbaseTx(tx);

    block.vtx.resize(1);
    block.vtx[0] = MakeTransactionRef(tx);
    RunCheckOnBlock(config, block);

    // No coinbase
    tx.vin[0].prevout = InsecureRandOutPoint();
    block.vtx[0] = MakeTransactionRef(tx);

    RunCheckOnBlock(config, block, "bad-cb-missing");

    // Invalid coinbase
    tx = CMutableTransaction(coinbaseTx);
    tx.vin[0].scriptSig.resize(0);
    block.vtx[0] = MakeTransactionRef(tx);

    RunCheckOnBlock(config, block, "bad-cb-length");

    // Oversize block.
    tx = CMutableTransaction(coinbaseTx);
    block.vtx[0] = MakeTransactionRef(tx);
    auto txSize = ::GetSerializeSize(tx, PROTOCOL_VERSION);
    auto maxTxCount = ((DEFAULT_EXCESSIVE_BLOCK_SIZE - 1) / txSize) - 1;

    for (size_t i = 1; i < maxTxCount; i++) {
        tx.vin[0].prevout = InsecureRandOutPoint();
        block.vtx.push_back(MakeTransactionRef(tx));
    }

    // Check that at this point, we still accept the block.
    RunCheckOnBlock(config, block);

    // But reject it with one more transaction as it goes over the maximum
    // allowed block size.
    tx.vin[0].prevout = InsecureRandOutPoint();
    block.vtx.push_back(MakeTransactionRef(tx));
    RunCheckOnBlock(config, block, "bad-blk-length");
}

BOOST_AUTO_TEST_CASE(blockserialization) {
    // While we have different serialization schemes for disk and network serialization,
    // for blocks in particular we want all schemes to produce the exact same data.
    // This test case serves the purpose of checking this. If ever this test fails, then
    // the fast ReadRawBlockFromDisk() function may not be used as an optimization for
    // serving blocks on the p2p network. (See: validation.cpp and net_processing.cpp).

    GlobalConfig config;

    CBlock block;
    CMutableTransaction tx;


    // Lets produce a block with a coinbase and two transactions.
    // Coinbase.
    tx.vin.resize(1);
    tx.vin[0].scriptSig.resize(10);
    tx.vout.resize(1);
    tx.vout[0].nValue = 42 * SATOSHI;
    const CTransaction coinbaseTx(tx);

    block.vtx.resize(1);
    tx = CMutableTransaction(coinbaseTx);
    block.vtx[0] = MakeTransactionRef(tx);
    size_t maxTxCount = 2;

    for (size_t i = 1; i < maxTxCount; i++) {
        tx.vin[0].prevout = InsecureRandOutPoint();
        block.vtx.push_back(MakeTransactionRef(tx));
    }

    // Check block validity.
    RunCheckOnBlock(config, block);

    // Check that block is serialized to the same binary data when using SER_NETWORK and SER_DISK
    CDataStream networkBlockData(SER_NETWORK, PROTOCOL_VERSION);
    networkBlockData << block;

    CDataStream diskBlockData(SER_DISK, PROTOCOL_VERSION);
    diskBlockData << block;

    BOOST_REQUIRE(Span{networkBlockData} == Span{diskBlockData});

    // Check that blocks deserialized from binary data when using SER_NETWORK and SER_DISK are same objects
    CBlock networkBlock;
    CBlock diskBlock;

    std::vector<uint8_t> data(diskBlockData.begin(), diskBlockData.end());
    CDataStream networkBlockDS(data, SER_NETWORK, PROTOCOL_VERSION);
    networkBlockDS >> networkBlock;

    CDataStream diskBlockDS(data, SER_DISK, PROTOCOL_VERSION);
    diskBlockDS >> diskBlock;

    // Deeply check that both blocks are equal
    BOOST_REQUIRE(networkBlock.GetHash() == diskBlock.GetHash());
    BOOST_REQUIRE(networkBlock.vtx.size() == diskBlock.vtx.size());
    for (size_t i = 0; i < networkBlock.vtx.size(); ++i) {
        BOOST_REQUIRE(*networkBlock.vtx[i] == *diskBlock.vtx[i]);
    }
    BOOST_REQUIRE_EQUAL(block.ToString(), diskBlock.ToString());
}

BOOST_AUTO_TEST_SUITE_END()
