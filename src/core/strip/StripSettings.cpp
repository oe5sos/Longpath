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

} // namespace NereusSDR::StripSettings
