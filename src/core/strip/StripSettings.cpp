// =================================================================
// src/core/strip/StripSettings.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See StripSettings.h for what persists and what
// deliberately does not.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/StripSettings.h"

#include "core/AppSettings.h"
#include "core/strip/StripChain.h"

namespace NereusSDR::StripSettings {

namespace {

QString key(const char* name)
{
    return QString::fromLatin1(kPrefix) + QString::fromLatin1(name);
}

void putF(const char* name, float v)
{
    AppSettings::instance().setValue(key(name), double(v));
}

// Read a float, falling back to whatever the stage itself already
// holds. That default matters: it means a settings file written before
// a control existed, or one edited into nonsense, restores the DSP's
// own default rather than zero — and zero is a real value for most of
// these, so "missing" and "set to nothing" must not look alike.
float getF(const char* name, float fallback)
{
    const QVariant v = AppSettings::instance().value(key(name));
    if (!v.isValid()) { return fallback; }
    bool ok = false;
    const double d = v.toDouble(&ok);
    return ok ? float(d) : fallback;
}

int getI(const char* name, int fallback)
{
    const QVariant v = AppSettings::instance().value(key(name));
    if (!v.isValid()) { return fallback; }
    bool ok = false;
    const int i = v.toInt(&ok);
    return ok ? i : fallback;
}

bool getB(const char* name, bool fallback)
{
    const QVariant v = AppSettings::instance().value(key(name));
    return v.isValid() ? v.toBool() : fallback;
}

} // namespace

void save(const StripChain& chain)
{
    AppSettings& s = AppSettings::instance();
    auto& c = const_cast<StripChain&>(chain);   // accessors are non-const

    // Per-stage enables. The master switch is not written — it loads
    // off every time, on purpose.
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto st = static_cast<StripChain::Stage>(i);
        s.setValue(key("Stage") + QString::number(i), c.stageEnabled(st));
    }

    putF("GateThresholdDb", c.gate().thresholdDb());
    putF("GateRatio",       c.gate().ratio());
    putF("GateAttackMs",    c.gate().attackMs());
    putF("GateReleaseMs",   c.gate().releaseMs());
    putF("GateHoldMs",      c.gate().holdMs());
    putF("GateFloorDb",     c.gate().floorDb());
    putF("GateReturnDb",    c.gate().returnDb());
    putF("GateLookaheadMs", c.gate().lookaheadMs());
    s.setValue(key("GateMode"), int(c.gate().mode()));

    putF("CompThresholdDb", c.comp().thresholdDb());
    putF("CompRatio",       c.comp().ratio());
    putF("CompAttackMs",    c.comp().attackMs());
    putF("CompReleaseMs",   c.comp().releaseMs());
    putF("CompKneeDb",      c.comp().kneeDb());
    putF("CompMakeupDb",    c.comp().makeupDb());
    putF("CompDriveDb",     c.comp().driveDb());
    s.setValue(key("CompPhaseStages"), c.comp().phaseRotatorStages());
    s.setValue(key("CompLimiterOn"),   c.comp().limiterEnabled());
    putF("CompLimiterCeilingDb", c.comp().limiterCeilingDb());

    putF("DeEssFrequencyHz", c.deEss().frequencyHz());
    putF("DeEssQ",           c.deEss().q());
    putF("DeEssThresholdDb", c.deEss().thresholdDb());
    putF("DeEssAmountDb",    c.deEss().amountDb());
    putF("DeEssAttackMs",    c.deEss().attackMs());
    putF("DeEssReleaseMs",   c.deEss().releaseMs());
    s.setValue(key("DeEssSlopeStages"), c.deEss().slopeStages());

    // EQ: the bands are the whole setting, so all of them go, with the
    // active count so a shorter set does not leave stale bands behind.
    const int bands = c.eq().activeBandCount();
    s.setValue(key("EqBandCount"), bands);
    s.setValue(key("EqMasterGain"), double(c.eq().masterGain()));
    s.setValue(key("EqFamily"), int(c.eq().filterFamily()));
    for (int i = 0; i < bands; ++i) {
        const ClientEq::BandParams p = c.eq().band(i);
        const QString b = key("EqBand") + QString::number(i);
        s.setValue(b + QStringLiteral("Freq"),    double(p.freqHz));
        s.setValue(b + QStringLiteral("Gain"),    double(p.gainDb));
        s.setValue(b + QStringLiteral("Q"),       double(p.q));
        s.setValue(b + QStringLiteral("Type"),    int(p.type));
        s.setValue(b + QStringLiteral("On"),      p.enabled);
        s.setValue(b + QStringLiteral("Slope"),   p.slopeDbPerOct);
    }

