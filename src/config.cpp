// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <config.h>

#include <chainparams.h>
#include <consensus/consensus.h> // DEFAULT_EXCESSIVE_BLOCK_SIZE, MAX_EXCESSIVE_BLOCK_SIZE
#include <policy/policy.h> // MAX_INV_BROADCAST_*

#include <algorithm>

GlobalConfig::GlobalConfig()
    : useCashAddr(DEFAULT_USE_CASHADDR), gbtCheckValidity(DEFAULT_GBT_CHECK_VALIDITY),
      allowUnconnectedMining(DEFAULT_ALLOW_UNCONNECTED_MINING),
      nExcessiveBlockSize(DEFAULT_EXCESSIVE_BLOCK_SIZE),
      // NB: The generated block size is normally set in init.cpp to use chain-specific
      //     defaults which are often smaller than the DEFAULT_EXCESSIVE_BLOCK_SIZE.
      varGeneratedBlockSizeParam(DEFAULT_EXCESSIVE_BLOCK_SIZE),
      nMaxMemPoolSize(DEFAULT_EXCESSIVE_BLOCK_SIZE * DEFAULT_MAX_MEMPOOL_SIZE_PER_MB) {}

bool GlobalConfig::SetExcessiveBlockSize(uint64_t blockSize) {
    // Do not allow maxBlockSize to be set below historic 1MB limit
    // It cannot be equal either because of the "must be big" UAHF rule.
    if (blockSize <= LEGACY_MAX_BLOCK_SIZE) {
        return false;
    }

    // We limit the excessive block size parameter to what the machine can physically address on 32-bit (2GB)
    if (blockSize > MAX_EXCESSIVE_BLOCK_SIZE) {
        return false;
    }

    nExcessiveBlockSize = blockSize;

    return true;
}

uint64_t GlobalConfig::GetExcessiveBlockSize() const {
    return nExcessiveBlockSize;
}

bool GlobalConfig::SetGeneratedBlockSizeBytes(uint64_t blockSize) {
    // Do not allow generated blocks to exceed the size of blocks we accept.
    if (blockSize > GetExcessiveBlockSize()) {
        return false;
    }

    varGeneratedBlockSizeParam = blockSize;
    return true;
}

bool GlobalConfig::SetGeneratedBlockSizePercent(double percent) {
    if (percent < 0.0 || percent > 100.0) {
        return false;
    }

    varGeneratedBlockSizeParam = percent;
    return true;
}

bool GlobalConfig::SetInvBroadcastRate(uint64_t rate) {
    if (rate > MAX_INV_BROADCAST_RATE) return false;
    nInvBroadcastRate = rate;
    return true;
}

bool GlobalConfig::SetInvBroadcastInterval(uint64_t interval) {
    if (interval > MAX_INV_BROADCAST_INTERVAL) return false;
    nInvBroadcastInterval = interval;
    return true;
}

uint64_t GlobalConfig::GetGeneratedBlockSize() const {
    uint64_t blockSize;

    struct Visitor {
        uint64_t &blockSize;
        const GlobalConfig &self;

        void operator()(uint64_t val) { blockSize = val; }
        void operator()(double percent) {
            blockSize = static_cast<uint64_t>(self.nExcessiveBlockSize * (percent / 100.0));
        }
    };

    std::visit(Visitor{blockSize, *this}, varGeneratedBlockSizeParam);

    // Maintain invariant: ensure that blockSize <= nExcessiveBlockSize
    blockSize = std::min(nExcessiveBlockSize, blockSize);

    return blockSize;
}

const CChainParams &GlobalConfig::GetChainParams() const {
    return Params();
}

static GlobalConfig gConfig;

const Config &GetConfig() {
    return gConfig;
}

Config &GetMutableConfig() {
    return gConfig;
}

void GlobalConfig::SetCashAddrEncoding(bool c) {
    useCashAddr = c;
}
bool GlobalConfig::UseCashAddrEncoding() const {
    return useCashAddr;
}

DummyConfig::DummyConfig()
    : chainParams(CreateChainParams(CBaseChainParams::REGTEST)) {}

DummyConfig::DummyConfig(const std::string &net)
    : chainParams(CreateChainParams(net)) {}

DummyConfig::DummyConfig(std::unique_ptr<CChainParams> chainParamsIn)
    : chainParams(std::move(chainParamsIn)) {}

void DummyConfig::SetChainParams(const std::string &net) {
    chainParams = CreateChainParams(net);
}

void GlobalConfig::SetExcessUTXOCharge(Amount fee) {
    excessUTXOCharge = fee;
}

Amount GlobalConfig::GetExcessUTXOCharge() const {
    return excessUTXOCharge;
}
