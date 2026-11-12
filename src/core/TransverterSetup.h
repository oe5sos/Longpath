// =================================================================
// src/core/TransverterSetup.h  (Longpath)
// =================================================================
//
// Longpath-original (window + offset, 2026-10-27), with ONE ported
// piece: the LO error (2026-11-08).
//
// Ported from Thetis source:
//   Project Files/Source/Console/xvtr.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (Longpath):
//   2026-10-27 — Transverter window and offset, Longpath-original,
//                 Martin Fischer (OE5SOS), AI-assisted via Anthropic
//                 Claude Code.
//   2026-11-08 — LO error ported from Thetis xvtr.cs TranslateFreq /
//                 udLOError, Martin Fischer (OE5SOS), with AI-assisted
//                 transformation via Anthropic Claude Code.
//                 Thetis v2.10.3.15-5 [@852bf0e].
// =================================================================

// --- From xvtr.cs ---
//=================================================================
// xvtr.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2013  Doug Wigley
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
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#pragma once

//
// ── Herkunft ────────────────────────────────────────────────────────
//
// Longpath-original bis auf den LO-Fehler (unten, bei loErrorKhz).
//
// ── Was auf der Antenne liegt, ist nicht, was das Geraet zeigt ──────
//
// Anlass, 2026-10-27. Der Betreiber baut eine 2-m-Conteststation:
// ein klassisches Funkgeraet arbeitet als ZF hinter einem Transverter
// (HA1YA MV-2, 144-148 MHz auf 28-30 MHz), und Longpath haengt mit
// einem SDR am ZF-Ausgang und zeigt das Band.
//
// Longpath sieht damit 28,3 MHz. Auf der Antenne liegt 144,3 MHz.
// Ohne Umrechnung traegt jedes Logbuch die falsche Frequenz, jeder
// Spot zeigt ins falsche Band, und der Betreiber muss im Kopf
// 116 MHz addieren -- waehrend er ein QSO fuehrt.
//
// ── Warum ein Fenster und nicht ein Bandpaar ────────────────────────
//
// Das Contestprogramm loest dasselbe ueber ein Paar von Bandnamen
// ("144" -> "1296"), weil es UKW-Baender kennt. Longpath kennt sie
// nicht: seine Band-Aufzaehlung endet bei 6 m, und 144 MHz hat dort
// keinen Eintrag. Ein Bandpaar waere hier also eine Erfindung, die
// nirgends anschliesst.
//
// Darum ein ZF-FENSTER plus Versatz: von welcher Frequenz bis welcher
// Frequenz gilt die Rechnung, und wieviel kommt dazu. Das ist genau
// das, was der Transverter tut (er mischt einen Bereich, nicht ein
// Band), es braucht keine neue Bandtabelle, und es traegt auch die
// krummen Faelle -- eine 2-m-Station mit 14-MHz-ZF zum Beispiel.
//
// ── Keine Zustandsklasse ────────────────────────────────────────────
//
// Eine Struktur mit reinen Funktionen, kein QObject. So laesst sich
// die Rechnung ohne Geraet, ohne Oberflaeche und ohne Einstellungen
// pruefen: Werte hinein, Werte heraus.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-27 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude Code.
//                Muster: Contestprogramm/src/core/Transverter.h
//                (eigener Baum, eigene Fassung -- nichts kopiert).
// =================================================================

#include <QString>
#include <QVector>

#include <QtGlobal>

namespace Longpath {

/// Ein Transverter zwischen dem ZF-Geraet und der Antenne.
///
/// RF = ZF + offsetHz, solange die ZF im Fenster [ifLowHz, ifHighHz]
/// liegt. Ausserhalb bleibt jede Frequenz unberuehrt -- wer mit
/// eingeschaltetem Transverter auf 14 MHz abstimmt, soll 14 MHz sehen
/// und nicht 130 MHz.
struct TransverterSetup {
    bool   enabled  {false};
    qint64 ifLowHz  {28000000};
    qint64 ifHighHz {32000000};    ///< MV-2: 28-32 -> 144-148 (2026-11-08)
    qint64 offsetHz {116000000};   ///< RF = ZF + offsetHz

    // ── LO-Fehler (2026-11-08, Port aus Thetis) ─────────────────────
    //
    // Der Quarzoszillator eines Transverters liegt nie genau auf dem
    // Nennwert; beim MV-2 auf 116 MHz typisch einige hundert Hz daneben.
    // Thetis fuehrt dafuer je Transverter ein eigenes Feld, in kHz mit
    // drei Stellen (also auf 1 Hz), +/-1000 kHz:
    //   From Thetis xvtr.cs:2794-2811 [@852bf0e] (udLOError0):
    //     DecimalPlaces = 3; Maximum = 1000; Minimum = -1000;
    // und rechnet ihn beim Umsetzen Antenne -> Geraet dazu (MHz):
    //   From Thetis xvtr.cs:5343-5345 [@852bf0e] (TranslateFreq):
    //     return (double)(freq -
    //         (double)lo_offset[i].Value +
    //         ((double)lo_error[i].Value / 1000.0));
    // Also ZF = RF - Versatz + Fehler, und umgekehrt RF = ZF + Versatz
    // - Fehler. Ab Werk 0: ohne Eintrag aendert sich keine Frequenz.
    double loErrorKhz {0.0};

