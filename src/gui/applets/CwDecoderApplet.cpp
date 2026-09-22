// =================================================================
// src/gui/applets/CwDecoderApplet.cpp  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR -- GPLv3): see CwDecoderApplet.h.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 SS5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "CwDecoderApplet.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/CwDecoder.h"
#include "core/audio/AudioTapRing.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCharFormat>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

namespace Longpath {

namespace {
// Half a second of interleaved-stereo float32 at the native WDSP RX rate
// (48 kHz), the RttyDecoderApplet's sizing: plenty for a 50 ms pump.
constexpr int kTapRingFloats = 48000;
constexpr int kPumpIntervalMs = 50;
// Cap the visible transcript so a long, unattended session does not grow
// the QPlainTextEdit's document without bound.
constexpr int kMaxCharCount = 4000;
// The band ggmorse searches around the operator's CW pitch. AetherSDR
// pads a known pitch by 150 Hz either side (setKnownParameters); the
// floor of 100 Hz is ggmorse's own lower bound of usefulness.
constexpr int kPitchPadHz = 150;
// AetherSDR's default sensitivity (PanadapterApplet m_cwCostThreshold
// 0.70): a decode whose ggmorse cost is at or above this is dropped.
// ggmorse's own "is decoding" line is 1.0, and band noise decodes as
// random letters at a cost near it -- without the gate an idle CW slice
// fills the transcript with garbage (seen live over a KiwiSDR,
// 2026-09-21). AetherSDR exposes the threshold as a slider; Longpath
// keeps the default fixed until the operator asks for the knob.
constexpr float kCostThreshold = 0.70f;
// AetherSDR's confidence bands (cost < 0.15 green, < 0.35 yellow, < 0.60
// orange, else red). Longpath keeps red for warnings and dims uncertain
// copy instead: primary / secondary / tertiary text tone.
constexpr float kCostSure = 0.15f;
constexpr float kCostFair = 0.35f;
} // namespace

CwDecoderApplet::CwDecoderApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();

    m_decoder = new CwDecoder(this);
    connect(m_decoder, &CwDecoder::textDecoded, this, &CwDecoderApplet::onTextDecoded);
    connect(m_decoder, &CwDecoder::statsUpdated, this, &CwDecoderApplet::onStatsUpdated);
    applyPitchBandFromSlice();
    m_decoder->start();

    // Unbind promptly when the bound slice goes away (the same safety net
    // RttyDecoderApplet has, for the same reason: a stale sliceIndex in the
    // tap would route whatever slice next reuses that index in here).
    if (m_model) {
        connect(m_model, &RadioModel::sliceRemoved, this, [this](int index) {
            if (m_slice && m_slice->sliceIndex() == index) {
                setSlice(nullptr);
            }
        });
    }

    m_tapRing = std::make_unique<AudioTapRing>(kTapRingFloats);

    m_pumpTimer = new QTimer(this);
    m_pumpTimer->setInterval(kPumpIntervalMs);
    connect(m_pumpTimer, &QTimer::timeout, this, [this]() {
        if (!m_tapRing || !m_decoder) { return; }
        static thread_local std::vector<float> scratch;
        const int want = kTapRingFloats / 4;   // ~125 ms stereo
        if (static_cast<int>(scratch.size()) < want) { scratch.resize(want); }
        const int got = m_tapRing->read(scratch.data(), want);
        if (got >= 2) {
            m_decoder->feedAudio(scratch.data(), got / 2);
        }
    });
    m_pumpTimer->start();
}

CwDecoderApplet::~CwDecoderApplet()
{
    // Stop the pump FIRST, then the tap, then the worker -- the order the
    // RTTY applet settled on for the same three pieces.
    if (m_pumpTimer) { m_pumpTimer->stop(); }
    if (m_model && m_model->audioEngine()) {
        m_model->audioEngine()->setCwTap(nullptr, -1);
    }
    if (m_decoder) { m_decoder->stop(); }
}

void CwDecoderApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->addWidget(appletTitleBar(QStringLiteral("CW DECODER")));

    // -- Row 1: status ---------------------------------------------------
    auto* statusRow = new QHBoxLayout();
    m_stats = new QLabel(QStringLiteral("— Hz · — WPM"), this);
    m_stats->setFont(Style::monoFont(font(), 11));
    m_stats->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    // Room for the widest reading ("1200 Hz · 55 WPM") from the start, so
    // the first live values do not have to wait for a relayout.
    m_stats->setMinimumWidth(m_stats->fontMetrics().horizontalAdvance(
        QStringLiteral("1200 Hz · 55 WPM")) + 8);
    statusRow->addWidget(m_stats);
    statusRow->addStretch(1);

    m_lockCapsule = new QLabel(QStringLiteral("AUTO"), this);
    m_lockCapsule->setAlignment(Qt::AlignCenter);
    // The stylesheet's padding is not part of the label's size hint, so
    // give the capsule room for its widest word ("LOCK WPM") outright.
    m_lockCapsule->setFixedSize(76, 20);
    m_lockCapsule->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 10px; padding: 0 10px; font-size: 9px; }")
        .arg(Style::kBadgeOffBg, Style::kTextTertiary, Style::kBorder));
    statusRow->addWidget(m_lockCapsule);
    root->addLayout(statusRow);

    root->addWidget(divider());

    // -- Row 2: decoded text ------------------------------------------------
    m_textOutput = new QPlainTextEdit(this);
    m_textOutput->setReadOnly(true);
    m_textOutput->setFont(Style::monoFont(font(), 12));
    m_textOutput->setMinimumHeight(90);
    m_textOutput->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: %1; color: %2; border: 1px solid %3; }")
        .arg(Style::kInsetBg, Style::kTextPrimary, Style::kInsetBorder));
    root->addWidget(m_textOutput);

    // -- Row 3: controls -----------------------------------------------------
    auto* ctrlRow = new QHBoxLayout();
    m_lockPitchBtn = blueToggle(QStringLiteral("LOCK Hz"), 86);
    m_lockPitchBtn->setCheckable(true);
    m_lockPitchBtn->setToolTip(QStringLiteral(
        "Tonhoehe auf dem erkannten Wert festhalten, statt ihr weiter zu folgen"));
    connect(m_lockPitchBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_decoder) { m_decoder->lockPitch(on); }
        onStatsUpdated(m_decoder ? m_decoder->estimatedPitch() : 0.0f,
                       m_decoder ? m_decoder->estimatedSpeed() : 0.0f);
    });
    ctrlRow->addWidget(m_lockPitchBtn);

    m_lockSpeedBtn = blueToggle(QStringLiteral("LOCK WPM"), 96);
    m_lockSpeedBtn->setCheckable(true);
    m_lockSpeedBtn->setToolTip(QStringLiteral(
        "Tempo auf dem erkannten Wert festhalten, statt ihm weiter zu folgen"));
    connect(m_lockSpeedBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_decoder) { m_decoder->lockSpeed(on); }
        onStatsUpdated(m_decoder ? m_decoder->estimatedPitch() : 0.0f,
                       m_decoder ? m_decoder->estimatedSpeed() : 0.0f);
    });
    ctrlRow->addWidget(m_lockSpeedBtn);
    ctrlRow->addStretch(1);

    m_clearBtn = styledButton(QStringLiteral("Leeren"), 60);
    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        m_textOutput->clear();
        m_charCount = 0;
    });
    ctrlRow->addWidget(m_clearBtn);

    root->addLayout(ctrlRow);
    root->addStretch(1);
}

void CwDecoderApplet::setSlice(SliceModel* slice)
{
    if (m_slice == slice) { return; }
    m_slice = slice;
    applyPitchBandFromSlice();
    updateAudioTap();
}

// The band the decoder searches for the tone: the operator's CW pitch
// (AppSettings "CWPitch", the Thetis-sourced value SliceModel's CW filter
// presets sit on) +/- 150 Hz.
void CwDecoderApplet::applyPitchBandFromSlice()
{
    if (!m_decoder) { return; }
    int pitch = AppSettings::instance().value(QStringLiteral("CWPitch"), 600).toInt();
    pitch = std::clamp(pitch, 100, 2000);
    m_decoder->setPitchRange(std::max(100, pitch - kPitchPadHz), pitch + kPitchPadHz);
}

void CwDecoderApplet::updateAudioTap()
{
    if (!m_model || !m_model->audioEngine() || !m_tapRing) { return; }
    if (m_slice) {
        m_model->audioEngine()->setCwTap(m_tapRing.get(), m_slice->sliceIndex());
    } else {
        m_model->audioEngine()->setCwTap(nullptr, -1);
    }
}

void CwDecoderApplet::syncFromModel()
{
    setSlice(m_model ? m_model->activeSlice() : nullptr);
}

void CwDecoderApplet::onTextDecoded(const QString& text, float cost)
{
    if (cost >= kCostThreshold) { return; }
    // ggmorse starts a new line on every pitch change; AetherSDR turns
    // them into spaces so the copy flows as one line (appendCwText).
    QString clean = text;
    clean.replace(QLatin1Char('\n'), QLatin1Char(' '));

    QTextCharFormat fmt;
    fmt.setForeground(QColor(QLatin1String(
        cost < kCostSure ? Style::kTextPrimary
        : cost < kCostFair ? Style::kTextSecondary
        : Style::kTextTertiary)));
    auto cursor = m_textOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(clean, fmt);
    m_textOutput->setTextCursor(cursor);
    m_textOutput->ensureCursorVisible();

    m_charCount += clean.size();
    if (m_charCount > kMaxCharCount) {
        // By character count, not by line: CW text has no line breaks.
        const int excess = m_charCount - kMaxCharCount;
        auto trimCursor = m_textOutput->textCursor();
        trimCursor.movePosition(QTextCursor::Start);
        trimCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, excess);
        trimCursor.removeSelectedText();
        m_charCount = m_textOutput->toPlainText().size();
    }
}

void CwDecoderApplet::onStatsUpdated(float pitchHz, float speedWpm)
{
    const QString hz = pitchHz > 0.0f ? QString::number(std::lround(pitchHz)) : QStringLiteral("—");
    const QString wpm = speedWpm > 0.0f ? QString::number(std::lround(speedWpm)) : QStringLiteral("—");
    m_stats->setText(QStringLiteral("%1 Hz · %2 WPM").arg(hz, wpm));

    const bool pl = m_decoder && m_decoder->isPitchLocked();
    const bool sl = m_decoder && m_decoder->isSpeedLocked();
    QString capsule = QStringLiteral("AUTO");
    if (pl && sl) { capsule = QStringLiteral("LOCK"); }
    else if (pl)  { capsule = QStringLiteral("LOCK Hz"); }
    else if (sl)  { capsule = QStringLiteral("LOCK WPM"); }
    m_lockCapsule->setText(capsule);
    m_lockCapsule->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 10px; padding: 0 10px; font-size: 9px; }")
        .arg((pl || sl) ? Style::kGreenBg : Style::kBadgeOffBg,
             (pl || sl) ? Style::kGreenText : Style::kTextTertiary,
             (pl || sl) ? Style::kGreenBorder : Style::kBorder));
}

} // namespace Longpath
