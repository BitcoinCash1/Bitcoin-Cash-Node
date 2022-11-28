// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/walletdb.h>

#include <chainparams.h>
#include <consensus/tx_check.h>
#include <consensus/validation.h>
#include <fs.h>
#include <key_io.h>
#include <protocol.h>
#include <serialize.h>
#include <sync.h>
#include <util/system.h>
#include <util/time.h>
#include <wallet/wallet.h>

#include <atomic>
#include <string>

//
// WalletBatch
//

bool WalletBatch::WriteName(const CTxDestination &address,
                            const std::string &strName) {
    if (!IsValidDestination(address)) {
        return false;
    }
    return WriteIC(std::make_pair(std::string("name"),
                                  EncodeLegacyAddr(address, Params())),
                   strName);
}

bool WalletBatch::EraseName(const CTxDestination &address) {
    // This should only be used for sending addresses, never for receiving
    // addresses, receiving addresses must always have an address book entry if
    // they're not change return.
    if (!IsValidDestination(address)) {
        return false;
    }
    return EraseIC(std::make_pair(std::string("name"),
                                  EncodeLegacyAddr(address, Params())));
}

bool WalletBatch::WritePurpose(const CTxDestination &address,
                               const std::string &strPurpose) {
    if (!IsValidDestination(address)) {
        return false;
    }
    return WriteIC(std::make_pair(std::string("purpose"),
                                  EncodeLegacyAddr(address, Params())),
                   strPurpose);
}

bool WalletBatch::ErasePurpose(const CTxDestination &address) {
    if (!IsValidDestination(address)) {
        return false;
    }
    return EraseIC(std::make_pair(std::string("purpose"),
                                  EncodeLegacyAddr(address, Params())));
}

bool WalletBatch::WriteTx(const CWalletTx &wtx) {
    return WriteIC(std::make_pair(std::string("tx"), wtx.GetId()), wtx);
}

bool WalletBatch::EraseTx(uint256 hash) {
    return EraseIC(std::make_pair(std::string("tx"), hash));
}

bool WalletBatch::WriteKeyMetadata(const CKeyMetadata &meta,
                                   const CPubKey &pubkey,
                                   const bool overwrite) {
    return WriteIC(std::make_pair(std::string("keymeta"), pubkey), meta,
                   overwrite);
}

bool WalletBatch::WriteKey(const CPubKey &vchPubKey, const CPrivKey &vchPrivKey,
                           const CKeyMetadata &keyMeta) {
    if (!WriteKeyMetadata(keyMeta, vchPubKey, false)) {
        return false;
    }

    // hash pubkey/privkey to accelerate wallet load
    std::vector<uint8_t> vchKey;
    vchKey.reserve(vchPubKey.size() + vchPrivKey.size());
    vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
    vchKey.insert(vchKey.end(), vchPrivKey.begin(), vchPrivKey.end());

    return WriteIC(std::make_pair(std::string("key"), vchPubKey), std::make_pair(vchPrivKey, Hash(vchKey)), false);
}

bool WalletBatch::WriteCryptedKey(const CPubKey &vchPubKey,
                                  const std::vector<uint8_t> &vchCryptedSecret,
                                  const CKeyMetadata &keyMeta) {
    if (!WriteKeyMetadata(keyMeta, vchPubKey, true)) {
        return false;
    }

    if (!WriteIC(std::make_pair(std::string("ckey"), vchPubKey),
                 vchCryptedSecret, false)) {
        return false;
    }
    EraseIC(std::make_pair(std::string("key"), vchPubKey));
    EraseIC(std::make_pair(std::string("wkey"), vchPubKey));
    return true;
}

bool WalletBatch::WriteMasterKey(unsigned int nID,
                                 const CMasterKey &kMasterKey) {
    return WriteIC(std::make_pair(std::string("mkey"), nID), kMasterKey, true);
}

bool WalletBatch::WriteCScript(const uint160 &hash,
                               const CScript &redeemScript) {
    return WriteIC(std::make_pair(std::string("cscript"), hash), redeemScript,
                   false);
}

bool WalletBatch::WriteWatchOnly(const CScript &dest,
                                 const CKeyMetadata &keyMeta) {
    if (!WriteIC(std::make_pair(std::string("watchmeta"), dest), keyMeta)) {
        return false;
    }
    return WriteIC(std::make_pair(std::string("watchs"), dest), '1');
}

