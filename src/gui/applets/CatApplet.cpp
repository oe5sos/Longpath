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
//   2026-10-11 — TCP-Schalter verdrahtet (CatServer).
//   2026-10-12 — PTY-Schalter verdrahtet (CatPtyServer); die vier
//                 Pfadbeschriftungen zeigen die echten Geraetepfade.
// =================================================================

#include "CatApplet.h"
#include "gui/styles/ThemeQss.h"
#include "core/AppSettings.h"
#include "core/CatPtyServer.h"
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
    // Auswaehlbar (2026-10-12): in diesen Beschriftungen steht ein Pfad,
    // den der Betreiber in WSJT-X oder fldigi EINTRAGEN muss. Etwas, das
    // man abtippen soll, soll man kopieren koennen.
    lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
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
            // Vorgabetext: ein Strich, KEIN erfundener Pfad.
            // Hier stand bis 2026-10-12 "/dev/ptyp0".."/dev/ptyp3" --
            // BSD-Altnamen, die es auf macOS und Linux nicht gibt. Ein
            // Pfad, den niemand oeffnen kann, ist schlimmer als kein
            // Pfad: man sucht den Fehler im Fremdprogramm.
            m_ptyPath[i] = makePathLabel(QStringLiteral("—"), this);
            row->addWidget(m_ptyPath[i]);
        }
        row->addStretch();

        vbox->addLayout(row);
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
// PTY ist seit 2026-10-12 verdrahtet (vierter Gang, CatPtyServer): vier
// Pseudoterminals, die sich wie serielle Schnittstellen verhalten, mit
// einer festen Verknuepfung je Kanal, damit der Pfad einen Neustart
// ueberlebt.
//
// VAX und IQ bleiben Attrappen und behalten ihre NyiOverlay-Marke: die
// gehoeren in den Tonbereich und haben mit CAT nichts zu tun.

