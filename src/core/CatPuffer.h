// =================================================================
// src/core/CatPuffer.h  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/CAT/TCPIPcatServer.cs [v2.10.3.15]
//     — ParseReceiveBuffer (275-296)
//   Project Files/Source/Console/CAT/SIOListenerII.cs  [v2.10.3.15]
//     — ParseString (199-248), SerialRXEventHandler (274-322)
// Original licences from the Thetis sources are included below.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-12 — Aus CatServer.cpp herausgehoben, AI-assisted via
//                 Anthropic Claude Code. Grund: der PTY-Weg braucht
//                 dieselbe Zerlegung wie der TCP-Weg. Thetis hat sie
//                 ZWEIMAL (TCPIPcatServer.ParseReceiveBuffer fuer das
//                 Netz, SIOListenerII.ParseString bzw.
//                 SerialRXEventHandler fuer die seriellen Kanaele) und
//                 die beiden Fassungen sind auseinandergelaufen -- die
//                 serielle kennt den Deckel bei 255 nicht und wirft die
//                 Zeilenumbrueche nicht weg. Hier steht sie einmal,
//                 damit niemand eine aendert und die andere vergisst
//                 (dieselbe Begruendung wie bei CatModeMap).
//
//                 Abweichung von Thetis mit Absicht: `nimm()` GIBT die
//                 vollstaendigen Befehle zurueck, statt sie in einer
//                 Schleife selbst abzuarbeiten. Thetis ruft den Parser
//                 mitten in der Schleife und greift danach wieder in
//                 den Puffer -- beim TCP-Weg hiess das, dass ein
//                 Fremdprogramm, das sich auf die Antwort hin trennt,
//                 den Puffer unter der laufenden Schleife wegzieht. Der
//                 alte Longpath-Code hat das mit einem erneuten
//                 Nachschlagen in der Hashtabelle abgefangen; wer die
//                 Befehle erst herausgibt und dann abarbeitet, hat das
//                 Problem nicht.
// =================================================================
//
//=================================================================
// TCPIPcatServer.cs
//=================================================================
// Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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
//
//=================================================================
// SIOListener.cs
//=================================================================
// Copyright (C) 2005  Bob Tracy
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

#pragma once

#include <QString>
#include <QStringList>

namespace Longpath {

/// Der Befehlspuffer einer CAT-Verbindung: Bytes hinein, vollstaendige
/// Befehle heraus.
///
/// Ein Puffer je Verbindung -- das ist keine Feinheit, sondern der Kern:
/// CAT kommt als Strom, und ein Befehl darf ueber zwei Pakete verteilt
/// ankommen, waehrend im selben Paket schon der naechste steht. Wer je
/// Lesevorgang einen Befehl erwartet, verliert beides.
class CatPuffer {
public:
    /// Rohe Zeichen anhaengen und alle Befehle herausgeben, die damit
    /// vollstaendig geworden sind (jeder einschliesslich seines ';').
    /// Was danach noch im Puffer liegt, bleibt fuer den naechsten Aufruf.
    QStringList nimm(const QString& zeichen);

    /// Was noch unvollstaendig im Puffer liegt (fuer Prueffaelle).
    int offen() const { return static_cast<int>(m_puffer.size()); }
    void leere() { m_puffer.clear(); }

    /// Hat der letzte `nimm()` den Puffer wegen Ueberlaenge verworfen?
    /// Der Aufrufer soll das melden koennen, statt es zu verschweigen.
    bool verworfen() const { return m_verworfen; }

    /// From Thetis CAT/TCPIPcatServer.cs:295 [v2.10.3.15]:
    ///   if (m_oneLineBuf.Length > 255) m_oneLineBuf.Clear();
    ///   // not likely to be a cat message if it got this long
    /// Ohne diesen Deckel haelt ein Programm, das nie ein ';' schickt,
    /// den Puffer offen, bis der Speicher ausgeht.
    ///
    /// Thetis' SERIELLER Weg (SIOListenerII.ParseString) hat den Deckel
    /// NICHT -- dort waechst `ParseLeftover` unbegrenzt. Longpath nimmt
    /// den Deckel fuer beide Wege: ein Terminalprogramm, das jemand
    /// versehentlich auf das Pseudoterminal richtet, schickt genau so
    /// einen endlosen Strom ohne ';'.
    static constexpr int kDeckel = 255;

private:
    QString m_puffer;
    bool    m_verworfen{false};
};

} // namespace Longpath
