// =================================================================
// src/gui/applets/CatApplet.h  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-18 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout mirrors AetherSDR `src/gui/CatApplet.{h,cpp}`
//                 (serial CAT / rigctld enable rows + PTT LEDs).
//                 All controls NYI — wired in later phase.
//   2026-05-10 — Phase 24 (Task 24.1): stripped TCI button row; TCI
//                 controls now live in TciApplet (Phase 21, 0b615a7).
// =================================================================
//
// --- From CAT/TCPIPcatServer.cs (Thetis) ---------------------------
//
// Ported from Thetis sources:
//   Project Files/Source/Console/CAT/TCPIPcatServer.cs [v2.10.3.15]
//     — line 442: `public static int DEFAULT_PORT = 31001;`
//
// Das ist ALLES, was aus Thetis in diese Datei kommt: eine Portnummer.
// Die vier CAT-Kanaele liegen darauf aufsteigend (kErsterPort). Das Feld
// selbst ist von AetherSDR abgeleitet, siehe den Kopf darueber.
//
// Der Lizenzkopf steht hier trotzdem vollstaendig: TCPIPcatServer.cs ist
// von Richard Samphire (MW0LGE) und traegt darum die
// Doppellizenzklausel. Eine Portnummer ist eine Tatsache und kein Werk
// -- die Herkunftszeile in THETIS-PROVENANCE.md fuehrt sie als
// `reference`, nicht als `port`. Der Kopf bleibt, weil
// verify-thetis-headers.py JEDE gelistete Datei prueft, und weil die
// Spur mehr wert ist als die gesparten Zeilen.
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

#pragma once
#include "AppletWidget.h"

class QPushButton;
class QLabel;
class QComboBox;

namespace Longpath {

// CAT / rigctld control interfaces.
// NYI — Phase 3K (CAT/rigctld) + 3-VAX (VAX/IQ).
// TCI controls live in TciApplet (Phase 21).
//
// Controls:
//   1. CAT TCP enable + LEDs — QPushButton green "TCP" + 4x QLabel (A/B/C/D)
//   2. CAT PTY enable + paths — QPushButton green "PTY" + 4x QLabel paths
//   3. VAX enable + meters — QPushButton green "VAX" + 4x QLabel channel status
//   4. VAX IQ enable + rate — QPushButton green "IQ" + QComboBox rate
class CatApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit CatApplet(RadioModel* model, QWidget* parent = nullptr);

    /// Eigenkennung == Panelkennung "Cat" (2026-10-13). Vorher
    /// wichen die beiden ab, und genau daran ging am 2026-08-18
    /// das Abloesen des RX-Panels kaputt -- check-applet-ids.py
    /// bewacht es seitdem und hat es hier angezeigt, sobald das
    /// Applet in m_appletsById auftauchte. Gespeicherte
    /// Anordnungen mit der alten Schreibweise liest
    /// AppletKeys::appletFor weiterhin (dort Zeile 49-52).
    QString appletId()    const override { return QStringLiteral("Cat"); }

    /// Den CAT-Server anhaengen (2026-10-11). Ohne ihn bleibt der
    /// TCP-Schalter gesperrt statt zu luegen.
    void setzeCatServer(class CatServer* srv);

    /// Den gespeicherten Schalterzustand anwenden (2026-10-11).
    /// Getrennt von setzeCatServer, weil das Binden von Ports und das
    /// Schreiben von Einstellungen nicht in den Aufbau eines Applets
    /// gehoeren -- der erste Versuch beendete damit den naechsten
    /// Fensteraufbau mit SIGTRAP.
    void stelleZustandWiederHer();
    QString appletTitle() const override { return QStringLiteral("CAT"); }
    void    syncFromModel() override;

private:
    void buildUI();

    // Control 1 — CAT TCP: enable button + 4 status LEDs (A/B/C/D)
    QPushButton* m_tcpBtn        = nullptr;
    QLabel*      m_tcpLed[4]     = {};

    // Control 2 — CAT PTY: enable button + 4 path labels
    QPushButton* m_ptyBtn        = nullptr;
    QLabel*      m_ptyPath[4]    = {};

    // Control 3 — VAX: enable button + 4 channel status labels
    QPushButton* m_vaxBtn        = nullptr;
    QLabel*      m_vaxStatus[4]  = {};

    // Control 5 — VAX IQ: enable button + rate combo
    QPushButton* m_iqBtn         = nullptr;
    QComboBox*   m_iqRateCombo   = nullptr;

    // Verdrahtung 2026-10-11
    class CatServer* m_cat = nullptr;
    void zeigeKanaele();
    /// From Thetis CAT/TCPIPcatServer.cs:442 [v2.10.3.15] --
    /// `public static int DEFAULT_PORT = 31001;`
    /// Die vier Kanaele liegen darauf aufsteigend.
    static constexpr quint16 kErsterPort = 31001;
};

} // namespace Longpath