    putF("LimiterCeilingDb",    c.limiter().ceilingDb());
    putF("LimiterOutputTrimDb", c.limiter().outputTrimDb());
    s.setValue(key("LimiterDcBlock"), c.limiter().dcBlockEnabled());
}

void restore(StripChain& c)
{
    AppSettings& s = AppSettings::instance();

    c.gate().setThresholdDb(getF("GateThresholdDb", c.gate().thresholdDb()));
    c.gate().setRatio      (getF("GateRatio",       c.gate().ratio()));
    c.gate().setAttackMs   (getF("GateAttackMs",    c.gate().attackMs()));
    c.gate().setReleaseMs  (getF("GateReleaseMs",   c.gate().releaseMs()));
    c.gate().setHoldMs     (getF("GateHoldMs",      c.gate().holdMs()));
    c.gate().setFloorDb    (getF("GateFloorDb",     c.gate().floorDb()));
    c.gate().setReturnDb   (getF("GateReturnDb",    c.gate().returnDb()));
    c.gate().setLookaheadMs(getF("GateLookaheadMs", c.gate().lookaheadMs()));
    // The gate's mode is written above but deliberately not restored
    // here. setMode() snaps ratio and floor to a preset pair, so
    // applying it would overwrite the two values the operator just
    // fine-tuned — and those are what actually determine the sound.
    // The saved mode is kept in the file so the panel can show which
    // preset the settings started from.

    c.comp().setThresholdDb(getF("CompThresholdDb", c.comp().thresholdDb()));
    c.comp().setRatio      (getF("CompRatio",       c.comp().ratio()));
    c.comp().setAttackMs   (getF("CompAttackMs",    c.comp().attackMs()));
    c.comp().setReleaseMs  (getF("CompReleaseMs",   c.comp().releaseMs()));
    c.comp().setKneeDb     (getF("CompKneeDb",      c.comp().kneeDb()));
    c.comp().setMakeupDb   (getF("CompMakeupDb",    c.comp().makeupDb()));
    c.comp().setDriveDb    (getF("CompDriveDb",     c.comp().driveDb()));
    c.comp().setPhaseRotatorStages(
        getI("CompPhaseStages", c.comp().phaseRotatorStages()));
    c.comp().setLimiterEnabled(
        getB("CompLimiterOn", c.comp().limiterEnabled()));
    c.comp().setLimiterCeilingDb(
        getF("CompLimiterCeilingDb", c.comp().limiterCeilingDb()));

    c.deEss().setFrequencyHz(getF("DeEssFrequencyHz", c.deEss().frequencyHz()));
    c.deEss().setQ          (getF("DeEssQ",           c.deEss().q()));
    c.deEss().setThresholdDb(getF("DeEssThresholdDb", c.deEss().thresholdDb()));
    c.deEss().setAmountDb   (getF("DeEssAmountDb",    c.deEss().amountDb()));
    c.deEss().setAttackMs   (getF("DeEssAttackMs",    c.deEss().attackMs()));
    c.deEss().setReleaseMs  (getF("DeEssReleaseMs",   c.deEss().releaseMs()));
    c.deEss().setSlopeStages(getI("DeEssSlopeStages", c.deEss().slopeStages()));

    const int bands = getI("EqBandCount", -1);
    if (bands > 0 && bands <= ClientEq::kMaxBands) {
        for (int i = 0; i < bands; ++i) {
            const QString b = key("EqBand") + QString::number(i);
            ClientEq::BandParams p = c.eq().band(i);
            const QVariant f = s.value(b + QStringLiteral("Freq"));
            if (!f.isValid()) { continue; }
            p.freqHz = float(f.toDouble());
            p.gainDb = float(s.value(b + QStringLiteral("Gain"), 0.0).toDouble());
            p.q      = float(s.value(b + QStringLiteral("Q"), 0.707).toDouble());
            p.type   = static_cast<ClientEq::FilterType>(
                s.value(b + QStringLiteral("Type"), 0).toInt());
            p.enabled = s.value(b + QStringLiteral("On"), true).toBool();
            p.slopeDbPerOct =
                s.value(b + QStringLiteral("Slope"), 12).toInt();
            c.eq().setBand(i, p);
        }
        c.eq().setActiveBandCount(bands);
    }
    {
        const QVariant g = s.value(key("EqMasterGain"));
        if (g.isValid()) { c.eq().setMasterGain(float(g.toDouble())); }
        const QVariant fam = s.value(key("EqFamily"));
        if (fam.isValid()) {
            c.eq().setFilterFamily(
                static_cast<ClientEq::FilterFamily>(fam.toInt()));
        }
    }

    c.limiter().setCeilingDb(
        getF("LimiterCeilingDb", c.limiter().ceilingDb()));
    c.limiter().setOutputTrimDb(
        getF("LimiterOutputTrimDb", c.limiter().outputTrimDb()));
    c.limiter().setDcBlockEnabled(
        getB("LimiterDcBlock", c.limiter().dcBlockEnabled()));

    // Enables last, so no stage starts running with half its parameters
    // restored. On a fast machine the difference is a few microseconds;
    // on the transmit path it is the difference between a clean start
    // and one burst of whatever the defaults happened to be.
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto st = static_cast<StripChain::Stage>(i);
        c.setStageEnabled(st,
            s.value(key("Stage") + QString::number(i), false).toBool());
    }
    // And the master stays where it is — off. See StripSettings.h.
}

