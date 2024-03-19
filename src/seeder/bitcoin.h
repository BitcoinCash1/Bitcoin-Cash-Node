// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <chainparams.h>
#include <compat.h>
#include <protocol.h>
#include <streams.h>

#include <string>
#include <vector>

static inline unsigned short GetDefaultPort() {
    return Params().GetDefaultPort();
}

// Returns a pointer to the latest checkpoint, or nullptr if the selected network lacks checkpoints
static inline const std::pair<const int, BlockHash> *GetCheckpoint() {
    const auto &mapCheckpoints = Params().Checkpoints().mapCheckpoints;
    if (!mapCheckpoints.empty()) {
        return &*mapCheckpoints.rbegin();
    }
    return nullptr;
}

// If we have a checkpoint, returns its height, otherwise returns 0.
static inline int GetRequireHeight() {
    if (auto *pair = GetCheckpoint()) {
        return pair->first;
    }
    return 0;
}

// After the 1000th addr, the seeder will only add one more address per addr
// message.
static const unsigned int ADDR_SOFT_CAP = 1000;

enum class PeerMessagingState {
    AwaitingMessages,
    Finished,
};

class CSeederNode {
protected:
    SOCKET sock;
    CDataStream vSend;
    CDataStream vRecv;
    uint32_t nHeaderStart;
    uint32_t nMessageStart;
    int nVersion;
    std::string strSubVer;
    int nStartingHeight;
    std::vector<CAddress> *vAddr;
    int ban;
    int64_t doneAfter;
    CAddress you;
    bool checkpointVerified;
    bool needAddrReply = false;

    int GetTimeout() const { return you.IsTor() ? 120 : 30; }

    void BeginMessage(const char *pszCommand);

    void AbortMessage();

    void EndMessage();

    void Send();

    void PushVersion();

    bool ProcessMessages();

    PeerMessagingState ProcessMessage(const std::string &msg_type,
                                      CDataStream &recv);

public:
    CSeederNode(const CService &ip, std::vector<CAddress> *vAddrIn);

    bool Run();

    int GetBan() const { return ban; }

    int GetClientVersion() const { return nVersion; }

    const std::string & GetClientSubVersion() const { return strSubVer; }

    int GetStartingHeight() const { return nStartingHeight; }

    ServiceFlags GetServices() const { return you.nServices; }

    bool IsCheckpointVerified() const { return checkpointVerified; }
};

bool TestNode(const CService &cip, int &ban, int &client, std::string &clientSV,
              int &blocks, std::vector<CAddress> *vAddr, ServiceFlags &services, bool &checkpointVerified);