    /// Grenze des Feldes, wie Thetis (+/-1000 kHz).
    static constexpr double kLoErrorGrenzeKhz = 1000.0;

    /// Der LO-Fehler in Hz (gerundet; Thetis zeigt 1 Hz Aufloesung).
    qint64 loErrorHz() const { return qRound64(loErrorKhz * 1000.0); }

    // ── Nur Empfang (2026-11-08) ────────────────────────────────────
    //
    // Die Station: der TS-590 sendet ueber DRV in den MV-2 (IFi) und
    // empfaengt an IFo1; Longpaths SDR haengt an IFo2, dem ZWEITEN
    // Empfangsausgang (MC-1), und zeigt das Band. Sendet dieses SDR,
    // geht seine Leistung rueckwaerts in einen Empfangsausgang.
    //
    // BandPlanGuard schuetzt davor NICHT: er rechnet 28,300 auf 144,300
    // um, findet 2 m in Region 1 erlaubt und gibt frei. Darum dieser
    // Schalter. AN ab Werk, weil das die verdrahtete Station ist; wer
    // das SDR selbst als Treiber an IFi haengt (Flex XVTR), schaltet
    // ihn ab. Er wirkt nur, solange der Transverter aktiv ist -- ohne
    // Transverter sendet Longpath wie immer.
    bool   nurEmpfang {true};

    /// Darf Longpaths Funkgeraet senden? Nein, solange der Transverter
    /// aktiv ist und auf "nur Empfang" steht -- gleich auf welcher
    /// Frequenz, denn der Anschluss haengt am Empfangsausgang.
    bool sperrtSenden() const { return active() && nurEmpfang; }

    // ── RX-Verstaerkung (2026-11-08) ────────────────────────────────
    //
    // Was der Empfangszweig des Transverters verstaerkt, wie ihn DIESES
    // Geraet sieht (MV-2 ab Werk 22 dB; an IFo2 abzueglich der Teilung
    // im MC-1). Sie wird von jedem Empfangspegel abgezogen -- die
    // Rechnung und ihre Herkunft stehen bei RadioModel::rxMeterOffsetDb.
    // Damit beziehen sich Spektrum, S-Meter und S-Stufen auf die ANTENNE.
    // Ab Werk 0: ohne Eintrag aendert sich kein Pegel.
    double rxGainDb {0.0};

    /// Der Pegelversatz fuer eine Empfangs-ZF: -rxGainDb, solange der
    /// Transverter aktiv ist und die ZF in seinem Fenster liegt, sonst 0.
    double pegelVersatzDb(qint64 ifHz) const
    {
        return (active() && coversIf(ifHz)) ? -rxGainDb : 0.0;
    }

    /// Ist das Fenster brauchbar und der Versatz von Null verschieden?
    bool configured() const
    {
        return ifHighHz > ifLowHz && ifLowHz > 0 && offsetHz != 0;
    }

    /// Eingeschaltet UND brauchbar eingestellt.
    bool active() const { return enabled && configured(); }

    /// Liegt diese ZF-Frequenz im Fenster?
    bool coversIf(qint64 ifHz) const
    {
        return ifHz >= ifLowHz && ifHz <= ifHighHz;
    }

    /// Liegt diese Antennenfrequenz im umgerechneten Fenster? Das
    /// ZF-Fenster mit Versatz und LO-Fehler auf die Antenne umgerechnet
    /// -- dieselbe Rechnung wie rfFrequencyHz.
    bool coversRf(qint64 rfHz) const
    {
        const qint64 d = offsetHz - loErrorHz();
        return rfHz >= ifLowHz + d && rfHz <= ifHighHz + d;
    }

    /// Was das Geraet zeigt -> was auf der Antenne liegt.
    /// Unveraendert, wenn nicht aktiv oder ausserhalb des Fensters.
    /// RF = ZF + Versatz - LO-Fehler (Umkehrung von TranslateFreq).
    qint64 rfFrequencyHz(qint64 ifHz) const
    {
        if (!active() || !coversIf(ifHz)) { return ifHz; }
        return ifHz + offsetHz - loErrorHz();
    }

    /// Was auf der Antenne liegt -> was dem Geraet zu sagen ist.
    /// Unveraendert, wenn nicht aktiv oder ausserhalb des Fensters.
    /// ZF = RF - Versatz + LO-Fehler (TranslateFreq, siehe loErrorKhz).
    qint64 ifFrequencyHz(qint64 rfHz) const
    {
        if (!active() || !coversRf(rfHz)) { return rfHz; }
        return rfHz - offsetHz + loErrorHz();
    }

