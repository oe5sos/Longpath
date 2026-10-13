// =================================================================
// src/core/strip/StripSettings.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See StripSettings.h for what persists and what
// deliberately does not.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/StripSettings.h"

#include <iterator>
#include "core/strip/EqBandLayout.h"

#include "core/AppSettings.h"
#include "core/strip/StripChain.h"

namespace Longpath::StripSettings {

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

// ── Wohin geschrieben und woraus gelesen wird ────────────────────────
//
// Normalerweise in die Einstellungen. Fuer EINEN Zweck aber nicht:
// copyParameters() schiebt eine ganze Kette in eine andere, und das
// soll weder die Einstellungsdatei anfassen noch den Weg ueber die
// Platte nehmen.
//
// Der Umweg ueber eine Umleitung statt ueber eine zweite Liste aller
// Felder ist Absicht. Die Liste in saveTo()/restoreFrom() ist lang und
// waechst mit jeder Stufe; eine zweite Fassung davon waere genau die
// Sorte Doppelung, die irgendwann auseinanderlaeuft und dann heisst:
// „beim Mithoeren fehlt der Kompressor-Vorhalt" und niemand weiss
// warum.
//
// Nur vom Bedienfaden aus zu benutzen -- es ist ein globaler Zeiger,
// und er wird fuer die Dauer EINES Kopiervorgangs gesetzt.
using Map = QHash<QString, QVariant>;
Map* g_redirect = nullptr;

// Waehrend eines Kopiervorgangs bleiben die Einschuebe aussen vor.
// Ein Plugin laedt man nicht zehnmal in der Sekunde neu, und sein
// Zustand als Base64 kostet bei jedem Abruf eine Serialisierung.
bool g_skipInserts = false;

void putV(const QString& k, const QVariant& v)
{
    if (g_redirect != nullptr) { g_redirect->insert(k, v); return; }
    AppSettings::instance().setValue(k, v);
}

QVariant getV(const QString& k)
{
    if (g_redirect != nullptr) { return g_redirect->value(k); }
    return AppSettings::instance().value(k);
}

QVariant getV(const QString& k, const QVariant& fallback)
{
    const QVariant v = getV(k);
    return v.isValid() ? v : fallback;
}

void putF(const QString& prefix, const char* name, float v)
{
    putV(key(prefix, name), double(v));
}

// Read a float, falling back to whatever the stage itself already
// holds. That default matters: it means a settings file written before
// a control existed, or one edited into nonsense, restores the DSP's
// own default rather than zero — and zero is a real value for most of
// these, so "missing" and "set to nothing" must not look alike.
float getF(const QString& prefix, const char* name, float fallback)
{
    const QVariant v = getV(key(prefix, name));
    if (!v.isValid()) { return fallback; }
    bool ok = false;
    const double d = v.toDouble(&ok);
    return ok ? float(d) : fallback;
}

int getI(const QString& prefix, const char* name, int fallback)
{
    const QVariant v = getV(key(prefix, name));
    if (!v.isValid()) { return fallback; }
    bool ok = false;
    const int i = v.toInt(&ok);
    return ok ? i : fallback;
}

bool getB(const QString& prefix, const char* name, bool fallback)
{
    const QVariant v = getV(key(prefix, name));
    return v.isValid() ? v.toBool() : fallback;
}

// Die Reihenfolge, in der die Stufen bis zum 2026-10-10 gespeichert
// wurden. Teil des Dateiformats -- nichts darf hier eingefuegt oder
// umgestellt werden, nur hinten angehaengt.
constexpr StripChain::Stage kLegacyStageOrder[] = {
    StripChain::Stage::Gate,   StripChain::Stage::Eq,
    StripChain::Stage::DeEss,  StripChain::Stage::Comp,
    StripChain::Stage::Tube,   StripChain::Stage::Pudu,
    StripChain::Stage::Reverb, StripChain::Stage::Limiter,
};

// Where the list of the operator's own presets lives.
const char kUserIndexKey[] = "ChannelStrip/UserPresets";

QString userPrefix(const QString& name)
{
    return QStringLiteral("ChannelStrip/User/") + name + QLatin1Char('/');
}

} // namespace

