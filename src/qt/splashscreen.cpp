// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "splashscreen.h"
#include "clientversion.h"
#include "util.h"

#include <QPainter>
#undef loop /* ugh, remove this when the #define loop is gone from util.h */
#include <QApplication>
#include <QFontDatabase>

SplashScreen::SplashScreen(const QPixmap &pixmap, Qt::WindowFlags f) :
    QSplashScreen(pixmap, f)
{
    QFontDatabase::addApplicationFont(":fonts/res/fonts/nasalization-rg.ttf");

    // Reference point, paddings
    int paddingLeftCol2         = 10;
    int paddingTopCol2          = 374;
    int line1 = 0;
    int line2 = 18;
    int line3 = 36;

    // Define text to place
    QString titleText       = QString("STRONG ENCRYPTION  FAST TRANSACTIONS");
    QString algoText        = QString("NEOSCRYPT");
    QString versionText     = QString("Version %1 ").arg(QString::fromStdString(FormatFullVersion()));
    QString copyrightText1  = QChar(0xA9)+QString(" 2009-%1 ").arg(COPYRIGHT_YEAR) + QString(tr("The Bitcoin developers"));
    QString copyrightText2  = QChar(0xA9)+QString(" 2014-%1 ").arg(COPYRIGHT_YEAR) + QString(tr("The UFO Coin developers"));

    // Load the bitmap for writing some text over it
    QPixmap newPixmap;
    if(GetBoolArg("-testnet")) {
        newPixmap     = QPixmap(":/images/splash_testnet");
    } else {
        newPixmap     = QPixmap(":/images/splash");
    }

    QPainter pixPaint(&newPixmap);

    if(GetBoolArg("-testnet")) {
        pixPaint.setPen(QColor(255,26,26));
    } else {
        pixPaint.setPen(QColor(138,255,44));
    }

    QFont* fontTitle = new QFont("Nasalization Rg");
    fontTitle->setPixelSize(17);

    QFont* font = new QFont("Nasalization Rg");
    font->setPixelSize(14);

    pixPaint.setFont(*font);
    pixPaint.drawText(320,paddingTopCol2+25,algoText);

    pixPaint.setPen(QColor(255,255,255));

    pixPaint.setFont(*fontTitle);
    pixPaint.drawText(40,46,titleText);

    pixPaint.setFont(*font);
    pixPaint.drawText(paddingLeftCol2,paddingTopCol2+line3,versionText);
    pixPaint.drawText(paddingLeftCol2,paddingTopCol2+line1,copyrightText1);
    pixPaint.drawText(paddingLeftCol2,paddingTopCol2+line2,copyrightText2);

    pixPaint.end();

    this->setPixmap(newPixmap);
}