void CatApplet::setzeCatServer(CatServer* srv)
{
    m_cat = srv;
    if (!m_tcpBtn) { return; }

    // Ohne Server bleibt der Schalter GESPERRT, nicht bloss wirkungslos.
    // Ein Schalter, der sich druecken laesst und nichts tut, ist genau
    // die Art Oberflaeche, die dieses Feld bis heute war.
    m_tcpBtn->setEnabled(srv != nullptr);
    if (!srv) {
        m_tcpBtn->setToolTip(QStringLiteral("Kein CAT-Server vorhanden"));
        return;
    }

    m_tcpBtn->setToolTip(
        QStringLiteral("CAT ueber TCP auf 127.0.0.1:%1..%2 (vier Kanaele)")
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

void CatApplet::setzeCatPtyServer(CatPtyServer* srv)
{
    m_catPty = srv;
    if (!m_ptyBtn) { return; }

    // Gesperrt, nicht bloss wirkungslos -- und mit dem GRUND im
    // Hinweistext. Ein Schalter, der sich druecken laesst und nichts
    // tut, ist genau die Art Oberflaeche, die dieses Feld bis gestern
    // war.
    const bool geht = srv && CatPtyServer::verfuegbar();
    m_ptyBtn->setEnabled(geht);
    if (!geht) {
        m_ptyBtn->setToolTip(srv
            ? QStringLiteral(
                  "Pseudoterminals gibt es auf dieser Plattform nicht. "
                  "WSJT-X, fldigi und hamlib koennen stattdessen den "
                  "TCP-Weg (Schalter darueber).")
            : QStringLiteral("Kein PTY-Server vorhanden"));
        zeigePfade();
        return;
    }

    m_ptyBtn->setToolTip(QStringLiteral(
        "CAT als serielle Schnittstelle: vier Pseudoterminals.\n"
        "Einzutragen ist die feste Verknuepfung %1 .. %2 -- die bleibt "
        "ueber Neustarts gleich.\nDer Geraetepfad darunter (/dev/tty...) "
        "wechselt bei jedem Start.")
            .arg(CatPtyServer::festerPfad(0),
                 CatPtyServer::festerPfad(CatPtyServer::kKanaele - 1)));

    connect(m_ptyBtn, &QPushButton::toggled, this, [this](bool an) {
        if (!m_catPty) { return; }
        if (an) {
            int gestartet = 0;
            for (int k = 0; k < CatPtyServer::kKanaele; ++k) {
                if (m_catPty->starte(k)) { ++gestartet; }
            }
            if (gestartet == 0) {
                // Kein einziges Pseudoterminal zu haben: den Schalter
                // zuruecknehmen, statt "an" zu zeigen, waehrend es
                // nichts gibt, was ein Fremdprogramm oeffnen koennte.
                QSignalBlocker b(m_ptyBtn);
                m_ptyBtn->setChecked(false);
            }
        } else {
            for (int k = 0; k < CatPtyServer::kKanaele; ++k) {
                m_catPty->beende(k);
            }
        }
        zeigePfade();
    });

    connect(srv, &CatPtyServer::pfadeGeaendert, this, [this]() { zeigePfade(); });
    connect(srv, &CatPtyServer::kanalAusgefallen, this, [this](int, QString) {
        zeigePfade();
        // Faellt der LETZTE Kanal aus, soll der Schalter nicht "an"
        // bleiben: sonst sieht es aus, als liefe die Anbindung.
        bool nochEiner = false;
        for (int k = 0; k < CatPtyServer::kKanaele; ++k) {
            if (m_catPty && m_catPty->laeuft(k)) { nochEiner = true; break; }
        }
        if (!nochEiner && m_ptyBtn && m_ptyBtn->isChecked()) {
            QSignalBlocker b(m_ptyBtn);
            m_ptyBtn->setChecked(false);
        }
    });

    // Den Schalterzustand MERKEN. Das Wiederherstellen macht
    // stelleZustandWiederHer(), erst wenn das Fenster steht -- siehe die
    // Notiz beim TCP-Schalter: der Aufbau eines Applets darf keine
    // Nebenwirkungen haben, die Geraete anlegen und Einstellungen
    // schreiben.
    connect(m_ptyBtn, &QPushButton::toggled, this, [](bool an) {
        AppSettings::instance().setValue(
            QStringLiteral("CatPtyEnabled"),
            an ? QStringLiteral("True") : QStringLiteral("False"));
    });

    zeigePfade();
}

void CatApplet::zeigePfade()
{
    for (int k = 0; k < 4; ++k) {
        QLabel* lbl = m_ptyPath[k];
        if (!lbl) { continue; }
        const bool laeuft = m_catPty && m_catPty->laeuft(k);
        if (!laeuft) {
            lbl->setText(QStringLiteral("—"));
            const QString grund = m_catPty ? m_catPty->grund(k) : QString();
            lbl->setToolTip(grund.isEmpty()
                ? QStringLiteral("Kanal %1 ist aus")
                      .arg(QChar(QLatin1Char('A' + k)))
                // Den Grund ANZEIGEN, nicht nur ins Protokoll schreiben.
                // Wer den Schalter drueckt und einen Strich sieht, hat
                // sonst keinen Anhaltspunkt.
                : QStringLiteral("Kanal %1: %2")
                      .arg(QChar(QLatin1Char('A' + k)), grund));
            continue;
        }

        const QString pfad = m_catPty->pfad(k);
        // In der Zeile steht nur der kurze Geraetename: vier ganze Pfade
        // nebeneinander passen in die Breite eines Applets nicht, und
        // der Platzhalter vorher war mit "/dev/ptyp0" sogar laenger.
        // Alles Lange steht im Hinweistext, und der Text ist auswaehlbar.
        lbl->setText(pfad.section(QLatin1Char('/'), -1));
        lbl->setToolTip(QStringLiteral(
            "Kanal %1\nGeraet jetzt: %2\nFest eintragen: %3\n\n"
            "Der Geraetepfad wechselt bei jedem Start von Longpath. In "
            "WSJT-X, fldigi oder im Logbuch die FESTE Zeile eintragen -- "
            "die zeigt immer auf das aktuelle Geraet.")
                .arg(QChar(QLatin1Char('A' + k)), pfad,
                     CatPtyServer::festerPfad(k)));
    }
}

void CatApplet::stelleZustandWiederHer()
{
    // Der PTY-Schalter zuerst, und UNABHAENGIG: die beiden Wege haengen
    // nicht aneinander, und ein fehlender TCP-Server darf nicht dazu
    // fuehren, dass die serielle Haelfte stumm bleibt. (Der erste
    // Entwurf hatte beides hinter einem gemeinsamen `return`.)
    if (m_ptyBtn && m_catPty && m_ptyBtn->isEnabled()
        && !m_ptyBtn->isChecked()
        && AppSettings::instance()
               .value(QStringLiteral("CatPtyEnabled"),
                      QStringLiteral("False"))
               .toString() == QStringLiteral("True")) {
        m_ptyBtn->setChecked(true);
    }

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
            ? QStringLiteral("Kanal %1 horcht auf Port %2, %3 Verbindung(en)")
                  .arg(QChar(QLatin1Char('A' + k))).arg(m_cat->port(k)).arg(n)
            : QStringLiteral("Kanal %1 ist aus")
                  .arg(QChar(QLatin1Char('A' + k))));
    }
}

void CatApplet::syncFromModel()
{
    // NYI — Phase 3K / 3-VAX
}

} // namespace Longpath
