// =================================================================
// src/core/ZeusProfilImport.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigene Datei. Keine Zeile Zeus-Code — nur
// die Bedeutung seiner Felder, Zeile fuer Zeile belegt in den
// Zitaten unten und im Kopf von ZeusProfilImport.h.
//
// See ZeusProfilImport.h for the full header.
// =================================================================

#include "core/ZeusProfilImport.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <array>

namespace Longpath {

namespace {

// Wahrheitswerte schreibt Longpaths Einstellungsdatei als „True"/
// „False"; Zeus liefert echtes JSON-bool.
QString wahr(bool b)
{
    return b ? QStringLiteral("True") : QStringLiteral("False");
}

QString zahl(double d)
{
    // Ganze Zahlen ohne Nachkomma, damit ein Profil aus Zeus genauso
    // in der Einstellungsdatei steht wie eines aus Thetis — sonst
    // unterscheiden sich zwei gleiche Profile nur in der Schreibweise.
    if (d == double(qint64(d))) { return QString::number(qint64(d)); }
    return QString::number(d, 'g', 10);
}

/// Ein Feld setzen, aber nur, wenn Zeus es wirklich mitgeschickt hat.
/// Ein fehlendes Feld ist bei Zeus eine Aussage („leave the operator's
/// current … untouched", Dtos.cs:3869) und darf nicht als Null
/// durchgehen.
void setzeWennDa(ThetisProfil& p, const QJsonObject& o, const char* zeusFeld,
                 const char* lpFeld)
{
    const QJsonValue v = o.value(QLatin1String(zeusFeld));
    if (v.isUndefined() || v.isNull()) { return; }
    if (v.isBool()) {
        p.werte.insert(QString::fromLatin1(lpFeld), wahr(v.toBool()));
    } else if (v.isDouble()) {
        p.werte.insert(QString::fromLatin1(lpFeld), zahl(v.toDouble()));
    } else if (v.isString()) {
        p.werte.insert(QString::fromLatin1(lpFeld), v.toString());
    }
}

// ── Die zehn CFC-Baender ─────────────────────────────────────────────
//
// Zeus traegt sie als Liste von Objekten (`CfcBand(FreqHz,
// CompLevelDb, PostGainDb)`, Dtos.cs:3610); Longpath fuehrt dreimal
// zehn EINZELNE Felder (CFCEqFreq0…9, CFCPreComp0…9, CFCPostEqGain0…9).
// Dass die Zahl zehn auf beiden Seiten dieselbe ist, ist kein Zufall:
// beide zitieren dieselbe pihpsdr-Grundlage (Dtos.cs:3663 nennt
// `vfo.c:284-314`).
void cfcBaender(ThetisProfil& p, const QJsonArray& baender)
{
    const int n = std::min(int(baender.size()), 10);
    if (baender.size() != 10) {
        p.hinweise << QStringLiteral(
            "Zeus schickte %1 CFC-Bänder statt zehn; übernommen wurden "
            "%2. Longpath führt fest zehn.")
            .arg(baender.size()).arg(n);
    }
    for (int i = 0; i < n; ++i) {
        const QJsonObject b = baender.at(i).toObject();
        p.werte.insert(QStringLiteral("CFCEqFreq%1").arg(i),
                       zahl(b.value(QStringLiteral("freqHz")).toDouble()));
        p.werte.insert(QStringLiteral("CFCPreComp%1").arg(i),
                       zahl(b.value(QStringLiteral("compLevelDb")).toDouble()));
        p.werte.insert(QStringLiteral("CFCPostEqGain%1").arg(i),
                       zahl(b.value(QStringLiteral("postGainDb")).toDouble()));
    }
}

// ── Der alte Sende-Entzerrer ─────────────────────────────────────────
//
// `LegacyTxEqConfig(Enabled, PreampDb, FrequenciesHz[10], GainsDb[10])`
// (Dtos.cs:3614). Longpath zaehlt seine zehn ab EINS (TXEQ1…TXEQ10,
// TxEqFreq1…10) — Thetis tut das auch, und unsere Felder kommen von
// dort.
void alterEntzerrer(ThetisProfil& p, const QJsonObject& eq)
{
    setzeWennDa(p, eq, "enabled",   "TXEQEnabled");
    setzeWennDa(p, eq, "preampDb",  "TXEQPreamp");

    const QJsonArray f = eq.value(QStringLiteral("frequenciesHz")).toArray();
    const QJsonArray g = eq.value(QStringLiteral("gainsDb")).toArray();
    const int n = std::min({int(f.size()), int(g.size()), 10});
    for (int i = 0; i < n; ++i) {
        p.werte.insert(QStringLiteral("TxEqFreq%1").arg(i + 1),
                       zahl(f.at(i).toDouble()));
        p.werte.insert(QStringLiteral("TXEQ%1").arg(i + 1),
                       zahl(g.at(i).toDouble()));
    }
}

void expander(ThetisProfil& p, const QJsonObject& d)
{
    // DexpConfig, Dtos.cs:3698-3711. Die Namen auf unserer Seite sind
    // die, auf die ThetisProfilImport seine „Dexp_*"-Spalten schon
    // abbildet — eine Abbildung, zwei Herkuenfte.
    setzeWennDa(p, d, "enabled",        "DEXP_Enabled");
    setzeWennDa(p, d, "thresholdDbv",   "Dexp_Threshold");
    setzeWennDa(p, d, "attackMs",       "DEXP_AttackTimeMs");
    setzeWennDa(p, d, "releaseMs",      "DEXP_ReleaseTimeMs");
    setzeWennDa(p, d, "expansionDb",    "DEXP_ExpansionRatioDb");
    setzeWennDa(p, d, "hysteresisDb",   "DEXP_HysteresisRatioDb");
    setzeWennDa(p, d, "detectorTauMs",  "DEXP_DetectorTauMs");
    setzeWennDa(p, d, "sideChannelFilterEnabled",
                "DEXP_SideChannelFilterEnabled");
    setzeWennDa(p, d, "sideChannelLowHz",  "DEXP_LowCutHz");
    setzeWennDa(p, d, "sideChannelHighHz", "DEXP_HighCutHz");
    setzeWennDa(p, d, "lookAheadEnabled",  "DEXP_LookAheadEnabled");
    setzeWennDa(p, d, "lookAheadMs",       "DEXP_LookAheadMs");

    // Zeus fuehrt die HALTEZEIT (HoldMs, Dtos.cs:3702); Longpath und
    // Thetis nicht. Nicht verschweigen: ein Tor ohne Haltezeit
    // schliesst hoerbar frueher.
    if (d.contains(QStringLiteral("holdMs"))) {
        p.hinweise << QStringLiteral(
            "Zeus' Haltezeit des Tors (%1 ms) hat in Longpath kein "
            "Gegenstück und bleibt liegen.")
            .arg(d.value(QStringLiteral("holdMs")).toDouble());
    }
}

/// Ein einzelnes Profil. Gibt leer zurueck, wenn es keines ist.
ThetisProfil einProfil(const QJsonObject& o, bool* ok)
{
    ThetisProfil p;
    *ok = false;

    p.name = o.value(QStringLiteral("name")).toString();
    if (p.name.isEmpty()) {
        p.name = o.value(QStringLiteral("id")).toString();
    }
    if (p.name.isEmpty()) { return p; }

    // ── Mikrofon und Pegelregelung ───────────────────────────────────
    setzeWennDa(p, o, "micGainDb",        "MicGain");
    setzeWennDa(p, o, "levelerMaxGainDb", "Lev_MaxGain");

    const QJsonObject lev = o.value(QStringLiteral("txLeveling")).toObject();
    if (!lev.isEmpty()) {
        setzeWennDa(p, lev, "levelerEnabled",   "Lev_On");
        setzeWennDa(p, lev, "levelerDecayMs",   "Lev_Decay");
        setzeWennDa(p, lev, "alcMaxGainDb",     "ALC_MaximumGain");
        setzeWennDa(p, lev, "alcDecayMs",       "ALC_Decay");
        setzeWennDa(p, lev, "compressorGainDb", "CompanderLevel");
        setzeWennDa(p, lev, "cessbEnabled",     "CESSB_On");

        // Der Kompressor-SCHALTER liegt bei uns ausserhalb des Profils
        // (hardware/<mac>/tx/cpdr/on) — dieselbe Lage wie bei Thetis,
        // und ThetisProfilImport::nichtUebernommen() sagt dort
        // dasselbe. Der Pegel kommt mit.
        if (lev.contains(QStringLiteral("compressorEnabled"))) {
            p.hinweise << QStringLiteral(
                "Zeus' Kompressor-Schalter steht in Longpath außerhalb "
                "des Profils; nur sein Pegel wurde übernommen.");
        }
        // CESSB-Bandbreite: Zeus kann 3 oder 4 kHz (Dtos.cs:965-967),
        // Longpath hat Thetis' feste Fassung.
        const int cb = lev.value(QStringLiteral("cessbBandwidthHz")).toInt();
        if (cb > 0 && cb != 3000) {
            p.hinweise << QStringLiteral(
                "Zeus' CESSB läuft auf %1 Hz; Longpath hat Thetis' feste "
                "3-kHz-Fassung.").arg(cb);
        }
    }

    // ── CFC und Phasendreher ─────────────────────────────────────────
    const QJsonObject cfc = o.value(QStringLiteral("cfcConfig")).toObject();
    if (!cfc.isEmpty()) {
        setzeWennDa(p, cfc, "enabled",       "CFCEnabled");
        setzeWennDa(p, cfc, "postEqEnabled", "CFCPostEqEnabled");
        setzeWennDa(p, cfc, "preCompDb",     "CFCPreComp");
        setzeWennDa(p, cfc, "prePeqDb",      "CFCPostEqGain");
        cfcBaender(p, cfc.value(QStringLiteral("bands")).toArray());

        // Die FFT-Groesse ist bei Zeus waehlbar (2048…16384,
        // Dtos.cs:3654); in Longpath liegt sie fest, der Setzer fehlt.
        const int fft = cfc.value(QStringLiteral("fftSize")).toInt();
        if (fft > 0 && fft != 16384) {
            p.hinweise << QStringLiteral(
                "Zeus rechnet die CFC über %1 Punkte; Longpath liegt "
                "fest auf 16384.").arg(fft);
        }
    }

    const QJsonObject rot = o.value(QStringLiteral("txPhaseRotator")).toObject();
    if (!rot.isEmpty()) {
        setzeWennDa(p, rot, "enabled",  "CFCPhaseRotatorEnabled");
        setzeWennDa(p, rot, "cornerHz", "CFCPhaseRotatorFreq");
        setzeWennDa(p, rot, "stages",   "CFCPhaseRotatorStages");
        setzeWennDa(p, rot, "reverse",  "CFCPhaseReverseEnabled");
    }

    // ── Das Sendefilter ──────────────────────────────────────────────
    //
    // ACHTUNG, Vorzeichen. Zeus traegt „operator-typed positive
    // magnitudes; server re-signs per mode-family" (Dtos.cs:3857).
    // Longpath fuehrt FilterLow/FilterHigh so, wie Thetis sie fuehrt —
    // bei unterem Seitenband also negativ. Welche Betriebsart gemeint
    // ist, steht in der Antwort nicht (`boundModes` kann leer sein),
    // also wird der BETRAG uebernommen und gesagt, dass er es ist.
    // Ein stillschweigend falsches Vorzeichen waere ein vertauschtes
    // Sendefilter.
    const QJsonValue lo = o.value(QStringLiteral("lowCutHz"));
    const QJsonValue hi = o.value(QStringLiteral("highCutHz"));
    if (lo.isDouble() || hi.isDouble()) {
        setzeWennDa(p, o, "lowCutHz",  "FilterLow");
        setzeWennDa(p, o, "highCutHz", "FilterHigh");
        p.hinweise << QStringLiteral(
            "Das Sendefilter kam als Betrag (%1–%2 Hz). Zeus setzt das "
            "Vorzeichen erst je Betriebsart; bei unterem Seitenband "
            "gehören die Werte negativ. Nachsehen.")
            .arg(qint64(lo.toDouble())).arg(qint64(hi.toDouble()));
    }

    // ── Tor, alter Entzerrer, AM-Träger ──────────────────────────────
    const QJsonObject dexp = o.value(QStringLiteral("dexp")).toObject();
    if (!dexp.isEmpty()) { expander(p, dexp); }

    const QJsonObject leq = o.value(QStringLiteral("legacyTxEq")).toObject();
    if (!leq.isEmpty()) { alterEntzerrer(p, leq); }

    setzeWennDa(p, o, "carrierLevelPercent", "AM_Carrier_Level");

    // ── Was liegenbleibt, und zwar laut ──────────────────────────────
    //
    // Zeus' Audio-Suite ist das, was bei uns der Kanalzug waere. Ihre
    // Schluessel sind nicht offengelegt. Ein Profilname ohne diesen
    // Satz waere eine Behauptung: der Betreiber sieht „eingelesen" und
    // weiss nicht, dass die halbe Kette fehlt.
    const QJsonObject nativ =
        o.value(QStringLiteral("nativePluginStates")).toObject();
    const QJsonObject vst =
        o.value(QStringLiteral("vstPluginStates")).toObject();
    const QJsonArray kette = o.value(QStringLiteral("chainOrder")).toArray();
    if (!nativ.isEmpty() || !vst.isEmpty() || !kette.isEmpty()) {
        p.hinweise << QStringLiteral(
            "Zeus' Audio-Suite (%1 Stufe(n)) gehört nicht hierher, "
            "sondern in den Kanalzug — dort gibt es „Von Zeus…\". "
            "Dieses Profil setzt nur die Sendekette des Funkgeräts.")
            .arg(std::max({nativ.size(), vst.size(), kette.size()}));
    }
    if (o.contains(QStringLiteral("targetSpectralDensity"))) {
        p.hinweise << QStringLiteral(
            "Zeus' Dichte-Achse (%1 von 100) hat in Longpath kein "
            "Gegenstück.")
            .arg(o.value(QStringLiteral("targetSpectralDensity")).toInt());
    }

    *ok = !p.werte.isEmpty();
    if (!*ok) {
        p.hinweise << QStringLiteral(
            "In diesem Profil stand kein einziges Feld, das Longpath "
            "führt.");
    }
    return p;
}

} // namespace

ZeusProfilImport::Ergebnis ZeusProfilImport::lies(const QByteArray& json)
{
    Ergebnis e;

    QJsonParseError jf{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &jf);
    if (jf.error != QJsonParseError::NoError) {
        e.fehler << QStringLiteral("Das ist kein JSON: %1 (Zeichen %2).")
                        .arg(jf.errorString()).arg(jf.offset);
        return e;
    }

    // Beide Formen, siehe den Kopf.
    QJsonArray liste;
    if (doc.isArray()) {
        liste = doc.array();
    } else if (doc.isObject()) {
        const QJsonValue p = doc.object().value(QStringLiteral("profiles"));
        if (p.isArray()) {
            liste = p.toArray();
        } else if (doc.object().contains(QStringLiteral("id"))
                   || doc.object().contains(QStringLiteral("name"))) {
            // Ein einzelnes Profil, wie `/last-loaded` es liefert.
            liste.append(doc.object());
        }
    }
    if (liste.isEmpty()) {
        e.fehler << QStringLiteral(
            "Darin steht keine Profilliste. Erwartet wird die Antwort "
            "von /api/tx-audio-profiles — ein Objekt mit „profiles\" "
            "oder die Liste selbst.");
        return e;
    }

    int leer = 0;
    for (const QJsonValue& v : std::as_const(liste)) {
        if (!v.isObject()) { ++leer; continue; }
        bool ok = false;
        ThetisProfil p = einProfil(v.toObject(), &ok);
        if (ok) { e.profile.append(p); } else { ++leer; }
    }
    if (leer > 0) {
        e.hinweise << QStringLiteral(
            "%1 Eintrag/Einträge trugen nichts, was Longpath führt.")
            .arg(leer);
    }
    if (e.profile.isEmpty() && e.fehler.isEmpty()) {
        e.fehler << QStringLiteral("Kein einziges Profil war lesbar.");
    }
    return e;
}

bool ZeusProfilImport::siehtNachZeusAus(const QByteArray& json)
{
    QJsonParseError jf{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &jf);
    if (jf.error != QJsonParseError::NoError) { return false; }

    // Woran es haengt: ein Profileintrag mit mindestens einem Feld,
    // das NUR Zeus so nennt. „name" allein waere zu wenig — das hat
    // jedes zweite JSON.
    auto zeusig = [](const QJsonObject& o) {
        static constexpr std::array<const char*, 5> kMarker{{
            "txLeveling", "cfcConfig", "txPhaseRotator",
            "processingMode", "schemaVersion",
        }};
        for (const char* m : kMarker) {
            if (o.contains(QLatin1String(m))) { return true; }
        }
        return false;
    };

    if (doc.isObject()) {
        const QJsonObject w = doc.object();
        if (zeusig(w)) { return true; }
        const QJsonArray p = w.value(QStringLiteral("profiles")).toArray();
        return !p.isEmpty() && zeusig(p.first().toObject());
    }
    if (doc.isArray() && !doc.array().isEmpty()) {
        return zeusig(doc.array().first().toObject());
    }
    return false;
}

QStringList ZeusProfilImport::nichtUebernommen()
{
    return {
        QStringLiteral(
            "Die Audio-Suite (nativePluginStates) — die gehört in "
            "Longpaths Kanalzug, nicht in ein TX-Profil. Im "
            "Kanalzugfenster gibt es dafür „Von Zeus…\"."),
        QStringLiteral(
            "VST-Einschübe (vstPluginStates): base64-Zustände fremder "
            "Plugins, nicht lesbar und nicht gemeint."),
        QStringLiteral(
            "Die Dichte-Achse (targetSpectralDensity, 0–100) hat kein "
            "Gegenstück."),
        QStringLiteral(
            "Die Bindung an Betriebsarten (boundModes) und die zuletzt "
            "benutzte Sendeleistung je Betriebsart (driveByFamily): "
            "Betriebszustand, keine Klangeinstellung — und Leistung "
            "liest Longpath aus fremden Profilen grundsätzlich nicht."),
        QStringLiteral(
            "Die Haltezeit des Tors (dexp.holdMs) — in Longpath gibt es "
            "sie nicht."),
        QStringLiteral(
            "Die CFC-FFT-Größe (2048…16384): Longpath liegt fest auf "
            "16384."),
    };
}

} // namespace Longpath
