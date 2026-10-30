// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die 80 kHz, nachgerechnet.
//
// Beim Verbinden mit der FLEX-8400 stand die Scheibe vier Mal
// reproduzierbar 80 kHz neben der Maschine (7.179→7.259,
// 7.3086→7.3886, 7.197→7.277, 7.074→7.154). Konstant und unabhaengig
// von der Frequenz — weil der Versatz nur an der Breite haengt:
//
//     Breite/2 − Breite·Randanteil  =  100 kHz − 20 kHz  =  80 kHz
//
// Das ist die neue Fenstermitte, nachdem das Fenster dem VFO an den
// Rand nachgescrollt ist (Thetis-Regel, SpectrumWidget::
// setVfoFrequency). Der Fehler war nicht das Scrollen, sondern dass
// diese Mitte danach als Abstimmbefehl in die Scheibe
// zurueckgeschrieben wurde.
//
// Dieser Stand haelt beides fest: die Zahl und das Urteil.

#include <QtTest>

#include "core/FensterRandScroll.h"

using namespace Longpath;
using namespace Longpath::FensterRandScroll;

class TstFensterRandscroll : public QObject
{
    Q_OBJECT

private slots:
    void dieAchtzigKilohertzStehenInDerRechnung()
    {
        // Der gemessene Fall: 200 kHz Ausschnitt, zehn Prozent Rand.
        QCOMPARE(mitteNebenVfoHz(200000.0), 80000.0);

        // Und damit genau die vier Messungen vom 29./30.10.
        const double v = mitteNebenVfoHz(200000.0);
        QCOMPARE(7179000.0 + v, 7259000.0);
        QCOMPARE(7308600.0 + v, 7388600.0);
        QCOMPARE(7197000.0 + v, 7277000.0);
        QCOMPARE(7074000.0 + v, 7154000.0);
    }

    void derVersatzHaengtNurAnDerBreite()
    {
        // Halbe Breite, halber Versatz — die Frequenz kommt darin
        // nicht vor. Das erklaert, warum er konstant war.
        QCOMPARE(mitteNebenVfoHz(100000.0), 40000.0);
        QCOMPARE(mitteNebenVfoHz(50000.0),  20000.0);
        // Ohne Ausschnitt gibt es keinen Versatz zu rechnen.
        QCOMPARE(mitteNebenVfoHz(0.0),  0.0);
        QCOMPARE(mitteNebenVfoHz(-1.0), 0.0);
    }

    void einRandVonFuenfzigProzentLaesstNichtsUebrig()
    {
        // Grenzfall: liegt der Randstreifen bei der halben Breite,
        // ist die neue Mitte der VFO selbst — dann gaebe es die
        // Rueckkopplung nicht. Beleg, dass die Formel wirklich an
        // beiden Groeszen haengt.
        QCOMPARE(mitteNebenVfoHz(200000.0, 0.50), 0.0);
    }

    void nurEinZugIstEinBefehl()
    {
        // Das Nachscrollen ist die FOLGE einer Abstimmung — kein
        // Befehl. Genau dieser Fall war die Rueckkopplung.
        QVERIFY(!mitteIstAbstimmbefehl(false, true));
        // Der Betreiber zieht den Panadapter: das ist einer.
        QVERIFY(mitteIstAbstimmbefehl(false, false));
        // Mit CTUN ist die Mitte ueberhaupt nicht die Frequenz.
        QVERIFY(!mitteIstAbstimmbefehl(true, false));
        QVERIFY(!mitteIstAbstimmbefehl(true, true));
    }
};

QTEST_APPLESS_MAIN(TstFensterRandscroll)
#include "tst_fenster_randscroll.moc"
