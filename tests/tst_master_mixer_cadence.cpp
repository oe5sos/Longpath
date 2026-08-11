// =================================================================
// tests/tst_master_mixer_cadence.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// The offline cadence simulation that found the TX-monitor crackle,
// pinned down as a regression test.
//
// History, because the numbers matter: the 2026-08-11 bench reported
// scratching in the self-monitor that four successive point fixes did
// not cure. This simulation reproduced it exactly — a 64-frame
// opportunistic producer against 1024-frame barrier-paced drains, with
// realistic UDP jitter, starved the monitor slot ~5×/s with the fixed
// 256-frame cushion (50 discontinuities per 30 s; σ=8 ms WLAN clumping
// gave 161). The adaptive cushion (doubles on starvation, never probed
// back down) plus seam fades (gain re-enters through the ramp at every
// prime/trim, residue plays out as a fade at starvation) converge
// every profile to ZERO steady-state discontinuities.
//
// This test replays those profiles with fixed seeds and asserts the
// converged state stays converged:
//   - no discontinuities at all in the second half of every run;
//   - a small bounded number of (faded, but detectable-by-amplitude)
//     seams during the first-half adaptation.
//
// The detector is amplitude-based: a 1 kHz sine at A=0.5 has a maximum
// natural sample-to-sample step of A·ω ≈ 0.065; anything above 0.15 is
// a seam the fades failed to mask.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>

#include "core/audio/MasterMixer.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>
#include <vector>

using NereusSDR::MasterMixer;

namespace {

struct CadenceResult {
    int clicks = 0;       // amplitude discontinuities, whole run
    int lateClicks = 0;   // ... in the second half (steady state)
    int drains = 0;
};

// One simulated session: monitor slot (-2, opportunistic) produces
// 64-frame blocks of a 1 kHz sine; an RX slice (1, barrier member)
// produces silent 1024-frame blocks whose arrival paces tryDrain —
// exactly AudioEngine::rxBlockReady's shape.
CadenceResult runCadence(int seconds, double monJitterMs, double rxJitterMs,
                         double driftPpm, unsigned seed)
{
    MasterMixer mix;
    mix.setSliceGain(1, 1.0f, 0.0f);
    mix.setSliceGain(-2, 1.0f, 0.0f);
    mix.setSliceOpportunistic(-2, true);

    std::mt19937 rng(seed);
    std::normal_distribution<double> monJit(0.0, monJitterMs);
    std::normal_distribution<double> rxJit(0.0, rxJitterMs);

    const double blockMs = 64.0 / 48.0;    // one producer block
    const double rxMs    = 1024.0 / 48.0;  // one drain period
    const double tEnd    = seconds * 1000.0;

    // Event timeline: (time, type). type 0 = monitor block, 1 = RX
    // block + drain. Jitter displaces each event independently, which
    // is harsher than reality (no reordering constraint) — good.
    std::vector<std::pair<double, int>> events;
    const double monPeriod = blockMs * (1.0 - driftPpm * 1e-6);
    for (double t = 0.0; t < tEnd; t += monPeriod) {
        events.push_back({t + monJit(rng), 0});
    }
    for (double t = 7.0; t < tEnd; t += rxMs) {
        events.push_back({t + rxJit(rng), 1});
    }
    std::sort(events.begin(), events.end());

    std::vector<float> mon(64 * 2);
    std::vector<float> rx(1024 * 2, 0.0f);
    std::vector<float> out(1024 * 2);

    double phase = 0.0;
    const double w = 2.0 * M_PI * 1000.0 / 48000.0;
    const double amplitude = 0.5;

    CadenceResult r;
    float prev = 0.0f;
    bool  have = false;

    for (const auto& ev : events) {
        if (ev.second == 0) {
            for (int i = 0; i < 64; ++i) {
                const float v = static_cast<float>(amplitude * std::sin(phase));
                phase += w;
                mon[static_cast<size_t>(2 * i) + 0] = v;
                mon[static_cast<size_t>(2 * i) + 1] = v;
            }
            mix.accumulate(-2, mon.data(), 64);
        } else {
            mix.accumulate(1, rx.data(), 1024);
            const int n = mix.tryDrain(out.data(), 1024);
            ++r.drains;
            for (int i = 0; i < n; ++i) {
                const float v = out[static_cast<size_t>(2 * i)];
                if (have && std::fabs(v - prev) > 0.15f) {
                    ++r.clicks;
                    if (ev.first > tEnd / 2.0) { ++r.lateClicks; }
                }
                prev = v;
                have = true;
            }
        }
    }
    return r;
}

} // namespace

class TstMasterMixerCadence : public QObject {
    Q_OBJECT

private slots:

    void profiles_data()
    {
        QTest::addColumn<double>("monJitterMs");
        QTest::addColumn<double>("rxJitterMs");
        QTest::addColumn<double>("driftPpm");
        QTest::addColumn<int>("maxTotalClicks");

        // Bounds are generous multiples of the observed values (0-2 per
        // 120 s), so the test flags regressions, not noise.
        QTest::newRow("lockstep")        << 0.0 << 0.0 <<    0.0 << 2;
        QTest::newRow("light-jitter")    << 0.5 << 1.0 <<    0.0 << 6;
        QTest::newRow("network-jitter")  << 1.0 << 3.0 <<    0.0 << 8;
        QTest::newRow("wlan-clumping")   << 2.0 << 8.0 <<    0.0 << 8;
        QTest::newRow("drift-plus")      << 0.5 << 1.0 <<  100.0 << 6;
        QTest::newRow("drift-minus")     << 0.5 << 1.0 << -100.0 << 6;
    }

    void profiles()
    {
        QFETCH(double, monJitterMs);
        QFETCH(double, rxJitterMs);
        QFETCH(double, driftPpm);
        QFETCH(int, maxTotalClicks);

        const CadenceResult r =
            runCadence(120, monJitterMs, rxJitterMs, driftPpm, 42u);

        QVERIFY2(r.drains > 5000,
                 qPrintable(QStringLiteral("only %1 drains — the timeline "
                                           "generator broke").arg(r.drains)));

        // The converged state is the contract: once the cushion has
        // adapted to the link, the monitor must be seam-free.
        QCOMPARE(r.lateClicks, 0);

        // Adaptation itself is allowed a handful of faded seams.
        QVERIFY2(r.clicks <= maxTotalClicks,
                 qPrintable(QStringLiteral("%1 discontinuities (bound %2) — "
                                           "adaptation regressed")
                                .arg(r.clicks).arg(maxTotalClicks)));
    }

    // The pathological pre-fix geometry, pinned so nobody "simplifies"
    // the cushion back out: with the adaptive cushion DISABLED by
    // resetting the slot each period there would be constant shredding.
    // Here we only assert the fixed configuration stays healthy under
    // a jitter profile 2× the WLAN case — the cushion may grow to its
    // bound but the steady state must still be clean.
    void extremeJitterStillConverges()
    {
        const CadenceResult r = runCadence(120, 4.0, 12.0, 0.0, 7u);
        QCOMPARE(r.lateClicks, 0);
    }
};

QTEST_MAIN(TstMasterMixerCadence)
#include "tst_master_mixer_cadence.moc"
