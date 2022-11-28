// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/splashscreen.h>

#include <clientversion.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <qt/guiutil.h>
#include <qt/networkstyle.h>
#include <ui_interface.h>
#include <util/system.h>
#include <version.h>

#include <QApplication>
#include <QCloseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QScreen>

#include <memory>

SplashScreen::SplashScreen(interfaces::Node &node,
                           const NetworkStyle *networkStyle)
    : QWidget(nullptr), curAlignment(0), m_node(node) {
    // set reference point, paddings
    int paddingRight = 20;
    int paddingTop = 50;
    int titleVersionVSpace = 17;
    int titleCopyrightVSpace = 40;

    float fontFactor = 1.0;
    float devicePixelRatio = 1.0;
#if QT_VERSION > 0x050100
    devicePixelRatio = static_cast<QGuiApplication *>(QCoreApplication::instance())->devicePixelRatio();
#endif

    // define text to place
    QString titleText = PACKAGE_NAME;
    QString versionText = QString("Version %1").arg(QString::fromStdString(FormatFullVersion()));
    QString copyrightText = QString::fromStdString(CopyrightHolders(strprintf("\xC2\xA9 %u-%u ", 2009, COPYRIGHT_YEAR)));
    QString titleAddText = networkStyle->getTitleAddText();

    QString font = QApplication::font().toString();

    // create a bitmap according to device pixelratio
    QSize splashSize(480 * devicePixelRatio, 320 * devicePixelRatio);
    pixmap = QPixmap(splashSize);

#if QT_VERSION > 0x050100
    // change to HiDPI if it makes sense
    pixmap.setDevicePixelRatio(devicePixelRatio);
#endif

    QPainter pixPaint(&pixmap);
    pixPaint.setPen(QColor(0xD9, 0xD9, 0xD9));

    // draw a linear gradient
    QLinearGradient gradient(QPoint(0, 0), QPoint(0, splashSize.height() / devicePixelRatio));
    gradient.setColorAt(0, QColor(0x09, 0x09, 0x09));
    gradient.setColorAt(1, QColor(0x2A, 0x2A, 0x2A));
    QRect rGradient(QPoint(0, 0), splashSize);
    pixPaint.fillRect(rGradient, gradient);

    // draw the bitcoin icon, expected size of PNG: 1024x1273
    QRect rectIcon(QPoint(20, 10), QSize(184, 229));

    const QSize requiredSize(184, 229);
    QPixmap icon(networkStyle->getSplashIcon().pixmap(requiredSize));

    pixPaint.drawPixmap(rectIcon, icon);

    // check font size and drawing with
    pixPaint.setFont(QFont(font, 30 * fontFactor));
    QFontMetrics fm = pixPaint.fontMetrics();
    int titleTextWidth = GUIUtil::TextWidth(fm, titleText);
    if (titleTextWidth > 220) {
        fontFactor = fontFactor * 220 / titleTextWidth;
    }

    pixPaint.setFont(QFont(font, 30 * fontFactor));
    fm = pixPaint.fontMetrics();
    titleTextWidth = GUIUtil::TextWidth(fm, titleText);
    pixPaint.drawText(pixmap.width() / devicePixelRatio - titleTextWidth - paddingRight, paddingTop, titleText);

    pixPaint.setFont(QFont(font, 15 * fontFactor));

    // if the version string is too long, reduce size
    fm = pixPaint.fontMetrics();
    int versionTextWidth = GUIUtil::TextWidth(fm, titleText);
    if (versionTextWidth > titleTextWidth + paddingRight - 10) {
        pixPaint.setFont(QFont(font, 10 * fontFactor));
        titleVersionVSpace -= 5;
    }
    pixPaint.drawText(pixmap.width() / devicePixelRatio - titleTextWidth - paddingRight + 2,
                      paddingTop + titleVersionVSpace, versionText);

    // draw copyright stuff
    {
        pixPaint.setFont(QFont(font, 10 * fontFactor));
        const int x = pixmap.width() / devicePixelRatio - titleTextWidth - paddingRight;
        const int y = paddingTop + titleCopyrightVSpace;
        QRect copyrightRect(x, y, pixmap.width() - x - paddingRight, pixmap.height() - y);
        pixPaint.drawText(copyrightRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, copyrightText);
    }

    // draw additional text if special network
    if (!titleAddText.isEmpty()) {
        QFont boldFont = QFont(font, 10 * fontFactor);
        boldFont.setWeight(QFont::Bold);
        pixPaint.setFont(boldFont);
        fm = pixPaint.fontMetrics();
        int titleAddTextWidth = GUIUtil::TextWidth(fm, titleAddText);
        pixPaint.drawText(pixmap.width() / devicePixelRatio - titleAddTextWidth - 10, 15, titleAddText);
    }

    pixPaint.end();

    // Set window title
    setWindowTitle(titleAddText.isEmpty() ? titleText : titleText + " " + titleAddText);

    // Resize window and move to center of desktop, disallow resizing
    QRect r(QPoint(), QSize(pixmap.size().width() / devicePixelRatio, pixmap.size().height() / devicePixelRatio));
    resize(r.size());
    setFixedSize(r.size());
    move(QGuiApplication::primaryScreen()->geometry().center() - r.center());

    subscribeToCoreSignals();
    installEventFilter(this);
}

