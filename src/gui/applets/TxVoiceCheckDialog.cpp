// =================================================================
// src/gui/applets/TxVoiceCheckDialog.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See the header for the shape and for why the
// measurement is taken where it is.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "gui/applets/TxVoiceCheckDialog.h"

#include "core/AudioEngine.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/TxChannel.h"
#include "core/TxWorkerThread.h"
#include "core/strip/StripChain.h"
#include "core/strip/StripTuner.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QAudio>
#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMediaDevices>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

QString secondaryLabelStyle()
{
    return QStringLiteral("QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(Style::kTextSecondary));
}

} // namespace

TxVoiceCheckDialog::TxVoiceCheckDialog(RadioModel* radio, QWidget* parent,
                                       bool embedded)
    : QDialog(parent)
    , m_embedded(embedded)
    , m_radio(radio)
{
    setWindowTitle(QStringLiteral("Voice check"));
    setModal(false);
    if (m_embedded) {
        // A QDialog dropped into a layout must not be a window, or Qt
        // floats it. This is the documented way to host one.
        setWindowFlags(Qt::Widget);
    }
    m_recorder.setSampleRate(kMicRateHz);
    // Both worker taps run at the mic rate by construction — they read
    // m_in before WDSP ever resamples anything.
    m_procRecorder.setSampleRate(kMicRateHz);
    buildUi();
    // The meter runs from the moment the window opens, not from the
    // moment the operator ticks a box. Whatever else is wrong, "is the
    // microphone reaching the transmit chain" should be answerable by
    // looking, and the answer must not depend on having found the right
    // control first.
    startLevelWatch();
    refreshButtons();
}

TxVoiceCheckDialog::~TxVoiceCheckDialog()
{
    // Order matters. The tap runs on the transmit worker thread and
    // hands a pointer straight to the recorder, so the gate is closed
    // first and the connection dropped second; destroying the recorder
    // with either still live is the one way this window could hurt
    // anything.
    if (m_radio && m_radio->txChannel()) {
        m_radio->txChannel()->setMicTapEnabled(false);
    }
    if (m_radio && m_radio->txWorker()) {
        m_radio->txWorker()->setVoiceTapEnabled(false);
    }
    QObject::disconnect(m_micTap);
    QObject::disconnect(m_procTap);
    QObject::disconnect(m_levelTap);
    stopPlayback();   // also lifts the RX quiet gate
}

// ── Building ─────────────────────────────────────────────────────────

