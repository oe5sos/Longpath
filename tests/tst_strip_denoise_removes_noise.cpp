// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_strip_denoise_removes_noise.cpp  (Longpath)
// =================================================================
// Die neunte Stufe: Entrauschen im Sendeweg.
//
// Ein Prüfstand, der nur „die Stufe übersetzt und stürzt nicht ab"
// sagt, ist bei einem neuronalen Netz nichts wert -- ein Netz, das
// nichts tut, besteht ihn auch. Hier wird deshalb GEMESSEN:
//
//   1. Rauschen hinein, und hinten muss deutlich weniger ankommen.
//   2. Bei Mischung 0 muss das Eingangssignal unverändert wieder
//      herauskommen -- nur um die Blocklänge verzögert. Das ist der
//      Beweis, dass der trockene Weg heil ist; ohne ihn löschen sich
//      trocken und entrauscht in der Mischung gegenseitig aus.
//   3. Ausgeschaltet heisst Bit für Bit unverändert, ohne Verzögerung.
//
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/strip/NoiseReduction.h"

#include <cmath>
#include <random>
#include <vector>

using namespace Longpath;

namespace {

constexpr int kRate  = 48000;
constexpr int kBlock = 240;     // zwei Blöcke je rnnoise-Rahmen

std::vector<float> whiteNoise(int n, float amp)
{
    std::mt19937 rng(20261010);
    std::uniform_real_distribution<float> d(-amp, amp);
    std::vector<float> v(static_cast<size_t>(n));
    for (float& x : v) { x = d(rng); }
    return v;
}

double rms(const std::vector<float>& v, int from, int to)
{
    double s = 0.0;
    for (int i = from; i < to; ++i) {
        s += double(v[size_t(i)]) * double(v[size_t(i)]);
    }
    const int n = std::max(1, to - from);
    return std::sqrt(s / n);
}

// Durch die Stufe schicken, blockweise wie im Kanalzug.
std::vector<float> run(NoiseReduction& nr, const std::vector<float>& in)
{
    std::vector<float> out = in;
    for (size_t i = 0; i + kBlock <= out.size(); i += kBlock) {
        nr.process(out.data() + i, kBlock, 1);
    }
    return out;
}

} // namespace

class TstStripDenoiseRemovesNoise : public QObject {
    Q_OBJECT

private slots:
    // Rauschen rein, deutlich weniger raus. Ohne diesen Fall belegt
    // der ganze Prüfstand nur, dass nichts abstürzt.
    void steadyNoiseIsActuallyRemoved()
    {
        NoiseReduction nr;
        nr.prepare(kRate);
        if (!nr.isUsable()) {
            QSKIP("kein rnnoise-Modell im Prüflauf gefunden");
        }
        nr.setEnabled(true);
        nr.setAmount(1.0f);

        // Zwei Sekunden, damit das Netz sein Rauschprofil findet.
        const auto in  = whiteNoise(kRate * 2, 0.05f);
        const auto out = run(nr, in);

        // Die erste halbe Sekunde zählt nicht: das Netz lernt noch.
        const double before = rms(in,  kRate, kRate * 2);
        const double after  = rms(out, kRate, kRate * 2);
        const double db = 20.0 * std::log10(std::max(after, 1e-12)
                                            / std::max(before, 1e-12));

        qInfo() << "Rauschen vorher" << before << "nachher" << after
                << "=" << db << "dB; eigene Messung"
                << nr.reductionDb() << "dB";
        QVERIFY2(db < -6.0,
                 qPrintable(QStringLiteral("nur %1 dB weniger Rauschen")
                                .arg(db, 0, 'f', 1)));
        QVERIFY2(nr.reductionDb() < -3.0,
                 qPrintable(QStringLiteral("die eigene Messung sagt %1 dB")
                                .arg(double(nr.reductionDb()), 0, 'f', 1)));
    }

    // ── Mischung 0: derselbe Ton, nur spaeter ───────────────────────
    //
    // Hier stand: „Das ist der Beweis, dass trocken und entrauscht
    // gleich alt sind." Das war er nicht, und der Satz hat einen
    // Fehler ein Vierteljahr lang gedeckt: dieser Fall fasst den
    // entrauschten Weg GAR NICHT an (Mischung 0 ist trocken allein).
    // Er zeigt, dass der trockene Weg unbeschaedigt durchkommt und
    // um genau so viel verzoegert, wie die Stufe meldet -- nicht
    // mehr.
    //
    // Dass die beiden Wege gleich alt sind, zeigt allein eine
    // Kreuzkorrelation zwischen ihnen. Die steht in
    // tests/tst_bandbreiten_genau (doTheDryAndTheDenoisedPathLineUp)
    // und hat am 2026-10-10 einen Versatz von 960 Abtastwerten
    // gefunden.
    //
    // Drei Rahmen, nicht einer: rnnoise liefert seinen Ausgang zwei
    // Rahmen hinter seinem Eingang, und der trockene Anteil laeuft
    // seitdem genauso lange mit.
    void dryPathIsIntactAndDelayedByThreeFrames()
    {
        NoiseReduction nr;
        nr.prepare(kRate);
        if (!nr.isUsable()) { QSKIP("kein rnnoise-Modell"); }
        nr.setEnabled(true);
        nr.setAmount(0.0f);

        const int frame = nr.latencySamples();
        QCOMPARE(frame, 480 * 3);

        const auto in  = whiteNoise(kRate / 2, 0.2f);
        const auto out = run(nr, in);

        // Um genau drei Rahmen verschoben, und zwar exakt: 32768 ist
        // eine Zweierpotenz, mal und geteilt ist in float verlustfrei.
        int checked = 0;
        for (int i = frame; i + frame < int(in.size()) - kBlock; ++i) {
            QCOMPARE(out[size_t(i)], in[size_t(i - frame)]);
            ++checked;
        }
        QVERIFY(checked > 10000);
    }

    // Aus heisst aus: kein Rechnen, keine Verzögerung.
    void switchedOffIsBitIdentical()
    {
        NoiseReduction nr;
        nr.prepare(kRate);
        nr.setEnabled(false);

        const auto in  = whiteNoise(4800, 0.3f);
        const auto out = run(nr, in);

        QCOMPARE(nr.latencySamples(), 0);
        for (size_t i = 0; i < in.size(); ++i) {
            QCOMPARE(out[i], in[i]);
        }
    }

    // Und eine andere Abtastrate wird nicht stillschweigend
    // umgerechnet -- die Stufe sagt, dass sie nicht kann.
    void anotherSampleRateIsRefusedNotFaked()
    {
        NoiseReduction nr;
        nr.prepare(44100.0);
        QVERIFY(!nr.isUsable());

        nr.setEnabled(true);
        const auto in  = whiteNoise(2400, 0.3f);
        const auto out = run(nr, in);
        QCOMPARE(nr.latencySamples(), 0);
        for (size_t i = 0; i < in.size(); ++i) {
            QCOMPARE(out[i], in[i]);
        }
    }
};

QTEST_MAIN(TstStripDenoiseRemovesNoise)
#include "tst_strip_denoise_removes_noise.moc"
