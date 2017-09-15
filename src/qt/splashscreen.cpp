// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "splashscreen.h"

#include "clientversion.h"
#include "init.h"
#include "ui_interface.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <QApplication>
#include <QPainter>
#include <QFontDatabase>

SplashScreen::SplashScreen(const QPixmap &pixmap, Qt::WindowFlags f, bool isTestNet) :
    QSplashScreen(pixmap, f)
{
    setAutoFillBackground(true);

    QFontDatabase::addApplicationFont(":fonts/res/fonts/nasalization-rg.ttf");

    // set reference point, paddings
    int paddingLeftCol2         = 10;
    int paddingTopCol2          = 374;
    int line1 = 0;
    int line2 = 18;
    int line3 = 36;

    float fontFactor            = 1.0;

    // define text to place
    QString titleText       = tr("STRONG ENCRYPTION  FAST TRANSACTIONS");
    QString algoText        = QString("NEOSCRYPT");
    QString versionText     = QString("Version %1").arg(QString::fromStdString(FormatFullVersion()));
    QString copyrightText1  = QChar(0xA9)+QString(" 2009-%1 ").arg(COPYRIGHT_YEAR) + QString(tr("The Bitcoin Core developers"));
    QString copyrightText2  = QChar(0xA9)+QString(" 2014-%1 ").arg(COPYRIGHT_YEAR) + QString(tr("The UFO Core developers"));
    QString testnetAddText  = QString(tr("[testnet]")); // define text to place as single text object

    // load the bitmap for writing some text over it
    QPixmap newPixmap;
    if(isTestNet) {
        newPixmap     = QPixmap(":/images/splash_testnet");
    }
    else {
        newPixmap     = QPixmap(":/images/splash");
    }

    QString font            = "Nasalization Rg";
    QPainter pixPaint(&newPixmap);
    pixPaint.setFont(QFont(font, 14*fontFactor));
    QFontMetrics fm = pixPaint.fontMetrics();
    int titleTextWidth  = fm.width(titleText);
    if(titleTextWidth > 440) {
        fontFactor = fontFactor * 440 / titleTextWidth;;
        pixPaint.setFont(QFont(font, 14*fontFactor));
    }

    if (isTestNet) {
        pixPaint.setPen(QColor(255,26,26));
        pixPaint.drawText(320,100,testnetAddText);
    } else {
        pixPaint.setPen(QColor(138,255,44));
    }

    pixPaint.drawText(320,paddingTopCol2+25,algoText);

    pixPaint.setPen(QColor(255,255,255));

    pixPaint.setFont(QFont(font, 13*fontFactor));
    pixPaint.drawText(40,46,titleText);

    pixPaint.setFont(QFont(font, 10*fontFactor));
    pixPaint.drawText(paddingLeftCol2,paddingTopCol2+line3,versionText);
    pixPaint.drawText(paddingLeftCol2,paddingTopCol2+line1,copyrightText1);
    pixPaint.drawText(paddingLeftCol2,paddingTopCol2+line2,copyrightText2);

    pixPaint.end();

    this->setPixmap(newPixmap);

    subscribeToCoreSignals();
}

SplashScreen::~SplashScreen()
{
    unsubscribeFromCoreSignals();
}

void SplashScreen::slotFinish(QWidget *mainWin)
{
    finish(mainWin);
}

static void InitMessage(SplashScreen *splash, const std::string &message)
{
    QMetaObject::invokeMethod(splash, "showMessage",
        Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(int, Qt::AlignBottom|Qt::AlignHCenter),
        Q_ARG(QColor, QColor(55,55,55)));
}

static void ShowProgress(SplashScreen *splash, const std::string &title, int nProgress)
{
    InitMessage(splash, title + strprintf("%d", nProgress) + "%");
}

#ifdef ENABLE_WALLET
static void ConnectWallet(SplashScreen *splash, CWallet* wallet)
{
    wallet->ShowProgress.connect(boost::bind(ShowProgress, splash, _1, _2));
}
#endif

void SplashScreen::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.InitMessage.connect(boost::bind(InitMessage, this, _1));
#ifdef ENABLE_WALLET
    uiInterface.LoadWallet.connect(boost::bind(ConnectWallet, this, _1));
#endif
}

void SplashScreen::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.InitMessage.disconnect(boost::bind(InitMessage, this, _1));
#ifdef ENABLE_WALLET
    if(pwalletMain)
        pwalletMain->ShowProgress.disconnect(boost::bind(ShowProgress, this, _1, _2));
#endif
}
