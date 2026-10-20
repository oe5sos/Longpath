// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_ltas_measures_the_chain.cpp  (Longpath)
// =================================================================
// Die Messung, auf der das Bild „was macht die Kette" steht.
//
// Ein Bild, das aus einer falschen Messung kommt, ist schlimmer als
// keines: es sieht ueberzeugend aus. Deshalb wird hier nicht
// geprueft, ob etwas gezeichnet wird, sondern ob die Zahlen stimmen
// -- gegen einen Filter mit bekannter Antwort.
//
// Modification history (Longpath):
//   2026-10-09 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/strip/ClientEq.h"
#include "core/strip/EqBandLayout.h"
#include "core/strip/LtasSpectrum.h"
#include "core/strip/StripChain.h"

#include <cmath>
#include <random>
#include <vector>

using namespace Longpath;

namespace {

constexpr int kRate = 48000;

// Rosa genug: weisses Rauschen durch einen Einpol-Tiefpass. Als
// Pruefsignal besser als ein Ton, weil ein Langzeitspektrum ueber
// ALLE Baender etwas aussagen soll.
std::vector<float> noise(int frames)
{
    std::mt19937 rng(12345);
    std::normal_distribution<float> n(0.0f, 0.2f);
    std::vector<float> x(static_cast<size_t>(frames));
    float z = 0.0f;
    for (size_t i = 0; i < x.size(); ++i) {
        z = 0.85f * z + 0.15f * n(rng);
        x[i] = z * 4.0f;
    }
    return x;
}

double levelAt(const std::vector<double>& spec, double hz)
{
    const std::vector<double>& grid = Ltas::centresHz();
    size_t best = 0;
    double bestD = 1e9;
    for (size_t i = 0; i < grid.size() && i < spec.size(); ++i) {
        const double d = std::abs(std::log2(grid[i] / hz));
        if (d < bestD) { bestD = d; best = i; }
    }
    return spec.at(best);
}

} // namespace

class TstLtasMeasuresTheChain : public QObject
{
    Q_OBJECT
private slots:

    void theGridCoversTheVoiceBand()
    {
        const std::vector<double>& g = Ltas::centresHz();
        QVERIFY(!g.empty());
        QVERIFY(std::abs(g.front() - 50.0) < 0.01);
        QVERIFY(g.back() >= 7000.0);
        // Streng steigend -- sonst zeichnet die Kurve Schleifen.
        for (size_t i = 1; i < g.size(); ++i) { QVERIFY(g[i] > g[i - 1]); }
    }

    void tooLittleMaterialIsNoMeasurement()
    {
        const std::vector<float> x = noise(1000);
        QVERIFY(Ltas::analyse(x.data(), int(x.size()), kRate).empty());
    }

    // Der eigentliche Fall: ein Filter mit BEKANNTER Antwort, und die
    // Differenz der beiden Langzeitspektren muss sie wiederfinden.
    void theDifferenceFindsAKnownFilter()
    {
        StripChain c;
        c.prepare(double(kRate));
        c.setEnabled(true);
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
        }
        EqBandLayout::ensureSeeded(c.eq());
        for (int i = 0; i < EqBandLayout::kBandCount; ++i) {
            ClientEq::BandParams p = c.eq().band(i);
            p.gainDb = 0.0f;
            p.enabled = false;
            c.eq().setBand(i, p);
        }
        // Eine Glocke: +10 dB bei 2 kHz, Guete 2.
        ClientEq::BandParams bell = c.eq().band(5);
        bell.type = ClientEq::FilterType::Peak;
        bell.freqHz = 2000.0f;
        bell.gainDb = 10.0f;
        bell.q = 2.0f;
        bell.enabled = true;
        c.eq().setBand(5, bell);
        c.setStageEnabled(StripChain::Stage::Eq, true);

        std::vector<float> in = noise(4 * kRate);
        std::vector<float> out = in;
        for (size_t at = 0; at + 64 <= out.size(); at += 64) {
            c.processMono(out.data() + at, 64);
        }

        const std::vector<double> a =
            Ltas::analyse(in.data(), int(in.size()), kRate);
        const std::vector<double> b =
            Ltas::analyse(out.data(), int(out.size()), kRate);
        QVERIFY(!a.empty());
        QCOMPARE(a.size(), b.size());

        const double lift = levelAt(b, 2000.0) - levelAt(a, 2000.0);
        qInfo() << "Anhebung bei 2 kHz, gemessen aus der Differenz:"
                << lift << "dB";
        QVERIFY2(lift > 6.0,
                 qPrintable(QStringLiteral("die Differenz findet die "
                                           "Glocke nicht: %1 dB").arg(lift)));

        // Und eine Oktave darunter darf fast nichts passieren -- sonst
        // misst das Raster nicht, sondern verschmiert.
        const double aside = levelAt(b, 500.0) - levelAt(a, 500.0);
        QVERIFY2(std::abs(aside) < 3.0,
                 qPrintable(QStringLiteral("bei 500 Hz %1 dB, obwohl die "
                                           "Glocke bei 2 kHz sitzt")
                                .arg(aside)));
    }
};

QTEST_APPLESS_MAIN(TstLtasMeasuresTheChain)
#include "tst_ltas_measures_the_chain.moc"
