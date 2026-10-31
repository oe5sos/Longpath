// =================================================================
// src/core/strip/ZeusKanalzug.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigene Datei. Keine Zeile Zeus-Code; die
// Bedeutung seiner Schluessel steht im Kopf von ZeusKanalzug.h, an
// einer laufenden Maschine am 2026-10-31 abgelesen.
//
// See ZeusKanalzug.h for the full header.
// =================================================================

#include "core/strip/ZeusKanalzug.h"

#include "core/strip/StripChain.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <cmath>

namespace Longpath {
namespace ZeusKanalzug {

namespace {

/// Zeus speichert JEDEN Wert als Zeichenkette ("‑45", "true"), auch
/// Zahlen. Das ist kein Zufall, sondern die Form des Beutels
/// (`Dictionary<string, Dictionary<string, string>>`), und wer hier
/// `toDouble()` auf dem JSON-Wert ruft, bekommt ueberall null.
bool hat(const QJsonObject& o, const char* schluessel)
{
    return o.contains(QLatin1String(schluessel));
}

double zahl(const QJsonObject& o, const char* schluessel, double vorgabe)
{
    const QJsonValue v = o.value(QLatin1String(schluessel));
    if (v.isUndefined() || v.isNull()) { return vorgabe; }
    if (v.isDouble()) { return v.toDouble(); }
    bool ok = false;
    const double d = v.toString().toDouble(&ok);
    return ok ? d : vorgabe;
}

bool flagge(const QJsonObject& o, const char* schluessel, bool vorgabe)
{
    const QJsonValue v = o.value(QLatin1String(schluessel));
    if (v.isUndefined() || v.isNull()) { return vorgabe; }
    if (v.isBool()) { return v.toBool(); }
    const QString s = v.toString().trimmed().toLower();
    if (s == QLatin1String("true")  || s == QLatin1String("1")) { return true; }
    if (s == QLatin1String("false") || s == QLatin1String("0")) { return false; }
    return vorgabe;
}

/// Ob eine Stufe laufen soll. Zeus fuehrt das als
/// `requestedEnabled` — „angefordert", weil eine Stufe auch aus
/// anderen Gruenden nicht laufen kann. Fuer uns ist es der Schalter.
bool laeuft(const QJsonObject& stufe)
{
    return flagge(stufe, "requestedEnabled", false);
}

QJsonObject stufeVon(const QJsonObject& suite, const char* id)
{
    return suite.value(QLatin1String(id)).toObject();
}

// ── Prozent und Anteile ──────────────────────────────────────────────
//
// Zeus rechnet in Prozent (`param:mix-percent 100`), unsere Stufen in
// Anteilen 0..1 (`setMix`, `setDryWet`). Einmal hier, nicht an zehn
// Stellen.
float anteil(const QJsonObject& o, const char* schluessel, double vorgabe)
{
    return float(std::clamp(zahl(o, schluessel, vorgabe) / 100.0, 0.0, 1.0));
}

// ── Der Entzerrer ────────────────────────────────────────────────────
//
// Zehn Baender, bei Zeus ab EINS gezaehlt, bei uns ab null. Die Arten
// zaehlen gleich (0 Glocke, 1 tiefe Flanke, 2 hohe Flanke) — beide
// stammen von AetherSDR.
//
// Zeus kennt daneben einen Kurvenmodus (`eq:mode` = "curve" mit
// `eq:curve`). Den haben wir nicht; er wird gemeldet, nicht
// nachgerechnet. Eine aus Stuetzstellen zurueckgerechnete Bandkurve
// waere eine Behauptung ueber einen Klang, den niemand geprueft hat.
int entzerrer(const QJsonObject& eq, StripChain& k, QStringList& hinweise)
{
    if (eq.isEmpty()) { return 0; }

    const QString modus = eq.value(QStringLiteral("eq:mode")).toString();
    if (!modus.isEmpty() && modus != QLatin1String("bands")) {
        hinweise << QStringLiteral(
            "Zeus' Entzerrer stand auf „%1\" statt auf Bändern. Die "
            "Bandwerte wurden trotzdem übernommen; seine freie Kurve "
            "hat in Longpath kein Gegenstück.").arg(modus);
    }

    int gesetzt = 0;
    for (int i = 1; i <= 10; ++i) {
        const QByteArray f = "param:band-" + QByteArray::number(i) + "-freq-hz";
        const QByteArray g = "param:band-" + QByteArray::number(i) + "-gain-db";
        const QByteArray q = "param:band-" + QByteArray::number(i) + "-q";
        const QByteArray t = "param:band-" + QByteArray::number(i) + "-type";
        if (!hat(eq, f.constData())) { continue; }

        ClientEq::BandParams b;
        b.freqHz = float(zahl(eq, f.constData(), 1000.0));
        b.gainDb = float(zahl(eq, g.constData(), 0.0));
        b.q      = float(zahl(eq, q.constData(), 0.707));
        const int art = int(zahl(eq, t.constData(), 0.0));
        // Nur die drei Arten, die auf beiden Seiten dieselbe Zahl
        // tragen. Eine vierte Zahl aus einer kuenftigen Zeus-Fassung
        // wird zur Glocke und gemeldet — nicht stillschweigend auf
        // unsere Nummer 3 (Tiefpass) gelegt.
        if (art >= 0 && art <= 2) {
            b.type = static_cast<ClientEq::FilterType>(art);
        } else {
            b.type = ClientEq::FilterType::Peak;
            hinweise << QStringLiteral(
                "Band %1 hatte die Art %2, die Longpath nicht kennt — "
                "als Glocke übernommen.").arg(i).arg(art);
        }
        b.enabled = true;
        k.eq().setBand(i - 1, b);
        ++gesetzt;
    }
    if (gesetzt > 0) {
        k.eq().setActiveBandCount(gesetzt);
        // Zeus' Eingangs- und Ausgangsverstaerkung des Entzerrers:
        // unsere Stufe hat EINE (`setMasterGain`, linear). Beide
        // zusammengezaehlt — das ist dieselbe Zahl, solange nichts
        // dazwischen begrenzt, und dazwischen liegt nichts.
        const double dB = zahl(eq, "param:input-gain-db", 0.0)
                        + zahl(eq, "param:output-gain-db", 0.0);
        k.eq().setMasterGain(float(std::pow(10.0, dB / 20.0)));
        k.setStageEnabled(StripChain::Stage::Eq, laeuft(eq));
        k.eq().setEnabled(laeuft(eq));
    }
    return gesetzt;
}

} // namespace

bool traegtKanalzug(const QJsonObject& profil)
{
    const QJsonObject suite =
        profil.value(QStringLiteral("nativePluginStates")).toObject();
    if (suite.isEmpty()) { return false; }
    // Mindestens eine Stufe, die wir wirklich setzen koennen. Ein
    // Beutel mit nur `native-chain` darin ist keine Kette.
    static constexpr const char* kUnsere[] = {
        "input-strip", "noise-gate", "de-esser", "eq",
        "compressor", "saturation", "exciter", "reverb",
        "limiter", "denoise",
    };
    for (const char* s : kUnsere) {
        if (suite.contains(QLatin1String(s))) { return true; }
    }
    return false;
}

Ergebnis anwenden(const QJsonObject& profil, StripChain& k)
{
    Ergebnis e;

    const QJsonObject suite =
        profil.value(QStringLiteral("nativePluginStates")).toObject();
    if (suite.isEmpty()) {
        e.fehler = QStringLiteral(
            "Dieses Zeus-Profil trägt keine Audio-Suite "
            "(nativePluginStates ist leer).");
        return e;
    }

    e.name = profil.value(QStringLiteral("name")).toString();
    if (e.name.isEmpty()) {
        e.name = profil.value(QStringLiteral("id")).toString();
    }

    // ── Eingangsstufe: Hoch- und Tiefpass, Phasendreher ──────────────
    //
    // Zeus hat dafuer eine eigene Stufe; bei uns sind es zwei
    // Entzerrerbaender und der Phasendreher IM Kompressor (siehe
    // StripChain.h: „The phase rotator lives inside the compressor
    // upstream and is not a separate link here").
    const QJsonObject ein = stufeVon(suite, "input-strip");
    if (!ein.isEmpty()) {
        const int stufen = int(zahl(ein, "param:rotator-stages", 0.0));
        if (stufen > 0 && laeuft(ein)) {
            k.comp().setPhaseRotatorStages(stufen);
            e.hinweise << QStringLiteral(
                "Zeus' Phasendreher (%1 Stufen bei %2 Hz) sitzt in "
                "Longpath im Kompressor; die Eckfrequenz ist dort "
                "fest.").arg(stufen)
                .arg(qint64(zahl(ein, "param:rotator-hz", 0.0)));
        }
        if (flagge(ein, "param:invert", false)) {
            e.hinweise << QStringLiteral(
                "Zeus drehte die Phase des Mikrofons um. Longpath hat "
                "dafür im Kanalzug keinen Schalter.");
        }
        const double hp = zahl(ein, "param:hpf-hz", 0.0);
        const double tp = zahl(ein, "param:lpf-hz", 0.0);
        if (hp > 0.0 || tp > 0.0) {
            e.hinweise << QStringLiteral(
                "Zeus' Sendefilter (%1–%2 Hz) steht in Longpath nicht "
                "im Kanalzug, sondern am Sendekanal — dort nachsehen.")
                .arg(qint64(hp)).arg(qint64(tp));
        }
        const double trim = zahl(ein, "param:trim-db", 0.0);
        if (std::abs(trim) > 0.01) {
            e.hinweise << QStringLiteral(
                "Zeus' Eingangstrimm (%1 dB) wurde auf die "
                "Entzerrer-Verstärkung gelegt.").arg(trim);
            k.eq().setMasterGain(float(std::pow(10.0, trim / 20.0)));
        }
        ++e.stufen;
    }

    // ── Entrauschen ──────────────────────────────────────────────────
    //
    // Zeus hat sieben Regler, wir einen (`setAmount`). Genommen wird
    // die Daempfung des Raumgeraeusches, auf 0..1 gebracht: −18 dB ist
    // bei Zeus der uebliche Stand, −30 dB das Ende der Fahnenstange.
    const QJsonObject rausch = stufeVon(suite, "denoise");
    if (!rausch.isEmpty()) {
        const double noiseDb = zahl(rausch, "param:noise-db", 0.0);
        const float a = float(std::clamp(-noiseDb / 30.0, 0.0, 1.0));
        k.nr().setAmount(a);
        k.setStageEnabled(StripChain::Stage::Denoise, laeuft(rausch));
        if (laeuft(rausch)) {
            e.hinweise << QStringLiteral(
                "Zeus' Entrauschen hat sieben Regler, Longpath einen. "
                "Übernommen wurde die Dämpfung (%1 dB) als Stärke %2.")
                .arg(noiseDb).arg(double(a), 0, 'f', 2);
        }
        ++e.stufen;
    }

    // ── Das Tor ──────────────────────────────────────────────────────
    const QJsonObject tor = stufeVon(suite, "noise-gate");
    if (!tor.isEmpty()) {
        auto& g = k.gate();
        // `param:mode`: 0 Expander, 1 Tor. Unsere Aufzaehlung zaehlt
        // genauso (ClientGate.h:51-54).
        const int m = int(zahl(tor, "param:mode", 0.0));
        g.setMode(m == 1 ? ClientGate::Mode::Gate
                         : ClientGate::Mode::Expander);
        g.setThresholdDb(float(zahl(tor, "param:threshold-db", -45.0)));
        g.setAttackMs(float(zahl(tor, "param:attack-ms", 2.0)));
        g.setHoldMs(float(zahl(tor, "param:hold-ms", 100.0)));
        g.setReleaseMs(float(zahl(tor, "param:release-ms", 250.0)));
        g.setRatio(float(zahl(tor, "param:ratio", 2.0)));
        // `range-db` ist bei Zeus negativ (−15 = fünfzehn dB hinunter),
        // unser `setFloorDb` erwartet dasselbe Vorzeichen.
        g.setFloorDb(float(zahl(tor, "param:range-db", -15.0)));
        g.setLookaheadMs(float(zahl(tor, "param:lookahead-ms", 0.0)));
        k.setStageEnabled(StripChain::Stage::Gate, laeuft(tor));
        if (hat(tor, "param:sc-hpf-hz")) {
            e.hinweise << QStringLiteral(
                "Das Seitenkettenfilter des Tors (%1 Hz) hat in "
                "Longpath kein Gegenstück.")
                .arg(qint64(zahl(tor, "param:sc-hpf-hz", 0.0)));
        }
        ++e.stufen;
    }

    // ── De-Esser ─────────────────────────────────────────────────────
    const QJsonObject ess = stufeVon(suite, "de-esser");
    if (!ess.isEmpty()) {
        auto& d = k.deEss();
        d.setFrequencyHz(float(zahl(ess, "param:frequency-hz", 4000.0)));
        d.setThresholdDb(float(zahl(ess, "param:threshold-db", -30.0)));
        d.setAttackMs(float(zahl(ess, "param:attack-ms", 1.0)));
        d.setReleaseMs(float(zahl(ess, "param:release-ms", 80.0)));
        // `range-db` positiv bei Zeus (10 = bis zu zehn dB weg),
        // `setAmountDb` bei uns negativ.
        d.setAmountDb(float(-std::abs(zahl(ess, "param:range-db", 10.0))));
        // Breit oder schmal: unser Gegenstueck ist die Guete.
        d.setQ(flagge(ess, "param:wide", false) ? 2.0f : 5.0f);
        k.setStageEnabled(StripChain::Stage::DeEss, laeuft(ess));
        if (hat(ess, "param:ratio")) {
            e.hinweise << QStringLiteral(
                "Zeus' De-Esser hat ein Verhältnis (%1); Longpath "
                "regelt über die Stärke in dB.")
                .arg(zahl(ess, "param:ratio", 0.0));
        }
        ++e.stufen;
    }

    // ── Entzerrer ────────────────────────────────────────────────────
    if (const int n = entzerrer(stufeVon(suite, "eq"), k, e.hinweise); n > 0) {
        ++e.stufen;
    }

    // ── Kompressor ───────────────────────────────────────────────────
    const QJsonObject komp = stufeVon(suite, "compressor");
    if (!komp.isEmpty()) {
        auto& c = k.comp();
        c.setThresholdDb(float(zahl(komp, "param:threshold-db", -18.0)));
        c.setRatio(float(zahl(komp, "param:ratio", 3.0)));
        c.setAttackMs(float(zahl(komp, "param:attack-ms", 5.0)));
        c.setReleaseMs(float(zahl(komp, "param:release-ms", 100.0)));
        c.setKneeDb(float(zahl(komp, "param:knee-db", 6.0)));
        c.setMakeupDb(float(zahl(komp, "param:makeup-db", 0.0)));
        c.setAutoMakeup(flagge(komp, "param:auto-makeup", false));
        k.setStageEnabled(StripChain::Stage::Comp, laeuft(komp));
        const double mix = zahl(komp, "param:mix-percent", 100.0);
        if (mix < 99.9) {
            e.hinweise << QStringLiteral(
                "Zeus mischte den Kompressor zu %1 %% bei (Parallel"
                "kompression). Longpaths Kompressor läuft voll.")
                .arg(mix);
        }
        ++e.stufen;
    }

    // ── Saettigung -> unsere Roehre ──────────────────────────────────
    const QJsonObject satt = stufeVon(suite, "saturation");
    if (!satt.isEmpty()) {
        auto& t = k.tube();
        t.setDriveDb(float(zahl(satt, "param:drive-db", 6.0)));
        t.setBiasAmount(anteil(satt, "param:bias-percent", 15.0));
        t.setOutputGainDb(float(zahl(satt, "param:output-db", 0.0)));
        t.setDryWet(anteil(satt, "param:mix-percent", 100.0));
        // `param:tone-hz` ist bei Zeus eine Frequenz, unser `setTone`
        // ein Anteil 0..1 ueber die Hoehen. 9000 Hz ist Zeus' Vorgabe
        // und steht fuer „offen"; darunter wird es dunkler.
        const double ton = zahl(satt, "param:tone-hz", 9000.0);
        t.setTone(float(std::clamp(ton / 9000.0, 0.0, 1.0)));
        k.setStageEnabled(StripChain::Stage::Tube, laeuft(satt));
        if (hat(satt, "param:oversample")) {
            e.hinweise << QStringLiteral(
                "Zeus rechnet die Sättigung %1-fach überabgetastet; "
                "Longpaths Röhre hat das nicht einstellbar.")
                .arg(qint64(zahl(satt, "param:oversample", 1.0)));
        }
        ++e.stufen;
    }

    // ── Anreicherer -> PUDU ──────────────────────────────────────────
    const QJsonObject anr = stufeVon(suite, "exciter");
    if (!anr.isEmpty()) {
        auto& p = k.pudu();
        p.setPooTuneHz(float(zahl(anr, "param:frequency-hz", 5000.0)));
        p.setPooDriveDb(float(zahl(anr, "param:drive-db", 6.0)));
        p.setPooMix(anteil(anr, "param:mix-percent", 20.0));
        k.setStageEnabled(StripChain::Stage::Pudu, laeuft(anr));
        ++e.stufen;
    }

    // ── Hall ─────────────────────────────────────────────────────────
    const QJsonObject hall = stufeVon(suite, "reverb");
    if (!hall.isEmpty()) {
        auto& r = k.reverb();
        r.setSize(anteil(hall, "param:size-percent", 50.0));
        r.setMix(anteil(hall, "param:mix-percent", 0.0));
        if (hat(hall, "param:decay-percent")) {
            // Zeus gibt den Nachhall in Prozent, wir in Sekunden.
            // Drei Sekunden sind bei uns das Ende der Fahnenstange.
            r.setDecayS(float(anteil(hall, "param:decay-percent", 40.0) * 3.0f));
        }
        if (hat(hall, "param:predelay-ms")) {
            r.setPreDelayMs(float(zahl(hall, "param:predelay-ms", 0.0)));
        }
        if (hat(hall, "param:damping-percent")) {
            r.setDamping(anteil(hall, "param:damping-percent", 50.0));
        }
        k.setStageEnabled(StripChain::Stage::Reverb, laeuft(hall));
        ++e.stufen;
    }

    // ── Begrenzer ────────────────────────────────────────────────────
    const QJsonObject lim = stufeVon(suite, "limiter");
    if (!lim.isEmpty()) {
        auto& l = k.limiter();
        l.setCeilingDb(float(zahl(lim, "param:ceiling-db", -1.0)));
        l.setOutputTrimDb(float(zahl(lim, "param:input-db", 0.0)));
        k.setStageEnabled(StripChain::Stage::Limiter, laeuft(lim));
        if (flagge(lim, "param:true-peak", false)) {
            e.hinweise << QStringLiteral(
                "Zeus' Begrenzer misst die wahre Spitze zwischen den "
                "Abtastwerten; Longpaths misst die Abtastwerte.");
        }
        ++e.stufen;
    }

    // ── Was Zeus hat und wir nicht ───────────────────────────────────
    //
    // Mit Werten, nicht als Sammelsatz: wer den Mehrband-Kompressor
    // von Hand nachstellen will, braucht die Zahlen, und wer es nicht
    // will, soll wenigstens wissen, was fehlt.
    const QJsonObject mb = stufeVon(suite, "mbcomp");
    if (!mb.isEmpty() && laeuft(mb)) {
        e.hinweise << QStringLiteral(
            "Zeus' Mehrband-Kompressor lief (Trennfrequenzen %1 / %2 / "
            "%3 Hz, Kopplung %4 %%, Antrieb %5 dB). Longpath hat keinen "
            "— ein Vierband-Kompressor ist kein Einband-Kompressor mit "
            "anderen Zahlen, also wurde nichts davon umgelegt.")
            .arg(qint64(zahl(mb, "param:crossover-1-hz", 0.0)))
            .arg(qint64(zahl(mb, "param:crossover-2-hz", 0.0)))
            .arg(qint64(zahl(mb, "param:crossover-3-hz", 0.0)))
            .arg(qint64(zahl(mb, "param:coupling-percent", 0.0)))
            .arg(zahl(mb, "param:drive-db", 0.0));
    }
    const QJsonObject punch = stufeVon(suite, "punch");
    if (!punch.isEmpty() && laeuft(punch)) {
        e.hinweise << QStringLiteral(
            "Zeus' Anschlag lief (Einsatz %1 dB, Halten %2 dB). "
            "Longpath hat die Stufe nicht.")
            .arg(zahl(punch, "param:attack-db", 0.0))
            .arg(zahl(punch, "param:sustain-db", 0.0));
    }
    const QJsonObject agc = stufeVon(suite, "broadcast-agc");
    if (!agc.isEmpty() && laeuft(agc)) {
        e.hinweise << QStringLiteral(
            "Zeus' Sendeautomatik lief (Ziel %1 dB, höchstens %2 dB "
            "hinauf und %3 dB hinunter). Longpath hat sie nicht; am "
            "nächsten kommt der Kompressor.")
            .arg(zahl(agc, "param:target-db", 0.0))
            .arg(zahl(agc, "param:max-gain-db", 0.0))
            .arg(zahl(agc, "param:max-cut-db", 0.0));
    }
    const QJsonObject bass = stufeVon(suite, "bass");
    if (!bass.isEmpty() && laeuft(bass)) {
        e.hinweise << QStringLiteral(
            "Zeus' Bassanhebung lief (%1 Hz, %2 %%). Longpath hat sie "
            "nicht — am nächsten kommt ein tiefes Entzerrerband.")
            .arg(qint64(zahl(bass, "param:frequency-hz", 0.0)))
            .arg(zahl(bass, "param:amount-percent", 0.0));
    }
    const QJsonObject vst =
        profil.value(QStringLiteral("vstPluginStates")).toObject();
    if (!vst.isEmpty()) {
        e.hinweise << QStringLiteral(
            "%1 VST-Einschub/-Einschübe blieben liegen: ihre Zustände "
            "sind base64 und gehören fremden Plugins.").arg(vst.size());
    }

    // ── Was Zeus NICHT hat ───────────────────────────────────────────
    //
    // Dieselbe Regel wie bei AetherPreset: ein eingelesenes Profil
    // darf nichts mitlaufen lassen, das der Betreiber in der Vorlage
    // nie gehoert hat.
    k.setStageEnabled(StripChain::Stage::Clip, false);

    // Die Reihenfolge. Zeus traegt sie als Liste in `native-chain`;
    // unsere Kette kann sie seit dem 2026-10-13 stellen.
    const QJsonObject nc = stufeVon(suite, "native-chain");
    const QString folge = nc.value(QStringLiteral("order")).toString();
    if (!folge.isEmpty()) {
        e.hinweise << QStringLiteral(
            "Zeus' Kettenreihenfolge war: %1. Longpaths Reihenfolge "
            "bleibt, wie sie eingestellt ist — die Stufen stehen nicht "
            "eins zu eins gegenüber.").arg(folge);
    }

    if (e.stufen == 0) {
        e.fehler = QStringLiteral(
            "In diesem Profil stand keine Stufe, die Longpath kennt.");
        return e;
    }
    e.ok = true;
    return e;
}

} // namespace ZeusKanalzug
} // namespace Longpath