bool WalletBatch::EraseWatchOnly(const CScript &dest) {
    if (!EraseIC(std::make_pair(std::string("watchmeta"), dest))) {
        return false;
    }
    return EraseIC(std::make_pair(std::string("watchs"), dest));
}

bool WalletBatch::WriteBestBlock(const CBlockLocator &locator) {
    // Write empty block locator so versions that require a merkle branch
    // automatically rescan
    WriteIC(std::string("bestblock"), CBlockLocator());
    return WriteIC(std::string("bestblock_nomerkle"), locator);
}

bool WalletBatch::ReadBestBlock(CBlockLocator &locator) {
    if (m_batch.Read(std::string("bestblock"), locator) &&
        !locator.vHave.empty()) {
        return true;
    }
    return m_batch.Read(std::string("bestblock_nomerkle"), locator);
}

bool WalletBatch::WriteOrderPosNext(int64_t nOrderPosNext) {
    return WriteIC(std::string("orderposnext"), nOrderPosNext);
}

bool WalletBatch::ReadPool(int64_t nPool, CKeyPool &keypool) {
    return m_batch.Read(std::make_pair(std::string("pool"), nPool), keypool);
}

bool WalletBatch::WritePool(int64_t nPool, const CKeyPool &keypool) {
    return WriteIC(std::make_pair(std::string("pool"), nPool), keypool);
}

bool WalletBatch::ErasePool(int64_t nPool) {
    return EraseIC(std::make_pair(std::string("pool"), nPool));
}

bool WalletBatch::WriteMinVersion(int nVersion) {
    return WriteIC(std::string("minversion"), nVersion);
}

class CWalletScanState {
public:
    unsigned int nKeys{0};
    unsigned int nCKeys{0};
    unsigned int nWatchKeys{0};
    unsigned int nKeyMeta{0};
    unsigned int m_unknown_records{0};
    bool fIsEncrypted{false};
    bool fAnyUnordered{false};
    int nFileVersion{0};
    std::vector<TxId> vWalletUpgrade;

    CWalletScanState() {}
};

