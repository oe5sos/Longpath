// =================================================================
// src/core/strip/AetherPreset.cpp  (Longpath)
//
// no-port-check: Longpath-eigene Zuordnungstafel, kein Port. Die
// AetherSDR-Stellenangaben unten sagen, WO sich die beiden Programme
// im Verhalten unterscheiden -- sie sind Fundstellen fuer einen
// Vergleich, nicht die Quelle dieses Codes.
// =================================================================
//
// Siehe AetherPreset.h. Jede Zuordnung unten wurde gegen BEIDE
// Quellen geprueft -- AetherSDR d58e2b8a und unsere eigene -- und
// die Stellen, an denen sich die zwei unterscheiden, stehen als
// Kommentar dort, wo der Unterschied gemacht wird.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-13 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/AetherPreset.h"

#include "core/strip/EqBandLayout.h"
#include "core/strip/StripChain.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <array>

namespace Longpath {
namespace AetherPreset {
namespace {

using Stage = StripChain::Stage;

float zahl(const QJsonObject& o, const char* schluessel, float vorgabe)
{
    const QJsonValue v = o.value(QLatin1String(schluessel));
    return v.isDouble() ? float(v.toDouble()) : vorgabe;
}

bool flagge(const QJsonObject& o, const char* schluessel, bool vorgabe)
{
    const QJsonValue v = o.value(QLatin1String(schluessel));
    return v.isBool() ? v.toBool() : vorgabe;
}

// Aethers Bandarten stehen als WORT in der Datei, nicht als Zahl.
// Ueber das Wort abzubilden ist das Sichere: beide Seiten haben
// zufaellig dieselbe Nummerierung, aber „zufaellig dieselbe" ist
// keine Zusage, und unsere Aufzaehlung hat hinten zwei Arten dazu
// bekommen (Notch, BandPass), die Aether nicht kennt.
bool bandart(const QString& wort, ClientEq::FilterType& aus)
{
    static const struct { const char* wort; ClientEq::FilterType art; } kTabelle[] = {
        { "Peak",      ClientEq::FilterType::Peak      },
        { "LowShelf",  ClientEq::FilterType::LowShelf  },
        { "HighShelf", ClientEq::FilterType::HighShelf },
        { "LowPass",   ClientEq::FilterType::LowPass   },
        { "HighPass",  ClientEq::FilterType::HighPass  },
        { "Notch",     ClientEq::FilterType::Notch     },
        { "BandPass",  ClientEq::FilterType::BandPass  },
    };
    for (const auto& e : kTabelle) {
        if (wort.compare(QLatin1String(e.wort), Qt::CaseInsensitive) == 0) {
            aus = e.art;
            return true;
        }
    }
    return false;
}

bool filterfamilie(const QString& wort, ClientEq::FilterFamily& aus)
{
    static const struct { const char* wort; ClientEq::FilterFamily fam; } kTabelle[] = {
        { "Butterworth", ClientEq::FilterFamily::Butterworth },
        { "Chebyshev",   ClientEq::FilterFamily::Chebyshev   },
        { "Bessel",      ClientEq::FilterFamily::Bessel      },
        { "Elliptic",    ClientEq::FilterFamily::Elliptic    },
    };
    for (const auto& e : kTabelle) {
        if (wort.compare(QLatin1String(e.wort), Qt::CaseInsensitive) == 0) {
            aus = e.fam;
            return true;
        }
    }
    return false;
}

// ── Die Kette ────────────────────────────────────────────────────────
//
// Bei Aether ist die Liste zugleich Reihenfolge UND Tor: was nicht
// darin steht, laeuft gar nicht, auch wenn sein eigenes „enabled"
// wahr ist (AetherSDR/src/core/AudioEngine.h:482-483). Bei uns ist
// die Reihenfolge eine Permutation ALLER Stufen und das Laufen haengt
// am Stufenschalter -- also: Liste in Reihenfolge, alles Uebrige
// hinten dran und ausgeschaltet.
//
// Drei Stufen setzen wir selbst, weil Aether sie an fester Stelle
// hat und nicht in der Liste fuehrt:
//   Entrauschen ganz vorn  (dort steht bei Aether rn2, vor der Kette)
//   Clipper                (kennt Aether nicht, bleibt aus)
//   Begrenzer ganz hinten  (Aethers finalLimiter laeuft immer, zuletzt)
StripChain::Order reihenfolgeAus(const QJsonArray& liste,
                                 std::array<bool, StripChain::kStageCount>& gelistet,
                                 int& verstanden)
{
    verstanden = 0;
    gelistet.fill(false);

    StripChain::Order folge{};
    int n = 0;
    const auto anhaengen = [&](Stage s) {
        if (n < StripChain::kStageCount) { folge[size_t(n++)] = s; }
    };

    anhaengen(Stage::Denoise);
    gelistet[size_t(Stage::Denoise)] = true;   // Platz, nicht Schalter

    for (const QJsonValue& v : liste) {
        Stage s{};
        if (!StripChain::stageFromToken(v.toString().toLatin1().constData(), s)) {
            continue;   // unbekannter Name: wie bei Aether still uebergehen
        }
        if (s == Stage::Denoise || s == Stage::Clip || s == Stage::Limiter) {
            continue;   // haben ihre feste Stelle, siehe oben
        }
        if (gelistet[size_t(s)]) { continue; }   // zweimal genannt
        gelistet[size_t(s)] = true;
        ++verstanden;
        anhaengen(s);
    }

    // Was in seiner Kette fehlt, bekommt trotzdem einen Platz -- es
    // wird gleich ausgeschaltet, und eine Reihenfolge ohne alle Stufen
    // waere keine Permutation.
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto s = static_cast<Stage>(i);
        if (s == Stage::Clip || s == Stage::Limiter) { continue; }
        if (gelistet[size_t(i)]) { continue; }
        anhaengen(s);
    }
    anhaengen(Stage::Clip);
    anhaengen(Stage::Limiter);
    return folge;
}

} // namespace

Ergebnis apply(const QJsonObject& wurzel, StripChain& c)
{
    Ergebnis e;

    const QString schema = wurzel.value(QStringLiteral("schema")).toString();
    if (!schema.startsWith(QStringLiteral("AetherSDR ChannelStripPreset"))) {
        e.fehler = QStringLiteral(
            "Das ist kein AetherSDR-Kanalzugprofil (Feld „schema\" lautet "
            "„%1\").").arg(schema.isEmpty() ? QStringLiteral("—") : schema);
        return e;
    }
    if (schema != QLatin1String(kSchema)) {
        e.hinweise << QStringLiteral(
            "Die Datei nennt sich „%1\"; gelesen wird sie nach den Regeln "
            "von „%2\". Prüfe die Werte, falls etwas fehlt.")
            .arg(schema, QLatin1String(kSchema));
    }

    // Der Name. „ (imported)" ist Aethers eigene Kollisionsaufloesung
    // beim Einlesen (ChannelStripPresets.cpp:381-388) und gehoert
    // nicht zur Benennung des Betreibers.
    e.name = wurzel.value(QStringLiteral("name")).toString();
    if (e.name.endsWith(QStringLiteral(" (imported)"))) {
        e.name.chop(int(sizeof(" (imported)") - 1));
    }

    // ── Reihenfolge und Mitgliedschaft ──────────────────────────────
    std::array<bool, StripChain::kStageCount> gelistet{};
    const QJsonArray liste = wurzel.value(QStringLiteral("chain")).toArray();
    if (liste.isEmpty()) {
        e.fehler = QStringLiteral("Das Profil trägt keine Kette („chain\").");
        return e;
    }
    int verstanden = 0;
    const StripChain::Order folge = reihenfolgeAus(liste, gelistet, verstanden);
    if (verstanden == 0) {
        // Jeder Name der Liste war unbekannt. Ohne diese Schranke käme
        // eine Kette heraus, in der JEDE Stufe aus ist -- lautlos, ohne
        // Fehler, und niemand wüsste warum.
        QStringList namen;
        for (const QJsonValue& v : liste) { namen << v.toString(); }
        e.fehler = QStringLiteral("Keiner der Stufennamen in „chain\" ist "
                                  "hier bekannt: %1")
                       .arg(namen.join(QStringLiteral(", ")));
        return e;
    }
    if (!c.setStageOrder(folge)) {
        e.fehler = QStringLiteral("Die Kette des Profils ergibt keine "
                                  "gültige Reihenfolge.");
        return e;
    }

    // ── Die Stufen, Werte wie in der Datei ──────────────────────────
    //
    // Eingeschaltet wird IMMER ueber StripChain::setStageEnabled: der
    // Audiofaden liest die Flagge der Kette, nicht die der Stufe. Ein
    // blosses stage().setEnabled(true) laesst die Stufe stumm.

    const QJsonObject gate = wurzel.value(QStringLiteral("gate")).toObject();
    if (!gate.isEmpty()) {
        // setMode ZUERST: es schnappt Verhaeltnis und Boden auf ein
        // Paar (ClientGate.cpp:86-99). Danach gesetzt, wuerde es
        // genau die beiden Werte ueberschreiben, die den Klang machen.
        const QString art = gate.value(QStringLiteral("mode")).toString();
        c.gate().setMode(art.compare(QStringLiteral("Gate"),
                                     Qt::CaseInsensitive) == 0
                             ? ClientGate::Mode::Gate
                             : ClientGate::Mode::Expander);
        c.gate().setThresholdDb(zahl(gate, "thresholdDb", c.gate().thresholdDb()));
        c.gate().setRatio      (zahl(gate, "ratio",       c.gate().ratio()));
        c.gate().setReturnDb   (zahl(gate, "returnDb",    c.gate().returnDb()));
        c.gate().setFloorDb    (zahl(gate, "floorDb",     c.gate().floorDb()));
        c.gate().setAttackMs   (zahl(gate, "attackMs",    c.gate().attackMs()));
        c.gate().setHoldMs     (zahl(gate, "holdMs",      c.gate().holdMs()));
        c.gate().setReleaseMs  (zahl(gate, "releaseMs",   c.gate().releaseMs()));
        c.gate().setLookaheadMs(zahl(gate, "lookaheadMs", c.gate().lookaheadMs()));
        c.setStageEnabled(Stage::Gate,
                          gelistet[size_t(Stage::Gate)]
                              && flagge(gate, "enabled", false));
    }

    const QJsonObject eq = wurzel.value(QStringLiteral("eq")).toObject();
    if (!eq.isEmpty()) {
        ClientEq::FilterFamily fam{};
        const QString famWort = eq.value(QStringLiteral("filterFamily")).toString();
        if (filterfamilie(famWort, fam)) {
            c.eq().setFilterFamily(fam);
        } else if (!famWort.isEmpty()) {
            e.hinweise << QStringLiteral(
                "Filterfamilie „%1\" ist hier unbekannt — es bleibt bei %2.")
                .arg(famWort).arg(int(c.eq().filterFamily()));
        }
        // LINEAR, nicht in dB. Als dB gelesen waere Martins 1,653 ein
        // Faktor 1,21 statt 1,65 -- die ganze Kette 2,7 dB zu leise.
        c.eq().setMasterGain(zahl(eq, "masterGain", c.eq().masterGain()));

        // Erst die Baender, dann die Anzahl -- dieselbe Reihenfolge wie
        // bei Aether (ChannelStripPresets.cpp). Umgekehrt rechnet der
        // Entzerrer einmal mit Baendern, die noch nicht gesetzt sind.
        const QJsonArray baender = eq.value(QStringLiteral("bands")).toArray();
        const int anzahl = eq.value(QStringLiteral("activeBandCount"))
                               .toInt(baender.size());
        int gesetzt = 0;
        for (const QJsonValue& v : baender) {
            if (gesetzt >= ClientEq::kMaxBands) { break; }
            const QJsonObject b = v.toObject();
            ClientEq::BandParams p = c.eq().band(gesetzt);
            p.freqHz        = zahl(b, "freqHz", p.freqHz);
            p.gainDb        = zahl(b, "gainDb", p.gainDb);
            p.q             = zahl(b, "q",      p.q);
            p.enabled       = flagge(b, "enabled", p.enabled);
            p.slopeDbPerOct = b.value(QStringLiteral("slopeDbPerOct"))
                                  .toInt(p.slopeDbPerOct);
            ClientEq::FilterType art{};
            if (bandart(b.value(QStringLiteral("type")).toString(), art)) {
                p.type = art;
            }
            c.eq().setBand(gesetzt, p);
            ++gesetzt;
        }
        // Ohne ein einziges Band bleibt die Anzahl, wie sie war: eine
        // Zahl zu setzen, hinter der keine Werte stehen, erklärt
        // vorhandene Bänder für aktiv, die aus einer ganz anderen
        // Einstellung stammen.
        if (gesetzt > 0) {
            c.eq().setActiveBandCount(qBound(1, anzahl, gesetzt));
            EqBandLayout::ensureSeeded(c.eq());
        } else {
            e.hinweise << QStringLiteral(
                "Der Entzerrer des Profils trägt keine Bänder — die "
                "bestehende Kurve bleibt stehen.");
        }
        // Unsere Oberflaeche arbeitet mit vierzehn Griffen; Aethers
        // Datei traegt zehn aktive. Der Rest wird hier aufgefuellt --
        // mit FLACHEN, ABGESCHALTETEN Baendern (EqBandLayout.h:84-110),
        // also ohne hoerbare Folge. Ohne das sieht die Kurve nach dem
        // Neustart anders aus als jetzt: unser Wiederherstellen fuellt
        // ohnehin auf (StripSettings.cpp:374), und zwei verschiedene
        // Zustaende fuer dieselbe Datei sind genau die Sorte
        // Unterschied, die man spaeter dem Klang zuschreibt.
        c.setStageEnabled(Stage::Eq,
                          gelistet[size_t(Stage::Eq)]
                              && flagge(eq, "enabled", false));
    }

    const QJsonObject deess = wurzel.value(QStringLiteral("deess")).toObject();
    if (!deess.isEmpty()) {
        c.deEss().setFrequencyHz(zahl(deess, "frequencyHz", c.deEss().frequencyHz()));
        c.deEss().setQ          (zahl(deess, "q",           c.deEss().q()));
        c.deEss().setThresholdDb(zahl(deess, "thresholdDb", c.deEss().thresholdDb()));
        c.deEss().setAmountDb   (zahl(deess, "amountDb",    c.deEss().amountDb()));
        c.deEss().setAttackMs   (zahl(deess, "attackMs",    c.deEss().attackMs()));
        c.deEss().setReleaseMs  (zahl(deess, "releaseMs",   c.deEss().releaseMs()));
        // Die Steilheit des Seitenwegs kennt Aether nicht. Ausdruecklich
        // auf die Vorgabe, nicht erben: sonst klingt dieselbe Datei auf
        // zwei Rechnern verschieden.
        c.deEss().setSlopeStages(2);
        c.setStageEnabled(Stage::DeEss,
                          gelistet[size_t(Stage::DeEss)]
                              && flagge(deess, "enabled", false));
    }

    const QJsonObject comp = wurzel.value(QStringLiteral("comp")).toObject();
    if (!comp.isEmpty()) {
        c.comp().setThresholdDb(zahl(comp, "thresholdDb", c.comp().thresholdDb()));
        c.comp().setRatio      (zahl(comp, "ratio",       c.comp().ratio()));
        c.comp().setKneeDb     (zahl(comp, "kneeDb",      c.comp().kneeDb()));
        c.comp().setAttackMs   (zahl(comp, "attackMs",    c.comp().attackMs()));
        c.comp().setReleaseMs  (zahl(comp, "releaseMs",   c.comp().releaseMs()));
        c.comp().setMakeupDb   (zahl(comp, "makeupDb",    c.comp().makeupDb()));
        c.comp().setLimiterEnabled(flagge(comp, "limiterEnabled",
                                          c.comp().limiterEnabled()));
        c.comp().setLimiterCeilingDb(zahl(comp, "limiterCeilingDb",
                                          c.comp().limiterCeilingDb()));
        // Drei Dinge, die es nur bei uns gibt. Alle drei ausdruecklich
        // neutral: der Auto-Ausgleich wuerde Martins 6,66 dB still um
        // 1,38 dB erhoehen, Drive und Phasendreher kaemen aus dem, was
        // vorher in der Kette stand.
        c.comp().setAutoMakeup(false);
        c.comp().setDriveDb(0.0f);
        c.comp().setPhaseRotatorStages(0);
        c.setStageEnabled(Stage::Comp,
                          gelistet[size_t(Stage::Comp)]
                              && flagge(comp, "enabled", false));
    }

    const QJsonObject tube = wurzel.value(QStringLiteral("tube")).toObject();
    if (!tube.isEmpty()) {
        const QString modell = tube.value(QStringLiteral("model")).toString();
        c.tube().setModel(modell == QStringLiteral("C") ? ClientTube::Model::C
                        : modell == QStringLiteral("B") ? ClientTube::Model::B
                                                        : ClientTube::Model::A);
        c.tube().setDriveDb       (zahl(tube, "driveDb",        c.tube().driveDb()));
        c.tube().setTone          (zahl(tube, "tone",           c.tube().tone()));
        c.tube().setDryWet        (zahl(tube, "dryWet",         c.tube().dryWet()));
        c.tube().setBiasAmount    (zahl(tube, "biasAmount",     c.tube().biasAmount()));
        c.tube().setEnvelopeAmount(zahl(tube, "envelopeAmount", c.tube().envelopeAmount()));
        c.tube().setAttackMs      (zahl(tube, "attackMs",       c.tube().attackMs()));
        c.tube().setReleaseMs     (zahl(tube, "releaseMs",      c.tube().releaseMs()));
        c.tube().setOutputGainDb  (zahl(tube, "outputGainDb",   c.tube().outputGainDb()));
        c.setStageEnabled(Stage::Tube,
                          gelistet[size_t(Stage::Tube)]
                              && flagge(tube, "enabled", false));
    }

    const QJsonObject pudu = wurzel.value(QStringLiteral("pudu")).toObject();
    if (!pudu.isEmpty()) {
        const QString art = pudu.value(QStringLiteral("mode")).toString();
        c.pudu().setMode(art.compare(QStringLiteral("Behringer"),
                                     Qt::CaseInsensitive) == 0
                             ? ClientPudu::Mode::Behringer
                             : ClientPudu::Mode::Aphex);
        c.pudu().setPooDriveDb    (zahl(pudu, "pooDriveDb",     c.pudu().pooDriveDb()));
        c.pudu().setPooTuneHz     (zahl(pudu, "pooTuneHz",      c.pudu().pooTuneHz()));
        c.pudu().setPooMix        (zahl(pudu, "pooMix",         c.pudu().pooMix()));
        c.pudu().setDooHarmonicsDb(zahl(pudu, "dooHarmonicsDb", c.pudu().dooHarmonicsDb()));
        c.pudu().setDooTuneHz     (zahl(pudu, "dooTuneHz",      c.pudu().dooTuneHz()));
        c.pudu().setDooMix        (zahl(pudu, "dooMix",         c.pudu().dooMix()));
        // Aether nennt diese Stufe „Enh"; StripChain::stageFromToken
        // kennt beide Namen, die Mitgliedschaft steht also schon fest.
        c.setStageEnabled(Stage::Pudu,
                          gelistet[size_t(Stage::Pudu)]
                              && flagge(pudu, "enabled", false));
    }

    const QJsonObject hall = wurzel.value(QStringLiteral("reverb")).toObject();
    if (!hall.isEmpty()) {
        c.reverb().setSize       (zahl(hall, "size",       c.reverb().size()));
        c.reverb().setDecayS     (zahl(hall, "decayS",     c.reverb().decayS()));
        c.reverb().setDamping    (zahl(hall, "damping",    c.reverb().damping()));
        c.reverb().setPreDelayMs (zahl(hall, "preDelayMs", c.reverb().preDelayMs()));
        c.reverb().setMix        (zahl(hall, "mix",        c.reverb().mix()));
        c.setStageEnabled(Stage::Reverb,
                          gelistet[size_t(Stage::Reverb)]
                              && flagge(hall, "enabled", false));
    }

    const QJsonObject begrenzer =
        wurzel.value(QStringLiteral("finalLimiter")).toObject();
    if (!begrenzer.isEmpty()) {
        c.limiter().setCeilingDb   (zahl(begrenzer, "ceilingDb",
                                         c.limiter().ceilingDb()));
        c.limiter().setOutputTrimDb(zahl(begrenzer, "outputTrimDb",
                                         c.limiter().outputTrimDb()));
        c.limiter().setDcBlockEnabled(flagge(begrenzer, "dcBlock",
                                             c.limiter().dcBlockEnabled()));
        // Aethers Endbegrenzer steht AUSSERHALB der Kettenliste und
        // laeuft immer, zuletzt. Bei uns ist er eine Stufe -- also
        // nach seinem eigenen Schalter ein.
        c.setStageEnabled(Stage::Limiter, flagge(begrenzer, "enabled", true));
        e.hinweise << QStringLiteral(
            "Der Endbegrenzer schneidet bei uns zusätzlich hart auf die "
            "Decke (%1 dBFS); Aether lässt den Folger allein arbeiten. "
            "Hörbar nur auf den letzten Zehntel-Dezibeln.")
            .arg(double(c.limiter().ceilingDb()), 0, 'f', 2);
    }

    // ── Was Aether vor und neben der Kette hat ──────────────────────
    //
    // rn2 ist sein rnnoise im Sendeweg, vor der Kette -- bei uns
    // Stage::Denoise, an derselben Stelle.
    c.setStageEnabled(Stage::Denoise,
                      wurzel.value(QStringLiteral("rn2")).toBool(false));

    // ── Was NUR wir haben ───────────────────────────────────────────
    //
    // Aus, ausdruecklich. Ein importiertes Profil darf nichts
    // mitlaufen lassen, das in Aether nie dabei war -- sonst beurteilt
    // der Betreiber eine Kette, die er so nie eingestellt hat.
    c.setStageEnabled(Stage::Clip, false);
    c.setInputAuto(false);
    c.setInputGainDb(0.0f);

    if (wurzel.contains(QStringLiteral("rx"))) {
        e.hinweise << QStringLiteral(
            "Der rx-Block (Aethers Kette für den EMPFANG) wurde "
            "überlesen — Longpath hat auf der Empfangsseite keinen "
            "Kanalzug.");
    }
    // ── Und was das KOSTET, nicht nur dass es so ist ────────────
    //
    // „mir geht einfach dieser druck in der stimme ab, mehr power"
    // (2026-10-14). Der Satz stand schon hier, aber als technische
    // Mitteilung: drei Stufen sind aus, weil Aether sie nicht kennt.
    // Richtig -- geraten wird nicht. Nur ist eine davon diejenige,
    // die den Druck macht, und das stand nirgends.
    //
    // Gemessen an seiner Stimme durch seinen eigenen Kanal
    // (50-4050 Hz), alles auf -20 LUFS gebracht, also bei GLEICHER
    // Lautheit verglichen: ohne Clipper -3,6 dBFS Spitze, mit 12 dB
    // Anhebung -6,7 dBFS. Das sind 3,1 dB weniger Spitze bei
    // gleichem Gehoer -- auf dem Band also 3 dB mehr Mittelwert bei
    // derselben Spitzenleistung. Der Pruefstand dazu:
    // tests/tst_bandbreiten_genau.cpp, whereThePunchComesFrom.
    e.hinweise << QStringLiteral(
        "Entrauschen, Clipper und die Eingangsautomatik sind aus: die "
        "kennt Aether nicht. Der Clipper ist dabei die Stufe, die die "
        "Dichte macht — an deiner Stimme durch 50–4050 Hz gemessen "
        "bringt er mit 12 dB Anhebung 3,1 dB weniger Spitze bei "
        "gleicher Lautheit, also 3 dB mehr Mittelwert bei derselben "
        "Spitzenleistung. Wenn dir \u201eDruck\u201c fehlt: dort "
        "liegt er.");

    e.ok = true;
    return e;
}

Ergebnis applyFile(const QString& pfad, StripChain& chain)
{
    Ergebnis e;

    QFile f(pfad);
    if (!f.open(QIODevice::ReadOnly)) {
        e.fehler = QStringLiteral("Datei nicht lesbar: %1").arg(f.errorString());
        return e;
    }
    QJsonParseError fehler{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &fehler);
    if (fehler.error != QJsonParseError::NoError || !doc.isObject()) {
        e.fehler = QStringLiteral("Die Datei ist kein lesbares JSON: %1")
                       .arg(fehler.errorString());
        return e;
    }
    e = apply(doc.object(), chain);
    if (e.ok && e.name.isEmpty()) {
        // Aether nimmt in diesem Fall den Dateinamen ohne Endung
        // (ChannelStripPresets.cpp:377-379) -- wir auch.
        e.name = QFileInfo(f).completeBaseName();
    }
    return e;
}

} // namespace AetherPreset
} // namespace Longpath
