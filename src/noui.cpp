// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <noui.h>

#include <ui_interface.h>
#include <util/system.h>

#include <boost/signals2/connection.hpp>
#include <boost/signals2/signal.hpp>

#include <cstdint>
#include <cstdio>
#include <string>

/** Store connections so we can disconnect them when suppressing output */
boost::signals2::connection noui_ThreadSafeMessageBoxConn;
boost::signals2::connection noui_ThreadSafeQuestionConn;
boost::signals2::connection noui_InitMessageConn;

bool noui_ThreadSafeMessageBox(const std::string &message,
                               const std::string &caption, unsigned int style) {
    bool fSecure = style & CClientUIInterface::SECURE;
    style &= ~CClientUIInterface::SECURE;

    std::string strCaption;
    // Check for usage of predefined caption
    switch (style) {
        case CClientUIInterface::MSG_ERROR:
            strCaption += _("Error");
            break;
        case CClientUIInterface::MSG_WARNING:
            strCaption += _("Warning");
            break;
        case CClientUIInterface::MSG_INFORMATION:
            strCaption += _("Information");
            break;
        default:
            // Use supplied caption (can be empty)
            strCaption += caption;
    }

    if (!fSecure) {
        LogPrintf("%s: %s\n", strCaption, message);
    }
    fprintf(stderr, "%s: %s\n", strCaption.c_str(), message.c_str());
    return false;
}

bool noui_ThreadSafeQuestion(
    const std::string & /* ignored interactive message */,
    const std::string &message, const std::string &caption,
    unsigned int style) {
    return noui_ThreadSafeMessageBox(message, caption, style);
}

void noui_InitMessage(const std::string &message) {
    LogPrintf("init message: %s\n", message);
}

void noui_connect() {
    noui_ThreadSafeMessageBoxConn =
        uiInterface.ThreadSafeMessageBox_connect(noui_ThreadSafeMessageBox);
    noui_ThreadSafeQuestionConn =
        uiInterface.ThreadSafeQuestion_connect(noui_ThreadSafeQuestion);
    noui_InitMessageConn = uiInterface.InitMessage_connect(noui_InitMessage);
}

bool noui_ThreadSafeMessageBoxSuppressed(const std::string &message,
                                         const std::string &caption,
                                         unsigned int style) {
    return false;
}

bool noui_ThreadSafeQuestionSuppressed(
    const std::string & /* ignored interactive message */,
    const std::string &message, const std::string &caption,
    unsigned int style) {
    return false;
}

void noui_InitMessageSuppressed(const std::string &message) {}

void noui_suppress() {
    noui_ThreadSafeMessageBoxConn.disconnect();
    noui_ThreadSafeQuestionConn.disconnect();
    noui_InitMessageConn.disconnect();
    noui_ThreadSafeMessageBoxConn = uiInterface.ThreadSafeMessageBox_connect(
        noui_ThreadSafeMessageBoxSuppressed);
    noui_ThreadSafeQuestionConn = uiInterface.ThreadSafeQuestion_connect(
        noui_ThreadSafeQuestionSuppressed);
    noui_InitMessageConn =
        uiInterface.InitMessage_connect(noui_InitMessageSuppressed);
}

void noui_reconnect() {
    noui_ThreadSafeMessageBoxConn.disconnect();
    noui_ThreadSafeQuestionConn.disconnect();
    noui_InitMessageConn.disconnect();
    noui_connect();
}
