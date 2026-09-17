// =================================================================
// src/gui/applets/CwDecoderApplet.cpp  (Longpath)
// =================================================================
//
// Longpath-original; see the header. Tap plumbing and scrollback cap
// mirror RttyDecoderApplet.cpp on purpose, so the two decoder panels
// behave identically.
// no-port-check: Longpath-original applet; ported logic is cited in
// core/CwDecoderCore.cpp.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-17 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "CwDecoderApplet.h"

#include "gui/HGauge.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/TriBtn.h"
#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/CwDecoder.h"
#include "core/audio/AudioTapRing.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

namespace Longpath {

namespace {
// Half a second of interleaved-stereo float32 at the native WDSP RX rate
// (48 kHz) -- the same sizing as the RTTY decoder's ring, for the same
// 50 ms pump.
constexpr int kTapRingFloats = 48000;
constexpr int kPumpIntervalMs = 50;

// Cap the visible transcript so an unattended session does not grow the
// QPlainTextEdit's document without bound.
constexpr int kMaxCharCount = 4000;

// Below this the decoder itself was unsure (a "?" pattern, or elements
// that fit neither dit nor dah); showing it would be noise on the screen.
constexpr float kMinConfidence = 0.15f;

QString capsuleStyle(const char* bg, const char* fg, const char* border)
{
    return QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 10px; padding: 0 10px; font-size: 10px; }")
        .arg(QLatin1String(bg), QLatin1String(fg), QLatin1String(border));
}
} // namespace

int CwDecoderApplet::cwPitchFromSettings()
{
    // Same key, default and limits as SliceModel's CW filter centring
    // (Thetis cw_pitch, display.cs:1023 -- default 600, udCWPitch 100..2000).
    int pitch = AppSettings::instance().value(QStringLiteral("CWPitch"), 600).toInt();
    if (pitch < CwDecoder::kMinPitchHz) { pitch = CwDecoder::kMinPitchHz; }
    if (pitch > CwDecoder::kMaxPitchHz) { pitch = CwDecoder::kMaxPitchHz; }
    return pitch;
}

CwDecoderApplet::CwDecoderApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();

    m_decoder = new CwDecoder(this);
    connect(m_decoder, &CwDecoder::textDecoded,
            this, &CwDecoderApplet::onTextDecoded);
    connect(m_decoder, &CwDecoder::statsUpdated,
            this, &CwDecoderApplet::onStatsUpdated);
    m_decoder->setPitchHz(m_pitchHz);
    m_decoder->start();

    // Unbind promptly when the bound slice goes away -- the same safety
    // net as the RTTY decoder: a stale sliceIndex in AudioEngine's tap
    // would otherwise route whatever slice next reuses that index here.
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
        // Fester Zwischenspeicher wie beim RTTY-Abholer: ein Zeitgeber,
        // der 20x/s laeuft, soll nicht 20x/s Speicher anfordern.
        static thread_local std::vector<float> scratch;
        const int want = kTapRingFloats / 4;  // ~125 ms Stereo, reichlich Rand
        if (static_cast<int>(scratch.size()) < want) { scratch.resize(static_cast<size_t>(want)); }
        const int got = m_tapRing->read(scratch.data(), want);
        if (got >= 2) {
            m_decoder->feedAudio(scratch.data(), got / 2);
        }
    });
    m_pumpTimer->start();
}

