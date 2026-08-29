// no-port-check: NereusSDR/Longpath-original. See header for scope.

// =================================================================
// src/core/PlaybackRadioConnection.cpp  (NereusSDR/Longpath)
// =================================================================
//
// NereusSDR/Longpath-original. Scope and rationale in the header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "PlaybackRadioConnection.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {
Q_LOGGING_CATEGORY(lcPlayback, "longpath.playback")
}

PlaybackRadioConnection::PlaybackRadioConnection(QObject* parent)
    : RadioConnection(parent)
{
}

PlaybackRadioConnection::~PlaybackRadioConnection() = default;

void PlaybackRadioConnection::init()
{
    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setInterval(kTickMs);
    connect(m_playbackTimer, &QTimer::timeout,
            this, &PlaybackRadioConnection::onPlaybackTick);
}

bool PlaybackRadioConnection::openRecording(const QString& wavPath, QString* error)
{
    WavStereoData data = readWavStereo(wavPath, error);
    if (!data.ok) {
        qCWarning(lcPlayback) << "Playback: failed to read" << wavPath
                              << (error ? *error : QString());
        return false;
    }
    if (data.sampleRate <= 0) {
        if (error) { *error = QStringLiteral("recording has no usable sample rate"); }
        return false;
    }

    m_data = std::move(data);
    m_meta = readIqRecordingDescription(wavPath);
    // readIqRecordingDescription() reads sampleRate from the sidecar
    // JSON independently of the WAV header; if the sidecar is missing
    // or stale, trust the WAV header itself (m_data.sampleRate) since
    // that's what actually determines playback pacing below.
    if (m_meta.sampleRate <= 0) {
        m_meta.sampleRate = m_data.sampleRate;
    }

    m_frameCursor   = 0;
    m_emittedBlocks = 0;
    m_blockFrames   = std::max(1, static_cast<int>(std::lround(
        static_cast<double>(m_data.sampleRate) * kTickMs / 1000.0)));

    qCInfo(lcPlayback) << "Playback: loaded" << wavPath
                       << m_data.interleaved.size() / 2 << "frames at"
                       << m_data.sampleRate << "Hz, block"
                       << m_blockFrames << "frames /" << kTickMs << "ms";
    return true;
}

void PlaybackRadioConnection::connectToRadio(const RadioInfo& info)
{
    Q_UNUSED(info);  // nothing to discover — see class comment

    if (!m_data.ok) {
        qCWarning(lcPlayback) << "Playback: connectToRadio() called without "
                                 "a loaded recording — call openRecording() first";
        emit connectFailed(ConnectFailure::Unreachable,
                           QStringLiteral("no recording loaded"));
        return;
    }

    // No handshake to wait for — the "radio" is a file already sitting
    // on disk, not a network peer. Going straight to Connected is
    // honest here, not a shortcut: there is nothing to be Connecting
    // to.
    setState(ConnectionState::Connected);
    if (m_playbackTimer) {
        m_playbackTimer->start();
    }
}

void PlaybackRadioConnection::disconnect()
{
    if (m_playbackTimer) {
        m_playbackTimer->stop();
    }
    // Stop, not Pause — matches the project's no-pause convention
    // (design doc §8: Thetis itself has no record/playback pause,
    // and adding one would be a Longpath-original feature needing
    // explicit sign-off). A later connectToRadio() replays from the
    // start.
    m_frameCursor = 0;
    setState(ConnectionState::Disconnected);
}

void PlaybackRadioConnection::onPlaybackTick()
{
    emitNextBlock();
}

void PlaybackRadioConnection::emitNextBlock()
{
    const qint64 totalFrames = m_data.interleaved.size() / 2;
    if (totalFrames <= 0) { return; }

    qint64 framesLeft = totalFrames - m_frameCursor;
    if (framesLeft <= 0) {
        if (m_eofBehavior == EndOfFileBehavior::Loop) {
            m_frameCursor = 0;
            framesLeft = totalFrames;
        } else {
            // Stop cleanly — plan doc C.4. Same shape as a real
            // connection losing its source: stop the timer, drop to
            // Disconnected. No connectFailed() here; reaching the end
            // of a recording is normal completion, not a failure.
            if (m_playbackTimer) { m_playbackTimer->stop(); }
            m_frameCursor = 0;
            setState(ConnectionState::Disconnected);
            return;
        }
    }

    const int framesThisBlock = static_cast<int>(
        std::min<qint64>(m_blockFrames, framesLeft));

    QVector<float> block(framesThisBlock * 2);
    std::copy_n(m_data.interleaved.constData() + m_frameCursor * 2,
               framesThisBlock * 2, block.data());

    m_frameCursor += framesThisBlock;
    ++m_emittedBlocks;

    emit iqDataReceived(/*hwReceiverIndex=*/0, block);
    emit frameReceived();
}

// ── No-ops: playback replays a fixed recording, see the header ──────

void PlaybackRadioConnection::setReceiverFrequency(int, quint64) {}
void PlaybackRadioConnection::setActiveReceiverCount(int) {}
void PlaybackRadioConnection::setSampleRate(int) {}

// ── Safe no-ops: receive-only, see the header ────────────────────────

void PlaybackRadioConnection::setTxFrequency(quint64) {}
void PlaybackRadioConnection::setAttenuator(int) {}
void PlaybackRadioConnection::setPreamp(bool) {}
void PlaybackRadioConnection::setTxDrive(int) {}
void PlaybackRadioConnection::setMox(bool) {}
void PlaybackRadioConnection::setAntennaRouting(AntennaRouting) {}
void PlaybackRadioConnection::sendTxIq(const float*, int) {}
void PlaybackRadioConnection::setTrxRelay(bool) {}
void PlaybackRadioConnection::setMicBoost(bool) {}
void PlaybackRadioConnection::setLineIn(bool) {}
void PlaybackRadioConnection::setMicTipRing(bool) {}
void PlaybackRadioConnection::setMicBias(bool) {}
void PlaybackRadioConnection::setLineInGain(int) {}
void PlaybackRadioConnection::setUserDigOut(quint8) {}
void PlaybackRadioConnection::setPuresignalRun(bool) {}
void PlaybackRadioConnection::setMicPTTDisabled(bool) {}
void PlaybackRadioConnection::setMicXlr(bool) {}
void PlaybackRadioConnection::setWatchdogEnabled(bool) {}

} // namespace Longpath