static bool ReadKeyValue(CWallet *pwallet, CDataStream &ssKey,
                         CDataStream &ssValue, CWalletScanState &wss,
                         std::string &strType, std::string &strErr)
    EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet) {
    try {
        // Unserialize
        // Taking advantage of the fact that pair serialization is just the two
        // items serialized one after the other.
        ssKey >> strType;
        if (strType == "name") {
            std::string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet
                           ->mapAddressBook[DecodeDestination(
                               strAddress, pwallet->chainParams)]
                           .name;
        } else if (strType == "purpose") {
            std::string strAddress;
            ssKey >> strAddress;
            ssValue >> pwallet
                           ->mapAddressBook[DecodeDestination(
                               strAddress, pwallet->chainParams)]
                           .purpose;
        } else if (strType == "tx") {
            TxId txid;
            ssKey >> txid;
            CWalletTx wtx(nullptr /* pwallet */, MakeTransactionRef());
            ssValue >> wtx;
            CValidationState state;
            bool isValid = wtx.IsCoinBase()
                               ? CheckCoinbase(*wtx.tx, state)
                               : CheckRegularTransaction(*wtx.tx, state);
            if (!isValid || wtx.GetId() != txid) {
                return false;
            }

            // Undo serialize changes in 31600
            if (31404 <= wtx.fTimeReceivedIsTxTime &&
                wtx.fTimeReceivedIsTxTime <= 31703) {
                if (!ssValue.empty()) {
                    char fTmp;
                    char fUnused;
                    std::string unused_string;
                    ssValue >> fTmp >> fUnused >> unused_string;
                    strErr = strprintf("LoadWallet() upgrading tx ver=%d %d %s",
                                       wtx.fTimeReceivedIsTxTime, fTmp,
                                       txid.ToString());
                    wtx.fTimeReceivedIsTxTime = fTmp;
                } else {
                    strErr =
                        strprintf("LoadWallet() repairing tx ver=%d %s",
                                  wtx.fTimeReceivedIsTxTime, txid.ToString());
                    wtx.fTimeReceivedIsTxTime = 0;
                }
                wss.vWalletUpgrade.push_back(txid);
            }

            if (wtx.nOrderPos == -1) {
                wss.fAnyUnordered = true;
            }

            pwallet->LoadToWallet(wtx);
        } else if (strType == "watchs") {
            wss.nWatchKeys++;
            CScript script;
            ssKey >> script;
            char fYes;
            ssValue >> fYes;
            if (fYes == '1') {
                pwallet->LoadWatchOnly(script);
            }
        } else if (strType == "key" || strType == "wkey") {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            if (!vchPubKey.IsValid()) {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }
            CKey key;
            CPrivKey pkey;
            uint256 hash;

            if (strType == "key") {
                wss.nKeys++;
                ssValue >> pkey;
            } else {
                CWalletKey wkey;
                ssValue >> wkey;
                pkey = wkey.vchPrivKey;
            }

            // Old wallets store keys as "key" [pubkey] => [privkey]
            // ... which was slow for wallets with lots of keys, because the
            // public key is re-derived from the private key using EC operations
            // as a checksum. Newer wallets store keys as "key"[pubkey] =>
            // [privkey][hash(pubkey,privkey)], which is much faster while
            // remaining backwards-compatible.
            try {
                ssValue >> hash;
            } catch (...) {
            }

            bool fSkipCheck = false;

            if (!hash.IsNull()) {
                // hash pubkey/privkey to accelerate wallet load
                std::vector<uint8_t> vchKey;
                vchKey.reserve(vchPubKey.size() + pkey.size());
                vchKey.insert(vchKey.end(), vchPubKey.begin(), vchPubKey.end());
                vchKey.insert(vchKey.end(), pkey.begin(), pkey.end());

                if (Hash(vchKey) != hash) {
                    strErr = "Error reading wallet database: CPubKey/CPrivKey "
                             "corrupt";
                    return false;
                }

                fSkipCheck = true;
            }

            if (!key.Load(pkey, vchPubKey, fSkipCheck)) {
                strErr = "Error reading wallet database: CPrivKey corrupt";
                return false;
            }
            if (!pwallet->LoadKey(key, vchPubKey)) {
                strErr = "Error reading wallet database: LoadKey failed";
                return false;
            }
        } else if (strType == "mkey") {
            unsigned int nID;
            ssKey >> nID;
            CMasterKey kMasterKey;
            ssValue >> kMasterKey;
            if (pwallet->mapMasterKeys.count(nID) != 0) {
                strErr = strprintf(
                    "Error reading wallet database: duplicate CMasterKey id %u",
                    nID);
                return false;
            }
            pwallet->mapMasterKeys[nID] = kMasterKey;
            if (pwallet->nMasterKeyMaxID < nID) {
                pwallet->nMasterKeyMaxID = nID;
            }
        } else if (strType == "ckey") {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            if (!vchPubKey.IsValid()) {
                strErr = "Error reading wallet database: CPubKey corrupt";
                return false;
            }
            std::vector<uint8_t> vchPrivKey;
            ssValue >> vchPrivKey;
            wss.nCKeys++;

            if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey)) {
                strErr = "Error reading wallet database: LoadCryptedKey failed";
                return false;
            }
            wss.fIsEncrypted = true;
        } else if (strType == "keymeta") {
            CPubKey vchPubKey;
            ssKey >> vchPubKey;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;
            wss.nKeyMeta++;

            pwallet->LoadKeyMetadata(vchPubKey.GetID(), keyMeta);
        } else if (strType == "watchmeta") {
            CScript script;
            ssKey >> script;
            CKeyMetadata keyMeta;
            ssValue >> keyMeta;
            wss.nKeyMeta++;
            pwallet->LoadScriptMetadata(ScriptID(script, false /* no p2sh_32 in wallet */), keyMeta);
        } else if (strType == "defaultkey") {
            // We don't want or need the default key, but if there is one set,
            // we want to make sure that it is valid so that we can detect
            // corruption
            CPubKey vchPubKey;
            ssValue >> vchPubKey;
            if (!vchPubKey.IsValid()) {
                strErr = "Error reading wallet database: Default Key corrupt";
                return false;
            }
        } else if (strType == "pool") {
            int64_t nIndex;
            ssKey >> nIndex;
            CKeyPool keypool;
            ssValue >> keypool;

            pwallet->LoadKeyPool(nIndex, keypool);
        } else if (strType == "version") {
            ssValue >> wss.nFileVersion;
            if (wss.nFileVersion == 10300) {
                wss.nFileVersion = 300;
            }
        } else if (strType == "cscript") {
            uint160 hash;
            ssKey >> hash;
            CScript script;
            ssValue >> script;
            if (!pwallet->LoadCScript(script)) {
                strErr = "Error reading wallet database: LoadCScript failed";
                return false;
            }
        } else if (strType == "orderposnext") {
            ssValue >> pwallet->nOrderPosNext;
        } else if (strType == "destdata") {
            std::string strAddress, strKey, strValue;
            ssKey >> strAddress;
            ssKey >> strKey;
            ssValue >> strValue;
            pwallet->LoadDestData(
                DecodeDestination(strAddress, pwallet->chainParams), strKey,
                strValue);
        } else if (strType == "hdchain") {
            CHDChain chain;
            ssValue >> chain;
            pwallet->SetHDChain(chain, true);
        } else if (strType == "flags") {
            uint64_t flags;
            ssValue >> flags;
            if (!pwallet->SetWalletFlags(flags, true)) {
                strErr = "Error reading wallet database: Unknown non-tolerable "
                         "wallet flags found";
                return false;
            }
        } else if (strType != "bestblock" && strType != "bestblock_nomerkle" &&
                   strType != "minversion" && strType != "acentry") {
            wss.m_unknown_records++;
        }
    } catch (...) {
        return false;
    }
    return true;
}

