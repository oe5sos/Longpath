// =================================================================
// src/core/CatPuffer.cpp  (Longpath)
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

// =================================================================
// Der Kopf spiegelt CatPuffer.h -- dort steht auch, warum die
// Zerlegung aus CatServer.cpp herausgehoben wurde.
// =================================================================

#include "CatPuffer.h"

namespace Longpath {

QStringList CatPuffer::nimm(const QString& zeichen)
{
    m_verworfen = false;
    m_puffer.append(zeichen);

    // From Thetis CAT/TCPIPcatServer.cs:275-296 [v2.10.3.15] --
    // ParseReceiveBuffer, am 2.10.3.9 (MW0LGE) auf mehrere Befehle in
    // einem Paket gefixt:
    //   m_oneLineBuf.Replace(Environment.NewLine, "");
    //     // so can be used a by raw telnet client
    //   while (lineEndIndex > -1) { msg = ...Substring(0, idx+1); ... }
    //   if (m_oneLineBuf.Length > 255) m_oneLineBuf.Clear();
    //
    // Zeilenumbrueche fliegen raus, DAMIT ein rohes telnet geht -- wer
    // dort "FA;" eintippt, schickt "FA;\r\n", und ohne diese Zeile
    // steckte das \r\n vor dem naechsten Befehl.
    //
    // Am Pseudoterminal gilt dasselbe aus einem zweiten Grund: wer
    // `echo "FA;" > /dev/ttys003` tippt, schickt ein \n hinterher, und
    // ein Terminalprogramm wie `screen` schickt beim Druck auf die
    // Eingabetaste ein \r. Thetis' serieller Weg
    // (SIOListenerII.cs:199-248 ParseString) wirft sie NICHT weg -- dort
    // sitzt hinter dem Kanal immer ein Fremdprogramm, das genau das
    // Protokoll spricht. Longpath geht davon aus, dass auch ein Mensch
    // einmal hineintippt.
    m_puffer.remove(QLatin1Char('\r'));
    m_puffer.remove(QLatin1Char('\n'));

    QStringList befehle;
    int ende = m_puffer.indexOf(QLatin1Char(';'));
    while (ende >= 0) {
        befehle.append(m_puffer.left(ende + 1));
        m_puffer.remove(0, ende + 1);
        ende = m_puffer.indexOf(QLatin1Char(';'));
    }

    if (m_puffer.size() > kDeckel) {
        m_puffer.clear();
        m_verworfen = true;
    }
    return befehle;
}

} // namespace Longpath
