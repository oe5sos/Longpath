// =================================================================
// src/gui/applets/DvkApplet.cpp  (NereusSDR)
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
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Port of AetherSDR `src/gui/DvkPanel.{h,cpp}` (DVK F-key
//                 slot grid + record/play controls). Renamed to
//                 DvkApplet in NereusSDR. All controls NYI.
// =================================================================

#include "DvkApplet.h"
#include "models/RadioModel.h"
#include <QMessageBox>
#include <QFileDialog>
#include "NyiOverlay.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>

namespace NereusSDR {

DvkApplet::DvkApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
}

// Eine Zeile auffrischen: Taste, Beschriftung, Dauer.
void DvkApplet::refreshSlot(int index)
{
    if (index < 0 || index >= kSlots || !m_model) { return; }
    const VoiceKeyerSlot& s = m_model->voiceKeyer().slot(index);

    if (m_slotKey[index]) {
        m_slotKey[index]->setText(s.shortcut);
    }
    if (m_slotLabel[index]) {
        // Leerer Platz sagt das auch. „Slot 3" allein laesst offen, ob
        // da etwas liegt.
        const QString name = s.label.isEmpty()
            ? QStringLiteral("Slot %1").arg(index + 1) : s.label;
        m_slotLabel[index]->setText(s.isEmpty()
            ? QStringLiteral("%1 — empty").arg(name)
            : QStringLiteral("%1 · %2 s").arg(name)
                  .arg(s.seconds, 0, 'f', 1));
    }
}

void DvkApplet::buildUI()
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

    // --- Die zehn Plaetze ---------------------------------------
    //
    // Was hier WIRKT und was nicht, und warum das so dasteht:
    //
    //   Laden (WAV)  — wirkt. Reine Dateiarbeit, kein Funkgeraet noetig.
    //   Beschriftung — wirkt, per Doppelklick auf den Namen.
    //   Rec / Play   — brauchen den Sendeweg (Mikrofonabgriff bzw.
    //                  Einspeisung samt Sendetastung) und sind bis dahin
    //                  gesperrt, MIT Begruendung im Tooltip.
    //
    // Ein gesperrter Knopf ohne Erklaerung ist ein Versprechen, das die
    // Anwendung nicht haelt — davon haben wir 72 im Setup, das reicht.
    for (int i = 0; i < kSlots; ++i) {
        auto* row = new QHBoxLayout;
        row->setSpacing(3);

        m_slotKey[i] = new QLabel(this);
        m_slotKey[i]->setFixedWidth(26);
        m_slotKey[i]->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; font-weight: bold; }")
                .arg(Style::kAmberText));
        row->addWidget(m_slotKey[i]);

        m_slotLabel[i] = new QLabel(this);
        m_slotLabel[i]->setMinimumWidth(70);
        m_slotLabel[i]->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; }").arg(Style::kTextSecondary));
        m_slotLabel[i]->setToolTip(QStringLiteral(
            "Double-click to name this announcement (CQ, 73, callsign …)."));
        row->addWidget(m_slotLabel[i], 1);

        m_recBtn[i]  = styledButton(QStringLiteral("\u25CF"));
        m_playBtn[i] = styledButton(QStringLiteral("\u25BA"));
        m_stopBtn[i] = styledButton(QStringLiteral("\u25A0"));
        m_loadBtn[i] = styledButton(QStringLiteral("WAV"));

        m_recBtn[i]->setToolTip(QStringLiteral(
            "Record from the microphone — needs the transmit path, which "
            "is the next step. Load a WAV in the meantime."));
        m_playBtn[i]->setToolTip(QStringLiteral(
            "Send this announcement — needs the transmit path, which is "
            "the next step."));
        m_loadBtn[i]->setToolTip(QStringLiteral(
            "Load a WAV file into this slot. Works now: the file is read "
            "and checked immediately, not when you press send."));

        row->addWidget(m_recBtn[i]);
        row->addWidget(m_playBtn[i]);
        row->addWidget(m_stopBtn[i]);
        row->addWidget(m_loadBtn[i]);

        vbox->addLayout(row);

        // Bis der Sendeweg haengt: gesperrt, aber mit Grund im Tooltip.
        m_recBtn[i]->setEnabled(false);
        m_playBtn[i]->setEnabled(false);
        m_stopBtn[i]->setEnabled(false);

        const int slot = i;
        connect(m_loadBtn[i], &QPushButton::clicked, this, [this, slot]() {
            if (!m_model) { return; }
            const QString path = QFileDialog::getOpenFileName(
                this, QStringLiteral("Load announcement"), QString{},
                QStringLiteral("WAV files (*.wav)"));
            if (path.isEmpty()) { return; }

            QString err;
            if (!m_model->voiceKeyer().importFile(slot, path, &err)) {
                QMessageBox::warning(this, QStringLiteral("Voice Keyer"),
                    QStringLiteral("Could not use that file:\n%1").arg(err));
                return;
            }
            refreshSlot(slot);
        });
    }

    if (m_model) {
        connect(&m_model->voiceKeyer(), &VoiceKeyerStore::slotChanged,
                this, &DvkApplet::refreshSlot);
    }

    // --- Control 2: Record level gauge (0-100) ---
    m_recLevel = new HGauge(this);
    m_recLevel->setRange(0.0, 100.0);
    m_recLevel->setYellowStart(70.0);
    m_recLevel->setRedStart(90.0);
    m_recLevel->setTitle(QStringLiteral("Rec Level"));
    vbox->addWidget(m_recLevel);
    NyiOverlay::markNyi(m_recLevel, QStringLiteral("3I-1"));

    // --- Control 3: Repeat toggle + interval slider (1..60s) ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_repeatBtn = greenToggle(QStringLiteral("Rpt"));
        m_repeatBtn->setCheckable(true);
        row->addWidget(m_repeatBtn);

        m_repeatSlider = new QSlider(Qt::Horizontal, this);
        m_repeatSlider->setRange(1, 60);
        m_repeatSlider->setValue(5);
        m_repeatSlider->setFixedHeight(18);
        row->addWidget(m_repeatSlider, 1);

        m_repeatValue = insetValue(QStringLiteral("5"));
        row->addWidget(m_repeatValue);

        vbox->addLayout(row);

        NyiOverlay::markNyi(m_repeatBtn,    QStringLiteral("3I-1"));
        NyiOverlay::markNyi(m_repeatSlider, QStringLiteral("3I-1"));
    }

    // --- Control 4: Semi break-in toggle ---
    m_semiBkBtn = greenToggle(QStringLiteral("Semi BK"));
    m_semiBkBtn->setCheckable(true);
    vbox->addWidget(m_semiBkBtn);
    NyiOverlay::markNyi(m_semiBkBtn, QStringLiteral("3I-1"));

    // --- Control 5: WAV file import button ---
    m_importBtn = styledButton(QStringLiteral("Import WAV\u2026"));
    vbox->addWidget(m_importBtn);
    NyiOverlay::markNyi(m_importBtn, QStringLiteral("3I-1"));

    vbox->addStretch();
    root->addWidget(body);
}

void DvkApplet::syncFromModel()
{
    // Die zehn Zeilen aus dem Speicher auffrischen. Aufnahme und
    // Wiedergabe bleiben gesperrt, bis der Sendeweg haengt — siehe die
    // Tooltips an den Knoepfen, die den Grund nennen.
    for (int i = 0; i < kSlots; ++i) {
        refreshSlot(i);
    }
}

} // namespace NereusSDR
