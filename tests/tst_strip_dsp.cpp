// The Aetherial Audio Channel Strip's DSP, checked as a port rather
// than as a design.
//
// These nine stages are AetherSDR's, unchanged apart from the namespace.
// So the question this file answers is not "is the compressor's curve
// right" — that is upstream's business and upstream's tests — but "did
// the port arrive intact and does it behave the same way on this
// machine". A port that compiles proves nothing: every one of these has
// atomics, a version counter and an audio-thread cache, and a
// transcription error in any of them produces a stage that is silently
// wrong rather than one that fails to build.
//
// So: every stage gets the same four questions asked of it, and the
// gate — the first stage in the chain and the one with the most state —
// gets its documented behaviour checked as well.
//
// The four:
//   disabled is bit-exact passthrough   (a bypass that isn't bit-exact
//                                        is not a bypass)
//   silence in gives silence out        (no self-oscillation, no DC)
//   nothing produces NaN or infinity    (one NaN reaches the radio and
//                                        stays there)
//   a long run stays bounded            (no slow integrator wind-up)
// no-port-check: the ported files carry their own citation headers.

#include <QtTest/QtTest>

#include "core/strip/ClientComp.h"
#include "core/strip/ClientDeEss.h"
#include "core/strip/ClientEq.h"
#include "core/strip/ClientFinalLimiter.h"
#include "core/strip/ClientGate.h"
#include "core/strip/ClientPhaseRotator.h"
#include "core/strip/ClientPudu.h"
#include "core/strip/ClientReverb.h"
#include "core/strip/ClientTube.h"

#include <cmath>
#include <functional>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr double kRate   = 48000.0;
constexpr int    kFrames = 512;

std::vector<float> tone(double hz, float amp, int frames, int channels)
{
    std::vector<float> v(static_cast<size_t>(frames * channels), 0.0f);
    for (int i = 0; i < frames; ++i) {
        const float s = amp * float(std::sin(2.0 * M_PI * hz * i / kRate));
        for (int c = 0; c < channels; ++c) {
            v[static_cast<size_t>(i * channels + c)] = s;
        }
    }
    return v;
}

bool allFinite(const std::vector<float>& v)
{
    for (float s : v) { if (!std::isfinite(s)) { return false; } }
    return true;
}

float peak(const std::vector<float>& v)
{
    float p = 0.0f;
    for (float s : v) { p = std::max(p, std::fabs(s)); }
    return p;
}

// One stage, reduced to the calls every stage shares.
//
// All nine have setEnabled(). An earlier version of this file said
// reverb and the limiter did not, because a grep missed them: both are
// declared with two spaces after `void`. They were therefore excluded
// from the bit-exact-bypass test — the one test that most needed to
// cover them, since a reverb whose bypass leaks is audible immediately.
struct Stage {
    const char* name;
    std::function<void(double)>              prepare;
    std::function<void(bool)>                setEnabled;
    std::function<void(float*, int, int)>    process;
    std::function<void()>                    reset;
};

} // namespace

class TstStripDsp : public QObject {
    Q_OBJECT
private slots:
    void every_stage_passes_through_untouched_when_disabled();
    void every_stage_turns_silence_into_silence();
    void no_stage_ever_produces_a_nan();
    void no_stage_winds_up_over_a_long_run();

    void the_gate_opens_above_threshold_and_closes_below();
    void the_gate_has_hysteresis();
    void the_gate_mode_snaps_ratio_and_range();
    void the_gate_floor_bounds_the_attenuation();
};

// ── The four questions, asked of everything ──────────────────────────

