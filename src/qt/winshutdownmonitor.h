// Copyright (c) 2014 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#ifdef WIN32
#include <QByteArray>
#include <QString>

#include <windef.h> // for HWND

#include <QAbstractNativeEventFilter>

class WinShutdownMonitor : public QAbstractNativeEventFilter {
public:
    /** Implements QAbstractNativeEventFilter interface for processing Windows
     * messages */
    bool nativeEventFilter(const QByteArray &eventType, void *pMessage,
                           long *pnResult);

    /** Register the reason for blocking shutdown on Windows to allow clean
     * client exit */
    static void registerShutdownBlockReason(const QString &strReason,
                                            const HWND &mainWinId);
};
#endif