void TxVoiceCheckDialog::buildUi()
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(16, 16, 16, 16);
    col->setSpacing(12);

    // ── Speak, then listen (2026-08-11, the AetherSDR way) ──────────
    //
    // The live "Hear myself" checkbox that used to sit here is gone,
    // asked for in plain words at the bench: real-time self-monitoring
    // through a block-batched network path carries latency and seams
    // no amount of buffering fully hides, and AetherSDR — where this
    // panel's whole workflow feels right — never does it. Its PUDU
    // monitor records the processed chain output and plays it back
    // (ClientPuduMonitor @31b29583); that is now the ONLY loop here:
    // record 15 s, hear it played back automatically, adjust, repeat.
    // (The on-air MON path for actual transmissions is untouched.)
    {
        auto* note = new QLabel(QStringLiteral(
            "Speak, then listen: recording captures your voice straight "
            "off the channel strip and plays it back to you when the "
            "countdown ends. Nothing is transmitted."), this);
        note->setWordWrap(true);
        note->setStyleSheet(secondaryLabelStyle());
        col->addWidget(note);

        // The meter is the honest part. If it moves, the microphone is
        // reaching the transmit chain and any silence is on the output
        // side; if it does not, nothing downstream can help.
        auto* meterRow = new QHBoxLayout;
        auto* meterCap = new QLabel(QStringLiteral("Mic"), this);
        meterCap->setStyleSheet(secondaryLabelStyle());
        meterRow->addWidget(meterCap);
        m_levelBar = new QProgressBar(this);
        m_levelBar->setRange(-60, 0);
        m_levelBar->setValue(-60);
        m_levelBar->setFormat(QStringLiteral("%v dBFS"));
        m_levelBar->setFixedHeight(14);
        meterRow->addWidget(m_levelBar, 1);
        col->addLayout(meterRow);

        m_liveStatus = new QLabel(this);
        m_liveStatus->setWordWrap(true);
        m_liveStatus->setStyleSheet(secondaryLabelStyle());
        col->addWidget(m_liveStatus);
    }

    // ── Recording ────────────────────────────────────────────────────
    {
        auto* ask = new QLabel(QStringLiteral(
            "Speak for %1 seconds as you would on the air — normal "
            "distance, normal level, a couple of sentences with pauses "
            "between them. The pauses matter: the noise floor can only "
            "be measured where you are not talking.")
                                   .arg(kRecordSeconds), this);
        ask->setWordWrap(true);
        col->addWidget(ask);

        auto* row = new QHBoxLayout;
        m_recordBtn = new QPushButton(this);
        m_recordBtn->setStyleSheet(Style::buttonBaseStyle());
        row->addWidget(m_recordBtn);

        m_progress = new QProgressBar(this);
        m_progress->setRange(0, kRecordSeconds);
        m_progress->setValue(0);
        m_progress->setTextVisible(true);
        row->addWidget(m_progress, 1);
        col->addLayout(row);

        // ── Listen back — the AetherSDR way ──────────────────────────
        //
        // Every take records both taps at once, so these two are an
        // A/B of the same words: what the microphone heard, and what
        // the strip + EQ made of it. Playback is from memory, off the
        // real-time path entirely — it cannot crackle, which is the
        // property that makes it fit to judge an EQ change with.
        auto* playRow = new QHBoxLayout;
        m_playRawBtn = new QPushButton(
            QStringLiteral("▶ Play mic (raw)"), this);
        m_playRawBtn->setStyleSheet(Style::buttonBaseStyle());
        m_playRawBtn->setToolTip(QStringLiteral(
            "The last take as the microphone heard it, before any "
            "processing. Press again to stop."));
        playRow->addWidget(m_playRawBtn);

        m_playProcBtn = new QPushButton(
            QStringLiteral("▶ Play after strip && EQ"), this);
        m_playProcBtn->setStyleSheet(Style::buttonBaseStyle());
        m_playProcBtn->setToolTip(QStringLiteral(
            "The same take through the whole transmit chain — strip, "
            "EQ, compressor, filter. Change a setting, record again, "
            "compare. Press again to stop."));
        playRow->addWidget(m_playProcBtn);
        playRow->addStretch(1);
        col->addLayout(playRow);
    }

    // ── What it found ────────────────────────────────────────────────
    m_findings = new QLabel(QStringLiteral("No recording yet."), this);
    m_findings->setWordWrap(true);
    m_findings->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_findings->setMinimumHeight(90);
    m_findings->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_findings->setStyleSheet(
        QStringLiteral("QLabel { background: %1; border: 1px solid %2; "
                       "padding: 10px; }")
            .arg(QString::fromLatin1(Style::kInsetBg),
                 QString::fromLatin1(Style::kInsetBorder)));
    col->addWidget(m_findings);

    {
        auto* row = new QHBoxLayout;
        // The strip first and named plainly, because it is the one the
        // operator can see a curve for. Setting two equalisers from two
        // windows is how a sound becomes impossible to explain — the
        // radio's own EQ is still available below, and says so.
        m_stripBtn = new QPushButton(
            QStringLiteral("Set up the channel strip"), this);
        m_stripBtn->setStyleSheet(Style::buttonBaseStyle());
        m_stripBtn->setToolTip(QStringLiteral(
            "Sets the high-pass, hum notches, tone, gate, de-esser and "
            "compressor from this measurement, and explains each one. "
            "Leaves the strip switched off."));
        row->addWidget(m_stripBtn);

        m_applyBtn = new QPushButton(
            QStringLiteral("…or just the radio's EQ"), this);
        m_applyBtn->setStyleSheet(Style::buttonBaseStyle());
        m_applyBtn->setToolTip(QStringLiteral(
            "The older path: writes the ten bands into WDSP's own "
            "transmit equaliser instead of the strip."));
        row->addWidget(m_applyBtn);

        m_advancedBtn = new QPushButton(QStringLiteral("Advanced ▸"), this);
        m_advancedBtn->setStyleSheet(Style::buttonBaseStyle());
        m_advancedBtn->setCheckable(true);
        row->addWidget(m_advancedBtn);
        row->addStretch(1);

        // Embedded in the strip's tab bar there is nothing to close —
        // the host window has its own button.
        if (!m_embedded) {
            auto* closeBtn = new QPushButton(QStringLiteral("Close"), this);
            closeBtn->setStyleSheet(Style::buttonBaseStyle());
            row->addWidget(closeBtn);
            connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
        }
        col->addLayout(row);
    }

    // ── Advanced ─────────────────────────────────────────────────────
    m_advanced = new QWidget(this);
    m_advanced->setVisible(false);
    {
        auto* acol = new QVBoxLayout(m_advanced);
        acol->setContentsMargins(0, 8, 0, 0);
        acol->setSpacing(8);

        m_bands = new QTableWidget(kVoiceBandCount, 4, m_advanced);
        m_bands->setHorizontalHeaderLabels({
            QStringLiteral("Band"), QStringLiteral("Measured"),
            QStringLiteral("Target"), QStringLiteral("Suggested")});
        m_bands->verticalHeader()->setVisible(false);
        m_bands->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_bands->setSelectionMode(QAbstractItemView::NoSelection);
        m_bands->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_bands->setMaximumHeight(280);
        acol->addWidget(m_bands);

        m_numbers = new QLabel(m_advanced);
        m_numbers->setWordWrap(true);
        m_numbers->setStyleSheet(secondaryLabelStyle());
        acol->addWidget(m_numbers);

        m_saveBtn = new QPushButton(
            QStringLiteral("Save the recording…"), m_advanced);
        m_saveBtn->setStyleSheet(Style::buttonBaseStyle());
        m_saveBtn->setToolTip(QStringLiteral(
            "A 16-bit WAV of exactly what was measured. Worth keeping "
            "before changing anything, so there is something to compare "
            "the next one against."));
        auto* srow = new QHBoxLayout;
        srow->addWidget(m_saveBtn);
        srow->addStretch(1);
        acol->addLayout(srow);
    }
    col->addWidget(m_advanced);

    // ── Wiring ───────────────────────────────────────────────────────

    m_countdown = new QTimer(this);
    m_countdown->setInterval(1000);

    m_levelTimer = new QTimer(this);
    m_levelTimer->setInterval(100);
    connect(m_levelTimer, &QTimer::timeout,
            this, &TxVoiceCheckDialog::updateLevel);

    connect(m_recordBtn, &QPushButton::clicked, this, [this]() {
        if (m_recorder.isRecording()) { stopRecording(); }
        else                          { startRecording(); }
    });

    connect(m_playRawBtn, &QPushButton::clicked, this, [this]() {
        if (m_playSink && !m_playingProcessed) { stopPlayback(); }
        else { playRecording(/*processed=*/false); }
    });
    connect(m_playProcBtn, &QPushButton::clicked, this, [this]() {
        if (m_playSink && m_playingProcessed) { stopPlayback(); }
        else { playRecording(/*processed=*/true); }
    });

    connect(m_countdown, &QTimer::timeout, this, [this]() {
        --m_secondsLeft;
        m_progress->setValue(kRecordSeconds - m_secondsLeft);
        if (m_secondsLeft <= 0) { stopRecording(); }
    });

    // The recorder drops the tail rather than wrapping, so a full buffer
    // means the rest would be silence pretending to be speech.
    connect(&m_recorder, &TxAudioRecorder::recordingFull, this,
            [this]() { stopRecording(); });

    connect(m_applyBtn, &QPushButton::clicked,
            this, &TxVoiceCheckDialog::applySuggestion);
    connect(m_stripBtn, &QPushButton::clicked,
            this, &TxVoiceCheckDialog::applyToStrip);
    connect(m_saveBtn, &QPushButton::clicked,
            this, &TxVoiceCheckDialog::saveRecording);
    connect(m_advancedBtn, &QPushButton::toggled, this, [this](bool on) {
        m_advanced->setVisible(on);
        m_advancedBtn->setText(on ? QStringLiteral("Advanced ▾")
                                  : QStringLiteral("Advanced ▸"));
        adjustSize();
    });
}

