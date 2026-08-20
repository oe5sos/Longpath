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
#include "core/TxWorkerThread.h"
#include "core/TxAudioRecorder.h"
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
#include <QFrame>

namespace Longpath {

namespace {
// Mikrofonrate des Sendewegs — dieselbe Zahl, die der Sprach-Selbsttest
// nimmt; beide lesen denselben Abgriff.
constexpr int kMicRateHz = 48000;
}


DvkApplet::DvkApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
}

// ── Aufnahme ─────────────────────────────────────────────────────────
//
// Derselbe Weg wie der Sprach-Selbsttest (TxVoiceCheckDialog): der
// Abgriff preStripAudioReady laeuft auf dem Sendefaden und reicht die
// Mikrofonwerte direkt in einen TxAudioRecorder. VOR der
// Sprachbearbeitung — eine Ansage soll klingen wie die Stimme, nicht
// wie die Stimme durch den heutigen Kompressor; beim Senden laeuft die
// Bearbeitung ohnehin darueber.
//
// ES WIRD NICHTS GETASTET. Der Abgriff liest mit, was das Mikrofon
// liefert; der Sender bleibt aus.
void DvkApplet::startRecording(int index)
{
    if (!m_model || index < 0 || index >= kSlots) { return; }

    TxWorkerThread* worker = m_model->txWorker();
    if (!worker) {
        QMessageBox::information(this, QStringLiteral("Voice Keyer"),
            QStringLiteral("The transmit path is not up yet — connect to "
                           "the radio first, then record."));
        return;
    }

    if (!m_recorder) {
        m_recorder = new TxAudioRecorder(this);
        m_recorder->setSampleRate(kMicRateHz);
        connect(m_recorder, &TxAudioRecorder::recordingFull, this,
                [this]() { finishRecording(); });
    }
    m_recorder->clear();
    m_recorder->start();

    QObject::disconnect(m_micTap);
    m_micTap = connect(worker, &TxWorkerThread::preStripAudioReady, m_recorder,
                       [this](const float* samples, int frames) {
        m_recorder->feed(samples, frames);
    }, Qt::DirectConnection);
    worker->setVoiceTapEnabled(true);

    m_recordingSlot = index;
    applyRowStyle(index);
    if (m_recBtn[index])  { m_recBtn[index]->setText(QStringLiteral("\u25A0")); }
    for (int i = 0; i < kSlots; ++i) {
        if (i != index && m_recBtn[i]) { m_recBtn[i]->setEnabled(false); }
        if (m_loadBtn[i]) { m_loadBtn[i]->setEnabled(false); }
    }
}

void DvkApplet::finishRecording()
{
    if (m_recordingSlot < 0 || !m_recorder) { return; }
    const int index = m_recordingSlot;
    m_recordingSlot = -1;
    applyRowStyle(index);

    // Reihenfolge wie im Sprach-Selbsttest: erst das Tor zu, dann die
    // Verbindung loesen. Andersherum liefe der Abgriff noch, waehrend
    // niemand mehr zuhoert.
    if (m_model && m_model->txWorker()) {
        m_model->txWorker()->setVoiceTapEnabled(false);
    }
    QObject::disconnect(m_micTap);
    m_recorder->stop();

    if (m_recBtn[index]) { m_recBtn[index]->setText(QStringLiteral("\u25CF")); }
    for (int i = 0; i < kSlots; ++i) {
        if (m_recBtn[i])  { m_recBtn[i]->setEnabled(m_model && m_model->txWorker()); }
        if (m_loadBtn[i]) { m_loadBtn[i]->setEnabled(true); }
    }

    if (!m_model) { return; }

    QVector<float> samples(m_recorder->recordedFrames());
    if (!samples.isEmpty()) {
        std::copy_n(m_recorder->samples(), samples.size(), samples.begin());
    }

    QString err;
    if (!m_model->voiceKeyer().setRecording(index, samples, kMicRateHz, &err)) {
        QMessageBox::warning(this, QStringLiteral("Voice Keyer"),
            QStringLiteral("Nothing was stored: %1").arg(err));
        return;
    }
    refreshSlot(index);
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
        // Ein leerer Platz sagt „empty" und sagt es kursiv und blass.
        // „Slot 3" allein laesst offen, ob da etwas liegt.
        const QString name = s.label.isEmpty()
            ? QStringLiteral("Slot %1").arg(index + 1) : s.label;
        m_slotLabel[index]->setText(s.isEmpty()
            ? QStringLiteral("%1 — empty").arg(name) : name);
        m_slotLabel[index]->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; background: transparent;%2 }")
            .arg(QLatin1String(s.isEmpty() ? Style::kTextScale
                                           : Style::kTextPrimary),
                 QLatin1String(s.isEmpty() ? " font-style: italic;" : "")));
    }
    if (m_slotLen[index]) {
        m_slotLen[index]->setText(s.isEmpty()
            ? QStringLiteral("\u2014")
            : QStringLiteral("%1 s").arg(s.seconds, 0, 'f', 1));
    }
}

