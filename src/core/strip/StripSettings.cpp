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

// Every key is built from a prefix so the same reader and writer serve
// both the live settings and each named preset. One code path, so a
// preset cannot quietly store a different set of fields from the live
// state — which is how preset systems come to have settings that only
// survive if you never use a preset.
QString key(const QString& prefix, const char* name)
{
    return prefix + QString::fromLatin1(name);
}

void putF(const QString& prefix, const char* name, float v)
{
    AppSettings::instance().setValue(key(prefix, name), double(v));
}

// Read a float, falling back to whatever the stage itself already
// holds. That default matters: it means a settings file written before
// a control existed, or one edited into nonsense, restores the DSP's
// own default rather than zero — and zero is a real value for most of
// these, so "missing" and "set to nothing" must not look alike.
float getF(const QString& prefix, const char* name, float fallback)
{
    const QVariant v = AppSettings::instance().value(key(prefix, name));
    if (!v.isValid()) { return fallback; }
    bool ok = false;
    const double d = v.toDouble(&ok);
    return ok ? float(d) : fallback;
}

int getI(const QString& prefix, const char* name, int fallback)
{
    const QVariant v = AppSettings::instance().value(key(prefix, name));
    if (!v.isValid()) { return fallback; }
    bool ok = false;
    const int i = v.toInt(&ok);
    return ok ? i : fallback;
}

bool getB(const QString& prefix, const char* name, bool fallback)
{
    const QVariant v = AppSettings::instance().value(key(prefix, name));
    return v.isValid() ? v.toBool() : fallback;
}

// Where the list of the operator's own presets lives.
const char kUserIndexKey[] = "ChannelStrip/UserPresets";

QString userPrefix(const QString& name)
{
    return QStringLiteral("ChannelStrip/User/") + name + QLatin1Char('/');
}

} // namespace

void saveTo(const QString& prefix, const StripChain& chain)
{
    AppSettings& s = AppSettings::instance();
    auto& c = const_cast<StripChain&>(chain);   // accessors are non-const

    // Per-stage enables. The master switch is not written — it loads
    // off every time, on purpose.
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto st = static_cast<StripChain::Stage>(i);
        s.setValue(key(prefix, "Stage") + QString::number(i), c.stageEnabled(st));
    }

    putF(prefix, "GateThresholdDb", c.gate().thresholdDb());
    putF(prefix, "GateRatio",       c.gate().ratio());
    putF(prefix, "GateAttackMs",    c.gate().attackMs());
    putF(prefix, "GateReleaseMs",   c.gate().releaseMs());
    putF(prefix, "GateHoldMs",      c.gate().holdMs());
    putF(prefix, "GateFloorDb",     c.gate().floorDb());
    putF(prefix, "GateReturnDb",    c.gate().returnDb());
    putF(prefix, "GateLookaheadMs", c.gate().lookaheadMs());
    s.setValue(key(prefix, "GateMode"), int(c.gate().mode()));

    putF(prefix, "CompThresholdDb", c.comp().thresholdDb());
    putF(prefix, "CompRatio",       c.comp().ratio());
    putF(prefix, "CompAttackMs",    c.comp().attackMs());
    putF(prefix, "CompReleaseMs",   c.comp().releaseMs());
    putF(prefix, "CompKneeDb",      c.comp().kneeDb());
    putF(prefix, "CompMakeupDb",    c.comp().makeupDb());
    putF(prefix, "CompDriveDb",     c.comp().driveDb());
    s.setValue(key(prefix, "CompPhaseStages"), c.comp().phaseRotatorStages());
    s.setValue(key(prefix, "CompLimiterOn"),   c.comp().limiterEnabled());
    putF(prefix, "CompLimiterCeilingDb", c.comp().limiterCeilingDb());

    putF(prefix, "DeEssFrequencyHz", c.deEss().frequencyHz());
    putF(prefix, "DeEssQ",           c.deEss().q());
    putF(prefix, "DeEssThresholdDb", c.deEss().thresholdDb());
    putF(prefix, "DeEssAmountDb",    c.deEss().amountDb());
    putF(prefix, "DeEssAttackMs",    c.deEss().attackMs());
    putF(prefix, "DeEssReleaseMs",   c.deEss().releaseMs());
    s.setValue(key(prefix, "DeEssSlopeStages"), c.deEss().slopeStages());

    // EQ: the bands are the whole setting, so all of them go, with the
    // active count so a shorter set does not leave stale bands behind.
    const int bands = c.eq().activeBandCount();
    s.setValue(key(prefix, "EqBandCount"), bands);
    s.setValue(key(prefix, "EqMasterGain"), double(c.eq().masterGain()));
    s.setValue(key(prefix, "EqFamily"), int(c.eq().filterFamily()));
    for (int i = 0; i < bands; ++i) {
        const ClientEq::BandParams p = c.eq().band(i);
        const QString b = key(prefix, "EqBand") + QString::number(i);
        s.setValue(b + QStringLiteral("Freq"),    double(p.freqHz));
        s.setValue(b + QStringLiteral("Gain"),    double(p.gainDb));
        s.setValue(b + QStringLiteral("Q"),       double(p.q));
        s.setValue(b + QStringLiteral("Type"),    int(p.type));
        s.setValue(b + QStringLiteral("On"),      p.enabled);
        s.setValue(b + QStringLiteral("Slope"),   p.slopeDbPerOct);
    }

    putF(prefix, "LimiterCeilingDb",    c.limiter().ceilingDb());
    putF(prefix, "LimiterOutputTrimDb", c.limiter().outputTrimDb());
    s.setValue(key(prefix, "LimiterDcBlock"), c.limiter().dcBlockEnabled());
}

