// =================================================================
// src/core/strip/StripTuner.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See StripTuner.h for what each decision is
// derived from and what this will not do.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/StripTuner.h"

#include "core/strip/StripChain.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR::StripTuner {

namespace {

// The band indices the panel and the settings agree on. Duplicated
// here rather than shared with StripWindow on purpose: this is core
// code and must not depend on a window, and the layout is pinned by
// tst_strip_tuner so the two cannot drift apart silently.
constexpr int kBandHighPass = 0;
constexpr int kBandHum1     = 1;
constexpr int kBandLow      = 4;
constexpr int kBandPresence = 5;
constexpr int kBandHigh     = 6;

void setBandGain(StripChain& c, int idx, double gainDb, bool enabled)
{
    ClientEq::BandParams p = c.eq().band(idx);
    p.gainDb  = float(gainDb);
    p.enabled = enabled;
    c.eq().setBand(idx, p);
}

} // namespace

// ── The individual decisions ─────────────────────────────────────────

double highPassHz(const VoiceAnalysis& a)
{
    // How far above the 1 kHz reference the two lowest bands sit. A
    // voice with proximity bass and a room with traffic outside both
    // show up here, and both want the same treatment.
    const double low = std::max(a.bandDb[0], a.bandDb[1]);   // 32 and 63 Hz

    // The mapping, and the reason for each end of it: at 60 Hz the
    // filter is doing almost nothing an SSB transmitter would not do
    // anyway, and at 200 Hz it has started removing the bottom of the
    // voice rather than what is under it.
    double hz = 80.0;
    if (low > 0.0)       { hz = 150.0; }   // low end above the reference
    else if (low > -10.0){ hz = 120.0; }
    else if (low > -20.0){ hz = 100.0; }
    return std::clamp(hz, 60.0, 200.0);
}

double gateThresholdDbFs(const VoiceAnalysis& a)
{
    const double sep = a.speechDbFs - a.noiseFloorDbFs;
    if (sep < kMinGateSeparationDb) {
        // Too close to tell apart. A gate placed inside that overlap
        // chops words instead of removing background, which sounds
        // like a bad connection and is blamed on the radio.
        return a.speechDbFs;
    }
    // A third of the way up from the floor. Not halfway: speech has a
    // long quiet tail — the end of a word is much quieter than its
    // middle — and a threshold at the midpoint eats it.
    return a.noiseFloorDbFs + sep / 3.0;
}

double compressorRatio(const VoiceAnalysis& a)
{
    // 10 dB of crest is an already-even voice and needs almost nothing;
    // 20 dB is a dynamic one and needs real work. Linear between, and
    // bounded so a wild measurement cannot produce a limiter.
    const double r = 1.5 + (a.crestFactorDb - 10.0) / 4.0;
    return std::clamp(r, 1.5, 6.0);
}

bool humWorthNotching(const VoiceAnalysis& a)
{
    return a.humBaseHz > 0 && -a.humDb < kHumWorthNotchingDb;
}

// ── Applying ─────────────────────────────────────────────────────────

