// =================================================================
// src/gui/applets/CatApplet.cpp  (Longpath)
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

#include "CatApplet.h"
#include "gui/styles/ThemeQss.h"
#include "core/AppSettings.h"
#include "core/CatServer.h"
#include "NyiOverlay.h"
#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>

namespace Longpath {

static QLabel* makeLed(const QString& name, QWidget* parent)
{
    auto* led = new QLabel(name, parent);
    led->setFixedSize(24, 14);
    led->setAlignment(Qt::AlignCenter);
    // Ein Glaschip (versenkt, Lichtkante unten) statt der flachen
    // Kapsel in Grau — Glas & Tiefe, Applets zweite Runde (2026-09-18).
    led->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %3;"
        " border-top-color: %4; border-bottom-color: %5; border-radius: 6px;"
        " font-size: 9px; font-weight: bold; }")
        .arg(Style::sunkenFill(Style::kInsetBg, 8, 4),
             Style::hexRole(Style::kTextSecondary),
             Style::hexRole(Style::kBorder),
             QLatin1String(Style::kGlassShade),
             QLatin1String(Style::kGlassLight)));
    return led;
}

static QLabel* makePathLabel(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 9px; }").arg(Style::kTextSecondary));
    return lbl;
}

CatApplet::CatApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
}

void CatApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    // Do NOT add appletTitleBar() here — AppletPanelWidget::wrapWithTitleBar
    // already prepends a host-side title bar from appletTitle(). Adding our
    // own here results in a double header. Same fix in PureSignalApplet.

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // --- Control 1: CAT TCP enable + 4 status LEDs (A/B/C/D) ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_tcpBtn = greenToggle(QStringLiteral("TCP"));
        m_tcpBtn->setCheckable(true);
        row->addWidget(m_tcpBtn);

        const QString ledNames[4] = {
            QStringLiteral("A"), QStringLiteral("B"),
            QStringLiteral("C"), QStringLiteral("D")
        };
        for (int i = 0; i < 4; ++i) {
            m_tcpLed[i] = makeLed(ledNames[i], this);
            row->addWidget(m_tcpLed[i]);
        }
        row->addStretch();

        vbox->addLayout(row);
    }

    // --- Control 2: CAT PTY enable + 4 path labels ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_ptyBtn = greenToggle(QStringLiteral("PTY"));
        m_ptyBtn->setCheckable(true);
        row->addWidget(m_ptyBtn);

        for (int i = 0; i < 4; ++i) {
            m_ptyPath[i] = makePathLabel(
                QStringLiteral("/dev/ptyp%1").arg(i), this);
            row->addWidget(m_ptyPath[i]);
        }
        row->addStretch();

        vbox->addLayout(row);
        NyiOverlay::markNyi(m_ptyBtn, QStringLiteral("3K"));
    }

    vbox->addWidget(divider());

    // --- Control 3: VAX enable + 4 channel status labels ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_vaxBtn = greenToggle(QStringLiteral("VAX"));
        m_vaxBtn->setCheckable(true);
        row->addWidget(m_vaxBtn);

        for (int i = 0; i < 4; ++i) {
            m_vaxStatus[i] = makeLed(QStringLiteral("Ch%1").arg(i + 1), this);
            row->addWidget(m_vaxStatus[i]);
        }
        row->addStretch();

        vbox->addLayout(row);
        NyiOverlay::markNyi(m_vaxBtn, QStringLiteral("3-VAX"));
    }

    // --- Control 4: VAX IQ enable + rate combo ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_iqBtn = greenToggle(QStringLiteral("IQ"));
        m_iqBtn->setCheckable(true);
        row->addWidget(m_iqBtn);

        m_iqRateCombo = new QComboBox(this);
        m_iqRateCombo->addItems({
            QStringLiteral("48000"),
            QStringLiteral("96000"),
            QStringLiteral("192000")
        });
        applyComboStyle(m_iqRateCombo);
        row->addWidget(m_iqRateCombo, 1);

        vbox->addLayout(row);

        NyiOverlay::markNyi(m_iqBtn,       QStringLiteral("3-VAX"));
        NyiOverlay::markNyi(m_iqRateCombo, QStringLiteral("3-VAX"));
    }

    vbox->addStretch();
    root->addWidget(body);
}

// ── Verdrahtung des TCP-Schalters (2026-10-11) ───────────────────────────────
//
// Longpath war fuer Fremdprogramme stumm. Der Schalter hier startet die
// vier CAT-Kanaele auf 31001..31004 (Thetis' Vorgabeport aufsteigend) --
// nur auf 127.0.0.1, weil CAT den Sender tasten kann.
//
// PTY, VAX und IQ bleiben Attrappen und behalten ihre NyiOverlay-Marke:
// PTY braucht Pseudoterminals (eigener Gang), VAX und IQ gehoeren in den
// Tonbereich und haben mit CAT nichts zu tun.

