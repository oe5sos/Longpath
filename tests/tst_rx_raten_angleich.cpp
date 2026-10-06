// Der Driftausgleich am Tonweg — die Verdrahtung, nicht das Regelgesetz.
//
// Das Gesetz selbst steht in tst_audio_rate_matcher. Hier zaehlen die
// Zusagen der Verdrahtung, und die erste ist die wichtigste:
//
//   * Meldet der Bus "Fuellstand unbekannt" (-1), gehen die Rahmen
//     UNVERAENDERT durch — derselbe Zeiger, nichts kopiert, nichts
//     gerechnet. Nicht jeder Hintergrund hat einen Ring, den man fragen
//     kann (HAL-Shm, PipeWire, FIFO). Wer -1 als 0 liest, sieht einen
//     dauerhaft leeren Puffer und regelt dagegen an, bis der Ton
//     verstimmt ist.
//
//   * Vor Ablauf der Anlaufzeit wird ebenfalls durchgereicht. Ein
//     Umtaster, der auf die ersten Bloecke nach dem Start anspringt,
//     regelt auf den Anlauf und nicht auf die Drift.
//
//   * Laeuft er, gleicht er die Drift wirklich aus: der Fuellstand des
//     Busses bleibt an der Zielverzoegerung stehen, statt ueber Stunden
//     davonzulaufen. Geprueft gegen eine virtuelle Uhr, mit der
//     Gegenprobe ohne Ausgleich daneben.

#include "core/audio/RxRatenAngleich.h"

#include <QtTest>

#include <cmath>
#include <vector>

using Longpath::RxRatenAngleich;

namespace {

constexpr int kRate   = 48000;
constexpr int kBlock  = 480;        // 10 ms
constexpr int kRing   = 4800;       // 100 ms, wie PortAudioBus
constexpr int kZiel   = kRing / 2;  // halber Ring -- Betreiberentscheidung

std::vector<float> stereoBlock(int rahmen, float wert = 0.25f)
{
    std::vector<float> v(static_cast<size_t>(rahmen) * 2);
    for (int i = 0; i < rahmen; ++i) {
        // Ein Ton, damit ein Umtaster ueberhaupt etwas zu tun hat.
        const float s = wert * std::sin(2.0f * float(M_PI) * 1000.0f * i / kRate);
        v[static_cast<size_t>(i) * 2 + 0] = s;
        v[static_cast<size_t>(i) * 2 + 1] = s;
    }
    return v;
}

struct Lauf {
    double minFuell{1e9}, maxFuell{-1e9}, endeFuell{0};
    long   ueber{0}, leer{0};
    double verhaeltnis{1.0};
};

/// Faehrt einen Bus mit driftendem Verbraucher.
/// `ppm` ist die Abweichung des GERAETS vom Erzeuger.
Lauf fahre(double ppm, long bloecke, bool mitAusgleich)
{
    RxRatenAngleich a(kRate, 2);
    const std::vector<float> block = stereoBlock(kBlock);

    double fuellung = kZiel;
    double schuld = 0.0;
    Lauf r;
    for (long i = 0; i < bloecke; ++i) {
        const qint64 gemeldet = mitAusgleich
            ? static_cast<qint64>(std::llround(fuellung)) : -1;
        const auto aus = a.verarbeite(block.data(), kBlock, gemeldet, kRing);
        fuellung += aus.anzahl;
        if (fuellung > kRing) { fuellung = kRing; r.ueber++; }

        // Das Geraet holt sich seine Rahmen in SEINEM Takt.
        schuld += kBlock * (1.0 + ppm * 1e-6);
        const int holt = static_cast<int>(schuld);
        schuld -= holt;
        if (fuellung < holt) { fuellung = 0; r.leer++; }
        else                 { fuellung -= holt; }

        if (i > bloecke / 10) {
            r.minFuell = std::min(r.minFuell, fuellung);
            r.maxFuell = std::max(r.maxFuell, fuellung);
        }
    }
    r.endeFuell = fuellung;
    r.verhaeltnis = a.verhaeltnis();
    return r;
}

}  // namespace

class TstRxRatenAngleich : public QObject
{
    Q_OBJECT

private slots:
    void unbekannterFuellstandGehtUnveraendertDurch();
    void vorDemAnlaufWirdDurchgereicht();
    void ohneAusgleichLaeuftDerPufferLeer();     // Gegenprobe
    void mitAusgleichBleibtErStehen();
    void unsinnGehtUnveraendertDurch();
    void dieMeldungLaeuftNichtZu();
};

void TstRxRatenAngleich::unbekannterFuellstandGehtUnveraendertDurch()
{
    // DER wichtigste Punkt. -1 heisst "weiss ich nicht", nicht "leer".
    RxRatenAngleich a(kRate, 2);
    const std::vector<float> block = stereoBlock(kBlock);
    for (int i = 0; i < 2000; ++i) {
        const auto aus = a.verarbeite(block.data(), kBlock, -1, kRing);
        QCOMPARE(aus.anzahl, kBlock);
        // Derselbe Zeiger: nicht kopiert, nicht gerechnet.
        QCOMPARE(aus.rahmen, block.data());
    }
    QCOMPARE(a.versatz(), qint64(0));
    QVERIFY(!a.regeltSchon());
}