namespace {

// Built fresh per test: these hold audio-thread state and a stage that
// has already seen signal is not the same stage.
std::vector<Stage> makeStages(ClientGate& gate, ClientEq& eq,
                              ClientDeEss& dess, ClientComp& comp,
                              ClientTube& tube, ClientPudu& pudu,
                              ClientReverb& verb, ClientFinalLimiter& lim,
                              ClientPhaseRotator& rot)
{
    std::vector<Stage> v;
    v.push_back({"gate",
        [&](double r){ gate.prepare(r); },
        [&](bool on){ gate.setEnabled(on); },
        [&](float* b, int f, int c){ gate.process(b, f, c); },
        [&](){ gate.reset(); }});
    v.push_back({"eq",
        [&](double r){ eq.prepare(r); },
        [&](bool on){ eq.setEnabled(on); },
        [&](float* b, int f, int c){ eq.process(b, f, c); },
        [&](){ eq.reset(); }});
    v.push_back({"de-esser",
        [&](double r){ dess.prepare(r); },
        [&](bool on){ dess.setEnabled(on); },
        [&](float* b, int f, int c){ dess.process(b, f, c); },
        [&](){ dess.reset(); }});
    v.push_back({"compressor",
        [&](double r){ comp.prepare(r); },
        [&](bool on){ comp.setEnabled(on); },
        [&](float* b, int f, int c){ comp.process(b, f, c); },
        [&](){ comp.reset(); }});
    v.push_back({"tube",
        [&](double r){ tube.prepare(r); },
        [&](bool on){ tube.setEnabled(on); },
        [&](float* b, int f, int c){ tube.process(b, f, c); },
        [&](){ tube.reset(); }});
    v.push_back({"pudu",
        [&](double r){ pudu.prepare(r); },
        [&](bool on){ pudu.setEnabled(on); },
        [&](float* b, int f, int c){ pudu.process(b, f, c); },
        [&](){ pudu.reset(); }});
    v.push_back({"phase rotator",
        [&](double r){ rot.prepare(r); },
        [&](bool on){ rot.setEnabled(on); },
        [&](float* b, int f, int c){ rot.process(b, f, c); },
        [&](){ rot.reset(); }});
    v.push_back({"reverb",
        [&](double r){ verb.prepare(r); },
        [&](bool on){ verb.setEnabled(on); },
        [&](float* b, int f, int c){ verb.process(b, f, c); },
        [&](){ verb.reset(); }});
    v.push_back({"final limiter",
        [&](double r){ lim.prepare(r); },
        [&](bool on){ lim.setEnabled(on); },
        [&](float* b, int f, int c){ lim.process(b, f, c); },
        [&](){ lim.reset(); }});
    return v;
}

} // namespace

#define STAGES                                                              \
    ClientGate gate; ClientEq eq; ClientDeEss dess; ClientComp comp;        \
    ClientTube tube; ClientPudu pudu; ClientReverb verb;                    \
    ClientFinalLimiter lim; ClientPhaseRotator rot;                         \
    auto stages = makeStages(gate, eq, dess, comp, tube, pudu, verb, lim, rot)

void TstStripDsp::every_stage_passes_through_untouched_when_disabled()
{
    STAGES;
    for (const Stage& s : stages) {
        s.prepare(kRate);
        s.setEnabled(false);

        const std::vector<float> in = tone(700.0, 0.3f, kFrames, 2);
        std::vector<float> out = in;
        s.process(out.data(), kFrames, 2);

        // Bit-exact, not approximately. A bypass that changes the last
        // bit is a bypass that will one day change more than that, and
        // the operator's A/B comparison silently stops being one.
        for (size_t i = 0; i < in.size(); ++i) {
            QVERIFY2(in[i] == out[i],
                     qPrintable(QStringLiteral("%1 altered sample %2 while "
                                               "disabled").arg(s.name).arg(i)));
        }
    }
}

void TstStripDsp::every_stage_turns_silence_into_silence()
{
    STAGES;
    for (const Stage& s : stages) {
        s.prepare(kRate);
        if (s.setEnabled) { s.setEnabled(true); }
        s.reset();

        std::vector<float> buf(static_cast<size_t>(kFrames * 2), 0.0f);
        // Several blocks: a stage that self-oscillates usually needs a
        // few passes round its own feedback path before it shows.
        for (int b = 0; b < 20; ++b) { s.process(buf.data(), kFrames, 2); }

        QVERIFY2(peak(buf) < 1e-6f,
                 qPrintable(QStringLiteral("%1 made %2 out of silence")
                                .arg(s.name).arg(peak(buf))));
    }
}

void TstStripDsp::no_stage_ever_produces_a_nan()
{
    STAGES;
    // Full-scale, DC, and a near-zero denormal-bait level. One NaN in
    // the transmit path does not go away: it propagates through every
    // filter's state and the radio keeps sending it.
    for (const Stage& s : stages) {
        s.prepare(kRate);
        if (s.setEnabled) { s.setEnabled(true); }

        for (float amp : {1.0f, 0.0f, 1e-20f, 0.999f}) {
            s.reset();
            std::vector<float> buf = tone(1000.0, amp, kFrames, 2);
            if (amp == 0.0f) {
                for (auto& v : buf) { v = 0.5f; }   // DC
            }
            for (int b = 0; b < 10; ++b) { s.process(buf.data(), kFrames, 2); }
            QVERIFY2(allFinite(buf),
                     qPrintable(QStringLiteral("%1 produced a non-finite "
                                               "sample at amplitude %2")
                                    .arg(s.name).arg(double(amp))));
        }
    }
}

void TstStripDsp::no_stage_winds_up_over_a_long_run()
{
    STAGES;
    // Ten seconds of speech-band tone. An integrator with a sign error
    // or a leak that never leaks passes a short test and then drifts.
    const int blocks = int(10.0 * kRate / kFrames);
    for (const Stage& s : stages) {
        s.prepare(kRate);
        if (s.setEnabled) { s.setEnabled(true); }
        s.reset();

        float worst = 0.0f;
        for (int b = 0; b < blocks; ++b) {
            std::vector<float> buf = tone(400.0, 0.5f, kFrames, 2);
            s.process(buf.data(), kFrames, 2);
            worst = std::max(worst, peak(buf));
            QVERIFY2(allFinite(buf), s.name);
        }
        // Generous: some of these have make-up gain. The point is that
        // it settles rather than climbing without limit.
        QVERIFY2(worst < 8.0f,
                 qPrintable(QStringLiteral("%1 reached %2 after ten seconds")
                                .arg(s.name).arg(double(worst))));
    }
}