void restoreFrom(const QString& prefix, StripChain& c)
{
    AppSettings& s = AppSettings::instance();

    c.gate().setThresholdDb(getF(prefix, "GateThresholdDb", c.gate().thresholdDb()));
    c.gate().setRatio      (getF(prefix, "GateRatio",       c.gate().ratio()));
    c.gate().setAttackMs   (getF(prefix, "GateAttackMs",    c.gate().attackMs()));
    c.gate().setReleaseMs  (getF(prefix, "GateReleaseMs",   c.gate().releaseMs()));
    c.gate().setHoldMs     (getF(prefix, "GateHoldMs",      c.gate().holdMs()));
    c.gate().setFloorDb    (getF(prefix, "GateFloorDb",     c.gate().floorDb()));
    c.gate().setReturnDb   (getF(prefix, "GateReturnDb",    c.gate().returnDb()));
    c.gate().setLookaheadMs(getF(prefix, "GateLookaheadMs", c.gate().lookaheadMs()));
    // The gate's mode is written above but deliberately not restored
    // here. setMode() snaps ratio and floor to a preset pair, so
    // applying it would overwrite the two values the operator just
    // fine-tuned — and those are what actually determine the sound.
    // The saved mode is kept in the file so the panel can show which
    // preset the settings started from.

    c.comp().setThresholdDb(getF(prefix, "CompThresholdDb", c.comp().thresholdDb()));
    c.comp().setRatio      (getF(prefix, "CompRatio",       c.comp().ratio()));
    c.comp().setAttackMs   (getF(prefix, "CompAttackMs",    c.comp().attackMs()));
    c.comp().setReleaseMs  (getF(prefix, "CompReleaseMs",   c.comp().releaseMs()));
    c.comp().setKneeDb     (getF(prefix, "CompKneeDb",      c.comp().kneeDb()));
    c.comp().setMakeupDb   (getF(prefix, "CompMakeupDb",    c.comp().makeupDb()));
    c.comp().setDriveDb    (getF(prefix, "CompDriveDb",     c.comp().driveDb()));
    c.comp().setPhaseRotatorStages(
        getI(prefix, "CompPhaseStages", c.comp().phaseRotatorStages()));
    c.comp().setLimiterEnabled(
        getB(prefix, "CompLimiterOn", c.comp().limiterEnabled()));
    c.comp().setLimiterCeilingDb(
        getF(prefix, "CompLimiterCeilingDb", c.comp().limiterCeilingDb()));

    c.deEss().setFrequencyHz(getF(prefix, "DeEssFrequencyHz", c.deEss().frequencyHz()));
    c.deEss().setQ          (getF(prefix, "DeEssQ",           c.deEss().q()));
    c.deEss().setThresholdDb(getF(prefix, "DeEssThresholdDb", c.deEss().thresholdDb()));
    c.deEss().setAmountDb   (getF(prefix, "DeEssAmountDb",    c.deEss().amountDb()));
    c.deEss().setAttackMs   (getF(prefix, "DeEssAttackMs",    c.deEss().attackMs()));
    c.deEss().setReleaseMs  (getF(prefix, "DeEssReleaseMs",   c.deEss().releaseMs()));
    c.deEss().setSlopeStages(getI(prefix, "DeEssSlopeStages", c.deEss().slopeStages()));

    const int bands = getI(prefix, "EqBandCount", -1);
    if (bands > 0 && bands <= ClientEq::kMaxBands) {
        for (int i = 0; i < bands; ++i) {
            const QString b = key(prefix, "EqBand") + QString::number(i);
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
        const QVariant g = s.value(key(prefix, "EqMasterGain"));
        if (g.isValid()) { c.eq().setMasterGain(float(g.toDouble())); }
        const QVariant fam = s.value(key(prefix, "EqFamily"));
        if (fam.isValid()) {
            c.eq().setFilterFamily(
                static_cast<ClientEq::FilterFamily>(fam.toInt()));
        }
    }

    c.limiter().setCeilingDb(
        getF(prefix, "LimiterCeilingDb", c.limiter().ceilingDb()));
    c.limiter().setOutputTrimDb(
        getF(prefix, "LimiterOutputTrimDb", c.limiter().outputTrimDb()));
    c.limiter().setDcBlockEnabled(
        getB(prefix, "LimiterDcBlock", c.limiter().dcBlockEnabled()));

    // Enables last, so no stage starts running with half its parameters
    // restored. On a fast machine the difference is a few microseconds;
    // on the transmit path it is the difference between a clean start
    // and one burst of whatever the defaults happened to be.
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto st = static_cast<StripChain::Stage>(i);
        c.setStageEnabled(st,
            s.value(key(prefix, "Stage") + QString::number(i), false).toBool());
    }
    // And the master stays where it is — off. See StripSettings.h.
}