bool WalletBatch::IsKeyType(const std::string &strType) {
    return (strType == "key" || strType == "wkey" || strType == "mkey" ||
            strType == "ckey");
}

DBErrors WalletBatch::LoadWallet(CWallet *pwallet) {
    CWalletScanState wss;
    bool fNoncriticalErrors = false;
    DBErrors result = DBErrors::LOAD_OK;

    LOCK(pwallet->cs_wallet);
    try {
        int nMinVersion = 0;
        if (m_batch.Read((std::string) "minversion", nMinVersion)) {
            if (nMinVersion > FEATURE_LATEST) {
                return DBErrors::TOO_NEW;
            }
            pwallet->LoadMinVersion(nMinVersion);
        }

        // Get cursor
        Dbc *pcursor = m_batch.GetCursor();
        if (!pcursor) {
            pwallet->WalletLogPrintf("Error getting wallet database cursor\n");
            return DBErrors::CORRUPT;
        }

        while (true) {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = m_batch.ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND) {
                break;
            }

            if (ret != 0) {
                pwallet->WalletLogPrintf(
                    "Error reading next record from wallet database\n");
                return DBErrors::CORRUPT;
            }

            // Try to be tolerant of single corrupt records:
            std::string strType, strErr;
            if (!ReadKeyValue(pwallet, ssKey, ssValue, wss, strType, strErr)) {
                // losing keys is considered a catastrophic error, anything else
                // we assume the user can live with:
                if (IsKeyType(strType) || strType == "defaultkey") {
                    result = DBErrors::CORRUPT;
                } else if (strType == "flags") {
                    // Reading the wallet flags can only fail if unknown flags
                    // are present.
                    result = DBErrors::TOO_NEW;
                } else {
                    // Leave other errors alone, if we try to fix them we might
                    // make things worse. But do warn the user there is
                    // something wrong.
                    fNoncriticalErrors = true;
                    if (strType == "tx") {
                        // Rescan if there is a bad transaction record:
                        gArgs.SoftSetBoolArg("-rescan", true);
                    }
                }
            }
            if (!strErr.empty()) {
                pwallet->WalletLogPrintf("%s\n", strErr);
            }
        }
        pcursor->close();
    } catch (...) {
        result = DBErrors::CORRUPT;
    }

    if (fNoncriticalErrors && result == DBErrors::LOAD_OK) {
        result = DBErrors::NONCRITICAL_ERROR;
    }

    // Any wallet corruption at all: skip any rewriting or upgrading, we don't
    // want to make it worse.
    if (result != DBErrors::LOAD_OK) {
        return result;
    }

    pwallet->WalletLogPrintf("nFileVersion = %d\n", wss.nFileVersion);

    pwallet->WalletLogPrintf("Keys: %u plaintext, %u encrypted, %u w/ "
                             "metadata, %u total. Unknown wallet records: %u\n",
                             wss.nKeys, wss.nCKeys, wss.nKeyMeta,
                             wss.nKeys + wss.nCKeys, wss.m_unknown_records);

    // nTimeFirstKey is only reliable if all keys have metadata
    if ((wss.nKeys + wss.nCKeys + wss.nWatchKeys) != wss.nKeyMeta) {
        pwallet->UpdateTimeFirstKey(1);
    }

    for (const TxId &txid : wss.vWalletUpgrade) {
        WriteTx(pwallet->mapWallet.at(txid));
    }

    // Rewrite encrypted wallets of versions 0.4.0 and 0.5.0rc:
    if (wss.fIsEncrypted &&
        (wss.nFileVersion == 40000 || wss.nFileVersion == 50000)) {
        return DBErrors::NEED_REWRITE;
    }

    if (wss.nFileVersion < CLIENT_VERSION) {
        // Update
        WriteVersion(CLIENT_VERSION);
    }

    if (wss.fAnyUnordered) {
        result = pwallet->ReorderTransactions();
    }

    return result;
}

