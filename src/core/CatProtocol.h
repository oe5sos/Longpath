// =================================================================
// src/core/CatProtocol.h  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/CAT/CATParser.cs   [v2.10.3.15]
//   Project Files/Source/Console/CAT/CATCommands.cs [v2.10.3.15]
//   Project Files/Source/Console/CAT/CATStructs.xml [v2.10.3.15]
// Original licences from the Thetis sources are included below.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-11 — Reimplemented in C++20/Qt6 for Longpath, AI-assisted
//                 via Anthropic Claude Code. Erster Gang: die zehn
//                 Befehle, die Fremdprogramme tatsaechlich sprechen
//                 (FA FB MD IF ID AI TX RX FR FT). Thetis' CAT ist
//                 18.081 Zeilen, davon 10.591 in CATCommands.cs mit
//                 dem vollen Kenwood-Satz plus allen ZZ-Erweiterungen;
//                 der Rest folgt in eigenen Gaengen.
// =================================================================
//
// --- From CATParser.cs ---
//=================================================================
// CATParser.cs
//=================================================================
// Copyright (C) 2012  Bob Tracy
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
// You may contact the author via email at: k5kdn@arrl.net
//=================================================================
//
// --- From CATCommands.cs ---
//=================================================================
// CATCommands.cs
//=================================================================
// Copyright (C) 2005  Bob Tracy
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
// You may contact the author via email at: k5kdn@arrl.net
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================

#pragma once

#include <QString>

#include <functional>
#include <optional>

namespace Longpath {

/// Die CAT-Protokollschicht: Kenwood-Befehlswoerter rein, Antwortwoerter
/// raus. Kennt WEDER Netz NOCH RadioModel -- was sie ueber das Geraet
/// weiss, steht in `Zustand`, und was sie aendern darf, haengt in `Haken`.
/// Dieselbe Trennung wie bei den Verstaerkeranbindungen (SpeProtocol,
/// Kpa500Protocol, AcomProtocol): die Protokollschicht ist am
/// Schreibtisch vollstaendig pruefbar, der Transport getrennt davon.
///
/// Warum das hier wichtig ist: ein CAT-Server, der falsch antwortet,
/// bringt WSJT-X dazu, auf der falschen Frequenz zu senden. Jede
/// Wortform in dieser Datei ist darum gegen Thetis belegt und im
/// Prueffall festgenagelt, nicht aus dem Kenwood-Handbuch erinnert.
class CatProtocol {
public:
    /// Was der Server gerade ueber das Geraet weiss. Der Aufrufer fuellt
    /// das vor jedem `antwortAuf()`; die Schicht haelt keinen Zustand.
    struct Zustand {
        quint64 vfoAHz{0};
        quint64 vfoBHz{0};
        /// Modus als KENWOOD-Zahl 1..9, nicht als DSPMode -- die
        /// Umrechnung gehoert in die Verdrahtung, damit diese Schicht
        /// Longpaths Modusbegriff nicht kennen muss.
        int  modeK{2};            // 2 = USB
        bool mox{false};
        bool split{false};
        bool ritOn{false};
        bool xitOn{false};
        /// RIT- und XIT-Ablage in Hz, GETRENNT (negativ erlaubt).
        ///
        /// Bis zum 2026-10-13 stand hier ein einziges `itHz` mit der
        /// Begruendung "welcher gilt, entscheidet ritOn/xitOn". Das war
        /// fuer den Kenwood-Satz richtig (dessen Statussatz zeigt nur
        /// EINEN Wert, und RU/RD/RC wirken auf den aktiven), aber es war
        /// **ein falsches Bild des Geraets**: `SliceModel` fuehrt
        /// `ritHz` und `xitHz` seit immer getrennt (SliceModel.h:281-283).
        /// Der ZZ-Satz braucht die Trennung: ZZXF stellt die XIT-Ablage,
        /// auch wenn RIT an ist, und ZZRF umgekehrt.
        int  ritHz{0};
        int  xitHz{0};
        /// Abstimmschritt in Hz. Thetis sendet einen Index-abhaengigen
        /// 4-Byte-Code (Step2String); wir senden den Schritt selbst,
        /// siehe die Notiz bei schrittFeld().
        int  stepHz{10};
        /// Antwort auf ID;. Thetis waehlt sie nach CATRigType
        /// (CATCommands.cs:291-...); 900 = SDR-1000.
        int  rigId{900};

        // ── Die Felder fuer die sechs Befehle aus der zweiten Fassung
        //    (uebernommen 2026-10-13, siehe
        //    docs/architecture/2026-10-12-cat-zwei-fassungen.md) ──────────
        /// Laeuft das Geraet? Antwort auf PS;.
        bool eingeschaltet{true};
        /// Empfangspegel in dBm fuer SM;. -200 heisst "weiss ich nicht".
        double signalDbm{-200.0};