// ── Band quiet during the record/listen cycle ────────────────────────
//
// The AetherSDR shape (ClientPuduMonitor::muteRxRequested): live RX is
// silenced from record start until playback ends, so the operator's
// voice is captured against silence and heard against silence — hum,
// hiss and pumping all become audible instead of drowning in band
// noise. This uses the same RX-only gate the live monitor used, but
// there is no monitor any more: no off-air chain switch, no MON mix-in.

void TxVoiceCheckDialog::setRxQuiet(bool on)
{
    if (m_rxQuiet == on) { return; }
    m_rxQuiet = on;
    if (m_radio) {
        if (AudioEngine* ae = m_radio->audioEngine()) {
            ae->setRxMutedForMonitor(on);
        }
    }
}

// ── Saying why nothing is happening ──────────────────────────────────

QString TxVoiceCheckDialog::micSourceName() const
{
    if (!m_radio) { return QStringLiteral("unknown"); }
    switch (m_radio->transmitModel().micSource()) {
    case MicSource::Pc:    return QStringLiteral("the computer's microphone");
    case MicSource::Radio: return QStringLiteral("the radio's mic socket");
    case MicSource::Vax:   return QStringLiteral("the VAX virtual device");
    }
    return QStringLiteral("unknown");
}

void TxVoiceCheckDialog::startLevelWatch()
{
    if (!m_radio || !m_radio->txChannel()) {
        m_liveStatus->setText(QStringLiteral(
            "Not connected to a radio. The microphone is read through the "
            "radio's transmit chain, so there is nothing to listen to "
            "yet — connect first. This window will pick it up on its own "
            "once you do."));
        // The timer still runs. It is the only thing that will notice
        // the radio arriving: this window has no other way to hear
        // about a connection, and the version without this sat dead for
        // the rest of the session — meter frozen, status line stale,
        // while recording worked perfectly beside it.
        m_tapsArmed = false;
        if (m_levelTimer && !m_levelTimer->isActive()) { m_levelTimer->start(); }
        return;
    }
    TxChannel* tx = m_radio->txChannel();

    m_micBlocks.store(0, std::memory_order_release);
    m_micPeakMicros.store(0, std::memory_order_release);
    m_watchTicks = 0;

    // Both taps are DirectConnection for the same reason as everywhere
    // else in this file: the pointers belong to the emitting channel's
    // scratch buffers and are gone by the next block. Neither lambda
    // does anything but arithmetic on atomics.
    QObject::disconnect(m_levelTap);
    m_levelTap = connect(tx, &TxChannel::micInputReady, this,
                         [this](const float* samples, int frames) {
        m_micBlocks.fetch_add(1, std::memory_order_acq_rel);
        float peak = 0.0f;
        for (int i = 0; i < frames; ++i) {
            peak = std::max(peak, std::fabs(samples[i]));
        }
        const auto micros = static_cast<unsigned>(
            std::min(1.0f, peak) * 1'000'000.0f);
        unsigned prev = m_micPeakMicros.load(std::memory_order_acquire);
        while (micros > prev
               && !m_micPeakMicros.compare_exchange_weak(
                      prev, micros, std::memory_order_acq_rel)) {
        }
    }, Qt::DirectConnection);

    // The tap has to be on for the meter to see anything. It is off by
    // default and costs a float conversion per block; while this window
    // is listening, that is a price worth paying for being able to say
    // what is wrong.
    tx->setMicTapEnabled(true);

    m_tapsArmed = true;
    m_liveStatus->setText(QStringLiteral("Listening…"));
    if (!m_levelTimer->isActive()) { m_levelTimer->start(); }
}