void saveTo(const QString& prefix, const StripChain& chain)
{
    auto& c = const_cast<StripChain&>(chain);   // accessors are non-const

    // Per-stage enables. The master switch is not written — it loads
    // off every time, on purpose.
    //
    // Ueber kLegacyStageOrder, nicht ueber den Aufzaehlungswert: am
    // 2026-10-10 kam das Entrauschen als neue erste Stufe dazu, und
    // damit waere aus jedem gespeicherten „Stage0" (Gate) ploetzlich
    // das Entrauschen geworden. Die Reihenfolge unten ist Teil des
    // Dateiformats und bewegt sich nicht mehr; was neu dazukommt,
    // bekommt einen eigenen Schluessel.
    for (int i = 0; i < int(std::size(kLegacyStageOrder)); ++i) {
        putV(key(prefix, "Stage") + QString::number(i),
             c.stageEnabled(kLegacyStageOrder[i]));
    }
    putV(key(prefix, "DenoiseOn"),
         c.stageEnabled(StripChain::Stage::Denoise));
    putF(prefix, "DenoiseAmount", c.nr().amount());
    putF(prefix, "InputGainDb",   c.inputGainDb());
    putV(key(prefix, "InputAuto"), c.inputAuto());
    putV(key(prefix, "ClipOn"), c.stageEnabled(StripChain::Stage::Clip));
    putF(prefix, "ClipDriveDb", c.clipper().driveDb());
    putF(prefix, "ClipKnee",    c.clipper().knee());
    putV(key(prefix, "ClipMatch"), c.clipper().levelMatch());

    putF(prefix, "GateThresholdDb", c.gate().thresholdDb());
    putF(prefix, "GateRatio",       c.gate().ratio());
    putF(prefix, "GateAttackMs",    c.gate().attackMs());
    putF(prefix, "GateReleaseMs",   c.gate().releaseMs());
    putF(prefix, "GateHoldMs",      c.gate().holdMs());
    putF(prefix, "GateFloorDb",     c.gate().floorDb());
    putF(prefix, "GateReturnDb",    c.gate().returnDb());
    putF(prefix, "GateLookaheadMs", c.gate().lookaheadMs());
    putV(key(prefix, "GateMode"), int(c.gate().mode()));

    putF(prefix, "CompThresholdDb", c.comp().thresholdDb());
    putF(prefix, "CompRatio",       c.comp().ratio());
    putF(prefix, "CompAttackMs",    c.comp().attackMs());
    putF(prefix, "CompReleaseMs",   c.comp().releaseMs());
    putF(prefix, "CompKneeDb",      c.comp().kneeDb());
    putF(prefix, "CompMakeupDb",    c.comp().makeupDb());
    putV(key(prefix, "CompAutoMakeup"), c.comp().autoMakeup());
    putF(prefix, "CompDriveDb",     c.comp().driveDb());
    putV(key(prefix, "CompPhaseStages"), c.comp().phaseRotatorStages());
    putV(key(prefix, "CompLimiterOn"),   c.comp().limiterEnabled());
    putF(prefix, "CompLimiterCeilingDb", c.comp().limiterCeilingDb());

    putF(prefix, "DeEssFrequencyHz", c.deEss().frequencyHz());
    putF(prefix, "DeEssQ",           c.deEss().q());
    putF(prefix, "DeEssThresholdDb", c.deEss().thresholdDb());
    putF(prefix, "DeEssAmountDb",    c.deEss().amountDb());
    putF(prefix, "DeEssAttackMs",    c.deEss().attackMs());
    putF(prefix, "DeEssReleaseMs",   c.deEss().releaseMs());
    putV(key(prefix, "DeEssSlopeStages"), c.deEss().slopeStages());

    // EQ: the bands are the whole setting, so all of them go, with the
    // active count so a shorter set does not leave stale bands behind.
    const int bands = c.eq().activeBandCount();
    putV(key(prefix, "EqBandCount"), bands);
    putV(key(prefix, "EqMasterGain"), double(c.eq().masterGain()));
    putV(key(prefix, "EqFamily"), int(c.eq().filterFamily()));
    for (int i = 0; i < bands; ++i) {
        const ClientEq::BandParams p = c.eq().band(i);
        const QString b = key(prefix, "EqBand") + QString::number(i);
        putV(b + QStringLiteral("Freq"),    double(p.freqHz));
        putV(b + QStringLiteral("Gain"),    double(p.gainDb));
        putV(b + QStringLiteral("Q"),       double(p.q));
        putV(b + QStringLiteral("Type"),    int(p.type));
        putV(b + QStringLiteral("On"),      p.enabled);
        putV(b + QStringLiteral("Slope"),   p.slopeDbPerOct);
    }

    // ── Die Reihenfolge der Stufen ──────────────────────────────
    //
    // Als Liste von Woertern, nicht als Zahlen: eine Datei, die
    // „4,5,6" sagt, ist beim naechsten Umbau des Aufzaehlungstyps
    // eine andere Kette, ohne dass jemand etwas merkt. Fehlt der
    // Schluessel, gilt die Vorgabe -- jede Datei von vor dem
    // 2026-10-13 ist damit richtig gelesen.
    {
        QStringList folge;
        for (const StripChain::Stage st : c.stageOrder()) {
            folge << QString::fromLatin1(StripChain::stageToken(st));
        }
        putV(key(prefix, "StageOrder"), folge.join(QLatin1Char(',')));
    }

    putF(prefix, "LimiterCeilingDb",    c.limiter().ceilingDb());
    putF(prefix, "LimiterOutputTrimDb", c.limiter().outputTrimDb());
    putV(key(prefix, "LimiterDcBlock"), c.limiter().dcBlockEnabled());

    // Tube, Reverb, Pudu (2026-08-11). These three stages arrived after
    // this serializer was written and never joined it — so their knobs
    // silently reset on every restart AND fell out of user presets,
    // which is how tst_strip_chain's round-trip caught it: a preset
    // with 9 dB of tube drive came back with 0.
    putV(key(prefix, "TubeModel"), int(c.tube().model()));
    putF(prefix, "TubeDriveDb",        c.tube().driveDb());
    putF(prefix, "TubeBiasAmount",     c.tube().biasAmount());
    putF(prefix, "TubeTone",           c.tube().tone());
    putF(prefix, "TubeOutputGainDb",   c.tube().outputGainDb());
    putF(prefix, "TubeDryWet",         c.tube().dryWet());
    putF(prefix, "TubeEnvelopeAmount", c.tube().envelopeAmount());
    putF(prefix, "TubeAttackMs",       c.tube().attackMs());
    putF(prefix, "TubeReleaseMs",      c.tube().releaseMs());

    putF(prefix, "ReverbSize",       c.reverb().size());
    putF(prefix, "ReverbDecayS",     c.reverb().decayS());
    putF(prefix, "ReverbDamping",    c.reverb().damping());
    putF(prefix, "ReverbPreDelayMs", c.reverb().preDelayMs());
    putF(prefix, "ReverbMix",        c.reverb().mix());

    // Die Betriebsart fehlte hier bis zum 2026-10-13: Aphex oder
    // Behringer ueberlebte keinen Neustart und fiel stumm auf Aphex
    // zurueck. Aufgefallen beim Abgleich mit Aethers Profilformat,
    // das sie als "mode" mitfuehrt.
    putV(key(prefix, "PuduMode"), int(c.pudu().mode()));
    putF(prefix, "PuduPooDriveDb",     c.pudu().pooDriveDb());
    putF(prefix, "PuduPooTuneHz",      c.pudu().pooTuneHz());
    putF(prefix, "PuduPooMix",         c.pudu().pooMix());
    putF(prefix, "PuduDooTuneHz",      c.pudu().dooTuneHz());
    putF(prefix, "PuduDooHarmonicsDb", c.pudu().dooHarmonicsDb());
    putF(prefix, "PuduDooMix",         c.pudu().dooMix());

    // ── Die Einschuebe ──────────────────────────────────────────────
    //
    // Geschrieben wird der Bezeichner, die Stelle, der Schalter und
    // der Zustand des Plugins als Base64. Ein leerer Platz schreibt
    // einen leeren Bezeichner -- also nichts, was auf einem anderen
    // Rechner etwas zu suchen anfaengt.
    for (int i = 0; !g_skipInserts && i < StripChain::kInsertCount; ++i) {
        const QString at = QString::number(i);
        const AudioPlugin* p = c.insertPlugin(i);
        AppSettings::instance().setValue(
            key(prefix, "InsertId") + at,
            p != nullptr ? p->descriptor().identifier : QString());
        AppSettings::instance().setValue(
            key(prefix, "InsertAt") + at, c.insertPosition(i));
        AppSettings::instance().setValue(
            key(prefix, "InsertOn") + at, c.insertEnabled(i));
        AppSettings::instance().setValue(
            key(prefix, "InsertState") + at,
            p != nullptr ? QString::fromLatin1(p->saveState().toBase64())
                         : QString());
    }
}

