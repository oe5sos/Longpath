// SPDX-License-Identifier: GPL-3.0-or-later
//
// Das Urteil der Flex-Frequenzspur: schlaegt sie an, wo sie soll —
// und schweigt sie, wo sie schweigen muss?
//
// Der offene Befund vom 2026-10-29: beim Verbinden steht die Scheibe
// jedes Mal genau 80 kHz neben der Maschine, und im Protokoll steht
// keine Zeile darueber, wer sie verstellt hat. Die Spur in
// MainWindow_Flex.cpp schliesst diese Luecke; hier steht, dass ihr
// Urteil taugt.
//
// Beide Richtungen gehoeren geprueft, nicht nur die eine: eine Spur,
// die bei jedem Handgriff des Bedienenden laut wird, waere Laerm und
// wuerde den echten Fall verdecken — genau die Klasse Fehler, die am
// 2026-10-30 drei Prueflaeufe umgeworfen hat (ein TCP-Connect, der
// Erfolg meldete, wo niemand lauschte).

#include <QtTest>

#include "core/FlexFrequenzSpur.h"

using namespace Longpath;
using namespace Longpath::FlexFrequenzSpur;

class TstFlexFrequenzspur : public QObject
{
    Q_OBJECT

private slots:
    void derGesuchteVersatzSchlaegtAn()
    {
        // Die drei am Geraet gemessenen Faelle, Hertz genau.
        QVERIFY(istVersatz(7259000.0, 7179000.0, false));
        QVERIFY(istVersatz(7388600.0, 7308600.0, false));
        QVERIFY(istVersatz(7154000.0, 7074000.0, false));
        // Auch nach unten — die Richtung ist nicht Teil der Aussage.
        QVERIFY(istVersatz(7074000.0, 7154000.0, false));
    }

    void einWertVonDerMaschineIstKeinVersatz()
    {
        // Wache gesetzt heiszt: der Wert KAM von der Maschine. Dann ist
        // jeder Abstand ein Zwischenstand, kein Befund — sonst meldete
        // die Spur das Nachziehen, das sie beobachten soll.
        QVERIFY(!istVersatz(7154000.0, 7074000.0, true));
        QVERIFY(!istVersatz(7388600.0, 7308600.0, true));
    }

    void ohneMeldungDerMaschineKeinUrteil()
    {
        // Die Maschine hat sich noch nicht gemeldet. Abwesenheit ist
        // keine Aussage: hier waere jede Meldung geraten.
        QVERIFY(!istVersatz(7154000.0, 0.0, false));
        QVERIFY(!istVersatz(7154000.0, -1.0, false));
    }

    void kleineUnterschiedeBleibenStill()
    {
        // Rundung und das normale Nachziehen der Scheibe duerfen nicht
        // anschlagen: sonst steht in jedem Protokoll eine Warnung, und
        // eine Warnung, die immer dasteht, liest niemand mehr.
        QVERIFY(!istVersatz(7074000.0, 7074000.0, false));
        QVERIFY(!istVersatz(7074100.0, 7074000.0, false));
        QVERIFY(!istVersatz(7074999.0, 7074000.0, false));
        // Genau auf der Schwelle noch nicht — erst darueber.
        QVERIFY(!istVersatz(7075000.0, 7074000.0, false));
        QVERIFY(istVersatz(7075001.0,  7074000.0, false));
    }

    void dieSchwelleLaesstSichSetzen()
    {
        // Fuer eine engere Messung am Geraet.
        QVERIFY(istVersatz(7074200.0, 7074000.0, false, /*schwelleHz=*/100.0));
        QVERIFY(!istVersatz(7074200.0, 7074000.0, false, /*schwelleHz=*/500.0));
        QCOMPARE(kSchwelleHz, 1000.0);
    }
};

QTEST_APPLESS_MAIN(TstFlexFrequenzspur)
#include "tst_flex_frequenzspur.moc"
