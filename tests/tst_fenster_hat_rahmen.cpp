// SPDX-License-Identifier: GPL-3.0-or-later
//
// „die punkte fuer die vergroesserung links oben sind wieder weg."
// (Betreiber, 2026-10-17) -- und schon am 2026-10-15: „auf vollformat
// kann ich nicht stellen."
//
// Zweimal dasselbe gemeldet. Die Behebung vom 15.10. (ca4b6ab7, fuenf
// setWindowFlag-Zeilen) lag auf einem nie gepushten Zweig UND war
// wirkungslos: am 2026-10-17 nachgemessen sind die Qt-Flags mit und
// ohne sie identisch (0x8800f001, alle Knopf-Hints gesetzt). Qt setzt
// sie ohnehin.
//
// Die wirkliche Ursache: `enterBorderlessFullSize()` nimmt mit
// Qt::FramelessWindowHint die ganze TITELLEISTE -- und mit ihr die drei
// Knoepfe. Das Fenster meldete sich dem System als `AXDialog` mit NULL
// Titelleisten-Knoepfen. Gerufen wurde es beim Start an zwei Stellen,
// mit der Begruendung „longpath sollte immer formatfüllend sein"
// (Betreiber 2026-09-27).
//
// Zwischen beiden Wuenschen gibt es keinen Widerspruch: showMaximized()
// fuellt den Schirm genauso und behaelt die Titelleiste. Am laufenden
// Fenster gemessen, nach der Umstellung:
//
//     subrole = AXStandardWindow   (vorher AXDialog)
//     AXCloseButton 1 · AXMinimizeButton 1 · AXFullScreenButton 1
//
// (Der gruene zaehlt bei vollbildfaehigen Fenstern als
// AXFullScreenButton, NICHT als AXZoomButton -- eine Abfrage nur auf
// AXZoomButton meldet 0 und sieht nach Fehler aus.)
//
// Dieser Lauf haelt fest, was damals niemand festhielt.

#include <QtTest>

#include "FensterFrist.h"
#include "gui/MainWindow.h"

using namespace Longpath;

class TstFensterHatRahmen : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void beimStartIstDasFensterNICHTRahmenlos()
    {
        MainWindow mw;
        mw.resize(1200, 800);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, kFensterFrist));
        QCoreApplication::processEvents();

        QVERIFY2(!mw.rahmenlosesVollbildFuerTest(),
                 "das Fenster startet rahmenlos -- dann nimmt es die "
                 "Titelleiste mit, und die drei Knoepfe links oben sind "
                 "weg (Betreiber 2026-10-15 und 2026-10-17)");
        QVERIFY2(!(mw.windowFlags() & Qt::FramelessWindowHint),
                 "FramelessWindowHint steht -- keine Titelleiste, keine "
                 "Knoepfe");
    }
};

QTEST_MAIN(TstFensterHatRahmen)
#include "tst_fenster_hat_rahmen.moc"
