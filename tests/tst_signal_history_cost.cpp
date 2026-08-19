// =================================================================
// tests/tst_signal_history_cost.cpp  (NereusSDR)
// =================================================================
//
// Was kostet der Signalerkenner?
//
// Diese Datei existiert wegen einer Zeile in der Bank-Matrix vom
// 2026-08-19 (§6, Zeile 6.7): „was der Erkenner eingeschaltet kostet,
// ist nicht auf der Waage gewesen". Sie stellt ihn auf die Waage, und
// zwar am Schreibtisch — die Funktion ist rein, also braucht die Messung
// kein Funkgeraet.
//
// ZWEI DINGE, DIE DIESE DATEI NICHT IST:
//
// 1. Kein Beweis fuer das Verhalten im Betrieb. Gemessen wird eine
//    Funktion, nicht die Anwendung. Was das Einschalten im laufenden
//    Programm an CPU-Anzeige bewegt, bleibt eine Frage an die Bank.
//
// 2. Keine scharfe Schranke. Die Grenze unten ist bewusst weit: sie soll
//    einen ECHTEN Einbruch fangen (etwa jemand macht aus der Fuellung
//    eine quadratische Schleife), nicht die Schwankung einer belasteten
//    Maschine melden. Ein Test, der bei fremder Last rot wird, wird
//    abgeschaltet und schuetzt dann nichts mehr.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QElapsedTimer>

#include "core/SignalHistoryStore.h"
#include "core/VoiceSignalDetector.h"

using namespace NereusSDR;

namespace {

// Ein volles Panadapter-Bild: 4096 Werte ueber 192 kHz. Das ist die
// Groessenordnung, in der der Erkenner im Betrieb laeuft.
constexpr int    kBins         = 4096;
constexpr double kCenterMhz    = 14.200;
constexpr double kBandwidthMhz = 0.192;
constexpr float  kNoiseDbm     = -120.0f;

// Ein belegtes Band: zwoelf Stationen plus zwei Stoerer. Die leere
// Messung waere geschmeichelt — der Erkenner arbeitet umso mehr, je mehr
// Bereiche er findet.
QVector<float> busyBand()
{
    QVector<float> bins(kBins, kNoiseDbm);
    const double hzPerBin = kBandwidthMhz * 1.0e6 / kBins;

    auto paint = [&](double offsetHz, double widthHz, float peak) {
        const int mid  = kBins / 2 + static_cast<int>(offsetHz / hzPerBin);
        const int half = static_cast<int>(widthHz / hzPerBin / 2.0);
        for (int i = mid - half; i <= mid + half; ++i) {
            if (i >= 0 && i < bins.size()) { bins[i] = peak; }
        }
    };

    for (int k = 0; k < 12; ++k) {
        paint(-80000.0 + k * 13000.0, 2400.0, kNoiseDbm + 25.0f + k);
    }
    paint(20000.0, 100.0,   kNoiseDbm + 40.0f);   // Traeger
    paint(60000.0, 20000.0, kNoiseDbm + 30.0f);   // Breitbandstoerer
    return bins;
}

} // namespace

class TestSignalHistoryCost : public QObject
{
    Q_OBJECT

private slots:

    // DAS PROTOKOLLIEREN ABSCHALTEN, sonst misst diese Datei den
    // falschen Gegenstand: der Erkenner schreibt je gefundenem Signal
    // eine qCDebug-Zeile, und bei 14 Signalen mal 200 Durchlaeufen waeren
    // das 2800 formatierte Zeilen INNERHALB der Messschleife. Der erste
    // Versuch hat genau das gemessen.
    //
    // Im Betrieb ist die Kategorie ohnehin aus (qCDebug-Kategorien sind
    // per Vorgabe stumm), die Messung bildet damit den Normalfall ab.
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.shistory=false"));
    }

    // Die eigentliche Messung. Der Wert wird ausgegeben, damit er im
    // Testprotokoll steht und sich ueber Versionen vergleichen laesst.
    void detectorCostPerFrame()
    {
        const QVector<float> bins = busyBand();

        // Einmal vorher laufen lassen, damit Zwischenspeicher warm sind
        // und die erste Messung nicht die Aufwaermzeit enthaelt.
        detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz, {}, kNoiseDbm,
                           QStringLiteral("USB"));

        constexpr int kRuns = 200;
        QElapsedTimer t;
        t.start();
        int found = 0;
        for (int i = 0; i < kRuns; ++i) {
            found = detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz, {},
                                       kNoiseDbm, QStringLiteral("USB")).size();
        }
        const double perCallUs =
            static_cast<double>(t.nsecsElapsed()) / kRuns / 1000.0;

        qDebug("Erkenner: %.1f us je Bild (%d Werte, %d gefundene Signale)",
              perCallUs, kBins, found);

        // Im Betrieb laeuft der Erkenner auf 10 Hz. Der Anteil an einem
        // Kern ist also perCallUs * 10 / 1e6.
        qDebug("bei 10 Hz entspricht das %.3f %% eines Kerns",
              perCallUs * 10.0 / 1.0e6 * 100.0);

        QVERIFY2(found > 0, "die Messung muss auf einem belegten Band messen");

        // Weite Schranke, siehe Kopf der Datei: 20 ms je Bild waere das
        // Hundertfache des Erwarteten und wuerde bei 10 Hz ein Fuenftel
        // eines Kerns fressen. Alles darunter ist eine Frage an die Bank,
        // nicht an diesen Test.
        QVERIFY2(perCallUs < 20000.0,
                 qPrintable(QString("Erkenner braucht %1 us je Bild — das ist "
                                    "keine Schwankung mehr, da ist etwas kaputt")
                                .arg(perCallUs, 0, 'f', 1)));
    }

    // Die Verwaltung laeuft im Sekundentakt ueber alle Eintraege und
    // zaehlt Zeitstempel. Bei einem vollen Band mit 15 s Vorgeschichte
    // ist das die groesste Liste, die im Betrieb vorkommt.
    void storeRebuildCost()
    {
        SignalHistoryStore store;
        const QVector<float> bins = busyBand();
        constexpr qint64 kT0 = 1'700'000'000'000LL;

        // 15 Sekunden bei 10 Hz einspeisen — das Fenster, das die
        // Verwaltung ueberhaupt behaelt (kQrmWindowMs).
        for (int i = 0; i < 150; ++i) {
            store.ingest(detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz, {},
                                            kNoiseDbm, QStringLiteral("USB")),
                         kT0 + i * 100);
        }

        QElapsedTimer t;
        t.start();
        constexpr int kRuns = 100;
        for (int i = 0; i < kRuns; ++i) {
            store.rebuild(kT0 + 15000, 10.0f);
        }
        const double perCallUs =
            static_cast<double>(t.nsecsElapsed()) / kRuns / 1000.0;

        qDebug("Verwaltung: %.1f us je Durchlauf (%lld Eintraege)",
              perCallUs, static_cast<long long>(store.entries().size()));

        QVERIFY2(store.entries().size() >= 10,
                 "die Messung muss auf einer gefuellten Liste messen");
        QVERIFY2(perCallUs < 20000.0,
                 qPrintable(QString("Verwaltung braucht %1 us — da ist etwas kaputt")
                                .arg(perCallUs, 0, 'f', 1)));
    }

    // Der Fall, der im Betrieb der haeufigste ist: das Merkmal ist aus.
    // Dann darf gar nichts passieren — und weil die Pruefung in
    // SpectrumWidget::updateSignalHistory vor allem anderen steht, ist
    // hier nur zu zeigen, dass eine leere Liste nichts kostet.
    void anEmptyBandIsCheap()
    {
        const QVector<float> quiet(kBins, kNoiseDbm);

        QElapsedTimer t;
        t.start();
        constexpr int kRuns = 200;
        for (int i = 0; i < kRuns; ++i) {
            detectVoiceSignals(quiet, kCenterMhz, kBandwidthMhz, {}, kNoiseDbm,
                               QStringLiteral("USB"));
        }
        const double perCallUs =
            static_cast<double>(t.nsecsElapsed()) / kRuns / 1000.0;

        qDebug("Erkenner auf leerem Band: %.1f us je Bild", perCallUs);
        QVERIFY(perCallUs < 20000.0);
    }
};

QTEST_MAIN(TestSignalHistoryCost)
#include "tst_signal_history_cost.moc"