DBErrors WalletBatch::FindWalletTx(std::vector<TxId> &txIds,
                                   std::vector<CWalletTx> &vWtx) {
    DBErrors result = DBErrors::LOAD_OK;

    try {
        int nMinVersion = 0;
        if (m_batch.Read((std::string) "minversion", nMinVersion)) {
            if (nMinVersion > FEATURE_LATEST) {
                return DBErrors::TOO_NEW;
            }
        }

        // Get cursor
        Dbc *pcursor = m_batch.GetCursor();
        if (!pcursor) {
            LogPrintf("Error getting wallet database cursor\n");
            return DBErrors::CORRUPT;
        }

        while (true) {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = m_batch.ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND) {
                break;
            }

            if (ret != 0) {
                LogPrintf("Error reading next record from wallet database\n");
                return DBErrors::CORRUPT;
            }

            std::string strType;
            ssKey >> strType;
            if (strType == "tx") {
                TxId txid;
                ssKey >> txid;

                CWalletTx wtx(nullptr /* pwallet */, MakeTransactionRef());
                ssValue >> wtx;

                txIds.push_back(txid);
                vWtx.push_back(wtx);
            }
        }
        pcursor->close();
    } catch (...) {
        result = DBErrors::CORRUPT;
    }

    return result;
}

DBErrors WalletBatch::ZapSelectTx(std::vector<TxId> &txIdsIn,
                                  std::vector<TxId> &txIdsOut) {
    // Build list of wallet TXs and hashes.
    std::vector<TxId> txIds;
    std::vector<CWalletTx> vWtx;
    DBErrors err = FindWalletTx(txIds, vWtx);
    if (err != DBErrors::LOAD_OK) {
        return err;
    }

    std::sort(txIds.begin(), txIds.end());
    std::sort(txIdsIn.begin(), txIdsIn.end());

    // Erase each matching wallet TX.
    bool delerror = false;
    std::vector<TxId>::iterator it = txIdsIn.begin();
    for (const TxId &txid : txIds) {
        while (it < txIdsIn.end() && (*it) < txid) {
            it++;
        }
        if (it == txIdsIn.end()) {
            break;
        }

        if ((*it) == txid) {
            if (!EraseTx(txid)) {
                LogPrint(BCLog::DB,
                         "Transaction was found for deletion but returned "
                         "database error: %s\n",
                         txid.GetHex());
                delerror = true;
            }
            txIdsOut.push_back(txid);
        }
    }

    if (delerror) {
        return DBErrors::CORRUPT;
    }
    return DBErrors::LOAD_OK;
}

DBErrors WalletBatch::ZapWalletTx(std::vector<CWalletTx> &vWtx) {
    // Build list of wallet TXs.
    std::vector<TxId> txIds;
    DBErrors err = FindWalletTx(txIds, vWtx);
    if (err != DBErrors::LOAD_OK) {
        return err;
    }

    // Erase each wallet TX.
    for (const TxId &txid : txIds) {
        if (!EraseTx(txid)) {
            return DBErrors::CORRUPT;
        }
    }

    return DBErrors::LOAD_OK;
}