Result applyAnalysis(const VoiceAnalysis& a, StripChain& c)
{
    Result r;

    if (!a.valid) {
        r.notes << QStringLiteral(
            "The measurement didn't succeed, so nothing was changed. "
            "Record again before letting anything set itself from it.");
        return r;
    }
    if (a.clippedSamples > 0) {
        // Refused, not merely warned about. A clipped recording's
        // spectrum is the spectrum of its own flat tops, and setting a
        // whole chain from it would be confidently wrong in every
        // stage at once.
        r.notes << QStringLiteral(
            "The recording was clipped, so nothing was changed. Turn the "
            "microphone gain down until the voice check reports no "
            "clipped samples, then measure again — everything below "
            "depends on that spectrum being real.");
        return r;
    }

    // ── High-pass ────────────────────────────────────────────────────
    {
        const double hz = highPassHz(a);
        ClientEq::BandParams p = c.eq().band(kBandHighPass);
        p.type = ClientEq::FilterType::HighPass;
        p.freqHz = float(hz);
        p.slopeDbPerOct = 24;
        p.enabled = true;
        c.eq().setBand(kBandHighPass, p);
        r.notes << QStringLiteral(
            "High-pass at %1 Hz. Below that is rumble and proximity "
            "bass: the transmitter will not send it, but it reaches the "
            "compressor first and eats the headroom your voice needs.")
                       .arg(hz, 0, 'f', 0);
    }

    // ── Mains hum ────────────────────────────────────────────────────
    if (humWorthNotching(a)) {
        for (int h = 1; h <= 3; ++h) {
            ClientEq::BandParams p = c.eq().band(kBandHum1 + h - 1);
            p.type = ClientEq::FilterType::Peak;
            p.freqHz = float(a.humBaseHz * h);
            p.q = 8.0f;
            p.gainDb = float(-18.0 + 3.0 * (h - 1));
            p.enabled = true;
            c.eq().setBand(kBandHum1 + h - 1, p);
        }
        r.notes << QStringLiteral(
            "Notches at %1, %2 and %3 Hz — the hum is only %4 dB under "
            "your voice, which is far too close. These hide it; they do "
            "not fix it. Something is picking up the mains, and finding "
            "that is worth more than any setting here.")
                       .arg(a.humBaseHz).arg(a.humBaseHz * 2)
                       .arg(a.humBaseHz * 3).arg(-a.humDb, 0, 'f', 1);
    } else {
        for (int h = 0; h < 3; ++h) {
            ClientEq::BandParams p = c.eq().band(kBandHum1 + h);
            p.enabled = false;
            c.eq().setBand(kBandHum1 + h, p);
        }
    }

    // ── Tone ─────────────────────────────────────────────────────────
    //
    // The ten-band suggestion folded onto the three controls the panel
    // has. Averaging pairs rather than picking one of each: a shelf at
    // 200 Hz affects both the 125 and 250 Hz bands, so using only one
    // of them would set it by half the evidence.
    {
        const double low = 0.5 * (a.suggestedEqDb[2] + a.suggestedEqDb[3]);
        const double pres = a.suggestedEqDb[6];
        const double high = 0.5 * (a.suggestedEqDb[7] + a.suggestedEqDb[8]);

        setBandGain(c, kBandLow,      std::min(0.0, low),  true);
        setBandGain(c, kBandPresence, std::min(0.0, pres), true);
        setBandGain(c, kBandHigh,     std::min(0.0, high), true);

        r.notes << QStringLiteral(
            "Tone: low %1 dB, presence %2 dB, high %3 dB. All cuts — "
            "reaching a target by boosting would raise whatever noise "
            "is in that band along with the voice.")
                       .arg(std::min(0.0, low),  0, 'f', 1)
                       .arg(std::min(0.0, pres), 0, 'f', 1)
                       .arg(std::min(0.0, high), 0, 'f', 1);
    }
    c.setStageEnabled(StripChain::Stage::Eq, true);

    // ── Gate ─────────────────────────────────────────────────────────
    {
        const double thr = gateThresholdDbFs(a);
        const double sep = a.speechDbFs - a.noiseFloorDbFs;
        if (sep < kMinGateSeparationDb) {
            c.setStageEnabled(StripChain::Stage::Gate, false);
            r.notes << QStringLiteral(
                "Gate left off. Your background is only %1 dB below your "
                "voice, and a gate placed inside that overlap chops the "
                "ends off words instead of removing the background.")
                           .arg(sep, 0, 'f', 1);
        } else {
            c.gate().setThresholdDb(float(std::clamp(thr, -80.0, 0.0)));
            c.gate().setMode(ClientGate::Mode::Expander);
            c.gate().setFloorDb(-15.0f);
            c.gate().setHoldMs(25.0f);
            c.gate().setReleaseMs(180.0f);
            c.setStageEnabled(StripChain::Stage::Gate, true);
            r.notes << QStringLiteral(
                "Gate at %1 dBFS, as an expander rather than a hard "
                "gate. Your background sits %2 dB under your voice, so "
                "this pushes it down without the pumping a hard gate "
                "would give you.")
                           .arg(thr, 0, 'f', 0).arg(sep, 0, 'f', 1);
        }
    }

    // ── De-esser ─────────────────────────────────────────────────────
    if (a.sibilanceDb > 3.0) {
        c.deEss().setFrequencyHz(6000.0f);
        c.deEss().setThresholdDb(float(std::clamp(a.speechDbFs + 6.0,
                                                  -60.0, 0.0)));
        c.deEss().setAmountDb(float(-std::clamp(a.sibilanceDb, 3.0, 12.0)));
        c.setStageEnabled(StripChain::Stage::DeEss, true);
        r.notes << QStringLiteral(
            "De-esser on, up to %1 dB. Your sibilance measures %2 dB "
            "above the speech band — set the frequency by ear on the "
            "word \"six\", because where it sits depends on your voice "
            "and your microphone rather than on a number.")
                       .arg(std::clamp(a.sibilanceDb, 3.0, 12.0), 0, 'f', 1)
                       .arg(a.sibilanceDb, 0, 'f', 1);
    } else {
        c.setStageEnabled(StripChain::Stage::DeEss, false);
    }

    // ── Compressor ───────────────────────────────────────────────────
    {
        const double ratio = compressorRatio(a);
        c.comp().setRatio(float(ratio));
        c.comp().setThresholdDb(float(std::clamp(a.speechDbFs, -60.0, -6.0)));
        c.comp().setAttackMs(8.0f);
        c.comp().setReleaseMs(150.0f);
        c.comp().setKneeDb(8.0f);
        c.comp().setMakeupDb(float(std::clamp(a.suggestedPreampDb, 0.0, 12.0)));
        c.comp().setPhaseRotatorStages(4);
        c.setStageEnabled(StripChain::Stage::Comp, true);
        r.notes << QStringLiteral(
            "Compressor %1:1 with the phase rotator at four stages. "
            "Your peaks are %2 dB above your average, and that number "
            "is the whole reason for the ratio. The rotator evens out "
            "the lopsided peaks speech has, which raises average power "
            "without making anything louder.")
                       .arg(ratio, 0, 'f', 1).arg(a.crestFactorDb, 0, 'f', 1);
    }

    // ── Limiter ──────────────────────────────────────────────────────
    c.limiter().setCeilingDb(-1.0f);
    c.setStageEnabled(StripChain::Stage::Limiter, true);
    r.notes << QStringLiteral(
        "Limiter on at -1 dB, last in the chain. Everything above it "
        "can add gain; this is what stops the sum arriving at the "
        "modulator hotter than it should.");

    // Left alone on purpose, and said so rather than silently skipped.
    r.notes << QStringLiteral(
        "Tube, exciter and reverb were left off. Nothing in a "
        "measurement can tell you whether you want them — they are "
        "matters of taste, and each costs something. The strip itself "
        "is still switched off; you decide when it goes in circuit.");

    r.changed = true;
    return r;
}

} // namespace NereusSDR::StripTuner