void restoreFrom(const QString& prefix, StripChain& c)
{
    // Zuerst die Reihenfolge: sie entscheidet, in welcher Folge die
    // Stufen laufen, nicht welche Werte sie tragen -- aber wer unten
    // beim Lesen eines Wertes stolpert, soll wenigstens die richtige
    // Kette vor sich haben.
    {
        const QString roh = getV(key(prefix, "StageOrder")).toString();
        if (!roh.isEmpty()) {
            const QStringList teile = roh.split(QLatin1Char(','),
                                                Qt::SkipEmptyParts);
            StripChain::Order folge{};
            int n = 0;
            for (const QString& t : teile) {
                StripChain::Stage st{};
                if (n < StripChain::kStageCount
                    && StripChain::stageFromToken(t.trimmed().toLatin1()
                                                      .constData(), st)) {
                    folge[size_t(n)] = st;
                    ++n;
                }
            }
            // setStageOrder() nimmt nur eine Permutation an; eine
            // unvollstaendige oder verdorbene Liste laesst die Vorgabe
            // stehen, statt eine halbe Kette zu bauen.
            if (n == StripChain::kStageCount) { c.setStageOrder(folge); }
        }
    }


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
    // Fehlt der Schluessel, ist die Kette aelter als der Ausgleich --
    // dann bleibt er aus, damit eine gespeicherte Einstellung nicht
    // ueber Nacht um mehrere Dezibel lauter wird.
    c.comp().setAutoMakeup (getB(prefix, "CompAutoMakeup", false));
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
            const QVariant f = getV(b + QStringLiteral("Freq"));
            if (!f.isValid()) { continue; }
            p.freqHz = float(f.toDouble());
            p.gainDb = float(getV(b + QStringLiteral("Gain"), 0.0).toDouble());
            p.q      = float(getV(b + QStringLiteral("Q"), 0.707).toDouble());
            p.type   = static_cast<ClientEq::FilterType>(
                getV(b + QStringLiteral("Type"), 0).toInt());
            p.enabled = getV(b + QStringLiteral("On"), true).toBool();
            p.slopeDbPerOct =
                getV(b + QStringLiteral("Slope"), 12).toInt();
            c.eq().setBand(i, p);
        }
        c.eq().setActiveBandCount(bands);
    }
    // Whatever was on disk — seven bands from an older version, nothing
    // at all on a first run — the operator gets the full set of handles.
    // Appends, never overwrites; see the note on ensureSeeded.
    EqBandLayout::ensureSeeded(c.eq());
    {
        const QVariant g = getV(key(prefix, "EqMasterGain"));
        if (g.isValid()) {
            // ── Eine gespeicherte Null wird nicht uebernommen ────────
            //
            // setMasterGain nimmt einen LINEAREN Faktor. Null heisst
            // dort „mal null", also vollstaendige Stille -- und zwar
            // dauerhaft, denn der Wert wird gespeichert und ueberlebt
            // jeden Profilwechsel. Danach ist nichts mehr zu hoeren,
            // egal welche Kurve, welches Profil, welcher Schalter.
            //
            // Dass hier repariert und nicht nur gewarnt wird, hat
            // einen nachpruefbaren Grund: der Ausgangssteller des
            // Entzerrers reicht nur bis -60 dB
            // (ClientEqOutputFader::kMeterMinDb). Eine echte Null ist
            // ueber die Oberflaeche GAR NICHT einstellbar -- sie kann
            // nur aus einem Fehler stammen, und sie stammt aus einem
            // (mein „Flat"-Knopf, 2026-10-09). Ein gueltiger Wert
            // waere hinter dem Ruecken des Betreibers nicht
            // anzutasten; dieser ist keiner.
            const double linear = g.toDouble();
            if (linear < 1e-6) {
                qWarning("StripSettings: gespeicherter Entzerrer-"
                         "Ausgangspegel ist 0 (voellige Stille) -- auf 1,0 "
                         "zurueckgesetzt. Ueber die Oberflaeche ist dieser "
                         "Wert nicht erreichbar; er stammt aus einem "
                         "Fehler.");
                c.eq().setMasterGain(1.0f);
            } else {
                c.eq().setMasterGain(float(linear));
            }
        }
        const QVariant fam = getV(key(prefix, "EqFamily"));
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

    // Tube, Reverb, Pudu — see the matching note in saveTo(). Defaults
    // are the current values, same convention as every stage above, so
    // a file from before these keys existed changes nothing.
    c.tube().setModel(static_cast<ClientTube::Model>(
        getI(prefix, "TubeModel", int(c.tube().model()))));
    c.tube().setDriveDb   (getF(prefix, "TubeDriveDb",      c.tube().driveDb()));
    c.tube().setBiasAmount(getF(prefix, "TubeBiasAmount",   c.tube().biasAmount()));
    c.tube().setTone      (getF(prefix, "TubeTone",         c.tube().tone()));
    c.tube().setOutputGainDb(
        getF(prefix, "TubeOutputGainDb", c.tube().outputGainDb()));
    c.tube().setDryWet    (getF(prefix, "TubeDryWet",       c.tube().dryWet()));
    c.tube().setEnvelopeAmount(
        getF(prefix, "TubeEnvelopeAmount", c.tube().envelopeAmount()));
    c.tube().setAttackMs  (getF(prefix, "TubeAttackMs",     c.tube().attackMs()));
    c.tube().setReleaseMs (getF(prefix, "TubeReleaseMs",    c.tube().releaseMs()));

    c.reverb().setSize      (getF(prefix, "ReverbSize",       c.reverb().size()));
    c.reverb().setDecayS    (getF(prefix, "ReverbDecayS",     c.reverb().decayS()));
    c.reverb().setDamping   (getF(prefix, "ReverbDamping",    c.reverb().damping()));
    c.reverb().setPreDelayMs(getF(prefix, "ReverbPreDelayMs", c.reverb().preDelayMs()));
    c.reverb().setMix       (getF(prefix, "ReverbMix",        c.reverb().mix()));

    c.pudu().setMode(static_cast<ClientPudu::Mode>(
        getI(prefix, "PuduMode", int(c.pudu().mode()))));
    c.pudu().setPooDriveDb    (getF(prefix, "PuduPooDriveDb",     c.pudu().pooDriveDb()));
    c.pudu().setPooTuneHz     (getF(prefix, "PuduPooTuneHz",      c.pudu().pooTuneHz()));
    c.pudu().setPooMix        (getF(prefix, "PuduPooMix",         c.pudu().pooMix()));
    c.pudu().setDooTuneHz     (getF(prefix, "PuduDooTuneHz",      c.pudu().dooTuneHz()));
    c.pudu().setDooHarmonicsDb(
        getF(prefix, "PuduDooHarmonicsDb", c.pudu().dooHarmonicsDb()));
    c.pudu().setDooMix        (getF(prefix, "PuduDooMix",         c.pudu().dooMix()));

    // Enables last, so no stage starts running with half its parameters
    // restored. On a fast machine the difference is a few microseconds;
    // on the transmit path it is the difference between a clean start
    // and one burst of whatever the defaults happened to be.
    for (int i = 0; i < int(std::size(kLegacyStageOrder)); ++i) {
        c.setStageEnabled(kLegacyStageOrder[i],
            getV(key(prefix, "Stage") + QString::number(i), false).toBool());
    }
    c.setStageEnabled(StripChain::Stage::Denoise,
                      getV(key(prefix, "DenoiseOn"), false).toBool());
    c.nr().setAmount(getF(prefix, "DenoiseAmount", 0.8f));
    c.setInputGainDb(getF(prefix, "InputGainDb", 0.0f));
    // Vorgabe AN. Jede Schwelle dieser Kette ist auf eine Spitze von
    // -12 dBFS gerechnet, und wer den Pegel nicht von Hand setzt,
    // hoert eine Kette, in der nichts greift -- das war der Zustand,
    // in dem der Betreiber sie beurteilt hat. Ein Vorverstaerker, der
    // erst nach einem Knopfdruck verstaerkt, ist keiner.
    c.setInputAuto(getB(prefix, "InputAuto", true));
    c.setStageEnabled(StripChain::Stage::Clip,
                      getV(key(prefix, "ClipOn"), false).toBool());
    c.clipper().setDriveDb(getF(prefix, "ClipDriveDb", 0.0f));
    c.clipper().setKnee(getF(prefix, "ClipKnee", 0.3f));
    c.clipper().setLevelMatch(getB(prefix, "ClipMatch", false));
    // And the master stays where it is — off. See StripSettings.h.

    // ── Die Einschuebe ──────────────────────────────────────────────
    //
    // Regel 2 aus AudioPlugin.h steht hier in Code: ein Plugin, das
    // auf diesem Rechner nicht installiert ist, laesst den Platz leer
    // und sonst nichts geschehen. Keine Warnung, keine Sperre -- die
    // Kette klingt dann wie ohne ihn, und das ist die einzige
    // Antwort, die auf einem fremden Rechner zu geben ist.
    //
    // Nachgeladen wird NUR, wenn ein Bezeichner dasteht. Wer nie
    // einen Einschub befuellt hat, loest hier keinen Suchlauf aus und
    // laedt keine fremde Bibliothek (Regel 1).
    for (int i = 0; !g_skipInserts && i < StripChain::kInsertCount; ++i) {
        const QString at = QString::number(i);
        c.setInsertPosition(i,
            getV(key(prefix, "InsertAt") + at, 0).toInt());
        c.setInsertEnabled(i,
            getV(key(prefix, "InsertOn") + at, true).toBool());

        const QString id =
            getV(key(prefix, "InsertId") + at, QString()).toString();
        const AudioPlugin* have = c.insertPlugin(i);
        if (id.isEmpty()) {
            if (have != nullptr) { c.setInsertPlugin(i, nullptr); }
            continue;
        }
        const QByteArray blob = QByteArray::fromBase64(
            getV(key(prefix, "InsertState") + at, QString())
                .toString().toLatin1());

        // Schon das richtige drin? Dann nicht neu laden -- sonst
        // verliert jeder Abgleichtakt des Mithoerers den Zustand des
        // Plugins und faengt von vorne an. Aber den gespeicherten
        // Zustand uebernehmen, WENN er sich geaendert hat: der
        // Mithoerer hat seine eigene Instanz desselben Plugins, und
        // ohne das bliebe sie auf dem Stand vom Laden. Man draehte an
        // einem Regler und hoerte nichts -- womit das Mithoeren beim
        // Einstellen eines Plugins genau dort nutzlos waere, wo es
        // gebraucht wird. applyInsertState() vergleicht erst und
        // haelt den Audiofaden nur an, wenn wirklich etwas anders ist.
        if (have != nullptr && have->descriptor().identifier == id) {
            c.applyInsertState(i, blob);
            continue;
        }

        std::unique_ptr<AudioPlugin> loaded = PluginHost::load(id);
        if (!loaded) { continue; }
        if (!blob.isEmpty()) { loaded->restoreState(blob); }
        c.setInsertPlugin(i, std::move(loaded));
    }
}