// ── Starting points ──────────────────────────────────────────────────

QVector<Preset> builtInPresets()
{
    return {
        {QStringLiteral("Clean voice"),
         QStringLiteral("High-pass, a little compression, nothing else. "
                        "What a voice should sound like before anyone "
                        "decides to improve it — and the right place to "
                        "start from.")},
        {QStringLiteral("DX — punchy"),
         QStringLiteral("Harder compression, presence lifted, low end "
                        "cut further. Costs naturalness and buys "
                        "intelligibility through noise. For working "
                        "stations you can barely hear.")},
        {QStringLiteral("Ragchew — easy"),
         QStringLiteral("Gentle throughout, gate as an expander rather "
                        "than a gate, more low end. For a long contact "
                        "with a strong signal, where the other operator "
                        "has to listen to you for an hour.")},
    };
}

bool applyBuiltIn(const QString& name, StripChain& c)
{
    auto band = [&c](int idx, ClientEq::FilterType t, float hz, float gain,
                     float q, bool on, int slope) {
        ClientEq::BandParams p;
        p.type = t; p.freqHz = hz; p.gainDb = gain; p.q = q;
        p.enabled = on; p.slopeDbPerOct = slope;
        c.eq().setBand(idx, p);
    };
    auto allOff = [&c]() {
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
        }
    };

    if (name == QLatin1String("Clean voice")) {
        allOff();
        band(0, ClientEq::FilterType::HighPass, 100.0f, 0.0f, 0.707f, true, 24);
        band(1, ClientEq::FilterType::Peak,  50.0f, -18.0f, 8.0f, false, 12);
        band(2, ClientEq::FilterType::Peak, 100.0f, -12.0f, 8.0f, false, 12);
        band(3, ClientEq::FilterType::Peak, 150.0f,  -9.0f, 8.0f, false, 12);
        band(4, ClientEq::FilterType::LowShelf,   200.0f,  0.0f, 0.707f, true, 12);
        band(5, ClientEq::FilterType::Peak,      2000.0f,  1.5f, 1.0f,   true, 12);
        band(6, ClientEq::FilterType::HighShelf, 3000.0f,  0.0f, 0.707f, true, 12);
        c.eq().setActiveBandCount(7);
        c.comp().setThresholdDb(-18.0f);
        c.comp().setRatio(2.5f);
        c.comp().setAttackMs(8.0f);
        c.comp().setReleaseMs(150.0f);
        c.comp().setKneeDb(8.0f);
        c.comp().setMakeupDb(3.0f);
        c.comp().setPhaseRotatorStages(4);
        c.setStageEnabled(StripChain::Stage::Eq, true);
        c.setStageEnabled(StripChain::Stage::Comp, true);
        c.setStageEnabled(StripChain::Stage::Limiter, true);
        c.limiter().setCeilingDb(-1.0f);
        return true;
    }

    if (name == QLatin1String("DX — punchy")) {
        allOff();
        band(0, ClientEq::FilterType::HighPass, 150.0f, 0.0f, 0.707f, true, 24);
        band(1, ClientEq::FilterType::Peak,  50.0f, -18.0f, 8.0f, false, 12);
        band(2, ClientEq::FilterType::Peak, 100.0f, -12.0f, 8.0f, false, 12);
        band(3, ClientEq::FilterType::Peak, 150.0f,  -9.0f, 8.0f, false, 12);
        band(4, ClientEq::FilterType::LowShelf,   200.0f, -3.0f, 0.707f, true, 12);
        band(5, ClientEq::FilterType::Peak,      2200.0f,  5.0f, 1.2f,   true, 12);
        band(6, ClientEq::FilterType::HighShelf, 3000.0f, -3.0f, 0.707f, true, 12);
        c.eq().setActiveBandCount(7);
        c.gate().setThresholdDb(-38.0f);
        c.gate().setFloorDb(-18.0f);
        c.gate().setHoldMs(30.0f);
        c.gate().setReleaseMs(150.0f);
        c.comp().setThresholdDb(-26.0f);
        c.comp().setRatio(5.0f);
        c.comp().setAttackMs(3.0f);
        c.comp().setReleaseMs(90.0f);
        c.comp().setKneeDb(4.0f);
        c.comp().setMakeupDb(7.0f);
        c.comp().setPhaseRotatorStages(6);
        c.deEss().setFrequencyHz(6500.0f);
        c.deEss().setThresholdDb(-28.0f);
        c.deEss().setAmountDb(-8.0f);
        // The de-esser is on here and not in "Clean voice" on purpose:
        // this much presence lift makes sibilance that was not a problem
        // before into one, and a preset that creates a fault it does not
        // then fix is a bad preset.
        for (auto st : {StripChain::Stage::Gate, StripChain::Stage::Eq,
                        StripChain::Stage::DeEss, StripChain::Stage::Comp,
                        StripChain::Stage::Limiter}) {
            c.setStageEnabled(st, true);
        }
        c.limiter().setCeilingDb(-1.0f);
        return true;
    }

    if (name == QLatin1String("Ragchew — easy")) {
        allOff();
        band(0, ClientEq::FilterType::HighPass, 80.0f, 0.0f, 0.707f, true, 12);
        band(1, ClientEq::FilterType::Peak,  50.0f, -18.0f, 8.0f, false, 12);
        band(2, ClientEq::FilterType::Peak, 100.0f, -12.0f, 8.0f, false, 12);
        band(3, ClientEq::FilterType::Peak, 150.0f,  -9.0f, 8.0f, false, 12);
        band(4, ClientEq::FilterType::LowShelf,   200.0f,  2.0f, 0.707f, true, 12);
        band(5, ClientEq::FilterType::Peak,      2000.0f,  1.0f, 0.8f,   true, 12);
        band(6, ClientEq::FilterType::HighShelf, 3000.0f,  1.0f, 0.707f, true, 12);
        c.eq().setActiveBandCount(7);
        c.gate().setMode(ClientGate::Mode::Expander);
        c.gate().setThresholdDb(-45.0f);
        c.gate().setReleaseMs(250.0f);
        c.comp().setThresholdDb(-14.0f);
        c.comp().setRatio(2.0f);
        c.comp().setAttackMs(15.0f);
        c.comp().setReleaseMs(250.0f);
        c.comp().setKneeDb(12.0f);
        c.comp().setMakeupDb(2.0f);
        c.comp().setPhaseRotatorStages(2);
        for (auto st : {StripChain::Stage::Gate, StripChain::Stage::Eq,
                        StripChain::Stage::Comp, StripChain::Stage::Limiter}) {
            c.setStageEnabled(st, true);
        }
        c.limiter().setCeilingDb(-1.0f);
        return true;
    }

    return false;
}

} // namespace NereusSDR::StripSettings