CwDecoderApplet::~CwDecoderApplet()
{
    // Pump first, then the tap, then the decoder -- the same order and
    // the same reason as RttyDecoderApplet: the ring is a C++ member and
    // the decoder a QObject child, destroyed at different points.
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
    m_trackedHz = new QLabel(QStringLiteral("— Hz"), this);
    m_trackedHz->setFont(Style::monoFont(font(), 11));
    m_trackedHz->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    m_trackedHz->setToolTip(QStringLiteral("Ton, auf den der Decoder gerade eingerastet ist."));
    statusRow->addWidget(m_trackedHz);
    statusRow->addStretch(1);

    m_wpmValue = new QLabel(QStringLiteral("— WPM"), this);
    m_wpmValue->setFont(Style::monoFont(font(), 11));
    m_wpmValue->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    m_wpmValue->setToolTip(QStringLiteral("Geschaetzte Gebegeschwindigkeit aus der Punktlaenge."));
    statusRow->addWidget(m_wpmValue);

    m_snrValue = new QLabel(QStringLiteral("— dB"), this);
    m_snrValue->setFont(Style::monoFont(font(), 11));
    m_snrValue->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    statusRow->addWidget(m_snrValue);

    // Zustand als umrandete Kapsel, HAUSSTIL.md Regel 5.
    m_toneCapsule = new QLabel(QStringLiteral("KEIN TON"), this);
    m_toneCapsule->setAlignment(Qt::AlignCenter);
    m_toneCapsule->setFixedHeight(20);
    m_toneCapsule->setStyleSheet(capsuleStyle(Style::kBadgeOffBg, Style::kTextTertiary, Style::kBorder));
    statusRow->addWidget(m_toneCapsule);
    root->addLayout(statusRow);

    // -- Row 2: signal -------------------------------------------------------
    m_signalGauge = new HGauge(this);
    m_signalGauge->setTitle(QStringLiteral("SNR"));
    m_signalGauge->setRange(0.0, 40.0);
    root->addWidget(m_signalGauge);

    root->addWidget(divider());

    // -- Row 3: decoded text ------------------------------------------------
    m_textOutput = new QPlainTextEdit(this);
    m_textOutput->setReadOnly(true);
    m_textOutput->setFont(Style::monoFont(font(), 12));
    m_textOutput->setMinimumHeight(90);
    m_textOutput->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: %1; color: %2; border: 1px solid %3; }")
        .arg(Style::kInsetBg, Style::kTextPrimary, Style::kInsetBorder));
    root->addWidget(m_textOutput);

    // -- Row 4: controls -----------------------------------------------------
    auto* ctrlRow = new QHBoxLayout();
    auto* pitchCaption = new QLabel(QStringLiteral("TON:"), this);
    pitchCaption->setFixedWidth(34);
    pitchCaption->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                    .arg(Style::kTextSecondary));
    pitchCaption->setToolTip(QStringLiteral(
        "Empfangston, um den der Decoder sucht (±125 Hz). Startet auf dem "
        "CW-Mithoerton, auf den auch der CW-Filter zentriert ist."));
    ctrlRow->addWidget(pitchCaption);
    m_pitchDown = new TriBtn(TriBtn::Left, this);
    ctrlRow->addWidget(m_pitchDown);
    m_pitchLabel = new QLabel(this);
    m_pitchLabel->setAlignment(Qt::AlignCenter);
    m_pitchLabel->setStyleSheet(Style::insetValueStyle());
    m_pitchLabel->setFixedWidth(64);
    ctrlRow->addWidget(m_pitchLabel);
    m_pitchUp = new TriBtn(TriBtn::Right, this);
    ctrlRow->addWidget(m_pitchUp);
    connect(m_pitchDown, &QPushButton::clicked, this, [this]() { setPitch(m_pitchHz - 25); });
    connect(m_pitchUp,   &QPushButton::clicked, this, [this]() { setPitch(m_pitchHz + 25); });
    m_pitchHz = cwPitchFromSettings();
    m_pitchLabel->setText(QStringLiteral("%1 Hz").arg(m_pitchHz));
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

void CwDecoderApplet::setPitch(int hz)
{
    hz = std::clamp(hz, CwDecoder::kMinPitchHz, CwDecoder::kMaxPitchHz);
    if (hz == m_pitchHz) { return; }
    m_pitchHz = hz;
    m_pitchLabel->setText(QStringLiteral("%1 Hz").arg(m_pitchHz));
    if (m_decoder) { m_decoder->setPitchHz(m_pitchHz); }
}

void CwDecoderApplet::setSlice(SliceModel* slice)
{
    if (m_slice == slice) { return; }
    m_slice = slice;
    // A new slice is a new signal: forget the old timing and noise floor.
    if (m_decoder) { m_decoder->reset(); }
    updateAudioTap();
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

void CwDecoderApplet::onTextDecoded(const QString& text, float confidence)
{
    if (confidence < kMinConfidence && text != QStringLiteral(" ")) { return; }

    auto cursor = m_textOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    m_textOutput->setTextCursor(cursor);
    m_textOutput->ensureCursorVisible();

    m_charCount += text.size();
    if (m_charCount > kMaxCharCount) {
        // Trim by character count, not by "line": decoded CW has no line
        // breaks at all, so a long run is one soft-wrapped paragraph (see
        // the RTTY applet for the same reasoning).
        const int excess = m_charCount - kMaxCharCount;
        auto trimCursor = m_textOutput->textCursor();
        trimCursor.movePosition(QTextCursor::Start);
        trimCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, excess);
        trimCursor.removeSelectedText();
        m_charCount = m_textOutput->toPlainText().size();
    }
}

void CwDecoderApplet::onStatsUpdated(float wpm, float snrDb, bool tonePresent, float trackedHz)
{
    m_wpmValue->setText(QStringLiteral("%1 WPM").arg(qRound(wpm)));
    m_snrValue->setText(QStringLiteral("%1 dB").arg(snrDb, 0, 'f', 1));
    m_trackedHz->setText(QStringLiteral("%1 Hz").arg(qRound(trackedHz)));
    m_signalGauge->setValue(std::max(0.0f, snrDb));

    if (tonePresent) {
        m_toneCapsule->setText(QStringLiteral("TON"));
        m_toneCapsule->setStyleSheet(capsuleStyle(Style::kGreenBg, Style::kGreenText, Style::kGreenBorder));
    } else {
        m_toneCapsule->setText(QStringLiteral("KEIN TON"));
        m_toneCapsule->setStyleSheet(capsuleStyle(Style::kBadgeOffBg, Style::kTextTertiary, Style::kBorder));
    }
}

} // namespace Longpath