SplashScreen::~SplashScreen() {
    unsubscribeFromCoreSignals();
}

bool SplashScreen::eventFilter(QObject *obj, QEvent *ev) {
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(ev);
        if (keyEvent->key() == Qt::Key_Q) {
            m_node.startShutdown();
        }
    }
    return QObject::eventFilter(obj, ev);
}

void SplashScreen::slotFinish(QWidget *mainWin) {
    Q_UNUSED(mainWin);

    /* If the window is minimized, hide() will be ignored. */
    /* Make sure we de-minimize the splashscreen window before hiding */
    if (isMinimized()) {
        showNormal();
    }
    hide();
    // No more need for this
    deleteLater();
}

static void InitMessage(SplashScreen *splash, const std::string &message) {
    QMetaObject::invokeMethod(splash, "showMessage", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(message)),
                              Q_ARG(int, Qt::AlignBottom | Qt::AlignHCenter),
                              Q_ARG(QColor, QColor(0xD9, 0xD9, 0xD9)));
}

static void ShowProgress(SplashScreen *splash, const std::string &title,
                         int nProgress, bool resume_possible) {
    InitMessage(splash, title + std::string("\n") +
                            (resume_possible
                                 ? _("(press q to shutdown and continue later)")
                                 : _("press q to shutdown")) +
                            strprintf("\n%d", nProgress) + "%");
}
#ifdef ENABLE_WALLET
void SplashScreen::ConnectWallet(std::unique_ptr<interfaces::Wallet> wallet) {
    m_connected_wallet_handlers.emplace_back(wallet->handleShowProgress(
        std::bind(ShowProgress, this, std::placeholders::_1,
                  std::placeholders::_2, false)));
    m_connected_wallets.emplace_back(std::move(wallet));
}
#endif

void SplashScreen::subscribeToCoreSignals() {
    // Connect signals to client
    m_handler_init_message = m_node.handleInitMessage(
        std::bind(InitMessage, this, std::placeholders::_1));
    m_handler_show_progress = m_node.handleShowProgress(
        std::bind(ShowProgress, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3));
#ifdef ENABLE_WALLET
    m_handler_load_wallet = m_node.handleLoadWallet(
        [this](std::unique_ptr<interfaces::Wallet> wallet) {
            ConnectWallet(std::move(wallet));
        });
#endif
}

void SplashScreen::unsubscribeFromCoreSignals() {
    // Disconnect signals from client
    m_handler_init_message->disconnect();
    m_handler_show_progress->disconnect();
    for (const auto &handler : m_connected_wallet_handlers) {
        handler->disconnect();
    }
    m_connected_wallet_handlers.clear();
    m_connected_wallets.clear();
}

void SplashScreen::showMessage(const QString &message, int alignment,
                               const QColor &color) {
    curMessage = message;
    curAlignment = alignment;
    curColor = color;
    update();
}

void SplashScreen::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.drawPixmap(0, 0, pixmap);
    QRect r = rect().adjusted(5, 5, -5, -5);
    painter.setPen(curColor);
    painter.drawText(r, curAlignment, curMessage);
}

void SplashScreen::closeEvent(QCloseEvent *event) {
    // allows an "emergency" shutdown during startup
    m_node.startShutdown();
    event->ignore();
}