// ── The gate, in its own right ───────────────────────────────────────

void TstStripDsp::the_gate_opens_above_threshold_and_closes_below()
{
    ClientGate g;
    g.prepare(kRate);
    g.setEnabled(true);
    g.setMode(ClientGate::Mode::Gate);
    g.setThresholdDb(-30.0f);
    g.setAttackMs(0.5f);
    g.setReleaseMs(20.0f);
    g.setHoldMs(0.0f);
    g.reset();

    // Loud: -6 dBFS, well above the threshold.
    for (int b = 0; b < 20; ++b) {
        std::vector<float> buf = tone(500.0, 0.5f, kFrames, 1);
        g.process(buf.data(), kFrames, 1);
    }
    QVERIFY2(g.gateOpen(), "the gate stayed shut on a -6 dBFS signal");
    QVERIFY(g.gainReductionDb() > -3.0f);

    // Quiet: -60 dBFS, well below.
    for (int b = 0; b < 200; ++b) {
        std::vector<float> buf = tone(500.0, 0.001f, kFrames, 1);
        g.process(buf.data(), kFrames, 1);
    }
    QVERIFY2(!g.gateOpen(), "the gate stayed open on a -60 dBFS signal");
    QVERIFY2(g.gainReductionDb() < -5.0f,
             qPrintable(QStringLiteral("only %1 dB of attenuation")
                            .arg(g.gainReductionDb())));
}

void TstStripDsp::the_gate_has_hysteresis()
{
    // The reason hysteresis exists: a signal sitting exactly on the
    // threshold makes a gate without it chatter open and shut at the
    // envelope rate, which is far more audible than either state.
    ClientGate g;
    g.prepare(kRate);
    g.setEnabled(true);
    g.setThresholdDb(-30.0f);
    g.setReturnDb(6.0f);
    QCOMPARE(g.returnDb(), 6.0f);

    g.reset();
    // Open it with a loud signal.
    for (int b = 0; b < 20; ++b) {
        std::vector<float> buf = tone(500.0, 0.5f, kFrames, 1);
        g.process(buf.data(), kFrames, 1);
    }
    QVERIFY(g.gateOpen());

    // Now sit just below the open threshold but above threshold-return.
    // -33 dBFS: below -30, above -36. It must stay open.
    for (int b = 0; b < 20; ++b) {
        std::vector<float> buf = tone(500.0, 0.0224f, kFrames, 1);
        g.process(buf.data(), kFrames, 1);
    }
    QVERIFY2(g.gateOpen(),
             "the gate closed inside its own hysteresis window");
}

void TstStripDsp::the_gate_mode_snaps_ratio_and_range()
{
    // The one-click Expander/Gate toggle only means anything if the two
    // presets are actually different.
    ClientGate g;
    g.setMode(ClientGate::Mode::Expander);
    const float expRatio = g.ratio();
    const float expFloor = g.floorDb();

    g.setMode(ClientGate::Mode::Gate);
    QVERIFY2(g.ratio() > expRatio, "gate mode must be the harder ratio");
    QVERIFY2(g.floorDb() < expFloor, "gate mode must be the deeper range");

    // And switching back restores the softer pair rather than leaving
    // the hard one in place.
    g.setMode(ClientGate::Mode::Expander);
    QCOMPARE(g.ratio(), expRatio);
    QCOMPARE(g.floorDb(), expFloor);
}

void TstStripDsp::the_gate_floor_bounds_the_attenuation()
{
    // The floor is what stops a gate from being a mute. An operator who
    // sets a shallow floor wants the background pushed down, not
    // removed — and a gate that ignores it sounds like a dropped
    // connection to the far end.
    ClientGate g;
    g.prepare(kRate);
    g.setEnabled(true);
    g.setThresholdDb(-20.0f);
    g.setFloorDb(-10.0f);
    g.setReleaseMs(5.0f);
    g.setHoldMs(0.0f);
    g.reset();

    for (int b = 0; b < 400; ++b) {
        std::vector<float> buf = tone(500.0, 0.0001f, kFrames, 1);
        g.process(buf.data(), kFrames, 1);
    }
    QVERIFY2(g.gainReductionDb() >= -10.5f,
             qPrintable(QStringLiteral("attenuation %1 dB went past the "
                                       "-10 dB floor")
                            .arg(g.gainReductionDb())));
}

QTEST_APPLESS_MAIN(TstStripDsp)
#include "tst_strip_dsp.moc"
