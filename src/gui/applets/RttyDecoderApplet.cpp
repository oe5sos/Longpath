#include "RttyDecoderApplet.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"
#include "gui/RttyDecoderSensitivity.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/RttyDecoder.h"
#include "core/audio/AudioTapRing.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace Longpath {

namespace {
QString baudKey()        { return QStringLiteral("RttyDecoder_BaudRate"); }
QString reverseKey()     { return QStringLiteral("RttyDecoder_ReversePolarity"); }
QString sensitivityKey() { return QStringLiteral("RttyDecoder_Sensitivity"); }

// Standard ham HF RTTY baud rates. 45.45 (60 wpm ITU-R teleprinter
// standard) is the universal default; the others are real, commonly-used
// alternates, not an invented list.
constexpr double kBaudRates[] = {45.45, 50.0, 56.88, 75.0, 100.0};

// Half a second of interleaved-stereo float32 at the native WDSP RX rate
// (48 kHz) -- matches AsrService's own ring sizing rationale (see
// MainWindow_Asr.cpp): reichlich for a 50ms pump timer.
constexpr int kTapRingFloats = 48000;
constexpr int kPumpIntervalMs = 50;

// Cap the visible transcript so a long, unattended RTTY session doesn't
// grow the QPlainTextEdit's document without bound.
constexpr int kMaxCharCount = 4000;
} // namespace

RttyDecoderApplet::RttyDecoderApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
    loadSettings();

    m_decoder = new RttyDecoder(this);
    connect(m_decoder, &RttyDecoder::textDecoded,
            this, &RttyDecoderApplet::onTextDecoded);
    connect(m_decoder, &RttyDecoder::statsUpdated,
            this, &RttyDecoderApplet::onStatsUpdated);
    m_decoder->setBaudRate(static_cast<float>(kBaudRates[m_baudCombo->currentIndex()]));
    m_decoder->setReversePolarity(m_reverseBtn->isChecked());
    m_decoder->start();

    // Unbind promptly when the bound slice goes away -- without this, the
    // stale sliceIndex left in AudioEngine's tap keeps routing whatever
    // slice RadioModel::addSlice() next reuses that index for (its
    // lowest-free-index reuse policy) into this decoder, silently -- not
    // a crash, but the wrong signal decodes under this applet's title.
    // RadeApplet and PhoneCwApplet share the same "single global applet,
    // bound once to whichever slice wireSliceToSpectrum() ran for" shape
    // and don't re-target to another slice either; this only closes the
    // stale-routing part, not full multi-slice reassignment.
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
        // Fester Zwischenspeicher, wie beim ASR-Abholer (MainWindow_Asr.cpp):
        // ein Zeitgeber, der 20x/s laeuft, soll nicht 20x/s Speicher anfordern.
        static thread_local std::vector<float> scratch;
        const int want = kTapRingFloats / 4;  // ~125ms Stereo, reichlich Rand
        if (static_cast<int>(scratch.size()) < want) { scratch.resize(want); }
        const int got = m_tapRing->read(scratch.data(), want);
        if (got >= 2) {
            m_decoder->feedAudio(scratch.data(), got / 2);
        }
    });
    m_pumpTimer->start();
}

RttyDecoderApplet::~RttyDecoderApplet()
{
    // Stop the pump FIRST: with m_tapRing/m_decoder as a genuine C++ member
    // (unique_ptr) and a QObject child respectively, they are destroyed at
    // two DIFFERENT points during teardown (the member in this destructor's
    // own unwind, the QObject child later via ~QObject()'s automatic child
    // cleanup) -- stopping the timer explicitly here removes any question
    // of whether its queued lambda could still fire in between and touch
    // either one after it's gone.
    if (m_pumpTimer) { m_pumpTimer->stop(); }
    if (m_model && m_model->audioEngine()) {
        m_model->audioEngine()->setRttyTap(nullptr, -1);
    }
    if (m_decoder) { m_decoder->stop(); }
}

void RttyDecoderApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->addWidget(appletTitleBar(QStringLiteral("RTTY DECODER")));

    // -- Row 1: status ---------------------------------------------------
    auto* statusRow = new QHBoxLayout();
    m_markShiftInfo = new QLabel(QStringLiteral("Mark — Hz · Shift — Hz"), this);
    m_markShiftInfo->setFont(Style::monoFont(font(), 11));
    m_markShiftInfo->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    statusRow->addWidget(m_markShiftInfo);
    statusRow->addStretch(1);

    m_snrValue = new QLabel(QStringLiteral("— dB"), this);
    m_snrValue->setFont(Style::monoFont(font(), 11));
    m_snrValue->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    statusRow->addWidget(m_snrValue);

    // Zustand als umrandete Kapsel, HAUSSTIL.md Regel 5.
    m_lockCapsule = new QLabel(QStringLiteral("KEIN LOCK"), this);
    m_lockCapsule->setAlignment(Qt::AlignCenter);
    m_lockCapsule->setFixedHeight(20);
    m_lockCapsule->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 10px; padding: 0 10px; font-size: 10px; }")
        .arg(Style::kBadgeOffBg, Style::kTextTertiary, Style::kBorder));
    statusRow->addWidget(m_lockCapsule);
    root->addLayout(statusRow);

    // -- Row 2: mark/space levels -----------------------------------------
    auto* levelRow = new QHBoxLayout();
    m_markLevel = new HGauge(this);
    m_markLevel->setTitle(QStringLiteral("MARK"));
    m_markLevel->setRange(0.0, 1.0);
    levelRow->addWidget(m_markLevel);
    m_spaceLevel = new HGauge(this);
    m_spaceLevel->setTitle(QStringLiteral("SPACE"));
    m_spaceLevel->setRange(0.0, 1.0);
    levelRow->addWidget(m_spaceLevel);
    root->addLayout(levelRow);

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
    m_baudCombo = new QComboBox(this);
    for (double b : kBaudRates) {
        m_baudCombo->addItem(QStringLiteral("%1 Bd").arg(b, 0, 'f', 2), b);
    }
    connect(m_baudCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (idx < 0) { return; }
        if (m_decoder) { m_decoder->setBaudRate(static_cast<float>(kBaudRates[idx])); }
        saveSettings();
    });
    ctrlRow->addWidget(m_baudCombo);

    m_reverseBtn = blueToggle(QStringLiteral("REV"), 50);
    m_reverseBtn->setCheckable(true);
    m_reverseBtn->setToolTip(QStringLiteral(
        "Umgekehrte Polaritaet: Space liegt ueber statt unter Mark."));
    connect(m_reverseBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_decoder) { m_decoder->setReversePolarity(on); }
        saveSettings();
    });
    ctrlRow->addWidget(m_reverseBtn);

    ctrlRow->addWidget(new QLabel(QStringLiteral("Empf."), this));
    m_sensitivitySlider = new QSlider(Qt::Horizontal, this);
    m_sensitivitySlider->setRange(0, 100);
    m_sensitivityValue = insetValue(QStringLiteral("0"), 32);
    connect(m_sensitivitySlider, &QSlider::valueChanged, this, [this](int v) {
        m_sensitivity = v;
        m_sensitivityValue->setText(QString::number(v));
        saveSettings();
    });
    ctrlRow->addLayout(sliderRow(QString(), m_sensitivitySlider, m_sensitivityValue, 0));

    m_clearBtn = styledButton(QStringLiteral("Leeren"), 60);
    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        m_textOutput->clear();
        m_charCount = 0;
    });
    ctrlRow->addWidget(m_clearBtn);

    root->addLayout(ctrlRow);
    root->addStretch(1);
}

void RttyDecoderApplet::loadSettings()
{
    auto& st = AppSettings::instance();

    const double savedBaud = st.value(baudKey(), QStringLiteral("45.45")).toString().toDouble();
    int baudIdx = 0;
    for (size_t i = 0; i < std::size(kBaudRates); ++i) {
        if (std::abs(kBaudRates[i] - savedBaud) < 0.01) { baudIdx = static_cast<int>(i); break; }
    }
    m_baudCombo->setCurrentIndex(baudIdx);

    const bool reverse = st.value(reverseKey(), QStringLiteral("False")).toString()
                          == QStringLiteral("True");
    m_reverseBtn->setChecked(reverse);

    const int sens = st.value(sensitivityKey(),
                              QString::number(kRttySensitivityDefault)).toInt();
    m_sensitivitySlider->setValue(sens);
    m_sensitivity = sens;
    m_sensitivityValue->setText(QString::number(sens));
}