void TxVoiceCheckDialog::stopLevelWatch()
{
    m_tapsArmed = false;
    m_levelTimer->stop();
    QObject::disconnect(m_levelTap);
    if (m_radio && m_radio->txChannel() && !m_recorder.isRecording()) {
        m_radio->txChannel()->setMicTapEnabled(false);
    }
    if (m_levelBar) { m_levelBar->setValue(-60); }
    if (m_liveStatus) { m_liveStatus->clear(); }
}

void TxVoiceCheckDialog::updateLevel()
{
    // Arm late if the radio was connected after this window opened.
    if (!m_tapsArmed) {
        if (m_radio && m_radio->txChannel()) { startLevelWatch(); }
        return;
    }

    const unsigned micros = m_micPeakMicros.exchange(
        0, std::memory_order_acq_rel);
    const double peak = double(micros) / 1'000'000.0;
    const int dbfs = peak > 0.0
        ? int(std::lround(std::clamp(20.0 * std::log10(peak), -60.0, 0.0)))
        : -60;
    m_levelBar->setValue(dbfs);
    m_peakSeenDb = std::max(m_peakSeenDb, double(dbfs));

    // Give it a moment before complaining. The pump is woken by mic
    // frames from the radio, so the first block can be a few tens of
    // milliseconds behind the checkbox.
    ++m_watchTicks;
    if (m_watchTicks < 12) { return; }          // 1.2 s

    const unsigned mic = m_micBlocks.load(std::memory_order_acquire);

    // First broken link, not the last. Each of these is a different
    // thing to go and do, and naming two at once helps nobody.
    if (mic == 0) {
        m_liveStatus->setText(QStringLiteral(
            "No microphone blocks are arriving. The transmit chain is "
            "fed from %1 — if your headset is plugged into the computer, "
            "that has to be selected under Setup ▸ Audio ▸ TX input, and "
            "the input device opened there.").arg(micSourceName()));
        return;
    }
    // Only worth saying when there is nothing to hear. Reported from the
    // bench: this claimed the microphone was muted while the meter read
    // -18 dBFS and the operator could hear themselves. A status line
    // that contradicts the meter beside it teaches people to ignore
    // both, so the flag is now a possible explanation for silence
    // rather than an announcement in its own right.
    if (peak <= 0.0 && m_radio && m_radio->transmitModel().micMute()) {
        m_liveStatus->setText(QStringLiteral(
            "Nothing is arriving and the microphone is muted — unmute it "
            "in the TX panel."));
        return;
    }
    if (peak <= 0.0 && m_watchTicks > 30) {
        m_liveStatus->setText(QStringLiteral(
            "The chain is running and blocks are arriving from %1, but "
            "they are silent. Check that this is the right input and "
            "that its level is up.").arg(micSourceName()));
        return;
    }
    m_liveStatus->setText(QStringLiteral(
        "Hearing you through %1 — ready to record.").arg(micSourceName()));
}