// ── Eine Kette in eine andere, ohne Umweg ueber die Platte ──────────
//
// „waere besser, dass sobald ich eine aenderung vorgenommen habe, die
// auch sofort zu hoeren ist."
//
// Der Mithoerer hat einen EIGENEN Kanalzug (zwei Audiofaeden in
// denselben Filterzustaenden waeren ein Datenrennen) und holte sich
// die Einstellung bisher aus den GESPEICHERTEN Werten. Gespeichert
// wird aber erst beim Loslassen der Maus -- also hoerte man eine
// Kurvenaenderung erst danach, und dann noch bis zu einer halben
// Sekunde spaeter. Genau so wurde es berichtet: „leider hoere ich
// beim EQ erst den unterschied, wenn ich per drag and drop
// ausgelassen habe."
//
// Hier wird deshalb direkt von Kette zu Kette kopiert, ueber eine
// Tabelle im Arbeitsspeicher. Dieselbe Feldliste wie beim Speichern,
// nur mit umgelegter Schreibrichtung -- siehe g_redirect.
//
// NICHT mitkopiert: der Hauptschalter (den fuehrt der Mithoerer
// selbst, als Ueberbrueckung) und die Einschuebe (ein Plugin laedt
// man nicht zehnmal in der Sekunde; dafuer gibt es
// StripChain::applyInsertState).
void copyParameters(const StripChain& from, StripChain& to)
{
    Map scratch;
    g_redirect = &scratch;
    g_skipInserts = true;
    saveTo(QString(), from);
    restoreFrom(QString(), to);
    g_skipInserts = false;
    g_redirect = nullptr;
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
        {QStringLiteral("Recommended — speech"),
         QStringLiteral("What I would set, and why each part is there. "
                        "Denoise first so the compressor never lifts the "
                        "room; a gentle gate behind it; a small, honest "
                        "EQ; 3:1 compression; 12 dB of clipping for the "
                        "talk power, and a brickwall above it. Every "
                        "number here was measured, not guessed. Tube, "
                        "exciter and reverb stay OFF — they colour, and "
                        "on SSB colour costs bandwidth you do not have. "
                        "A starting point like any other: adjust from it.")},
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

    // ── Die Empfehlung ──────────────────────────────────────────────
    //
    // „kannst du mal deine empfehlung bei allem eingeben?"
    //
    // Hier steht sie, als Ausgangspunkt wie jeder andere -- nicht als
    // Automatik. Jede Zeile hat einen Grund, und die Gründe stammen
    // aus der Arbeit dieses Tages:
    //
    //   * Entrauschen VOR allem: ein Kompressor mit Anhebung macht
    //     aus einem leisen Lüfter ein Rauschen, das mitsendet.
    //   * Das Gate darf deshalb sanft sein -- es muss den Raum nicht
    //     mehr alleine wegräumen.
    //   * Der Entzerrer bleibt klein. Drei Griffe, keine Kurve aus
    //     sieben Bergen; was dort zuviel ist, hört die Gegenseite als
    //     Anstrengung.
    //   * 3:1 ist die Alltagseinstellung: hörbar ruhiger und immer
    //     noch erkennbar eine Stimme.
    //   * Der Clipper bringt die Sprechleistung, nicht der
    //     Kompressor. Unter 12 dB Drive tut er nachweislich gar
    //     nichts -- seine Kante liegt bei Vollaussteuerung.
    //   * Röhre, Exciter und Hall AUS. Sie färben, und auf SSB kostet
    //     Farbe Bandbreite, die man nicht hat. Wer sie will, schaltet
    //     sie einzeln dazu und hört hin.
    // QStringLiteral, nicht QLatin1String: der Gedankenstrich ist in
    // UTF-8 drei Bytes, und QLatin1String liest sie einzeln als
    // Latin-1 -- der Vergleich ist dann immer falsch, und das Profil
    // tut gar nichts. Genau so ist es beim ersten Lauf passiert.
    if (name == QStringLiteral("Recommended — speech")) {
        allOff();

        // ── Gemessen, nicht geschaetzt ──────────────────────────
        //
        // Der Messlauf vom 2026-10-10 (tst_kanalzug_messlauf) hat den
        // Preis des Entrauschens in Zahlen gefasst:
        //
        //   Anteil   4 kHz    6 kHz   Pausen   Sprache
        //    0,20    -0,2     -0,3     -1,9     -1,6
        //    0,35    -0,6     -0,7     -3,7     -2,6
        //    0,50    -1,7     -1,9     -6,0     -2,9
        //    0,70    -6,7     -7,6    -10,5     -2,4
        //    1,00   -13,1    -23,9    -66,9     -0,1
        //
        // ── Und am Abend war sie ungueltig ──────────────────────
        //
        // Die Zahlen oben sind echt und messen die falsche Sache:
        // gemessen wurde an einer NACHGEBAUTEN Stimme, und rnnoise
        // hat die mit einer Wahrscheinlichkeit von 0,13 fuer Sprache
        // gehalten. Was dort steht, ist also, wie stark ein
        // Sprachnetz etwas wegdrueckt, das es nicht als Sprache
        // erkennt -- nicht der Preis des Entrauschens.
        //
        // Neu gemessen an wirklich gesprochener Sprache
        // (tst_bandbreiten_genau, Sprachwahrscheinlichkeit 0,98),
        // und erst nachdem ein Versatz von 20 ms zwischen trockenem
        // und entrauschtem Weg behoben war:
        //
        //   Anteil   2 kHz   4 kHz   Sprache   Pausen   Stoerabstand
        //    0,20    -0,1    -0,1     -0,1      -2,0       +1,9
        //    0,35    -0,1    -0,2     -0,1      -3,8       +3,7
        //    0,50    -0,2    -0,2     -0,1      -6,0       +5,9
        //    0,70    -0,2    -0,3     -0,1     -10,4      +10,3
        //    1,00    -0,2    -0,4     -0,1     -28,8      +28,7
        //
        // Die Stufe kostet an JEDER Stellung ein Zehntel Dezibel
        // Sprache. Es gibt also keinen Grund, niedrig zu bleiben.
        // Der Grund, sie nicht ganz aufzudrehen, ist ein anderer:
        // voll aufgedreht steht kein trockenes Signal mehr daneben,
        // das einen Kunstlaut des Netzes verdecken koennte. 0,70
        // bringt den Raum vierzig Dezibel unter die Stimme -- auf
        // SSB unhoerbar -- und laesst 30 % echtes Signal als Anker.
        c.nr().setAmount(0.70f);
        c.setStageEnabled(StripChain::Stage::Denoise, true);

        // ── Der Pegel zuerst, und von selbst ────────────────────
        //
        // Jede Zahl, die danach kommt, steht in dBFS und ist auf eine
        // Sprachspitze von -12 dBFS gerechnet. Am Geraet des
        // Betreibers kamen -39,6 dBFS an -- 28 dB zu wenig. Dann
        // liegt die Gate-Schwelle UEBER der Stimme, der Kompressor
        // greift nie, der Clipper schneidet nichts ab, und die Kette
        // tut genau zwei Dinge: Woerter verschlucken und faerben.
        //
        // Ein Profil, das die Schwellen setzt, aber den Pegel nicht,
        // setzt deshalb gar nichts.
        c.setInputAuto(true);

        // Gemessen: bei einem Grundgeräusch von -48 dBFS EFFEKTIV
        // liegen dessen Spitzen um -38, und genau dort stand die
        // Schwelle -- das Gate hat deshalb gar nichts getan.
        //
        //   Schwelle   Pausen gegen roh
        //    -50 dB        0,0
        //    -42 dB        0,0
        //    -38 dB        0,0   <- war eingestellt
        //    -34 dB       -3,0
        //    -28 dB       -7,5
        //
        // Eine Schwelle gehört über die SPITZEN des Raums, nicht
        // über seinen Effektivwert. -34 ist der Anfang der Wirkung
        // und liegt immer noch elf Dezibel unter der Sprache.
        c.gate().setThresholdDb(-34.0f);
        c.gate().setFloorDb(-18.0f);
        c.gate().setAttackMs(3.0f);
        c.gate().setHoldMs(40.0f);
        c.gate().setReleaseMs(180.0f);
        c.gate().setReturnDb(5.0f);
        c.setStageEnabled(StripChain::Stage::Gate, true);

        // ── Nichts wegnehmen, was nicht weg muss ────────────────
        //
        // „also roh, volle bandbreite — auf dem signal sollte man
        //  aufbauen … aber die abstriche sind leider viel zu groß."
        //
        // Hier stand ein Hochpass bei 110 Hz mit 24 dB je Oktave.
        // Gemessen an echter Sprache, bei GLEICHER LAUTHEIT gegen
        // das rohe Signal (tst_bandbreiten_genau::whatTheChainTakesAway):
        //
        //   Band      mit Entzerrer   ohne Entzerrer
        //    63 Hz        -6,7 dB         -1,5 dB
        //    80 Hz        -8,0 dB         -0,4 dB
        //   100 Hz        -4,9 dB         -0,2 dB
        //   125 Hz        -1,7 dB         +0,4 dB
        //   630 Hz        -1,5 dB         -0,2 dB
        //
        // Acht Dezibel bei 80 Hz. Der Grundton einer Männerstimme
        // liegt bei 100 bis 125 Hz -- der Hochpass stand also auf
        // dem Grundton und hat ihm das Fundament genommen. Ohne den
        // Entzerrer ist die Kette unter 1 kHz durchsichtig; mit ihm
        // war sie das Gegenteil dessen, was der Betreiber hört, wenn
        // er roh vergleicht.
        //
        // 60 Hz mit 12 dB je Oktave: bei 110 Hz praktisch nichts
        // mehr (-0,5 dB), bei 30 Hz zwölf Dezibel weniger Rumpeln.
        // Ein Hochpass hat eine Aufgabe -- Trittschall, Griffgeräusch
        // und der Tiefenanteil eines Plosivs fressen Aussteuerung,
        // die der Clipper braucht --, aber sie liegt UNTER der
        // Stimme und nicht in ihr.
        band(0, ClientEq::FilterType::HighPass, 60.0f, 0.0f, 0.707f, true, 12);
        band(1, ClientEq::FilterType::Peak,  50.0f, -18.0f, 8.0f, false, 12);
        band(2, ClientEq::FilterType::Peak, 100.0f, -12.0f, 8.0f, false, 12);
        band(3, ClientEq::FilterType::Peak, 150.0f,  -9.0f, 8.0f, false, 12);
        // Das Tiefenregal nahm 1,5 dB unter 200 Hz -- derselbe
        // Abstrich am selben Ende, nur sanfter. Steht jetzt auf
        // null und bleibt als Griff stehen, für den, der es will.
        band(4, ClientEq::FilterType::LowShelf,   200.0f,  0.0f, 0.707f, true, 12);
        band(5, ClientEq::FilterType::Peak,      2000.0f,  2.0f, 1.2f,   true, 12);
        band(6, ClientEq::FilterType::HighShelf, 3000.0f,  0.0f, 0.707f, true, 12);
        // Die sieben freien Griffe: zwei davon benutzt, der Rest
        // flach und aus -- so steht in der einfachen Ansicht etwas,
        // das man nachvollziehen kann, und nicht ein Berg aus sieben.
        for (int i = EqBandLayout::kFirstSpare;
             i < EqBandLayout::kBandCount; ++i) {
            c.eq().setBand(i, EqBandLayout::spare(i - EqBandLayout::kFirstSpare));
        }
        {
            // PRÄSENZ 2,5 kHz leicht vor -- und MULM 650 Hz NICHT
            // mehr zurück. Gemessen kostete der Griff 1,5 dB bei
            // 630 Hz, und dort sitzt bei einer Männerstimme der
            // Körper. Wer ihn dumpf findet, zieht den Griff selbst
            // herunter; ein Vorgabewert, der etwas wegnimmt, gehört
            // nicht in ein Profil, auf dem man aufbauen soll.
            ClientEq::BandParams prae0 = EqBandLayout::spare(5);
            prae0.gainDb = 2.5f; prae0.q = 2.0f; prae0.enabled = true;
            c.eq().setBand(EqBandLayout::kFirstSpare + 5, prae0);
        }
        c.eq().setActiveBandCount(EqBandLayout::kBandCount);
        c.setStageEnabled(StripChain::Stage::Eq, true);

        // ── Die Schwelle gilt fuer den SEITENWEG ────────────────
        //
        // -28 stand hier und hat nie gegriffen, nicht ein einziges
        // Mal. Gemessen an gesprochener Sprache mit -12 dBFS
        // Spitze: der Seitenweg -- ein Bandpass mit 24 dB je Oktave
        // um 6 kHz -- erreicht hoechstens -25,3 dBFS, und die
        // geglaettete Huellkurve liegt noch darunter. Eine Schwelle
        // fuer einen De-Esser gehoert deshalb rund 13 dB unter die,
        // die man im Vollband waehlen wuerde.
        //
        //   Schwelle   5..8 kHz   1..3 kHz   Minderung
        //     -28        0,00       0,00       0,00
        //     -34       -0,00       0,00      -2,40
        //     -40       -0,43       0,00      -6,00
        //     -46       -1,27       0,01      -6,00
        //
        // -36 ist der Anfang der Wirkung. Die Mitte bleibt dabei
        // unberuehrt (0,00 dB in 1..3 kHz): die Stufe senkt nur ihr
        // eigenes Band, nicht das ganze Signal.
        //
        // Wichtig fuer die Kanalwahl: bei 6 kHz bewacht diese Stufe
        // eine Stelle, die der Sender im DX-Kanal um 63,9 dB
        // abschneidet und im 2700er um 58,5 dB. Sie lohnt sich nur
        // im 4-kHz-Kanal; in den schmalen kann man sie abschalten,
        // ohne etwas zu verlieren.
        c.deEss().setFrequencyHz(6000.0f);
        c.deEss().setThresholdDb(-36.0f);
        c.setStageEnabled(StripChain::Stage::DeEss, true);

        c.comp().setThresholdDb(-22.0f);
        c.comp().setRatio(3.0f);
        c.comp().setAttackMs(8.0f);
        c.comp().setReleaseMs(150.0f);
        c.comp().setKneeDb(8.0f);
        // ── Drei von Hand, der Rest gerechnet ────────────────────
        //
        // Der Ausgleich steuert bei dieser Schwelle und diesem
        // Verhaeltnis 6,7 dB bei: (-12 - (-22)) * (1 - 1/3). Ohne ihn
        // nahm der Kompressor dem Clipper genau diese 6,7 dB weg --
        // und der Clipper ist die Stufe, die den Druck macht. Allein
        // das Einschalten bringt die Lautheit bei gleicher Spitze von
        // -15,9 auf -13,0 LUFS.
        c.comp().setMakeupDb(3.0f);
        c.comp().setAutoMakeup(true);
        c.comp().setPhaseRotatorStages(4);
        c.setStageEnabled(StripChain::Stage::Comp, true);

        // Ebenfalls gemessen: bei Sprachspitzen von -12 dBFS und
        // einer Kante bei Vollaussteuerung passiert unter 12 dB
        // Drive GAR NICHTS -- der Clipper war nur eine Verstaerkung.
        //
        //   Drive   Spitze  Scheitelfaktor
        //     3 dB   -8,7    11,7 -> 11,6
        //     9 dB   -2,7    11,7 -> 11,6
        //    12 dB   -0,3    11,7 -> 11,1
        //    15 dB    0,0    11,7 ->  9,0
        //
        // 12 dB ist der Anfang der Wirkung und zugleich die Stelle,
        // an der der Begrenzer dahinter (Decke -1 dB) die Spitze
        // uebernimmt. Mehr ist eine Entscheidung des Betreibers, die
        // er an der Zahl auf der Seite ablesen kann.
        // ── 18, nicht 12 ────────────────────────────────────────
        //
        // ── 12, und zwar MIT dem Ausgleich darueber ─────────────
        //
        // Hier stand zuerst 18. Das war richtig gemessen und trotzdem
        // falsch: die Reihe entstand, BEVOR der Ausgleich im
        // Kompressor eingeschaltet war. Der schickt jetzt 6,7 dB mehr
        // in den Clipper, und damit verschiebt sich dessen bester
        // Punkt um genau diese 6,7 dB nach unten. Zwei Aenderungen,
        // die einander bedingen -- deshalb wurde nach der ersten neu
        // gemessen.
        //
        // Mit Ausgleich, an echter Sprache
        // (tst_bandbreiten_genau::howMuchDriveTheClipperTakes):
        //
        //   Drive   Lautheit   Scheitelfaktor   abgeschnitten   Klirr
        //     0     -18,1 LUFS     15,1 dB          0,0 dB      -80,9
        //     6     -15,3 LUFS     12,3 dB          1,2 dB      -80,8
        //    12     -11,1 LUFS      8,2 dB          7,0 dB      -49,3
        //    15      -9,6 LUFS      6,7 dB         10,0 dB      -21,1
        //    18      -8,4 LUFS      5,7 dB         13,0 dB      -14,2
        //
        // Den Ausschlag gibt der Klirr: -49 dB sind 0,34 % und bei
        // einem Clipper in Ordnung, -21 dB sind 9 % und -14 dB sind
        // 19 %. Das ist kein Druck mehr, das ist Verzerrung.
        //
        // 12 dB bringen 8,2 dB Scheitelfaktor -- genau der Bereich,
        // in dem ein Sendekanalzug arbeitet -- und gegenueber dem
        // Stand von heute Mittag 4,8 dB mehr Lautheit bei gleicher
        // Spitze. Der Klirr am Ton ist dabei der schlimmste Fall: ein
        // Sinus steht dauernd an der Kante, eine Stimme nur in den
        // Spitzen.
        c.clipper().setDriveDb(12.0f);
        c.clipper().setKnee(0.2f);
        c.clipper().setLevelMatch(false);
        c.setStageEnabled(StripChain::Stage::Clip, true);

        // ── -2, nicht -1 ────────────────────────────────────────
        //
        // Gemessen an echter Sprache durch diese Kette: der
        // Begrenzer selbst laesst 0,52 dB ueber seine Decke (keine
        // Vorausschau, siehe ClientFinalLimiter.h), und das
        // Kanalfilter des Senders hebt die Spitzen danach noch
        // einmal um bis zu 0,69 dB -- bei -1,0 dB Decke stand nach
        // dem DX-Kanal +0,21 dBFS, also ueber Vollaussteuerung.
        //
        //   Kanal            Spitze bei Decke -1,0   Ueberschwinger
        //   200-2500            +0,21 dBFS              0,69 dB
        //   150-2700            -0,05 dBFS              0,43 dB
        //   100-3200            -0,13 dBFS              0,35 dB
        //    50-4050            +0,07 dBFS              0,55 dB
        //
        // 1,2 dB Luft decken den schlimmsten Fall. Wer die Kette
        // ohne Kanalfilter hoert, verliert damit ein Dezibel
        // Lautstaerke; wer sie sendet, verliert nichts und schneidet
        // nicht ab.
        c.limiter().setCeilingDb(-2.0f);
        c.setStageEnabled(StripChain::Stage::Limiter, true);

        // Röhre, PUDU und Hall bleiben aus -- allOff() hat sie
        // ausgeschaltet, und hier steht ausdrücklich nichts.
        return true;
    }

    if (name == QLatin1String("Clean voice")) {
        allOff();
        band(0, ClientEq::FilterType::HighPass, 100.0f, 0.0f, 0.707f, true, 24);
        band(1, ClientEq::FilterType::Peak,  50.0f, -18.0f, 8.0f, false, 12);
        band(2, ClientEq::FilterType::Peak, 100.0f, -12.0f, 8.0f, false, 12);
        band(3, ClientEq::FilterType::Peak, 150.0f,  -9.0f, 8.0f, false, 12);
        band(4, ClientEq::FilterType::LowShelf,   200.0f,  0.0f, 0.707f, true, 12);
        band(5, ClientEq::FilterType::Peak,      2000.0f,  1.5f, 1.0f,   true, 12);
        band(6, ClientEq::FilterType::HighShelf, 3000.0f,  0.0f, 0.707f, true, 12);
        // Seven shaped bands, then seven spare handles at unity and
        // switched off. The presets are tuned and must sound exactly as
        // they did before the handles arrived — see EqBandLayout.h.
        for (int i = EqBandLayout::kFirstSpare;
             i < EqBandLayout::kBandCount; ++i) {
            c.eq().setBand(i, EqBandLayout::spare(i - EqBandLayout::kFirstSpare));
        }
        c.eq().setActiveBandCount(EqBandLayout::kBandCount);
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

    if (name == QStringLiteral("DX — punchy")) {
        allOff();
        band(0, ClientEq::FilterType::HighPass, 150.0f, 0.0f, 0.707f, true, 24);
        band(1, ClientEq::FilterType::Peak,  50.0f, -18.0f, 8.0f, false, 12);
        band(2, ClientEq::FilterType::Peak, 100.0f, -12.0f, 8.0f, false, 12);
        band(3, ClientEq::FilterType::Peak, 150.0f,  -9.0f, 8.0f, false, 12);
        band(4, ClientEq::FilterType::LowShelf,   200.0f, -3.0f, 0.707f, true, 12);
        band(5, ClientEq::FilterType::Peak,      2200.0f,  5.0f, 1.2f,   true, 12);
        band(6, ClientEq::FilterType::HighShelf, 3000.0f, -3.0f, 0.707f, true, 12);
        // Seven shaped bands, then seven spare handles at unity and
        // switched off. The presets are tuned and must sound exactly as
        // they did before the handles arrived — see EqBandLayout.h.
        for (int i = EqBandLayout::kFirstSpare;
             i < EqBandLayout::kBandCount; ++i) {
            c.eq().setBand(i, EqBandLayout::spare(i - EqBandLayout::kFirstSpare));
        }
        c.eq().setActiveBandCount(EqBandLayout::kBandCount);
        // Gemessen: bei einem Grundgeräusch von -48 dBFS EFFEKTIV
        // liegen dessen Spitzen um -38, und genau dort stand die
        // Schwelle -- das Gate hat deshalb gar nichts getan.
        //
        //   Schwelle   Pausen gegen roh
        //    -50 dB        0,0
        //    -42 dB        0,0
        //    -38 dB        0,0   <- war eingestellt
        //    -34 dB       -3,0
        //    -28 dB       -7,5
        //
        // Eine Schwelle gehört über die SPITZEN des Raums, nicht
        // über seinen Effektivwert. -34 ist der Anfang der Wirkung
        // und liegt immer noch elf Dezibel unter der Sprache.
        c.gate().setThresholdDb(-34.0f);
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
        // ── Der Clipper fehlte ──────────────────────────────────
        //
        // „DX — punchy" hiess bis zum 2026-10-10 punchy und hatte die
        // einzige Stufe nicht eingeschaltet, die den Scheitelfaktor
        // wirklich senkt. Gemessen traegt der Clipper 6,3 der 8,9 dB
        // Dichte; alles andere zusammen 2,6. Ein Profil fuer DX ohne
        // Clipper ist ein Profil ohne das, wofuer man es waehlt.
        c.setInputAuto(true);
        c.clipper().setDriveDb(12.0f);
        c.clipper().setKnee(0.2f);
        c.clipper().setLevelMatch(false);
        c.comp().setAutoMakeup(true);
        for (auto st : {StripChain::Stage::Gate, StripChain::Stage::Eq,
                        StripChain::Stage::DeEss, StripChain::Stage::Comp,
                        StripChain::Stage::Clip,
                        StripChain::Stage::Limiter}) {
            c.setStageEnabled(st, true);
        }
        c.limiter().setCeilingDb(-1.0f);
        return true;
    }

    if (name == QStringLiteral("Ragchew — easy")) {
        allOff();
        band(0, ClientEq::FilterType::HighPass, 80.0f, 0.0f, 0.707f, true, 12);
        band(1, ClientEq::FilterType::Peak,  50.0f, -18.0f, 8.0f, false, 12);
        band(2, ClientEq::FilterType::Peak, 100.0f, -12.0f, 8.0f, false, 12);
        band(3, ClientEq::FilterType::Peak, 150.0f,  -9.0f, 8.0f, false, 12);
        band(4, ClientEq::FilterType::LowShelf,   200.0f,  2.0f, 0.707f, true, 12);
        band(5, ClientEq::FilterType::Peak,      2000.0f,  1.0f, 0.8f,   true, 12);
        band(6, ClientEq::FilterType::HighShelf, 3000.0f,  1.0f, 0.707f, true, 12);
        // Seven shaped bands, then seven spare handles at unity and
        // switched off. The presets are tuned and must sound exactly as
        // they did before the handles arrived — see EqBandLayout.h.
        for (int i = EqBandLayout::kFirstSpare;
             i < EqBandLayout::kBandCount; ++i) {
            c.eq().setBand(i, EqBandLayout::spare(i - EqBandLayout::kFirstSpare));
        }
        c.eq().setActiveBandCount(EqBandLayout::kBandCount);
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

} // namespace Longpath::StripSettings