// ── The live settings ────────────────────────────────────────────────

void save(const StripChain& chain)
{
    saveTo(QString::fromLatin1(kPrefix), chain);
}

void restore(StripChain& chain)
{
    restoreFrom(QString::fromLatin1(kPrefix), chain);
}

// ── The operator's own presets ───────────────────────────────────────

QStringList userPresetNames()
{
    return AppSettings::instance()
        .value(QString::fromLatin1(kUserIndexKey)).toStringList();
}

bool saveUserPreset(const QString& rawName, const StripChain& chain)
{
    const QString name = rawName.trimmed();
    // Refused rather than silently mangled. A name with a slash in it
    // would open a second level of settings keys and the preset would
    // reappear somewhere unexpected — or not at all.
    if (name.isEmpty() || name.contains(QLatin1Char('/'))) { return false; }

    saveTo(userPrefix(name), chain);

    QStringList names = userPresetNames();
    if (!names.contains(name)) {
        names.append(name);
        names.sort(Qt::CaseInsensitive);
        AppSettings::instance().setValue(QString::fromLatin1(kUserIndexKey),
                                         names);
    }
    // Saving under an existing name overwrites it, deliberately: that is
    // what "save" means to someone who has just adjusted the preset they
    // are already using, and a dialog asking them to confirm it every
    // time is a dialog they will stop reading.
    return true;
}

bool applyUserPreset(const QString& name, StripChain& chain)
{
    if (!userPresetNames().contains(name)) { return false; }
    restoreFrom(userPrefix(name), chain);
    return true;
}

bool removeUserPreset(const QString& name)
{
    QStringList names = userPresetNames();
    if (!names.removeAll(name)) { return false; }
    AppSettings::instance().setValue(QString::fromLatin1(kUserIndexKey), names);
    // The keys themselves are left behind. They are small, and a preset
    // deleted by accident can be recovered by saving under the same
    // name — whereas removing them would need a key-by-key sweep that
    // has to be kept in step with every field ever added, which is
    // exactly the kind of list that falls out of date silently.
    return true;
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
