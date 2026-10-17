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
        /// RIT- bzw. XIT-Wert in Hz (negativ erlaubt). Welcher von
        /// beiden gilt, entscheidet ritOn/xitOn -- wie Thetis
        /// (CATCommands.cs IF(): erst RIT, sonst XIT).
        int  itHz{0};
        /// Abstimmschritt in Hz. Thetis sendet einen Index-abhaengigen
        /// 4-Byte-Code (Step2String); wir senden den Schritt selbst,
        /// siehe die Notiz bei schrittFeld().
        int  stepHz{10};
        /// Antwort auf ID;. Thetis waehlt sie nach CATRigType
        /// (CATCommands.cs:291-...); 900 = SDR-1000.
        int  rigId{900};
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
    };

    CatProtocol() = default;

    void setzeZustand(const Zustand& z) { m_z = z; }
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

    /// Fehlerantwort. From Thetis CAT/CATParser.cs:56 -- Error1 = "?;".
    static QString abgelehnt() { return QStringLiteral("?;"); }

    /// Hilfen, die auch die Verdrahtung braucht (und die Prueffaelle).
    static QString mitNullen(quint64 wert, int stellen);
    /// Das 4-Zeichen-Schrittfeld des Statussatzes.
    static QString schrittFeld(int stepHz);
    /// Das 6-Zeichen-Feld fuer RIT/XIT: Vorzeichen + 5 Stellen.
    /// From Thetis CAT/CATCommands.cs IF() -- incr.
    static QString itFeld(int hz);

    /// Hat ein Anrufer AI1; gesetzt? Die Schicht merkt es sich; das
    /// Senden von selbst gehoert in den Transport (eigener Gang).
    bool selbstmeldung() const { return m_selbstmeldung; }

private:
    Zustand m_z;
    Haken   m_h;
    bool    m_selbstmeldung{false};
};

} // namespace Longpath
