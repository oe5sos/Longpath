// =================================================================
// tests/tst_ft8_sendeplan.cpp  (Longpath)
// =================================================================
//
// Die drei Zeiten einer FT8-Sendung: wann getastet wird, ab wo aus
// der Wellenform gezogen wird, und wie lange der Sender an bleibt.
//
// Alle drei hingen vorher an derselben Zahl — den 1180 ms fuehrender
// Stille, die ft8_lib der Wellenform voranstellt — und alle drei
// gingen schief. Was das am Geraet kostete, steht im Kopf von
// `Ft8Tx::sendeplan`.
//
// Der wichtigste Fall heiszt `dieAlteRechnungSchnittDasEndeAb`: er
// rechnet die ALTE Fassung nach und belegt, dass sie 880 ms des
// Signals verlor. Ohne ihn belegt jede gruene Zahl hier nur, dass die
// neue Rechnung zu sich selbst passt.
//
// Longpath-original.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>

#include "core/Ft8Tx.h"

using namespace Longpath;
using P = Ft8Decoder::Protocol;

class TestFt8Sendeplan : public QObject
{
    Q_OBJECT

private slots:
    void dieFuehrendeStilleIstDieHalbeDifferenz()
    {
        // ft8_lib zentriert: (Fenster − Signal) / 2.
        QCOMPARE(Ft8Tx::stilleMs(P::Ft8), (15000 - 12640) / 2);   // 1180
        QCOMPARE(Ft8Tx::stilleMs(P::Ft4), (7500 - 5040) / 2);     // 1230
    }

    // ── Die Gegenprobe ───────────────────────────────────────────────
    void dieAlteRechnungSchnittDasEndeAb()
    {
        // ALT: getastet zur Fenstergrenze, Wellenform von vorn,
        // getastet `signalMs + 300`.
        const qint64 altTasten   = 0;
        const qint64 altTonBei   = Ft8Tx::stilleMs(P::Ft8);          // 1180
        const qint64 altDauer    = Ft8Tx::signalMs(P::Ft8) + 300;    // 12940
        const qint64 altSenderAus = altTasten + altDauer;            // 12940
        const qint64 altTonEnde   = altTonBei + Ft8Tx::signalMs(P::Ft8);

        // Das Signal lief 880 ms laenger, als der Sender an war.
        QCOMPARE(altTonEnde - altSenderAus, qint64(880));
        QVERIFY2(altTonEnde > altSenderAus,
                 "Wenn die alte Rechnung nichts abschnitt, prueft "
                 "dieser Prueflauf nichts.");

        // NEU: nichts wird abgeschnitten.
        const Ft8Tx::Sendeplan p = Ft8Tx::sendeplan(P::Ft8, 250, 500, 300);
        const qint64 senderAus = p.tastenNachMs + p.tastdauerMs;
        const qint64 tonEnde   = p.tonBeiMs + Ft8Tx::signalMs(P::Ft8);
        QCOMPARE(senderAus - tonEnde, qint64(300));   // genau der Nachlauf
    }

    void derZeitversatzTrifftDieNorm()
    {
        const Ft8Tx::Sendeplan p = Ft8Tx::sendeplan(P::Ft8, 250, 500, 300);
        // Der erste Ton kommt dort, wo WSJT-X ihn erwartet.
        QCOMPARE(p.tonBeiMs, qint64(500));
        // Und das ergibt sich aus Tasten + uebriger Stille, nicht aus
        // einer zweiten Zahl, die man nebenher pflegen muesste.
        QCOMPARE(p.tastenNachMs + (Ft8Tx::stilleMs(P::Ft8) - p.probenAbMs),
                 p.tonBeiMs);
    }

    void dieFristKommtAusDerStilleUndNichtObendrauf()
    {
        const Ft8Tx::Sendeplan ohne = Ft8Tx::sendeplan(P::Ft8, 0,   500, 300);
        const Ft8Tx::Sendeplan mit  = Ft8Tx::sendeplan(P::Ft8, 250, 500, 300);
        // Spaeter getastet …
        QCOMPARE(mit.tastenNachMs - ohne.tastenNachMs, qint64(250));
        // … dafuer genau so viel weiter vorn aus der Wellenform …
        QCOMPARE(mit.probenAbMs - ohne.probenAbMs, qint64(250));
        // … und der Ton geht zur selben Zeit hinaus.
        QCOMPARE(mit.tonBeiMs, ohne.tonBeiMs);
    }

    void dieWellenformReichtFuerDieGanzeSendung()
    {
        for (const P p : {P::Ft8, P::Ft4}) {
            const Ft8Tx::Sendeplan s = Ft8Tx::sendeplan(p, 250, 500, 300);
            const qint64 vorhanden = Ft8Tx::fensterMs(p) - s.probenAbMs;
            QVERIFY2(vorhanden >= s.tastdauerMs,
                     qPrintable(QStringLiteral("nur %1 ms Material fuer "
                                               "%2 ms Sendung")
                                    .arg(vorhanden).arg(s.tastdauerMs)));
        }
    }

    void derSenderBleibtKuerzerAnAlsDasFenster()
    {
        // Betreiber am 2026-10-22: „hat zu lange gedauert." Wer zu
        // spaet auf Empfang zurueckkommt, hoert den Anfang der
        // Antwort nicht.
        for (const P p : {P::Ft8, P::Ft4}) {
            const Ft8Tx::Sendeplan s = Ft8Tx::sendeplan(p, 250, 500, 300);
            QVERIFY2(s.tastenNachMs + s.tastdauerMs < Ft8Tx::fensterMs(p),
                     qPrintable(QStringLiteral("Sender bis %1 ms, Fenster "
                                               "nur %2 ms")
                                    .arg(s.tastenNachMs + s.tastdauerMs)
                                    .arg(Ft8Tx::fensterMs(p))));
        }
    }

    void ft4RechnetMitSeinenEigenenZahlen()
    {
        const Ft8Tx::Sendeplan s = Ft8Tx::sendeplan(P::Ft4, 250, 500, 300);
        QCOMPARE(s.tonBeiMs, qint64(500));
        QCOMPARE(s.probenAbMs, qint64(1230 + 250 - 500));   // 980
        QCOMPARE(s.tastdauerMs, qint64(500 + 5040 + 300 - 250));
    }

    void unsinnigeVorgabenWerdenEingefangen()
    {
        // Eine Frist, die laenger ist als die ganze Stille: dann
        // bliebe nichts zu ueberspringen.
        const Ft8Tx::Sendeplan lang = Ft8Tx::sendeplan(P::Ft8, 99999, 500, 300);
        QCOMPARE(lang.tastenNachMs, qint64(Ft8Tx::stilleMs(P::Ft8)));
        QVERIFY(lang.probenAbMs >= 0);

        // Ein Zielversatz vor der Frist ist unmoeglich: vor dem
        // Tasten geht kein Ton hinaus.
        const Ft8Tx::Sendeplan frueh = Ft8Tx::sendeplan(P::Ft8, 250, 0, 300);
        QVERIFY(frueh.tonBeiMs >= frueh.tastenNachMs);
        QVERIFY(frueh.probenAbMs >= 0);
        QVERIFY(frueh.tastdauerMs > 0);
    }
};

QTEST_MAIN(TestFt8Sendeplan)
#include "tst_ft8_sendeplan.moc"