        // ── Die Felder fuer den ZZ-Satz (2026-10-13) ───────────────────
        //
        // Warum modeZZ NEBEN modeK steht und nicht daraus gerechnet
        // wird: die beiden sind zwei verlustbehaftete Abbildungen
        // DESSELBEN Modus. Kenwood kennt kein DSB/SAM/DRM und hat fuer
        // CW nur eine Zahl -- modeK -> modeZZ muesste das Seitenband
        // erraten. Beide kommen darum aus dem einen Longpath-Modus,
        // siehe CatModeMap.h.
        /// Modus als ZZ-Zahl 0..11 (= Thetis' Mode2String). 1 = USB.
        int  modeZZ{1};
        /// Die Filterkanten der empfangenden Scheibe in Hz, negativ
        /// erlaubt (LSB). Fuer ZZFL/ZZFH.
        int  filterLowHz{150};
        int  filterHighHz{2850};
        /// Rauschsperre: 0 = aus, 1 = an, 2 = Stimmsperre.
        /// From Thetis CAT/CATCommands.cs ZZSO -- "[2.10.3.5]MW0LGE 2 is
        /// vsql".
        int  squelch{0};
        /// Antwort auf ZZZM; -- die Gerätebezeichnung, veraenderliche
        /// Laenge. Thetis gibt HardwareSpecific.Model.ToString().
        QString modell{QStringLiteral("LONGPATH")};
        /// Antwort auf ZZZV; -- die Fassung ohne Firmware-Teil.
        QString fassung{QStringLiteral("0.0.0")};
    };

    /// Was ein Befehl aendern darf. Nicht gesetzte Haken heissen: dieser
    /// Befehl wird mit `?;` abgelehnt, statt stillschweigend ins Leere
    /// zu laufen -- ein Fremdprogramm, dem das Setzen quittiert wird,
    /// ohne dass etwas geschieht, sendet danach auf der alten Frequenz.
    struct Haken {
        std::function<void(quint64)> setzeVfoA;
        std::function<void(quint64)> setzeVfoB;
        std::function<void(int)>     setzeModeK;
        std::function<void(bool)>    setzeMox;
        std::function<void(bool)>    setzeSplit;
        // ── fuer die RIT/XIT-Familie (2026-10-13) ──────────────────────
        std::function<void(bool)>    setzeRit;
        std::function<void(bool)>    setzeXit;
        /// Die beiden Ablagen setzen (Hz, negativ erlaubt). GETRENNT --
        /// einen Haken "setze die aktive Ablage" gibt es bewusst nicht
        /// mehr: welcher der beiden gemeint ist, entscheidet der
        /// BEFEHL (ZZRF gegen ZZXF), und bei Kenwoods RU/RD/RC die
        /// Flagge. Ein Haken, der "die aktive" meint, verschob die
        /// Entscheidung in die Verdrahtung, wo sie niemand sieht.
        std::function<void(int)>     setzeRitHz;
        std::function<void(int)>     setzeXitHz;
        // ── fuer den ZZ-Satz (2026-10-13) ──────────────────────────────
        /// Modus als ZZ-Zahl 0..11. Eigener Haken und nicht setzeModeK,
        /// weil der Aufrufer sonst ZZ -> Kenwood rechnen muesste und
        /// dabei DSB, SAM und DRM verloren gingen.
        std::function<void(int)>     setzeModeZZ;
        std::function<void(int)>     setzeFilterLow;
        std::function<void(int)>     setzeFilterHigh;
        std::function<void(int)>     setzeSquelch;
    };

    CatProtocol() = default;

    void setzeZustand(const Zustand& z) { m_z = z; }

    /// Der NACHLESER: holt den frischen Zustand, nachdem ein Setzen
    /// durch den Haken gegangen ist.
    ///
    /// Ohne ihn quittiert ein Setzen blind. Das ist keine theoretische
    /// Sorge -- die zweite CAT-Fassung dieses Projekts hat es am
    /// 2026-10-11 am Draht gemessen: "RT1 answered silently and the
    /// following RT still said RT0". Der Haken war da, er lief, und die
    /// Scheibe dahinter fehlte; ein Contestprogramm glaubte, seine RIT
    /// sei an.
    ///
    /// Mit Nachleser prueft jedes Setzen, ob es gewirkt hat, und
    /// antwortet sonst mit `?;`. Ohne Nachleser bleibt es beim alten
    /// Verhalten (blind quittieren) -- dann ist es die Entscheidung des
    /// Aufrufers, nicht ein Versehen.
    void setzeNachleser(std::function<Zustand()> leser)
    {
        m_nachleser = std::move(leser);
    }
    const Zustand& zustand() const { return m_z; }
    void setzeHaken(const Haken& h) { m_h = h; }

    /// Ein Befehlswort (mit oder ohne abschliessendes ';') hinein, die
    /// Antwort heraus. Leere Antwort = nichts zu senden (ein Setzen
    /// wird nicht quittiert, so wie bei Kenwood). `?;` = abgelehnt.
    ///
    /// From Thetis CAT/CATParser.cs:108-215 [v2.10.3.15] -- Get(string):
    /// erst CheckFormat(), dann ein Schalter auf das zweistellige
    /// Praefix, dann cmdlist.XX(suffix).
    QString antwortAuf(const QString& befehl);