void MaybeCompactWalletDB() {
    static std::atomic<bool> fOneThread;
    if (fOneThread.exchange(true)) {
        return;
    }
    if (!gArgs.GetBoolArg("-flushwallet", DEFAULT_FLUSHWALLET)) {
        return;
    }

    for (const std::shared_ptr<CWallet> &pwallet : GetWallets()) {
        WalletDatabase &dbh = pwallet->GetDBHandle();

        unsigned int nUpdateCounter = dbh.nUpdateCounter;

        if (dbh.nLastSeen != nUpdateCounter) {
            dbh.nLastSeen = nUpdateCounter;
            dbh.nLastWalletUpdate = GetTime();
        }

        if (dbh.nLastFlushed != nUpdateCounter &&
            GetTime() - dbh.nLastWalletUpdate >= 2) {
            if (BerkeleyBatch::PeriodicFlush(dbh)) {
                dbh.nLastFlushed = nUpdateCounter;
            }
        }
    }

    fOneThread = false;
}

//
// Try to (very carefully!) recover wallet file if there is a problem.
//
bool WalletBatch::Recover(const fs::path &wallet_path, void *callbackDataIn,
                          bool (*recoverKVcallback)(void *callbackData,
                                                    CDataStream ssKey,
                                                    CDataStream ssValue),
                          std::string &out_backup_filename) {
    return BerkeleyBatch::Recover(wallet_path, callbackDataIn,
                                  recoverKVcallback, out_backup_filename);
}

bool WalletBatch::Recover(const fs::path &wallet_path,
                          std::string &out_backup_filename) {
    // recover without a key filter callback
    // results in recovering all record types
    return WalletBatch::Recover(wallet_path, nullptr, nullptr,
                                out_backup_filename);
}

bool WalletBatch::RecoverKeysOnlyFilter(void *callbackData, CDataStream ssKey,
                                        CDataStream ssValue) {
    CWallet *dummyWallet = reinterpret_cast<CWallet *>(callbackData);
    CWalletScanState dummyWss;
    std::string strType, strErr;
    bool fReadOK;
    {
        // Required in LoadKeyMetadata():
        LOCK(dummyWallet->cs_wallet);
        fReadOK = ReadKeyValue(dummyWallet, ssKey, ssValue, dummyWss, strType,
                               strErr);
    }
    if (!IsKeyType(strType) && strType != "hdchain") {
        return false;
    }
    if (!fReadOK) {
        LogPrintf("WARNING: WalletBatch::Recover skipping %s: %s\n", strType,
                  strErr);
        return false;
    }

    return true;
}

bool WalletBatch::VerifyEnvironment(const fs::path &wallet_path,
                                    std::string &errorStr) {
    return BerkeleyBatch::VerifyEnvironment(wallet_path, errorStr);
}

bool WalletBatch::VerifyDatabaseFile(const fs::path &wallet_path,
                                     std::string &warningStr,
                                     std::string &errorStr) {
    return BerkeleyBatch::VerifyDatabaseFile(wallet_path, warningStr, errorStr,
                                             WalletBatch::Recover);
}

bool WalletBatch::WriteDestData(const CTxDestination &address,
                                const std::string &key,
                                const std::string &value) {
    if (!IsValidDestination(address)) {
        return false;
    }
    return WriteIC(
        std::make_pair(
            std::string("destdata"),
            std::make_pair(EncodeLegacyAddr(address, Params()), key)),
        value);
}

bool WalletBatch::EraseDestData(const CTxDestination &address,
                                const std::string &key) {
    if (!IsValidDestination(address)) {
        return false;
    }
    return EraseIC(std::make_pair(
        std::string("destdata"),
        std::make_pair(EncodeLegacyAddr(address, Params()), key)));
}

bool WalletBatch::WriteHDChain(const CHDChain &chain) {
    return WriteIC(std::string("hdchain"), chain);
}

bool WalletBatch::WriteWalletFlags(const uint64_t flags) {
    return WriteIC(std::string("flags"), flags);
}

bool WalletBatch::TxnBegin() {
    return m_batch.TxnBegin();
}

bool WalletBatch::TxnCommit() {
    return m_batch.TxnCommit();
}

bool WalletBatch::TxnAbort() {
    return m_batch.TxnAbort();
}

bool WalletBatch::ReadVersion(int &nVersion) {
    return m_batch.ReadVersion(nVersion);
}

bool WalletBatch::WriteVersion(int nVersion) {
    return m_batch.WriteVersion(nVersion);
}