// ── Recording ────────────────────────────────────────────────────────

void TxVoiceCheckDialog::startRecording()
{
    if (!m_radio || !m_radio->txChannel()) {
        m_findings->setText(QStringLiteral(
            "No transmit channel. Connect to the radio first — the "
            "microphone is read through it."));
        return;
    }
    TxChannel* tx = m_radio->txChannel();
    TxWorkerThread* worker = m_radio->txWorker();
    if (!worker) {
        m_findings->setText(QStringLiteral(
            "The transmit pump is not running yet — connect to the radio "
            "first."));
        return;
    }

    m_haveResult = false;
    stopPlayback();
    m_recorder.clear();
    m_procRecorder.clear();
    m_micBlocksAtStart = m_micBlocks.load(std::memory_order_acquire);
    m_peakSeenDb = -60.0;

    // ── The AetherSDR shape, both halves (2026-08-11) ────────────────
    //
    // Both takes come from the WORKER's pre/post-strip taps — the audio
    // domain, full bandwidth, before WDSP's DEXP, ALC and the 2.7 kHz
    // TX filter ever see the block. The old sip1 source sat BEHIND all
    // of that, which is why every take sounded dull and pumped no
    // matter what the strip did. No off-air chain switch is needed any
    // more: the worker runs whenever mic frames arrive.
    //
    // DirectConnection is mandatory, not a preference: the tap hands
    // out a pointer to the worker's scratch, overwritten next block.
    QObject::disconnect(m_micTap);
    m_micTap = connect(worker, &TxWorkerThread::preStripAudioReady,
                       &m_recorder,
                       [this](const float* samples, int frames) {
        m_recorder.feed(samples, frames);
    }, Qt::DirectConnection);

    QObject::disconnect(m_procTap);
    m_procTap = connect(worker, &TxWorkerThread::postStripAudioReady,
                        &m_procRecorder,
                        [this](const float* samples, int frames) {
        m_procRecorder.feed(samples, frames);
    }, Qt::DirectConnection);

    worker->setVoiceTapEnabled(true);
    tx->setMicTapEnabled(true);   // the level meter's tap, unchanged

    // Quiet the band for the whole record-then-listen cycle, exactly
    // as AetherSDR's monitor does: captured against silence, heard
    // against silence. Lifted when playback finishes.
    setRxQuiet(true);

    m_recorder.start();
    m_procRecorder.start();

    m_secondsLeft = kRecordSeconds;
    m_progress->setValue(0);
    m_countdown->start();
    m_findings->setText(QStringLiteral(
        "Listening… speak normally. Playback starts by itself when the "
        "countdown ends."));
    refreshButtons();
}

void TxVoiceCheckDialog::stopRecording()
{
    // The countdown reaching zero and the buffer filling up can both
    // arrive; without this the analysis would run twice and the second
    // run would report on a recording that had already been cleared.
    if (!m_recorder.isRecording() && !m_countdown->isActive()) { return; }
    m_countdown->stop();
    m_recorder.stop();
    m_procRecorder.stop();

    if (m_radio && m_radio->txWorker()) {
        m_radio->txWorker()->setVoiceTapEnabled(false);
    }
    QObject::disconnect(m_micTap);
    QObject::disconnect(m_procTap);

    m_progress->setValue(kRecordSeconds);
    runAnalysis();
    refreshButtons();

    // The AetherSDR loop closes itself: the take plays back the moment
    // it ends, processed side first — speak, then listen, no third
    // click in between. The band stays quiet until playback finishes.
    if (m_procRecorder.hasRecording()) {
        playRecording(/*processed=*/true);
    } else {
        setRxQuiet(false);
    }
}

