// =================================================================
// src/core/ThetisProfilImport.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/database.cs [v2.10.3.15]
//     — 4299-4510   AddTXProfileTable: die 208 Spalten eines TX-Profils
//     — 9470, 9546  DataSet "Data", WriteXml(..., WriteSchema)
//     — 11263-11292 ExpandOldTxProfileTable: fehlende Spalten aus
//                   "Default" fuellen, die alte "Default"-Zeile NICHT
//                   uebernehmen
//   Project Files/Source/Console/setup.cs [v2.10.3.15]
//     — 12448-12520 ExportCurrentTxProfile: die Einzelausfuhr
//     — 3000-3120   isTXProfileSettingDifferent<>: welche Spalte an
//                   welchem Bedienelement haengt (die Belegstelle fuer
//                   JEDE Zuordnung in dieser Datei)
//     — 18959, 18965 die dB->linear-Umrechnung der DEXP-Verhaeltnisse
// Original licences from the Thetis sources are included below.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-13 — Erster Gang: der LESER. Zeus hat in 2.0.50
//                 angekuendigt, Thetis-TX-Profile einlesen zu koennen;
//                 Longpath ist selbst ein Thetis-Port, die Felder
//                 stehen also fast eins zu eins gegenueber. Diese
//                 Schicht kennt WEDER AppSettings NOCH die Oberflaeche:
//                 Dateiinhalt hinein, Profile und Hinweise heraus, am
//                 Schreibtisch vollstaendig pruefbar. Dieselbe Trennung
//                 wie bei CatProtocol und den Verstaerkeranbindungen.
//                 Reimplemented in C++20/Qt6, AI-assisted via Anthropic
//                 Claude Code.
// =================================================================
//
//=================================================================
// database.cs
//=================================================================
// Copyright (C) 2004-2009  FlexRadio Systems
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: FlexRadio Systems, 8900 Marybank Dr,
// Austin, TX 78750, USA.
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================


#include "ThetisProfilImport.h"

#include <QXmlStreamReader>

#include <array>