void TstRxRatenAngleich::vorDemAnlaufWirdDurchgereicht()
{
    RxRatenAngleich a(kRate, 2);
    const std::vector<float> block = stereoBlock(kBlock);
    // Anlaufzeit sind drei Sekunden = 300 Bloecke zu 10 ms.
    for (int i = 0; i < 200; ++i) {
        const auto aus = a.verarbeite(block.data(), kBlock, kZiel, kRing);
        QCOMPARE(aus.rahmen, block.data());
        QCOMPARE(aus.anzahl, kBlock);
    }
    QVERIFY(!a.regeltSchon());

    for (int i = 0; i < 200; ++i) { a.verarbeite(block.data(), kBlock, kZiel, kRing); }
    QVERIFY(a.regeltSchon());
}

void TstRxRatenAngleich::ohneAusgleichLaeuftDerPufferLeer()
{
    // Gegenprobe. Ohne sie belegt das gruene Ergebnis unten nichts.
    // +4 ppm heisst: das Geraet holt schneller, als nachkommt.
    // 100 ppm statt der gemessenen 4: dieselbe Aussage, fuenfundzwanzigmal
    // schneller. Bei 4 ppm braucht der Puffer 1,25 Mio Bloecke (3,5 h
    // simuliert), bis er leer ist -- der Stand lief damit 92 s und waere
    // auf einem langsameren CI-Rechner in den Zeitdeckel gelaufen. Billige
    // USB-Tonkarten liegen ohnehin in dieser Groessenordnung daneben.
    const Lauf r = fahre(100.0, 80000, /*mitAusgleich=*/false);
    qInfo("ohne Ausgleich, +100 ppm: Ende %.0f Rahmen, Leerlaeufe %ld",
          r.endeFuell, r.leer);
    QVERIFY2(r.leer > 0,
             "Ohne Ausgleich muesste der Puffer leerlaufen -- tut er es "
             "nicht, ist die Simulation zu gutmuetig und beweist nichts.");
}

void TstRxRatenAngleich::mitAusgleichBleibtErStehen()
{
    for (double ppm : {4.0, -4.0}) {
        const Lauf r = fahre(ppm, 200000, /*mitAusgleich=*/true);
        qInfo("%+.0f ppm: Fuellstand %.0f..%.0f (Ziel %d), Ende %.0f, "
              "var %.9f, Ueber %ld Leer %ld",
              ppm, r.minFuell, r.maxFuell, kZiel, r.endeFuell,
              r.verhaeltnis, r.ueber, r.leer);
        QCOMPARE(r.ueber, 0L);
        QCOMPARE(r.leer, 0L);
        // Die Zielverzoegerung ist der halbe Ring (Betreiberentscheidung
        // 2026-10-06, wie Thetis). Ein paar Millisekunden Spiel sind in
        // Ordnung; zwanzig waeren ein Fuenftel des Rings.
        QVERIFY2(std::abs(r.endeFuell - kZiel) < 0.1 * kRing,
                 qPrintable(QString::number(r.endeFuell, 'f', 0)));
    }
}

void TstRxRatenAngleich::unsinnGehtUnveraendertDurch()
{
    RxRatenAngleich a(kRate, 2);
    const std::vector<float> block = stereoBlock(kBlock);
    QCOMPARE(a.verarbeite(nullptr, kBlock, kZiel, kRing).anzahl, 0);
    QCOMPARE(a.verarbeite(block.data(), 0, kZiel, kRing).anzahl, 0);
    // Ringgroesse 0: es gibt kein Ziel, also nichts zu regeln.
    const auto aus = a.verarbeite(block.data(), kBlock, kZiel, 0);
    QCOMPARE(aus.rahmen, block.data());
    QCOMPARE(aus.anzahl, kBlock);
}

void TstRxRatenAngleich::dieMeldungLaeuftNichtZu()
{
    // "Im Auge behalten" geht nur, wenn man etwas sieht -- aber eine Zeile
    // je Tonblock waeren HUNDERT JE SEKUNDE, und ein Protokoll, das
    // zulaeuft, liest niemand. Genau diese beiden Fehler haelt der Stand
    // auseinander.
    RxRatenAngleich a(kRate, 2);
    const std::vector<float> block = stereoBlock(kBlock);

    // Vor der Anlaufzeit: gar nichts. Eine Zeile "Verhaeltnis 1,0" waere
    // eine Aussage ueber nichts -- da regelt noch niemand.
    std::int64_t t = 1'000'000;
    for (int i = 0; i < 100; ++i) {
        a.verarbeite(block.data(), kBlock, kZiel, kRing);
        QVERIFY2(a.protokollZeile(t).isEmpty(), "vor dem Anlauf gemeldet");
        t += 10;
    }

    // Anlauf vorbei.
    for (int i = 0; i < 400; ++i) { a.verarbeite(block.data(), kBlock, kZiel, kRing); }
    QVERIFY(a.regeltSchon());

    const QString erste = a.protokollZeile(t);
    QVERIFY2(!erste.isEmpty(), "nach dem Anlauf kam keine Zeile");
    QVERIFY2(erste.contains(QStringLiteral("Verhaeltnis")), qPrintable(erste));
    QVERIFY2(erste.contains(QStringLiteral("ppm")), qPrintable(erste));
    QVERIFY2(erste.contains(QStringLiteral("Versatz")), qPrintable(erste));

    // Danach eine Minute lang Ruhe -- auch bei tausend Aufrufen.
    for (int i = 0; i < 1000; ++i) {
        t += 50;
        QVERIFY2(a.protokollZeile(t).isEmpty(),
                 "innerhalb der Minute wurde ein zweites Mal gemeldet");
    }
    // Und nach der Minute wieder genau eine.
    t += 60'000;
    QVERIFY(!a.protokollZeile(t).isEmpty());
    QVERIFY(a.protokollZeile(t).isEmpty());
}

QTEST_MAIN(TstRxRatenAngleich)
#include "tst_rx_raten_angleich.moc"