// ── Playback ─────────────────────────────────────────────────────────
//
// The AetherSDR shape (ClientPuduMonitor @3a1f59e): a memory buffer
// through a QAudioSink on the UI thread. Nothing here touches the
// real-time path, which is the whole point — a recording judged
// through a crackling live monitor is a recording judged wrong.

void TxVoiceCheckDialog::playRecording(bool processed)
{
    stopPlayback();

    const TxAudioRecorder& rec = processed ? m_procRecorder : m_recorder;
    if (!rec.hasRecording() || rec.isRecording()) { return; }

    // Band quiet while listening — the other half of the AetherSDR
    // contract. Lifted in stopPlayback (which every path ends in).
    setRxQuiet(true);
    const float* src = rec.samples();
    if (src == nullptr) { return; }
    const int frames = rec.recordedFrames();

    // Mono float → mono int16, once, into a member the QBuffer wraps
    // without copying.
    m_playPcm.resize(frames * int(sizeof(qint16)));
    auto* dst = reinterpret_cast<qint16*>(m_playPcm.data());
    for (int i = 0; i < frames; ++i) {
        const float v = std::clamp(src[i], -1.0f, 1.0f);
        dst[i] = static_cast<qint16>(std::lround(v * 32767.0f));
    }

    QAudioFormat fmt;
    fmt.setSampleRate(rec.sampleRate());
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);

    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (!dev.isFormatSupported(fmt)) {
        m_findings->setText(QStringLiteral(
            "The output device refused 48 kHz mono — playback is not "
            "possible on it. The recording itself is fine and can be "
            "saved from Advanced."));
        return;
    }

    m_playBuf = new QBuffer(&m_playPcm, this);
    m_playBuf->open(QIODevice::ReadOnly);
    m_playSink = new QAudioSink(dev, fmt, this);
    m_playingProcessed = processed;
    connect(m_playSink, &QAudioSink::stateChanged, this,
            [this](QAudio::State s) {
        if (s == QAudio::IdleState || s == QAudio::StoppedState) {
            stopPlayback();
        }
    });
    m_playSink->start(m_playBuf);
    refreshButtons();
}

void TxVoiceCheckDialog::stopPlayback()
{
    // Every playback path ends here, so this is where the band comes
    // back — mirrors ClientPuduMonitor lifting its RX mute on playback
    // end, whatever ended it.
    setRxQuiet(false);
    if (m_playSink) {
        // Null the member BEFORE stop(): stop() re-emits stateChanged
        // synchronously, and the handler calls back in here.
        QAudioSink* sink = m_playSink;
        m_playSink = nullptr;
        sink->disconnect(this);
        sink->stop();
        sink->deleteLater();
    }
    if (m_playBuf) {
        m_playBuf->close();
        m_playBuf->deleteLater();
        m_playBuf = nullptr;
    }
    // Buttons exist for every call after buildUi(); the destructor path
    // may arrive here with the UI already half-gone.
    if (m_playRawBtn && m_playProcBtn) { refreshButtons(); }
}

// ── Analysis ─────────────────────────────────────────────────────────

void TxVoiceCheckDialog::runAnalysis()
{
    if (!m_recorder.hasRecording()) {
        m_findings->setText(QStringLiteral(
            "Nothing was recorded. If the radio is connected and the "
            "microphone is selected in Setup, check that the mic isn't "
            "muted."));
        m_haveResult = false;
        return;
    }

    const float* mono = m_recorder.samples();
    const int frames  = m_recorder.recordedFrames();
    if (mono == nullptr || frames <= 0) {
        m_findings->setText(QStringLiteral("The recording was empty."));
        m_haveResult = false;
        return;
    }

    m_result = VoiceAnalyzer::analyse(mono, frames,
                                      m_recorder.sampleRate());
    m_haveResult = m_result.valid;

    showFindings();
    fillAdvancedTable();
}

