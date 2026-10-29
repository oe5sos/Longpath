// =================================================================
// tests/tst_flex_sendetakt.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigener Pruefstand.
//
// Der Sendeton zur Flex ist ein Echtzeitstrom: was die Pumpe
// hinausgibt, spielt die Maschine ab. Sie hatte aber keine Uhr — sie
// schob bei JEDEM Taktschlag starr 480 Rahmen hinaus, ganz gleich,
// wann der Schlag kam. Auf einem Hauptfaden, der nebenher Wasserfall,
// Spektrum und Kachel malt, kommt er nicht alle 5 ms.
//
// Am Geraet gemessen (FLEX-8400, Protokoll 2026-10-29, 13:05 bis
// 13:07, vier Sendungen von je 12,94 s). Hinausgehen muessten je
// 621.120 Rahmen:
//
//     180.000 Rahmen = 13,9 kHz = 0,29-fache Geschwindigkeit
//     332.640 Rahmen = 25,7 kHz = 0,54-fache
//     165.600 Rahmen = 12,8 kHz = 0,27-fache
//     716.160 Rahmen = 55,3 kHz = 1,15-fache
//
// Der Ton ging also mal auf ein Viertel gedehnt, mal zu schnell
// hinaus. Dasselbe erklaert den spaeten Anfang: bis die Pumpe durch
// die fuehrende Stille der Wellenform war, vergingen 3,6 s statt 0,5.
// Betreiber: „leistung kommt zeitversetzt."
//
// Hier steht die Rechnung, die das entscheidet. Gegenprobe gegen die
// alte Fassung ist der Fall `derAlteStarreBlockWaereDoppeltSoSchnell`:
// er rechnet vor, was 480 Rahmen je 5-ms-Schlag ergeben haetten.
// =================================================================

#include <QtTest/QtTest>

#include "gui/MainWindow.h"

using Longpath::MainWindow;

namespace {
constexpr qint64 kRate = 48000;          // Sendeweg, RadioModel.cpp:2648
constexpr qint64 kMax  = kRate / 10;     // 100 ms Deckel je Schlag
constexpr qint64 kMs   = 1000000LL;      // eine Millisekunde in ns
}

class TstFlexSendetakt : public QObject
{
    Q_OBJECT

private slots:
    // Ein punktgenauer Schlag nach 5 ms bringt 240 Rahmen — nicht 480.
    void einSchlagNachFuenfMsBringtZweihundertvierzig()
    {
        QCOMPARE(MainWindow::flexFaelligeRahmen(5 * kMs, 0, kRate, kMax),
                 qint64(240));
    }

    // Ein SPAETER Schlag holt auf. Das ist der Kern: die alte Fassung
    // gab auch nach 34 ms nur ihren festen Block heraus.
    void einSpaeterSchlagHoltAuf()
    {
        QCOMPARE(MainWindow::flexFaelligeRahmen(34 * kMs, 0, kRate, kMax),
                 qint64(1632));          // 34 ms * 48
    }

    // Und ein frueher wartet, statt vorzulaufen.
    void einFrueherSchlagWartet()
    {
        // 5 ms verstrichen, 240 Rahmen sind schon hinaus: nichts offen.
        QCOMPARE(MainWindow::flexFaelligeRahmen(5 * kMs, 240, kRate, kMax),
                 qint64(0));
        // Und wer vorgelaufen ist, bekommt auch nichts.
        QCOMPARE(MainWindow::flexFaelligeRahmen(5 * kMs, 480, kRate, kMax),
                 qint64(0));
    }

    // Ueber eine ganze Sendung muss die Summe stimmen — das ist die
    // Zahl, die beim Gegenueber ueber lesbar oder gedehnt entscheidet.
    // Hier laufen beide Rechnungen durch DIESELBEN unregelmaeszigen
    // Schlaege, damit der Vergleich einer ist.
    void ueberEineGanzeSendungStimmtDieSumme()
    {
        const auto tempoFuer = [](bool mitUhr) {
            // Schlaege wie am Geraet gemessen: 5, 9, 34, 12, 8 ms im
            // Wechsel — im Mittel 13,6 statt der bestellten 5.
            const qint64 takte[] = { 5, 9, 34, 12, 8 };
            qint64 verstrichenNs = 0;
            qint64 geliefert     = 0;
            int    i             = 0;
            while (verstrichenNs < 12940 * kMs) {
                verstrichenNs += takte[i++ % 5] * kMs;
                geliefert += mitUhr
                    ? MainWindow::flexFaelligeRahmen(
                          verstrichenNs, geliefert, kRate, kMax)
                    : 480;               // die alte starre Zahl
            }
            // Gegen die WIRKLICH verstrichene Zeit, nicht gegen die
            // bestellte: der letzte Schlag faellt hinter 12,94 s.
            const double sollRahmen =
                double(verstrichenNs) * double(kRate) / 1.0e9;
            return double(geliefert) / sollRahmen;
        };

        // ── Gegenprobe zuerst: so waere es ohne die Uhr ──────────────
        //
        // Sie zaehlte Schlaege, nicht Zeit. 480 Rahmen stimmen nur,
        // wenn der Schlag punktgenau alle 10 ms kommt.
        const double ohneUhr = tempoFuer(false);
        QVERIFY2(ohneUhr < 0.95 || ohneUhr > 1.05,
                 qPrintable(QStringLiteral(
                     "die alte Fassung haette %1-fach geliefert — waere "
                     "sie richtig gewesen, braeuchte es diesen "
                     "Pruefstand nicht").arg(ohneUhr)));

        // Und so ist es mit ihr.
        const double mitUhr = tempoFuer(true);
        QVERIFY2(mitUhr > 0.999 && mitUhr < 1.001,
                 qPrintable(QStringLiteral("Tempo %1-fach").arg(mitUhr)));
    }

    // Der Deckel begrenzt den einzelnen Schwall, verschluckt aber
    // nichts: der Rest bleibt faellig.
    void derDeckelBegrenztOhneZuVerschlucken()
    {
        // Eine volle Sekunde Haenger.
        QCOMPARE(MainWindow::flexFaelligeRahmen(1000 * kMs, 0, kRate, kMax),
                 kMax);
        // Danach steht der Rest weiter offen.
        QCOMPARE(MainWindow::flexFaelligeRahmen(1000 * kMs, kMax, kRate, kMax),
                 kMax);
    }

    void unsinnigeEingabenGebenNull()
    {
        QCOMPARE(MainWindow::flexFaelligeRahmen(-1, 0, kRate, kMax), qint64(0));
        QCOMPARE(MainWindow::flexFaelligeRahmen(5 * kMs, 0, 0, kMax), qint64(0));
        QCOMPARE(MainWindow::flexFaelligeRahmen(5 * kMs, 0, kRate, 0), qint64(0));
    }
};

QTEST_MAIN(TstFlexSendetakt)
#include "tst_flex_sendetakt.moc"
