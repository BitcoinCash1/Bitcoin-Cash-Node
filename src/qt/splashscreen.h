// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QSplashScreen>
#include <functional>

#include <memory>

class NetworkStyle;

namespace interfaces {
class Handler;
class Node;
class Wallet;
}; // namespace interfaces

/** Class for the splashscreen with information of the running client.
 *
 * @note this is intentionally not a QSplashScreen. Bitcoin Core initialization
 * can take a long time, and in that case a progress window that cannot be moved
 * around and minimized has turned out to be frustrating to the user.
 */
class SplashScreen : public QWidget {
    Q_OBJECT

public:
    explicit SplashScreen(interfaces::Node &node,
                          const NetworkStyle *networkStyle);
    ~SplashScreen();

protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

public Q_SLOTS:
    /** Slot to call finish() method as it's not defined as slot */
    void slotFinish(QWidget *mainWin);

    /** Show message and progress */
    void showMessage(const QString &message, int alignment,
                     const QColor &color);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    /** Connect core signals to splash screen */
    void subscribeToCoreSignals();
    /** Disconnect core signals to splash screen */
    void unsubscribeFromCoreSignals();
    /** Connect wallet signals to splash screen */
    void ConnectWallet(std::unique_ptr<interfaces::Wallet> wallet);

    QPixmap pixmap;
    QString curMessage;
    QColor curColor;
    int curAlignment;

    interfaces::Node &m_node;
    std::unique_ptr<interfaces::Handler> m_handler_init_message;
    std::unique_ptr<interfaces::Handler> m_handler_show_progress;
    std::unique_ptr<interfaces::Handler> m_handler_load_wallet;
    std::list<std::unique_ptr<interfaces::Wallet>> m_connected_wallets;
    std::list<std::unique_ptr<interfaces::Handler>> m_connected_wallet_handlers;
};