    /// Einmessen (Longpath-eigen): ein Signal mit bekannter Frequenz
    /// (eine Bake) erscheint bei `angezeigtHz`. Liefert den LO-Fehler
    /// in kHz, der die Anzeige auf `wahrHz` bringt; der bisherige Fehler
    /// steckt in `angezeigtHz` schon drin.
    double loErrorFuer(qint64 angezeigtHz, qint64 wahrHz) const
    {
        return loErrorKhz + static_cast<double>(angezeigtHz - wahrHz) / 1000.0;
    }

    /// "28.000-30.000 -> 144.000-146.000 MHz (+116 MHz)", oder leer,
    /// wenn nichts Brauchbares eingestellt ist. Fuer die Statuszeile.
    QString describe() const;

    // ── Vorgaben ────────────────────────────────────────────────────
    //
    // Die ueblichen Kombinationen, damit niemand 116000000 von Hand
    // tippt. Frei einstellbar bleibt alles: die Vorgabe fuellt nur die
    // drei Felder.
    struct Preset {
        const char* name;      ///< "2 m / 144 MHz (28 MHz IF)"
        qint64      ifLowHz;
        qint64      ifHighHz;
        qint64      offsetHz;
    };
    static QVector<Preset> presets();

    // ── Einstellungen ───────────────────────────────────────────────
    //
    // Schluessel: Transverter/Enabled, /IfLowHz, /IfHighHz, /OffsetHz,
    // /RxOnly (nurEmpfang), /RxGainDb (rxGainDb), beide 2026-11-08.
    static TransverterSetup load();
    void save() const;

    // ── Die eine Fassung, die die Anzeige liest (2026-10-28) ────────
    //
    // Den Versatz braucht nicht ein Ort, sondern vier: die Zahl in der
    // VFO-Anzeige, die Frequenzachse im Panadapter, die Bandplan-
    // Abschnitte darin und der Logbucheintrag. Alle vier liegen weit
    // auseinander -- VfoDisplayItem entsteht tief im Instrumenten-
    // system (ItemGroup), wo es keinen Weg gibt, ihm beim Anlegen
    // etwas mitzugeben, ohne die Eigenschaftsmasken und die
    // Speicherung mit anzufassen.
    //
    // Darum eine prozessweite Fassung, wie AppSettings::instance() es
    // vormacht: EINE Stationseinstellung, von vielen gelesen, von
    // genau einer Stelle geschrieben (CompanionRigController, nachdem
    // er geladen oder der Bediener gespeichert hat).
    //
    // Nur vom Hauptfaden aus. Alle vier Leser malen oder schreiben
    // Logbuch, und beides laeuft dort.
    //
    // Der Pruefstand setzt sie direkt -- das ist der Grund, warum
    // setCurrent() oeffentlich ist und nicht nur der Controller sie
    // kennt: so laesst sich "aus wenn aus" ohne Einstellungsdatei
    // belegen.
    static const TransverterSetup& current();
    static void setCurrent(const TransverterSetup& t);

    /// Kurzweg fuer die Anzeige: ZF -> RF nach der aktuellen Fassung.
    /// Ist kein Transverter aktiv, kommt die Zahl unveraendert zurueck.
    static qint64 displayHz(qint64 ifHz) { return current().rfFrequencyHz(ifHz); }

    /// Kurzweg fuer die Gegenrichtung: RF -> ZF. Fuer alles, was eine
    /// Antennenfrequenz auf die Achse des Panadapters legen muss --
    /// die Bandplan-Abschnitte zum Beispiel.
    static qint64 panHz(qint64 rfHz) { return current().ifFrequencyHz(rfHz); }

    // ── Das Logbuch (2026-10-28) ────────────────────────────────────
    //
    // Longpaths Band-Aufzaehlung endet bei 6 m (darum das ZF-Fenster
    // und kein Bandpaar, siehe oben). bandFromFrequency(144300000) hat
    // also keinen Eintrag -- und ein ADIF-Satz ohne BAND oder mit
    // BAND=10M bei einer 144-MHz-Frequenz ist falsch, nicht nur
    // unschoen: der Satz wandert in ein Diplom-Programm.
    //
    // Hier steht deshalb eine kleine, geschlossene Tabelle der
    // ADIF-Bandnamen -- NICHT eine neue Bandverwaltung fuer Longpath.
    // Sie wird nur betreten, wenn ein Transverter aktiv ist, sie
    // entscheidet nichts ueber das Senden, und sie kennt genau die
    // Baender, die ein Transverter ausgibt.
    //
    /// ADIF-Bandname fuer eine ANTENNENfrequenz ("2m", "70cm", …),
    /// oder leer, wenn keiner passt.
    static QString adifBand(qint64 rfHz);

    /// Frequenz und Band fuer einen Logbucheintrag.
    ///
    /// Gibt false zurueck, wenn kein Transverter im Spiel ist. Dann
    /// bleibt *mhz und *band unberuehrt und der Aufrufer behaelt seine
    /// eigene Rechnung -- bitgleich wie vorher. Das ist der Zweck
    /// dieser Form: an den Aufrufstellen soll NICHTS passieren, solange
    /// niemand einen Transverter eingestellt hat.
    static bool logbuchFrequenz(qint64 ifHz, double* mhz, QString* band);
};

} // namespace Longpath
