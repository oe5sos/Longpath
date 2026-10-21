// =================================================================
// src/gui/applets/RecordingApplet.cpp  (Longpath)
// =================================================================
// Siehe RecordingApplet.h fuer das Warum.
//
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer,
//                geschrieben mit Claude Opus 5 (Anthropic).
// =================================================================

#include "RecordingApplet.h"
#include <QFileInfo>
#include <QFileDialog>

#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "models/RecordingModel.h"

#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Longpath {

namespace {

// Punktgroesse nur anfassen, wenn es eine gibt. Kopflos laeuft Qt mit
// PIXELgroessen; dort ist pointSizeF() == -1, und -1 + 2 ist eine
// Ein-Punkt-Schrift. Genau das ist hier passiert: die Uhren waren
// 2 px breit, „00:00" mass 3 px. Dieselbe Wache steht in
// QsoRecorderApplet.cpp:100 -- und dieselbe Klasse Fehler ist in
// HAUSSTIL.md „Die Falle, die einen Monat Typografie unsichtbar
// gemacht hat" aufgeschrieben.
void stupsPunktgroesse(QFont& f, double delta)
{
    if (f.pointSizeF() > 0.0) { f.setPointSizeF(f.pointSizeF() + delta); }
}

QLabel* gruppenKopf(QWidget* parent, const QString& text)
{
    // Regel 1 des Hausstils: jede Bedienung steht in einer benannten
    // Gruppe.
    auto* l = new QLabel(text.toUpper(), parent);
    QFont f = l->font();
    stupsPunktgroesse(f, -2.0);
    f.setBold(true);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                         .arg(QLatin1String(Style::kTextGroupHead)));
    return l;
}

QLabel* uhr(QWidget* parent)
{
    // Regel 4: Zahlen sind Monospace. Regel 7: Unbekannt ist ein
    // Strich, keine Null -- „00:00" waere eine Aussage ueber eine
    // Aufnahme, die es nicht gibt.
    auto* l = new QLabel(QStringLiteral("——"), parent);
    QFont f = l->font();
    f.setStyleHint(QFont::Monospace);
    stupsPunktgroesse(f, 2.0);
    f.setBold(true);
    l->setFont(f);

    // Eine feste Mindestbreite aus der breitesten Belegung. Ohne sie
    // drueckt der Dehner daneben die Beschriftung auf ihre
    // Wunschbreite zusammen -- und eine Uhr, die beim Laufen von
    // „——" auf „00:00" springt, laesst die Knoepfe daneben wandern.
    const QFontMetrics fm(f);
    l->setMinimumWidth(fm.horizontalAdvance(QStringLiteral("00:00")) + 6);
    return l;
}

QString mmss(double sekunden)
{
    if (sekunden <= 0.0) { return QStringLiteral("——"); }
    const int ganz = static_cast<int>(sekunden);
    return QStringLiteral("%1:%2")
        .arg(ganz / 60, 2, 10, QLatin1Char('0'))
        .arg(ganz % 60, 2, 10, QLatin1Char('0'));
}

}  // namespace

RecordingApplet::RecordingApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();

    if (m_model) {
        RecordingModel& rec = m_model->recording();
        connect(&rec, &RecordingModel::audioRecordingChanged,
                this, [this](bool) { refreshState(); });
        connect(&rec, &RecordingModel::audioSecondsChanged,
                this, [this](double) { refreshState(); });
        connect(&rec, &RecordingModel::audioSamplesLostChanged,
                this, [this](bool) { refreshState(); });
        connect(&rec, &RecordingModel::iqRecordingChanged,
                this, [this](bool) { refreshState(); });
        connect(&rec, &RecordingModel::iqSecondsChanged,
                this, [this](double) { refreshState(); });
        connect(&rec, &RecordingModel::iqSamplesLostChanged,
                this, [this](bool) { refreshState(); });
        connect(&rec, &RecordingModel::scheduleRunningChanged,
                this, [this](bool) { refreshState(); });
        connect(&rec, &RecordingModel::scheduleCountChanged,
                this, [this](int) { refreshState(); });
        connect(&rec, &RecordingModel::readyChanged,
                this, [this](bool) { refreshState(); });
        connect(&rec, &RecordingModel::lastErrorChanged,
                this, [this](const QString&) { refreshState(); });
    }

    refreshState();
}

void RecordingApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(6, 4, 6, 6);
    vbox->setSpacing(4);

    // ── Ton ──────────────────────────────────────────────────────────
    vbox->addWidget(gruppenKopf(body, QStringLiteral("Audio")));
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        m_audioBtn = styledButton(QStringLiteral("● Record"), 82);
        m_audioBtn->setToolTip(QStringLiteral(
            "Record the receive audio to a WAV file. Tapped before the "
            "mixer and the volume control — turning the speaker down "
            "does not make the recording quieter. Nothing is "
            "transmitted."));
        connect(m_audioBtn, &QPushButton::clicked,
                this, &RecordingApplet::onAudioClicked);
        row->addWidget(m_audioBtn);

        m_audioClock = uhr(body);
        row->addWidget(m_audioClock);
        row->addStretch(1);

        m_audioGap = new QLabel(QStringLiteral("GAP"), body);
        m_audioGap->setToolTip(QStringLiteral(
            "Samples were lost — the recording has a gap. The part "
            "written so far is intact."));
        QFont gf = m_audioGap->font();
        stupsPunktgroesse(gf, -1.0);
        gf.setBold(true);
        m_audioGap->setFont(gf);
        m_audioGap->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                      .arg(QLatin1String(Style::kAmberWarn)));
        m_audioGap->setVisible(false);
        row->addWidget(m_audioGap);

        vbox->addLayout(row);
    }

    // ── I/Q ──────────────────────────────────────────────────────────
    vbox->addSpacing(4);
    vbox->addWidget(gruppenKopf(body, QStringLiteral("I/Q")));
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        m_iqBtn = styledButton(QStringLiteral("● Record"), 82);
        m_iqBtn->setToolTip(QStringLiteral(
            "Record the raw I/Q stream. Needs a direct radio — a "
            "receiver fed by another device (KiwiSDR, SunSDR, Flex) "
            "delivers finished audio, not I/Q."));
        connect(m_iqBtn, &QPushButton::clicked,
                this, &RecordingApplet::onIqClicked);
        row->addWidget(m_iqBtn);

        m_iqClock = uhr(body);
        row->addWidget(m_iqClock);
        row->addStretch(1);

        vbox->addLayout(row);

        m_iqNote = new QLabel(body);
        QFont nf = m_iqNote->font();
        stupsPunktgroesse(nf, -2.0);
        m_iqNote->setFont(nf);
        m_iqNote->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                    .arg(QLatin1String(Style::kTextInactive)));
        m_iqNote->setVisible(false);
        vbox->addWidget(m_iqNote);
    }

    // ── Wiedergabe ───────────────────────────────────────────────────
    vbox->addSpacing(6);
    vbox->addWidget(gruppenKopf(body, QStringLiteral("Playback")));
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        m_playBtn = styledButton(QStringLiteral("▶ Open…"), 82);
        m_playBtn->setToolTip(QStringLiteral(
            "Play an I/Q recording back through the whole chain — "
            "same DSP, same waterfall, same audio as a live radio. "
            "Replaces a running radio connection; a recording never "
            "transmits."));
        connect(m_playBtn, &QPushButton::clicked,
                this, &RecordingApplet::onPlaybackClicked);
        row->addWidget(m_playBtn);

        m_playFile = new QLabel(QStringLiteral("——"), body);
        QFont pf = m_playFile->font();
        stupsPunktgroesse(pf, -1.0);
        m_playFile->setFont(pf);
        m_playFile->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                      .arg(QLatin1String(Style::kTextInactive)));
        row->addWidget(m_playFile);
        row->addStretch(1);

        vbox->addLayout(row);
    }

    // ── Zeitplan ─────────────────────────────────────────────────────
    vbox->addSpacing(6);
    vbox->addWidget(gruppenKopf(body, QStringLiteral("Schedule")));
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        m_planText = new QLabel(QStringLiteral("none"), body);
        m_planText->setStyleSheet(
            QStringLiteral("QLabel { color: %1; }")
                .arg(QLatin1String(Style::kTextSecondary)));
        row->addWidget(m_planText);
        row->addStretch(1);

        m_planState = new QLabel(body);
        QFont pf = m_planState->font();
        stupsPunktgroesse(pf, -1.0);
        pf.setBold(true);
        m_planState->setFont(pf);
        row->addWidget(m_planState);

        vbox->addLayout(row);
    }

    // ── Was schiefging ───────────────────────────────────────────────
    //
    // Regel: sagen, WAS fehlt. „geht nicht" schickt den Betreiber
    // suchen; „the sample rate is not known yet" sagt ihm, dass er
    // verbinden muss.
    m_error = new QLabel(body);
    QFont ef = m_error->font();
    stupsPunktgroesse(ef, -2.0);
    m_error->setFont(ef);
    m_error->setWordWrap(true);
    m_error->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                               .arg(QLatin1String(Style::kAmberWarn)));
    m_error->setVisible(false);
    vbox->addWidget(m_error);

    root->addWidget(body);
}