namespace Longpath {

namespace {

// ── Die Abbildung ────────────────────────────────────────────────────────
//
// 57 Spalten heissen auf beiden Seiten GLEICH, und das ist kein Zufall:
// Longpaths Profilfelder sind aus derselben Tabelle gezogen worden
// (MicProfileManager::defaultProfileValues() zitiert durchweg
// database.cs-Zeilen). Die stehen darum nicht einzeln hier, sondern
// werden in `longpathFeld()` durchgelassen, wenn Longpath das Feld
// kennt.
//
// Hier stehen nur die UMBENANNTEN -- und jede Zeile hat ihre
// Belegstelle in Thetis' eigenem Code, nicht in meiner Vermutung:
// `isTXProfileSettingDifferent<T>(dr, "<Spalte>", <Bedienelement>)` in
// setup.cs sagt, an welchem Regler eine Spalte haengt.
struct Umbenannt { const char* thetis; const char* longpath; };

constexpr std::array<Umbenannt, 11> kUmbenannt{{
    // setup.cs:3060  chkDEXPEnable.Checked
    {"Dexp_On",           "DEXP_Enabled"},
    // setup.cs:3078  (int)udDEXPDetTau.Value
    {"Dexp_Tau",          "DEXP_DetectorTauMs"},
    // setup.cs:3066  (int)udDEXPAttack.Value
    {"Dexp_Attack",       "DEXP_AttackTimeMs"},
    // setup.cs:3070  (int)udDEXPRelease.Value
    {"Dexp_Release",      "DEXP_ReleaseTimeMs"},
    // setup.cs:3074  (decimal)udDEXPExpansionRatio.Value
    //
    // ACHTUNG, die Spalte heisst irrefuehrend: "Dexp_Attenuate" traegt
    // NICHT eine Daempfung, sondern das EXPANSIONSVERHAELTNIS in dB.
    // Nachgelesen, nicht erraten -- und genau das ist der Grund, warum
    // hier keine Zeile nach Namensaehnlichkeit gesetzt wurde: ein
    // falsch abgebildetes Verhaeltnis verstellt den Sendeton, und die
    // Anzeige sieht dabei richtig aus.
    {"Dexp_Attenuate",    "DEXP_ExpansionRatioDb"},
    {"Dexp_Hysterisis",   "DEXP_HysteresisRatioDb"},
    // setup.cs:3090  chkSCFEnable.Checked / udSCFLowCut / udSCFHighCut
    {"Dexp_SCF_On",       "DEXP_SideChannelFilterEnabled"},
    {"Dexp_SCF_Low",      "DEXP_LowCutHz"},
    {"Dexp_SCF_High",     "DEXP_HighCutHz"},
    // setup.cs:3098  chkDEXPLookAheadEnable.Checked / udDEXPLookAhead
    {"Dexp_LookAhead_On", "DEXP_LookAheadEnabled"},
    {"Dexp_LookAhead",    "DEXP_LookAheadMs"},
}};

// Die Spalten, die Longpath kennt. Gezogen aus
// MicProfileManager::defaultProfileValues() -- hier noch einmal
// aufgefuehrt, weil diese Schicht den Profilverwalter absichtlich NICHT
// kennt (sie soll ohne AppSettings pruefbar sein). Ein Prueffall
// vergleicht die beiden Listen, damit sie nicht auseinanderlaufen.
constexpr std::array<const char*, 107> kLongpathFelder{{
    "MicGain", "Mic_Input_Boost", "Mic_XLR", "Line_Input_On",
    "Line_Input_Level", "Mic_TipRing", "Mic_Bias", "Mic_PTT_Disabled",
    "Mic_LineInGain", "Mic_UserDigOut", "Dexp_Threshold", "VOX_GainScalar",
    "VOX_HangTime", "AntiVox_Gain", "MonitorVolume", "Mic_Source",
    "TwoToneFreq1", "TwoToneFreq2", "TwoToneLevel", "TwoTonePower",
    "TwoToneFreq2Delay", "TwoToneInvert", "TwoTonePulsed",
    "TwoToneDrivePowerOrigin", "TXEQEnabled", "TXEQPreamp",
    "TXEQ1", "TXEQ2", "TXEQ3", "TXEQ4", "TXEQ5", "TXEQ6", "TXEQ7",
    "TXEQ8", "TXEQ9", "TXEQ10",
    "TxEqFreq1", "TxEqFreq2", "TxEqFreq3", "TxEqFreq4", "TxEqFreq5",
    "TxEqFreq6", "TxEqFreq7", "TxEqFreq8", "TxEqFreq9", "TxEqFreq10",
    "TXParaEQData", "Lev_On", "Lev_MaxGain", "Lev_Decay",
    "ALC_MaximumGain", "ALC_Decay",
    "CFCPhaseRotatorEnabled", "CFCPhaseReverseEnabled",
    "CFCPhaseRotatorFreq", "CFCPhaseRotatorStages", "CFCEnabled",
    "CFCPostEqEnabled", "CFCPreComp", "CFCPostEqGain",
    "CFCEqFreq0", "CFCEqFreq1", "CFCEqFreq2", "CFCEqFreq3", "CFCEqFreq4",
    "CFCEqFreq5", "CFCEqFreq6", "CFCEqFreq7", "CFCEqFreq8", "CFCEqFreq9",
    // Die zwanzig CFC-BANDWERTE. Sie heissen auf beiden Seiten gleich
    // (database.cs:4780-4800), und Longpath fuehrt sie seit immer --
    // eingetragen in einer SCHLEIFE, nicht als Literale. Dass sie hier
    // zuerst FEHLTEN, hat der Prueffall
    // `dieFeldlisteDecktSichMitDemProfilverwalter` gefunden: ich hatte
    // die Felder aus defaultProfileValues() mit einem grep nach
    // `out.insert(QStringLiteral("..."` gezaehlt und damit genau die
    // zwanzig uebersehen, die in der Schleife stehen. Ohne den Fall
    // waeren bei jedem Einlesen die CFC-Baender stumm liegengeblieben.
    "CFCPreComp0", "CFCPreComp1", "CFCPreComp2", "CFCPreComp3",
    "CFCPreComp4", "CFCPreComp5", "CFCPreComp6", "CFCPreComp7",
    "CFCPreComp8", "CFCPreComp9",
    "CFCPostEqGain0", "CFCPostEqGain1", "CFCPostEqGain2", "CFCPostEqGain3",
    "CFCPostEqGain4", "CFCPostEqGain5", "CFCPostEqGain6", "CFCPostEqGain7",
    "CFCPostEqGain8", "CFCPostEqGain9",
    "CFCParaEQData", "CompanderLevel", "CESSB_On", "FilterLow",
    "FilterHigh",
    "DEXP_Enabled", "DEXP_DetectorTauMs", "DEXP_AttackTimeMs",
    "DEXP_ReleaseTimeMs", "DEXP_ExpansionRatioDb",
    "DEXP_HysteresisRatioDb", "DEXP_LookAheadEnabled", "DEXP_LookAheadMs",
    "DEXP_LowCutHz", "DEXP_HighCutHz", "DEXP_SideChannelFilterEnabled",
    // Seit 2026-10-13 im Profil, mit demselben Namen wie bei Thetis.
    "AM_Carrier_Level",
}};

bool istLongpathFeld(QStringView name)
{
    for (const char* f : kLongpathFelder) {
        if (name == QLatin1String(f)) { return true; }
    }
    return false;
}

/// Welche Spalten Wahrheitswerte tragen. Aus database.cs:4299-4510
/// (`typeof(bool)`); hier nur die, die auch uebernommen werden.
bool istWahrheitsspalte(QStringView lpFeld)
{
    static constexpr std::array<const char*, 15> kBool{{
        "Mic_Input_Boost", "Mic_XLR", "Line_Input_On", "Mic_TipRing",
        "Mic_Bias", "Mic_PTT_Disabled", "TwoToneInvert", "TwoTonePulsed",
        "TXEQEnabled", "Lev_On", "CFCPhaseRotatorEnabled",
        "CFCPhaseReverseEnabled", "CFCEnabled", "CFCPostEqEnabled",
        "CESSB_On",
    }};
    for (const char* f : kBool) {
        if (lpFeld == QLatin1String(f)) { return true; }
    }
    // Die DEXP-Schalter heissen auf unserer Seite anders und tragen das
    // "Enabled" im Namen.
    return lpFeld.endsWith(QLatin1String("Enabled"))
           && lpFeld.startsWith(QLatin1String("DEXP_"));
}

} // namespace

QString ThetisProfilImport::longpathFeld(const QString& thetisSpalte)
{
    for (const Umbenannt& u : kUmbenannt) {
        if (thetisSpalte == QLatin1String(u.thetis)) {
            return QString::fromLatin1(u.longpath);
        }
    }
    // Namensgleich -- aber nur, wenn Longpath das Feld wirklich fuehrt.
    // Thetis hat 208 Spalten, Longpath 87; die 121 uebrigen gehen NICHT
    // stillschweigend durch.
    if (istLongpathFeld(thetisSpalte)) { return thetisSpalte; }
    return {};
}

QHash<QString, QString> ThetisProfilImport::nichtUebernommen()
{
    // Die Spalten, die absichtlich draussen bleiben, mit dem Grund in
    // Worten. Nicht vollstaendig ueber alle 121 -- aufgefuehrt sind die,
    // die ein Betreiber im Sendeweg VERMISSEN wuerde. Fuer alles andere
    // genuegt der Sammelhinweis.
    QHash<QString, QString> g;
    g.insert(QStringLiteral("CompanderOn"),
             QStringLiteral("Der Kompander-Schalter liegt in Longpath "
                            "AUSSERHALB des Profils (hardware/<mac>/tx/"
                            "cpdr/on) -- genau wie bei Thetis, wo die "
                            "Spalte aus console.CPDR gefuellt wird, also "
                            "aus dem globalen Zustand. Der PEGEL "
                            "(CompanderLevel) kommt mit."));
    g.insert(QStringLiteral("Power"),
             QStringLiteral("Sendeleistung wird nicht eingelesen. Ein "
                            "fremdes Profil darf die Leistung an dieser "
                            "Station nicht setzen."));
    g.insert(QStringLiteral("Tune_Power"),
             QStringLiteral("Abstimmleistung wird nicht eingelesen, aus "
                            "demselben Grund wie die Sendeleistung: ein "
                            "fremdes Profil darf an dieser Station keine "
                            "Leistung setzen."));
    g.insert(QStringLiteral("TX_AF_Level"),
             QStringLiteral("Longpath fuehrt den Sende-Tonpegel nicht im "
                            "Profil."));
    g.insert(QStringLiteral("FMMicGain"),
             QStringLiteral("Longpath hat keine eigene FM-Mikrofon"
                            "verstaerkung im Profil."));
    g.insert(QStringLiteral("MicMute"),
             QStringLiteral("Stummschaltung ist ein Betriebszustand, "
                            "keine Klangeinstellung."));
    g.insert(QStringLiteral("VOX_On"),
             QStringLiteral("VOX an/aus ist Betriebszustand; die "
                            "Haltezeit (VOX_HangTime) kommt mit."));
    g.insert(QStringLiteral("DXOn"),
             QStringLiteral("Thetis' DX-Kompander gibt es in Longpath "
                            "nicht."));
    g.insert(QStringLiteral("DXLevel"),
             QStringLiteral("Der Pegel des DX-Kompanders, den es in "
                            "Longpath nicht gibt -- ohne die Stufe hat "
                            "ihr Pegel keine Bedeutung."));
    g.insert(QStringLiteral("TXEQNumBands"),
             QStringLiteral("Longpaths Sende-Equalizer hat FEST zehn "
                            "Baender (TransmitModel::txEqNumBands). Ein "
                            "Thetis-Profil mit drei Baendern bringt "
                            "seine zehn Werte trotzdem mit -- Thetis "
                            "speichert immer alle zehn."));
    g.insert(QStringLiteral("Pure_Signal_Enabled"),
             QStringLiteral("PureSignal haengt am Geraet, nicht am "
                            "Profil."));
    g.insert(QStringLiteral("PA_Profile"),
             QStringLiteral("Endstufenprofile haben in Longpath ihre "
                            "eigene Verwaltung (PaProfileManager)."));
    return g;
}

QString ThetisProfilImport::wahrheitswert(const QString& roh, bool* ok)
{
    const QString t = roh.trimmed().toLower();
    if (ok) { *ok = true; }
    if (t == QLatin1String("true") || t == QLatin1String("1")) {
        return QStringLiteral("True");
    }
    if (t == QLatin1String("false") || t == QLatin1String("0")) {
        return QStringLiteral("False");
    }
    if (ok) { *ok = false; }
    return {};
}

ThetisProfilImport::Ergebnis
ThetisProfilImport::lies(const QByteArray& inhalt)
{
    Ergebnis e;
    if (inhalt.isEmpty()) {
        e.fehler << QStringLiteral("Die Datei ist leer.");
        return e;
    }

    QXmlStreamReader r(inhalt);

    // ── 1. Die Wurzel ────────────────────────────────────────────────────
    // From database.cs:9470 -- `ds = new DataSet("Data")`. Beide
    // Dateiarten tragen darum `<Data>` als Wurzel. Erkannt wird am
    // INHALT: der Betreiber darf die Datei umbenennen.
    if (!r.readNextStartElement()) {
        e.fehler << QStringLiteral("Das ist keine XML-Datei.");
        return e;
    }
    if (r.name() != QLatin1String("Data")) {
        e.fehler << QStringLiteral(
            "Das Wurzelelement heisst \"%1\", erwartet wird \"Data\" "
            "(so nennt Thetis seine Datenbank). Entweder ist es keine "
            "Thetis-Datei oder eine aus einer Fassung, die Longpath "
            "nicht kennt.").arg(r.name().toString());
        return e;
    }

    // ── 2. Durch die Tabellen ────────────────────────────────────────────
    //
    // Ein DataSet-XML hat je ZEILE ein Element, dessen Name die TABELLE
    // ist. Die Einzelausfuhr enthaelt nur `<TXProfile>`-Elemente
    // (setup.cs:12486-12497 wirft alle anderen Tabellen weg), eine
    // Sicherung auch alle anderen. Daran wird die Art unterschieden --
    // und sonst an nichts: die Endung ist bei beiden .xml.
    bool andereTabellen = false;
    int  uebersprungen  = 0;
    QStringList fremdeSpalten;

    while (!r.atEnd() && !r.hasError()) {
        if (r.readNext() != QXmlStreamReader::StartElement) { continue; }
        if (r.name() == QLatin1String("schema")) {
            r.skipCurrentElement();
            continue;
        }
        if (r.name() != QLatin1String("TXProfile")) {
            andereTabellen = true;
            r.skipCurrentElement();
            continue;
        }

        // ── 3. Eine Profilzeile ──────────────────────────────────────────
        ThetisProfil p;
        QStringList leer;
        while (r.readNextStartElement()) {
            const QString spalte = r.name().toString();
            const QString wert   = r.readElementText().trimmed();
            if (spalte == QLatin1String("Name")) {
                p.name = wert;
                continue;
            }
            const QString feld = longpathFeld(spalte);
            if (feld.isEmpty()) {
                if (!fremdeSpalten.contains(spalte)) {
                    fremdeSpalten << spalte;
                }
                continue;
            }
            if (wert.isEmpty()) {
                // DataSet laesst eine DBNull-Spalte GANZ weg; eine
                // leere Spalte ist also etwas anderes und bleibt
                // ungesetzt, damit der Vorgabewert greift.
                leer << spalte;
                continue;
            }
            if (istWahrheitsspalte(feld)) {
                bool ok = false;
                const QString w = wahrheitswert(wert, &ok);
                if (!ok) {
                    p.hinweise << QStringLiteral(
                        "%1: \"%2\" ist kein Wahrheitswert -- "
                        "uebersprungen.").arg(spalte, wert);
                    continue;
                }
                p.werte.insert(feld, w);
            } else {
                p.werte.insert(feld, wert);
            }
        }

        if (p.name.isEmpty()) {
            ++uebersprungen;
            continue;
        }
        // From database.cs:11287 -- `if ("Default" != (string)(newRow
        // ["Name"]))`: Thetis nimmt die fremde "Default"-Zeile
        // ausdruecklich NICHT herein. Hier genauso: sie traegt keine
        // Absicht des Betreibers, sondern den Anlegezustand seiner
        // Thetis-Fassung, und wuerde ueber ein gleichnamiges
        // Longpath-Profil fallen.
        if (p.name.compare(QLatin1String("Default"), Qt::CaseInsensitive) == 0) {
            e.hinweise << QStringLiteral(
                "Das Profil \"Default\" wurde uebergangen -- es traegt "
                "den Anlegezustand der Thetis-Fassung, nicht eine "
                "Einstellung des Betreibers. Thetis macht es beim "
                "eigenen Einlesen genauso.");
            continue;
        }
        if (p.werte.isEmpty()) {
            e.hinweise << QStringLiteral(
                "Das Profil \"%1\" trug keine uebernehmbare "
                "Einstellung.").arg(p.name);
            continue;
        }
        if (!leer.isEmpty()) {
            p.hinweise << QStringLiteral(
                "Ohne Wert in der Datei, Vorgabe bleibt stehen: %1")
                .arg(leer.join(QStringLiteral(", ")));
        }
        e.profile << p;
    }

    if (r.hasError()) {
        e.fehler << QStringLiteral("XML-Fehler in Zeile %1: %2")
                        .arg(r.lineNumber()).arg(r.errorString());
        return e;
    }

    if (e.profile.isEmpty() && e.hinweise.isEmpty()) {
        e.fehler << QStringLiteral(
            "Die Datei hat die Form einer Thetis-Datenbank, enthaelt "
            "aber kein einziges TX-Profil.");
        return e;
    }

    e.art = andereTabellen ? Art::Datenbanksicherung : Art::Profilausfuhr;
    if (uebersprungen > 0) {
        e.hinweise << QStringLiteral(
            "%1 Profilzeile(n) ohne Namen uebersprungen.")
            .arg(uebersprungen);
    }
    if (!fremdeSpalten.isEmpty()) {
        // Der Sammelhinweis. Thetis hat 208 Spalten, Longpath fuehrt 87
        // davon -- das ist kein Fehler, sondern der Unterschied zweier
        // Programme. Genannt werden sie trotzdem, denn sonst sieht der
        // Betreiber einen Profilnamen und weiss nicht, was fehlt.
        const QHash<QString, QString> gruende = nichtUebernommen();
        QStringList mitGrund;
        for (const QString& s : fremdeSpalten) {
            if (gruende.contains(s)) {
                mitGrund << QStringLiteral("%1 (%2)").arg(s, gruende.value(s));
            }
        }
        e.hinweise << QStringLiteral(
            "%1 Thetis-Spalten hat Longpath nicht und laesst sie "
            "liegen.").arg(fremdeSpalten.size());
        for (const QString& m : mitGrund) { e.hinweise << m; }
    }
    return e;
}

} // namespace Longpath
