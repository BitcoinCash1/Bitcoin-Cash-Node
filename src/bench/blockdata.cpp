// Copyright (c) 2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/blockdata.h>

#include <bench/data.h>
#include <chain.h>
#include <chainparams.h>
#include <clientversion.h>
#include <node/blockstorage.h>
#include <primitives/block.h>
#include <primitives/blockhash.h>
#include <streams.h>
#include <undo.h>
#include <util/system.h>
#include <validation.h>

#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

BlockData::BlockData(int blockHeight) {
    std::function<const std::vector<uint8_t>()> GetBlock;
    std::function<const std::vector<uint8_t>()> GetCoinsSpent;
    if (blockHeight == 413567) {
        GetBlock = benchmark::data::Get_block413567;
        GetCoinsSpent = benchmark::data::Get_coins_spent_413567;
    } else if (blockHeight == 556034) {
        GetBlock = benchmark::data::Get_block556034;
        GetCoinsSpent = benchmark::data::Get_coins_spent_556034;
    } else {
        throw std::runtime_error("Unknown block height in BlockData::BlockData(). Expected one of: 413567, 556034"); 
    }
    assert(bool(GetBlock) && bool(GetCoinsSpent));
    const std::vector<uint8_t> &data = GetBlock();
    const std::vector<uint8_t> &coinsSpent = GetCoinsSpent();
    const CChainParams &chainParams = Params();

    // Fetch the main block data
    VectorReader stream(SER_NETWORK, PROTOCOL_VERSION, data, 0);
    stream >> block;
    blockHash = block.GetHash();
    blockIndex.phashBlock = &blockHash;
    blockIndex.nBits = block.nBits;
    blockIndex.nHeight = blockHeight;

    // Create a blockindex for the previous block
    prevBlockHash = BlockHash();
    prevBlockIndex.phashBlock = &prevBlockHash;
    blockIndex.pprev = &prevBlockIndex;

    // Create undo data for the block
    CBlockUndo blockundo;
    blockundo.vtxundo.resize(block.vtx.size() - 1);
    CCoinsView dummy;
    CCoinsViewCache coinsCache(&dummy);
    std::map<COutPoint, Coin> coinsMap;
    VectorReader(SER_NETWORK, PROTOCOL_VERSION, coinsSpent, 0) >> coinsMap;
    for (const auto & [out, coin] : coinsMap) {
        coinsCache.AddCoin(out, coin, false);
    }
    size_t txIndex = 0;
    for (const auto &ptx : block.vtx) {
        const CTransaction &tx = *ptx;
        if (tx.IsCoinBase()) {
            continue;
        }
        SpendCoins(coinsCache, tx, blockundo.vtxundo.at(txIndex), blockIndex.nHeight);
        ++txIndex;
    }

    // Write the undo data to disk
    const fs::path blocksDir = GetBlocksDir();
    FlatFileSeq fileSeq = FlatFileSeq(blocksDir, "rev", UNDOFILE_CHUNK_SIZE);
    FlatFilePos pos(0, 0);
    const int maxBlockFiles = 100000;
    for (pos.nFile = 0; pos.nFile < maxBlockFiles; ++pos.nFile) {
        // Find the first non-existent numbered .rev file
        CAutoFile afile(fileSeq.Open(pos, true), SER_DISK, CLIENT_VERSION);
        if (afile.IsNull()) break;
    }
    if (pos.nFile >= maxBlockFiles) {
        throw std::runtime_error("Too many undo data files!");
    }
    unsigned int nAddSize = ::GetSerializeSize(blockundo, CLIENT_VERSION) + 40;
    bool out_of_space;
    fileSeq.Allocate(pos, nAddSize, out_of_space);
    if (out_of_space) {
        throw std::runtime_error("Disk space is low!");
    }
    {
        CAutoFile fileout(fileSeq.Open(pos, false), SER_DISK, CLIENT_VERSION);
        unsigned int nSize = GetSerializeSize(blockundo, fileout.GetVersion());
        fileout << chainParams.DiskMagic() << nSize;
        const long filePos = std::ftell(fileout.Get());
        assert(filePos >= 0L && filePos <= static_cast<int64_t>(std::numeric_limits<unsigned int>::max()));
        pos.nPos = static_cast<unsigned int>(filePos);
        fileout << blockundo;
        CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
        hasher << blockIndex.pprev->GetBlockHash();
        hasher << blockundo;
        fileout << hasher.GetHash();
    }

    // Update blockindex with the new undo info
    blockIndex.nFile = pos.nFile;
    blockIndex.nUndoPos = pos.nPos;
    blockIndex.nStatus = blockIndex.nStatus.withUndo();
}

BlockData::~BlockData() {
    // Remove the temporary undo data file
    FlatFilePos pos(blockIndex.nFile, blockIndex.nUndoPos);
    fs::path filePath = FlatFileSeq(GetBlocksDir(), "rev", UNDOFILE_CHUNK_SIZE).FileName(pos);
    fs::remove(filePath);
}