void RecordingApplet::onAudioClicked()
{
    if (!m_model) { return; }
    RecordingModel& rec = m_model->recording();
    if (rec.audioRecording()) { rec.stopAudio(); }
    else                      { rec.startAudio(); }
}

void RecordingApplet::onIqClicked()
{
    if (!m_model) { return; }
    RecordingModel& rec = m_model->recording();
    if (rec.iqRecording()) { rec.stopIq(); }
    else                   { rec.startIq(); }
}

void RecordingApplet::onPlaybackClicked()
{
    if (m_model == nullptr) {
        return;
    }
    RecordingModel& rec = m_model->recording();
    if (rec.playing()) {
        rec.stopPlayback();
        return;
    }
    const QString pfad = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open recording"), rec.recordingFolder(),
        QStringLiteral("Recordings (*.wav);;All files (*)"));
    if (pfad.isEmpty()) {
        return;
    }
    rec.startPlayback(pfad);
}

void RecordingApplet::refreshState()
{
    if (!m_model) { return; }
    const RecordingModel& rec = m_model->recording();

    const bool bereit = rec.ready();
    const bool tonAn  = rec.audioRecording();
    const bool iqAn   = rec.iqRecording();

    m_audioBtn->setText(tonAn ? QStringLiteral("■ Stop")
                              : QStringLiteral("● Record"));
    m_audioBtn->setEnabled(bereit);
    m_audioClock->setText(tonAn ? mmss(rec.audioSeconds())
                                : QStringLiteral("——"));
    m_audioClock->setStyleSheet(
        QStringLiteral("QLabel { color: %1; }")
            .arg(QLatin1String(tonAn ? Style::kAmberText
                                     : Style::kTextInactive)));
    m_audioGap->setVisible(rec.audioSamplesLost());

    m_iqBtn->setText(iqAn ? QStringLiteral("■ Stop")
                          : QStringLiteral("● Record"));
    m_iqBtn->setEnabled(bereit);
    m_iqClock->setText(iqAn ? mmss(rec.iqSeconds()) : QStringLiteral("——"));
    m_iqClock->setStyleSheet(
        QStringLiteral("QLabel { color: %1; }")
            .arg(QLatin1String(iqAn ? Style::kAmberText
                                    : Style::kTextInactive)));

    const bool spielt = rec.playing();
    // Ohne Radio geht keine Wiedergabe (RecordingModel::startPlayback
    // verweigert sie) -- dann muss der Knopf AUS sein, sonst klickt
    // man ins Leere. Laeuft gerade eine, bleibt er an: Stoppen muss
    // immer gehen.
    m_playBtn->setEnabled(bereit || spielt);
    m_playBtn->setText(spielt ? QStringLiteral("■ Stop")
                              : QStringLiteral("▶ Open…"));
    m_playFile->setText(spielt ? QFileInfo(rec.playbackFile()).fileName()
                               : QStringLiteral("——"));
    m_playFile->setStyleSheet(
        QStringLiteral("QLabel { color: %1; }")
            .arg(QLatin1String(spielt ? Style::kAmberText
                                      : Style::kTextInactive)));

    const int n = rec.scheduleCount();
    m_planText->setText(n == 0 ? QStringLiteral("none")
                               : QStringLiteral("%1 entries").arg(n));
    if (n == 0) {
        m_planState->setText(QString());
    } else {
        const bool an = rec.scheduleRunning();
        m_planState->setText(an ? QStringLiteral("ARMED")
                                : QStringLiteral("OFF"));
        m_planState->setStyleSheet(
            QStringLiteral("QLabel { color: %1; }")
                .arg(QLatin1String(an ? Style::kGreenText
                                      : Style::kTextInactive)));
    }

    const QString fehler = rec.lastError();
    m_error->setText(fehler);
    m_error->setVisible(!fehler.isEmpty());

    // Die Notiz erscheint nur, wenn sie etwas erklaert: steht sie
    // dauernd da, liest sie niemand mehr.
    const bool iqNote = !fehler.isEmpty()
                        && fehler.contains(QStringLiteral("I/Q"));
    m_iqNote->setText(QStringLiteral("no I/Q on this receiver"));
    m_iqNote->setVisible(iqNote);
}

void RecordingApplet::syncFromModel()
{
    refreshState();
}

}  // namespace Longpath