void CatApplet::setzeCatServer(CatServer* srv)
{
    m_cat = srv;
    if (!m_tcpBtn) { return; }

    // Ohne Server bleibt der Schalter GESPERRT, nicht bloss wirkungslos.
    // Ein Schalter, der sich druecken laesst und nichts tut, ist genau
    // die Art Oberflaeche, die dieses Feld bis heute war.
    m_tcpBtn->setEnabled(srv != nullptr);
    if (!srv) {
        m_tcpBtn->setToolTip(tr("No CAT server available"));
        return;
    }

    m_tcpBtn->setToolTip(
        tr("CAT over TCP on 127.0.0.1:%1..%2 (four channels)")
            .arg(kErsterPort).arg(kErsterPort + CatServer::kKanaele - 1));

    connect(m_tcpBtn, &QPushButton::toggled, this, [this](bool an) {
        if (!m_cat) { return; }
        if (an) {
            int gestartet = 0;
            for (int k = 0; k < CatServer::kKanaele; ++k) {
                if (m_cat->starte(k, static_cast<quint16>(kErsterPort + k))) {
                    ++gestartet;
                }
            }
            if (gestartet == 0) {
                // Alle vier Ports belegt: den Schalter zuruecknehmen,
                // statt "an" zu zeigen, waehrend niemand horcht.
                QSignalBlocker b(m_tcpBtn);
                m_tcpBtn->setChecked(false);
            }
        } else {
            for (int k = 0; k < CatServer::kKanaele; ++k) { m_cat->beende(k); }
        }
        zeigeKanaele();
    });

    connect(srv, &CatServer::verbindungenGeaendert,
            this, [this](int, int) { zeigeKanaele(); });

    // Den Schalterzustand MERKEN -- das Wiederherstellen macht
    // stelleZustandWiederHer(), und zwar erst, wenn das Fenster steht.
    //
    // Hier im Aufbau ein setChecked() zu rufen, hat beim ersten Versuch
    // den naechsten Fensteraufbau mit SIGTRAP beendet: der Aufbau eines
    // Applets soll keine Nebenwirkungen haben, die Kanaele binden und
    // Einstellungen schreiben, waehrend MainWindow noch nicht fertig ist.
    connect(m_tcpBtn, &QPushButton::toggled, this, [](bool an) {
        AppSettings::instance().setValue(
            QStringLiteral("CatTcpEnabled"),
            an ? QStringLiteral("True") : QStringLiteral("False"));
    });

    zeigeKanaele();
}

void CatApplet::stelleZustandWiederHer()
{
    if (!m_tcpBtn || !m_cat) { return; }
    // Ohne das muesste der Betreiber nach jedem Start klicken, bevor
    // WSJT-X oder fldigi das Geraet finden -- und waehrend er das sucht,
    // sieht es aus wie ein Programm, das die Anbindung verloren hat.
    //
    // Der Schluessel liegt NICHT per Geraet (hardware/<mac>/): CAT ist
    // eine Eigenschaft des Arbeitsplatzes, nicht des Funkgeraets.
    const bool an = AppSettings::instance()
                        .value(QStringLiteral("CatTcpEnabled"),
                               QStringLiteral("False"))
                        .toString() == QStringLiteral("True");
    if (!an || m_tcpBtn->isChecked()) { return; }
    // setChecked loest das toggled-Signal aus, das die Kanaele startet --
    // und sich selbst zuruecknimmt, wenn kein Port zu haben ist.
    m_tcpBtn->setChecked(true);
}

void CatApplet::zeigeKanaele()
{
    for (int k = 0; k < 4; ++k) {
        QLabel* led = m_tcpLed[k];
        if (!led) { continue; }
        const bool laeuft = m_cat && m_cat->laeuft(k);
        const int  n      = m_cat ? m_cat->verbindungen(k) : 0;
        // Die Beschriftung bleibt A/B/C/D; die Anzahl der Verbindungen
        // kommt als Zusatz dazu, damit man sieht, WER haengt und nicht
        // nur, dass der Kanal offen ist.
        led->setText(n > 0 ? QStringLiteral("%1:%2")
                                 .arg(QChar(QLatin1Char('A' + k))).arg(n)
                           : QString(QChar(QLatin1Char('A' + k))));
        led->setEnabled(laeuft);
        led->setToolTip(laeuft
            ? tr("Channel %1 listening on port %2, %3 connection(s)")
                  .arg(QChar(QLatin1Char('A' + k))).arg(m_cat->port(k)).arg(n)
            : tr("Channel %1 is off")
                  .arg(QChar(QLatin1Char('A' + k))));
    }
}

void CatApplet::syncFromModel()
{
    // NYI — Phase 3K / 3-VAX
}

} // namespace Longpath