void TxVoiceCheckDialog::showFindings()
{
    if (!m_result.valid) {
        // The bare refusal is not enough. "Recording is too short" after
        // a full fifteen seconds is a contradiction from the operator's
        // side, and the only way to tell a silent microphone from a
        // microphone that never arrived is to print what was counted.
        const QString why = m_result.problem.isEmpty()
            ? QStringLiteral("The recording could not be analysed.")
            : m_result.problem;
        m_findings->setText(QStringLiteral(
            "%1\n\nWhat arrived: %2 s of audio (%3 samples) from %4 mic "
            "blocks, peak %5 dBFS.\n"
            "At 48 kHz a full %6 s take is about %7 blocks — anything far "
            "short of that means the microphone is not reaching the "
            "transmit chain, and the source is the place to look.")
                .arg(why)
                .arg(m_recorder.recordedSeconds(), 0, 'f', 2)
                .arg(m_recorder.recordedFrames())
                .arg(m_micBlocks.load(std::memory_order_acquire) - m_micBlocksAtStart)
                .arg(m_peakSeenDb, 0, 'f', 1)
                .arg(kRecordSeconds)
                .arg(kRecordSeconds * kMicRateHz / 64));
        return;
    }
    if (m_result.findings.isEmpty()) {
        m_findings->setText(QStringLiteral(
            "Nothing worth changing. Your audio measures close to the "
            "target already."));
        return;
    }
    QString text;
    for (const QString& line : m_result.findings) {
        text += QStringLiteral("• %1\n\n").arg(line);
    }
    m_findings->setText(text.trimmed());
}

void TxVoiceCheckDialog::fillAdvancedTable()
{
    if (!m_bands) { return; }

    for (int i = 0; i < kVoiceBandCount; ++i) {
        const double hz = kVoiceBandHz[static_cast<size_t>(i)];
        const QString name = hz >= 1000.0
            ? QStringLiteral("%1 kHz").arg(hz / 1000.0, 0, 'g', 2)
            : QStringLiteral("%1 Hz").arg(hz, 0, 'f', 0);

        auto put = [this, i](int colIdx, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_bands->setItem(i, colIdx, item);
        };
        auto* nameItem = new QTableWidgetItem(name);
        m_bands->setItem(i, 0, nameItem);

        if (!m_result.valid) {
            put(1, QStringLiteral("—"));
            put(2, QStringLiteral("%1")
                       .arg(VoiceAnalyzer::targetDb(hz), 0, 'f', 1));
            put(3, QStringLiteral("—"));
            continue;
        }
        put(1, QStringLiteral("%1 dB")
                   .arg(m_result.bandDb[static_cast<size_t>(i)], 0, 'f', 1));
        put(2, QStringLiteral("%1 dB")
                   .arg(VoiceAnalyzer::targetDb(hz), 0, 'f', 1));
        put(3, QStringLiteral("%1 dB")
                   .arg(m_result.suggestedEqDb[static_cast<size_t>(i)],
                        0, 'f', 1));
    }

    if (!m_result.valid) {
        m_numbers->setText(QStringLiteral(
            "Measured levels are relative to the 1 kHz band."));
        return;
    }

    QStringList bits;
    bits << QStringLiteral("Speech analysed: %1 s of %2 s (%3%)")
                .arg(m_result.analysedSeconds, 0, 'f', 1)
                .arg(m_recorder.recordedSeconds(), 0, 'f', 1)
                .arg(m_result.speechFraction * 100.0, 0, 'f', 0);
    bits << QStringLiteral("Speech %1 dBFS, noise floor %2 dBFS "
                           "(%3 dB apart)")
                .arg(m_result.speechDbFs, 0, 'f', 1)
                .arg(m_result.noiseFloorDbFs, 0, 'f', 1)
                .arg(m_result.speechDbFs - m_result.noiseFloorDbFs, 0, 'f', 1);
    bits << QStringLiteral("Crest factor %1 dB")
                .arg(m_result.crestFactorDb, 0, 'f', 1);
    if (m_result.humBaseHz > 0) {
        bits << QStringLiteral("Hum at %1 Hz, %2 dB below speech")
                    .arg(m_result.humBaseHz).arg(-m_result.humDb, 0, 'f', 1);
    } else {
        bits << QStringLiteral("No mains hum found");
    }
    bits << QStringLiteral("Sibilance %1 dB (5-8 kHz against 1-3 kHz)")
                .arg(m_result.sibilanceDb, 0, 'f', 1);
    bits << QStringLiteral("Clipped samples: %1").arg(m_result.clippedSamples);
    bits << QStringLiteral("Suggested make-up gain %1 dB")
                .arg(m_result.suggestedPreampDb, 0, 'f', 1);
    bits << QStringLiteral("Levels are relative to the 1 kHz band; the "
                           "suggestion only ever cuts.");
    m_numbers->setText(bits.join(QStringLiteral("\n")));
}