    /// Der Statussatz, den IF; liefert: 35 Zeichen.
    /// From Thetis CAT/CATCommands.cs IF() [v2.10.3.15].
    QString statussatz() const;

    /// Der ERWEITERTE Statussatz, den ZZIF; liefert: 36 Zeichen.
    ///
    /// From Thetis CAT/CATCommands.cs:3386-3441 [v2.10.3.15] -- ZZIF().
    /// Das zusaetzliche Zeichen gegenueber IF; ist das Modusfeld: ZZ
    /// fuehrt den Modus zweistellig (Mode2String), Kenwood einstellig.
    /// Alles andere liegt feldweise gleich.
    QString zzStatussatz() const;

    /// Das 5-Zeichen-Feld Vorzeichen + 4 Stellen, wie ZZRF, ZZFL, ZZFH
    /// und ZZXF es fuehren.
    ///
    /// From Thetis CAT/CATCommands.cs:5904-5940 (ZZRF) -- dort
    /// `sign + AddLeadingZeros(|x|).Substring(1)`.
    ///
    /// ABWEICHUNG, mit Absicht: Thetis klammert beim SETZEN auf
    /// +-99999 (ZZRF) bzw. +-10000 (ZZFL), kann in der Antwort aber nur
    /// vier Stellen fuehren -- aus 12000 wird dort stillschweigend
    /// "+2000", aus 10000 "+0000". Longpath klammert auf +-9999, also
    /// auf das, was das Feld tragen kann. Ein geklammerter Wert, den der
    /// Nachleser danach als Abweichung erkennt und mit `?;` meldet, ist
    /// besser als eine Zahl, die etwas anderes behauptet.
    static QString vorzeichenFeld4(int hz);

    /// Fehlerantwort. From Thetis CAT/CATParser.cs:56 -- Error1 = "?;".
    static QString abgelehnt() { return QStringLiteral("?;"); }

    /// Hilfen, die auch die Verdrahtung braucht (und die Prueffaelle).
    static QString mitNullen(quint64 wert, int stellen);
    /// Das 4-Zeichen-Schrittfeld des Statussatzes.
    static QString schrittFeld(int stepHz);
    /// Die Ablage, die der Statussatz zeigt: erst RIT, sonst XIT,
    /// sonst 0 -- wie Thetis (CATCommands.cs IF(): `if(RITOn) ... else
    /// if(XITOn)`). Nicht beide zugleich.
    static int aktiveAblage(const Zustand& z);

    /// Das 6-Zeichen-Feld fuer RIT/XIT: Vorzeichen + 5 Stellen.
    /// From Thetis CAT/CATCommands.cs IF() -- incr.
    static QString itFeld(int hz);

    /// Der Kenwood-S-Meter-Wert 0..30 aus einem Pegel in dBm.
    ///
    /// From Thetis CAT/CATCommands.cs:896-931 [v2.10.3.15] -- SM. Die
    /// Kennlinie hat einen Knick bei S9: darunter rechnet sie
    /// (dBm+127)/6 * 1.6667, darueber 15 + (dBm+73). Uebernommen aus der
    /// zweiten CAT-Fassung dieses Projekts, die sie gegen hamlibs
    /// TS-2000-Treiber gefahren hat.
    static int sMeterAusDbm(double dbm);

    /// Hat ein Anrufer AI1; gesetzt? Die Schicht merkt es sich; das
    /// Senden von selbst gehoert in den Transport (eigener Gang).
    bool selbstmeldung() const { return m_selbstmeldung; }

    /// Welche Form ein Befehlswort hat. Die drei Fragen werden EINMAL
    /// in antwortAuf() entschieden und weitergegeben, damit die
    /// ZZ-Haelfte sie nicht nachrechnet und dabei abdriftet.
    struct Form {
        bool setzen{false};
        bool abfragen{false};
        /// Der Befehl SELBST ist die Tat, ohne Nutzdaten (nSet == 0).
        bool tat{false};
        /// Erwartete Laenge der Antwortdaten. VERAENDERLICH, weil
        /// ZZZM; und ZZZV; eine Antwort tragen, deren Laenge erst beim
        /// Antworten feststeht.
        int  nAns{-1};
    };

private:
    /// Die ZZ-Haelfte des Verteilers. Gibt die Antwortdaten zurueck;
    /// `std::nullopt` heisst abgelehnt (`?;`), eine leere Zeichenkette
    /// heisst "nichts zu senden".
    std::optional<QString> zzDaten(const QString& praefix,
                                   const QString& suffix,
                                   Form& f);

    /// Nach einem Setzen: frisch nachlesen und pruefen, ob der Wunsch
    /// angekommen ist. Ohne Nachleser immer true (altes Verhalten).
    bool hatGewirkt(const std::function<bool(const Zustand&)>& pruefung);

    Zustand m_z;
    Haken   m_h;
    std::function<Zustand()> m_nachleser;
    bool    m_selbstmeldung{false};
};

} // namespace Longpath
