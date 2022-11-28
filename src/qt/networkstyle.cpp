// Copyright (c) 2014-2016 The Bitcoin Core developers
// Copyright (c) 2020-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/networkstyle.h>

#include <qt/guiconstants.h>

#include <QApplication>

#include <cstring>
#include <type_traits>
#include <vector>

static const struct {
    const char *networkId;
    const char *appName;
    const int iconColorHue;
    const char *titleAddText;
} network_styles[] = {{"main",    QAPP_APP_NAME_DEFAULT,    0, ""                                             },
                      {"test",    QAPP_APP_NAME_TESTNET,  120, QT_TRANSLATE_NOOP("SplashScreen", "[testnet]") },
                      {"test4",   QAPP_APP_NAME_TESTNET4, 300, QT_TRANSLATE_NOOP("SplashScreen", "[testnet4]")},
                      {"scale",   QAPP_APP_NAME_SCALENET, 240, QT_TRANSLATE_NOOP("SplashScreen", "[scalenet]")},
                      {"chip",    QAPP_APP_NAME_CHIPNET,   60, QT_TRANSLATE_NOOP("SplashScreen", "[chipnet]")},
                      {"regtest", QAPP_APP_NAME_TESTNET,  180, "[regtest]"                                   }};
static const unsigned network_styles_count =
    sizeof(network_styles) / sizeof(*network_styles);

// titleAddText needs to be const char* for tr()
NetworkStyle::NetworkStyle(const QString &_appName, int iconColorHue, const char *_titleAddText)
    : appName(_appName), titleAddText(qApp->translate("SplashScreen", _titleAddText)) {

    // load pixmap
    QPixmap pixmaps[] = { {":/icons/bitcoin_splash"}, {":icons/bitcoin_noletters"} };

    if (iconColorHue) {
        for (auto & pixmap : pixmaps) {

            // generate QImage from QPixmap
            QImage img = pixmap.toImage();
            std::vector<QRgb> scanLineBuffer(img.width());

            // traverse though lines
            for (int y = 0; y < img.height(); y++) {
                QRgb *scL = scanLineBuffer.data();
                // copy scanLine to our buffer to guarantee aligned access
                static_assert(std::is_trivially_constructible_v<QRgb> && std::is_trivially_copyable_v<QRgb>);
                std::memcpy(scL, img.scanLine(y), img.width() * sizeof(QRgb));

                // loop through pixels
                for (int x = 0; x < img.width(); x++) {

                    int r, g, b, a;
                    a = qAlpha(scL[x]);
                    QColor col(scL[x]);
                    col.getRgb(&r, &g, &b);

                    // set the pixel
                    col.setHsl(iconColorHue, 128, qGray(r, g, b), a);
                    scL[x] = col.rgba();
                }

                // copy changes back to image scanLine
                std::memcpy(img.scanLine(y), scL, img.width() * sizeof(QRgb));
            }

            // convert back to QPixmap
            pixmap.convertFromImage(img);
        }
    }

    splashIcon = QIcon(pixmaps[0]);
    trayAndWindowIcon = QIcon(pixmaps[1].scaled(QSize(256, 256)));
}

const NetworkStyle *NetworkStyle::instantiate(const QString &networkId) {
    for (unsigned x = 0; x < network_styles_count; ++x) {
        if (networkId == network_styles[x].networkId) {
            return new NetworkStyle(network_styles[x].appName, network_styles[x].iconColorHue, network_styles[x].titleAddText);
        }
    }
    return nullptr;
}
