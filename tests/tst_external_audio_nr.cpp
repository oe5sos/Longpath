// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_external_audio_nr.cpp  (Longpath)
// =================================================================
// Rauschminderung am Rechner fuer fremden Ton (Flex, Kiwi, SunSDR).
//
// Der Anlass: NR3 lief hier nie -- die rnnoise-Stufe verwirft Stereo
// still, und der Ton kam unveraendert hinten heraus. "Uebersetzt und
// stuerzt nicht ab" faellt so etwas nicht auf, darum wird GEMESSEN:
//
//   1. NR2, NR3, NR4: Stereo-Rauschen hinein, deutlich weniger hinaus,
//      beide Kanaele gleich -- in ungleich langen Stuecken, weil NR2 und
//      NR4 in festen Bloecken rechnen.
//   2. Aus (und NR1, die Geraete-NR): Bit fuer Bit unveraendert.
//
// Modification history (Longpath):
//   2026-11-25 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/ExternalAudioNr.h"
#include "core/strip/NoiseReduction.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using namespace Longpath;

namespace {

constexpr int kRate = 48000;
constexpr int kSeconds = 4;

std::vector<float> stereoNoise()
{
    std::mt19937 rng(20261125);
    std::uniform_real_distribution<float> d(-0.05f, 0.05f);
    std::vector<float> v(static_cast<size_t>(kRate * kSeconds * 2));
    for (size_t i = 0; i < v.size(); i += 2) { v[i] = v[i + 1] = d(rng); }
    return v;
}

// Wie im Tonweg: Stuecke verschiedener Laenge, auch laenger als ein Block.
void run(ExternalAudioNr& nr, std::vector<float>& io)
{
    const int lengths[] = {128, 480, 37, 1500};
    const int frames = static_cast<int>(io.size() / 2);
    for (int pos = 0, k = 0; pos < frames; ++k) {
        const int n = std::min(lengths[k % 4], frames - pos);
        nr.process(io.data() + 2 * pos, n);
        pos += n;
    }
}

// Pegel der letzten Sekunde -- vorher lernen die Schaetzer noch.
double lastSecondRms(const std::vector<float>& v)
{
    double s = 0.0;
    const size_t from = v.size() - static_cast<size_t>(kRate * 2);
    for (size_t i = from; i < v.size(); ++i) { s += double(v[i]) * v[i]; }
    return std::sqrt(s / double(v.size() - from));
}

} // namespace

class TstExternalAudioNr : public QObject {
    Q_OBJECT

private slots:
    void noiseIsRemoved_data()
    {
        QTest::addColumn<int>("slot");
        QTest::addColumn<double>("minDb");
        QTest::newRow("NR2") << int(NrSlot::NR2) << 10.0;
        QTest::newRow("NR3") << int(NrSlot::NR3) << 10.0;
        QTest::newRow("NR4") << int(NrSlot::NR4) << 5.0;
    }

    void noiseIsRemoved()
    {
        QFETCH(int, slot);
        QFETCH(double, minDb);

        ExternalAudioNr nr;
        nr.setSlot(static_cast<NrSlot>(slot));
        if (!nr.isActive()) {
            QSKIP("Verfahren in diesem Bau nicht verfuegbar (WDSP/Modell)");
        }
        const auto in = stereoNoise();
        auto out = in;
        run(nr, out);

        const double db =
            20.0 * std::log10(lastSecondRms(in) / (lastSecondRms(out) + 1e-12));
        qInfo() << "Daempfung" << db << "dB";
        QVERIFY2(db >= minDb, qPrintable(QString::number(db)));
        for (size_t i = 0; i < out.size(); i += 2) {
            QCOMPARE(out[i], out[i + 1]);
        }
    }

    void offAndNr1PassThrough()
    {
        for (const NrSlot slot : {NrSlot::Off, NrSlot::NR1}) {
            ExternalAudioNr nr;
            nr.setSlot(slot);
            const auto in = stereoNoise();
            auto out = in;
            run(nr, out);
            QVERIFY(out == in);
        }
    }
};

QTEST_GUILESS_MAIN(TstExternalAudioNr)
#include "tst_external_audio_nr.moc"