void RttyDecoderApplet::saveSettings() const
{
    auto& st = AppSettings::instance();
    st.setValue(baudKey(), QString::number(kBaudRates[m_baudCombo->currentIndex()]));
    st.setValue(reverseKey(), m_reverseBtn->isChecked() ? QStringLiteral("True")
                                                         : QStringLiteral("False"));
    st.setValue(sensitivityKey(), QString::number(m_sensitivity));
}

void RttyDecoderApplet::setSlice(SliceModel* slice)
{
    if (m_slice == slice) { return; }

    disconnect(m_markHzConn);
    disconnect(m_shiftHzConn);
    m_slice = slice;

    if (m_slice) {
        m_markHzConn = connect(m_slice, &SliceModel::rttyMarkHzChanged,
                               this, [this](int) { applyMarkShiftFromSlice(); });
        m_shiftHzConn = connect(m_slice, &SliceModel::rttyShiftHzChanged,
                                this, [this](int) { applyMarkShiftFromSlice(); });
    }
    applyMarkShiftFromSlice();
    updateAudioTap();
}

void RttyDecoderApplet::applyMarkShiftFromSlice()
{
    if (!m_slice) {
        m_markShiftInfo->setText(QStringLiteral("Mark — Hz · Shift — Hz"));
        return;
    }
    const int mark  = m_slice->rttyMarkHz();
    const int shift = m_slice->rttyShiftHz();
    m_markShiftInfo->setText(QStringLiteral("Mark %1 Hz · Shift %2 Hz")
                                  .arg(mark).arg(shift));
    if (m_decoder) {
        m_decoder->setMarkFreqHz(mark);
        m_decoder->setShiftHz(shift);
    }
}

void RttyDecoderApplet::updateAudioTap()
{
    if (!m_model || !m_model->audioEngine() || !m_tapRing) { return; }
    if (m_slice) {
        m_model->audioEngine()->setRttyTap(m_tapRing.get(), m_slice->sliceIndex());
    } else {
        m_model->audioEngine()->setRttyTap(nullptr, -1);
    }
}

void RttyDecoderApplet::syncFromModel()
{
    setSlice(m_model ? m_model->activeSlice() : nullptr);
}

void RttyDecoderApplet::onTextDecoded(const QString& text, float confidence)
{
    if (confidence < rttyConfThresholdFor(m_sensitivity)) { return; }

    auto cursor = m_textOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
    m_textOutput->setTextCursor(cursor);
    m_textOutput->ensureCursorVisible();

    m_charCount += text.size();
    if (m_charCount > kMaxCharCount) {
        // NextCharacter, not Down: RTTY text has no guaranteed line breaks
        // at all (only appears if the far station sends a Baudot CR/LF),
        // so a long unbroken run is one continuously WRAPPED paragraph --
        // "lines" moved by QTextCursor::Down are the widget's current
        // soft-wrapped visual rows (panel-width- and font-size-dependent),
        // not a fixed character count. Trimming by exact character count
        // instead removes precisely the excess regardless of wrap width.
        const int excess = m_charCount - kMaxCharCount;
        auto trimCursor = m_textOutput->textCursor();
        trimCursor.movePosition(QTextCursor::Start);
        trimCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, excess);
        trimCursor.removeSelectedText();
        m_charCount = m_textOutput->toPlainText().size();
    }
}

void RttyDecoderApplet::onStatsUpdated(float markLevel, float spaceLevel, float snrDb, bool locked)
{
    m_markLevel->setValue(markLevel);
    m_spaceLevel->setValue(spaceLevel);

    m_snrValue->setText(QStringLiteral("%1 dB").arg(snrDb, 0, 'f', 1));

    if (locked) {
        m_lockCapsule->setText(QStringLiteral("LOCK"));
        m_lockCapsule->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 10px; padding: 0 10px; font-size: 10px; }")
            .arg(Style::kGreenBg, Style::kGreenText, Style::kGreenBorder));
    } else {
        m_lockCapsule->setText(QStringLiteral("KEIN LOCK"));
        m_lockCapsule->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; color: %2; border: 1px solid %3;"
            " border-radius: 10px; padding: 0 10px; font-size: 10px; }")
            .arg(Style::kBadgeOffBg, Style::kTextTertiary, Style::kBorder));
    }
}

} // namespace Longpath