// Zebra macht zehn Zeilen abzaehlbar; rot sagt, welche gerade aufnimmt.
// Der linke Balken traegt den Zustand noch einmal ohne Farbe.
void DvkApplet::applyRowStyle(int index)
{
    if (index < 0 || index >= kSlots || !m_slotRow[index]) { return; }

    // ── DER SELEKTOR MUSS DEN NAMEN TRAGEN ───────────────────────────
    //
    // Erste Fassung schrieb „QWidget { background: … }" auf die Zeile.
    // Qt-Stilvorlagen KASKADIEREN: der Selektor traf damit nicht nur die
    // Zeile, sondern jedes Kind darin — und ueberschrieb Grund und Rand
    // der vier Knoepfe und der Beschriftungen gleich mit.
    //
    // Ergebnis, gesehen am 2026-08-20 an der laufenden Anwendung: zehn
    // Zeilen, in denen NUR noch das kleine Tastenfeld stand. Name,
    // Dauer, Aufnahme, Wiedergabe, Stopp, WAV — alles unsichtbar. Es
    // war da, es war anklickbar, man sah es nur nicht.
    //
    // „QWidget#dvkSlotRow" trifft die Zeile und sonst nichts.
    const QString sel = QStringLiteral("QWidget#dvkSlotRow");

    if (index == m_recordingSlot) {
        m_slotRow[index]->setStyleSheet(
            sel + QStringLiteral(" { background: %1;"
                                 " border-left: 2px solid %2; }")
                      .arg(QLatin1String(Style::kInsetBg),
                           QLatin1String(Style::kRedBg)));
    } else if (index % 2 == 1) {
        m_slotRow[index]->setStyleSheet(
            sel + QStringLiteral(" { background: %1;"
                                 " border-left: 2px solid transparent; }")
                      .arg(QLatin1String(Style::kPanelBg)));
    } else {
        m_slotRow[index]->setStyleSheet(
            sel + QStringLiteral(" { background: transparent;"
                                 " border-left: 2px solid transparent; }"));
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
        // Die Zeile ist ein eigenes Widget, nicht bloss eine Anordnung:
        // nur so kann sie einen Grund tragen. Zebra macht zehn Zeilen
        // abzaehlbar, und die laufende Aufnahme faerbt die GANZE Zeile —
        // ein roter Punkt allein wird fuer „auf Sendung" gehalten.
        m_slotRow[i] = new QWidget(this);
        // Der Name ist nicht Zierde: die Stilvorlage unten muss sich auf
        // GENAU dieses Widget beziehen. Siehe applyRowStyle().
        m_slotRow[i]->setObjectName(QStringLiteral("dvkSlotRow"));
        auto* row = new QHBoxLayout(m_slotRow[i]);
        row->setContentsMargins(4, 1, 3, 1);
        row->setSpacing(3);

        // Die TASTE steht links. Im Betrieb greift man nach der Taste,
        // nicht nach dem Namen.
        m_slotKey[i] = new QLabel(m_slotRow[i]);
        m_slotKey[i]->setFixedWidth(26);
        m_slotKey[i]->setAlignment(Qt::AlignCenter);
        m_slotKey[i]->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; font-weight: bold;"
            " background: %2; border: 1px solid %3; border-radius: 3px; }")
                .arg(QLatin1String(Style::kAmberText),
                     QLatin1String(Style::kInsetBg),
                     QLatin1String(Style::kAmberBorder)));
        row->addWidget(m_slotKey[i]);

        m_slotLabel[i] = new QLabel(m_slotRow[i]);
        m_slotLabel[i]->setMinimumWidth(70);
        m_slotLabel[i]->setToolTip(QStringLiteral(
            "Double-click to name this announcement (CQ, 73, callsign …)."));
        row->addWidget(m_slotLabel[i], 1);

        // Die Dauer in eigener Spalte, rechtsbuendig und in
        // Ziffernbreite: sie ist die einzige Zahl, die vor dem Senden
        // zaehlt, und sie soll beim Auffrischen nicht springen.
        m_slotLen[i] = new QLabel(m_slotRow[i]);
        m_slotLen[i]->setFixedWidth(42);
        m_slotLen[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QFont lenFont = m_slotLen[i]->font();
        lenFont.setStyleHint(QFont::Monospace);
        lenFont.setPointSizeF(lenFont.pointSizeF() - 1.0);
        m_slotLen[i]->setFont(lenFont);
        m_slotLen[i]->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                        .arg(QLatin1String(Style::kTextScale)));
        row->addWidget(m_slotLen[i]);

        m_recBtn[i]  = styledButton(QStringLiteral("\u25CF"));
        m_playBtn[i] = styledButton(QStringLiteral("\u25BA"));
        m_stopBtn[i] = styledButton(QStringLiteral("\u25A0"));
        m_loadBtn[i] = styledButton(QStringLiteral("WAV"));

        m_recBtn[i]->setToolTip(QStringLiteral(
            "Record from the microphone into this slot (up to 30 s). "
            "Press again to stop. This does NOT transmit — it listens to "
            "what the microphone delivers. Needs a connected radio."));
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

        vbox->addWidget(m_slotRow[i]);
        applyRowStyle(i);
        // Gleich fuellen und nicht erst beim naechsten Abgleich: sonst
        // steht die Zeile leer da, bis irgendwer syncFromModel ruft —
        // und genau so sah sie beim Selbsttest aus.
        refreshSlot(i);

        // Aufnahme geht, sobald der Sendeweg steht — sie TASTET NICHT,
        // sie liest nur mit. Wiedergabe bleibt gesperrt, bis die Tastung
        // im MoxController haengt; der Tooltip sagt es.
        m_recBtn[i]->setEnabled(m_model && m_model->txWorker() != nullptr);
        m_playBtn[i]->setEnabled(false);
        m_stopBtn[i]->setEnabled(false);

        connect(m_recBtn[i], &QPushButton::clicked, this, [this, i]() {
            if (m_recordingSlot == i) { finishRecording(); }
            else if (m_recordingSlot < 0) { startRecording(i); }
        });

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

    // Was ab hier kommt, gilt fuer den GANZEN Speicher, nicht fuer eine
    // Zeile. Eine Trennlinie sagt das, bevor jemand „Rpt" fuer den
    // zuletzt angeklickten Platz haelt.
    {
        auto* rule = new QFrame(this);
        rule->setFrameShape(QFrame::HLine);
        rule->setFixedHeight(1);
        rule->setStyleSheet(QStringLiteral("background: %1; border: none;")
                                .arg(QLatin1String(Style::kBorderSubtle)));
        vbox->addSpacing(4);
        vbox->addWidget(rule);
        vbox->addSpacing(2);
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

    // Ein globaler „Import WAV…"-Knopf stand hier bis 2026-08-19. Er
    // ist entfallen: jede der zehn Zeilen hat ihren eigenen WAV-Knopf,
    // und ein Import ohne Ziel-Platz muesste erst fragen, in welche
    // Zeile — derselbe Weg mit einem Zwischenschritt mehr.

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

} // namespace Longpath