// ── Applying ─────────────────────────────────────────────────────────

void TxVoiceCheckDialog::applySuggestion()
{
    if (!m_radio || !m_haveResult) { return; }

    TransmitModel& tx = m_radio->transmitModel();

    // Round once, here, and show the operator the rounded values in the
    // same breath. The model takes whole decibels; a dialog that
    // reported -6.4 dB and set -6 would be lying by a small amount every
    // time, which is the kind of thing that makes people distrust the
    // whole feature.
    QStringList applied;
    for (int i = 0; i < kVoiceBandCount; ++i) {
        const int dB = std::clamp(
            int(std::lround(m_result.suggestedEqDb[static_cast<size_t>(i)])),
            TransmitModel::kTxEqBandDbMin, TransmitModel::kTxEqBandDbMax);
        tx.setTxEqBand(i, dB);
        if (dB != 0) {
            applied << QStringLiteral("%1 %2 dB")
                           .arg(kVoiceBandHz[static_cast<size_t>(i)], 0, 'f', 0)
                           .arg(dB);
        }
    }
    const int preamp = std::clamp(
        int(std::lround(m_result.suggestedPreampDb)),
        TransmitModel::kTxEqPreampDbMin, TransmitModel::kTxEqPreampDbMax);
    tx.setTxEqPreamp(preamp);
    tx.setTxEqEnabled(true);

    m_findings->setText(
        applied.isEmpty()
            ? QStringLiteral("Applied: the equaliser is now flat, with "
                             "%1 dB of make-up gain. Nothing needed "
                             "cutting.").arg(preamp)
            : QStringLiteral("Applied, and the equaliser is switched on:\n"
                             "%1\nMake-up gain %2 dB.\n\n"
                             "Listen with \"Hear myself\", and record "
                             "again to see what changed.")
                  .arg(applied.join(QStringLiteral(", "))).arg(preamp));
}

void TxVoiceCheckDialog::applyToStrip()
{
    if (!m_radio || !m_haveResult) { return; }
    StripChain* chain = m_radio->stripChain();
    if (!chain) {
        m_findings->setText(QStringLiteral(
            "There is no channel strip — it is created with the transmit "
            "pump, so connect to the radio first."));
        return;
    }

    const StripTuner::Result r = StripTuner::applyAnalysis(m_result, *chain);

    QString text;
    for (const QString& n : r.notes) {
        text += QStringLiteral("• %1\n\n").arg(n);
    }
    if (r.changed) {
        text += QStringLiteral(
            "Open Tools ▸ Channel strip to hear it, switch it on there, "
            "and use A/B to compare. Then record again — the second "
            "measurement is the one that tells you whether any of this "
            "helped.");
    }
    m_findings->setText(text.trimmed());
}

void TxVoiceCheckDialog::saveRecording()
{
    if (!m_recorder.hasRecording()) { return; }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save recording"),
        QStringLiteral("nereus-voice.wav"),
        QStringLiteral("WAV (*.wav)"));
    if (path.isEmpty()) { return; }

    QString err;
    if (!m_recorder.saveWav(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Voice check"),
            QStringLiteral("Couldn't save the recording:\n%1").arg(err));
    }
}

void TxVoiceCheckDialog::refreshButtons()
{
    const bool recording = m_recorder.isRecording();
    m_recordBtn->setText(recording
        ? QStringLiteral("Stop")
        : QStringLiteral("Record %1 s and analyse").arg(kRecordSeconds));
    m_applyBtn->setEnabled(m_haveResult && !recording);
    if (m_stripBtn) { m_stripBtn->setEnabled(m_haveResult && !recording); }
    if (m_saveBtn) { m_saveBtn->setEnabled(m_recorder.hasRecording()); }

    const bool playing = (m_playSink != nullptr);
    if (m_playRawBtn) {
        m_playRawBtn->setEnabled(!recording && m_recorder.hasRecording());
        m_playRawBtn->setText(playing && !m_playingProcessed
            ? QStringLiteral("■ Stop")
            : QStringLiteral("▶ Play mic (raw)"));
    }
    if (m_playProcBtn) {
        m_playProcBtn->setEnabled(!recording
                                  && m_procRecorder.hasRecording());
        m_playProcBtn->setText(playing && m_playingProcessed
            ? QStringLiteral("■ Stop")
            : QStringLiteral("▶ Play after strip && EQ"));
    }
}

} // namespace Longpath
