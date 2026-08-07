// =================================================================
// src/core/TxAudioRecorder.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see TxAudioRecorder.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "TxAudioRecorder.h"

#include <QFile>
#include <QMetaObject>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace NereusSDR {

namespace {

void appendLe32(QByteArray& b, quint32 v)
{
    const char bytes[4] = {char(v & 0xff), char((v >> 8) & 0xff),
                           char((v >> 16) & 0xff), char((v >> 24) & 0xff)};
    b.append(bytes, 4);
}

void appendLe16(QByteArray& b, quint16 v)
{
    const char bytes[2] = {char(v & 0xff), char((v >> 8) & 0xff)};
    b.append(bytes, 2);
}

} // namespace

TxAudioRecorder::TxAudioRecorder(QObject* parent) : QObject(parent)
{
    setSampleRate(m_sampleRate);
}

void TxAudioRecorder::setSampleRate(int hz)
{
    if (hz <= 0) { return; }
    // Refused rather than honoured while recording: resizing the buffer
    // under the audio thread would be a use-after-free at best.
    if (isRecording()) { return; }

    m_sampleRate = hz;
    m_buffer.assign(static_cast<size_t>(hz) * kMaxSeconds, 0.0f);
    m_written.store(0, std::memory_order_release);
    m_announcedFull.store(false, std::memory_order_release);
}

double TxAudioRecorder::recordedSeconds() const
{
    if (m_sampleRate <= 0) { return 0.0; }
    return static_cast<double>(recordedFrames()) / m_sampleRate;
}

void TxAudioRecorder::start()
{
    if (isRecording()) { return; }
    // Each recording starts empty. Appending to the last one would make
    // "record, listen, adjust, record again" compare a new take against
    // a mixture of both.
    m_written.store(0, std::memory_order_release);
    m_announcedFull.store(false, std::memory_order_release);
    m_recording.store(true, std::memory_order_release);
    emit recordingChanged(true);
}

void TxAudioRecorder::stop()
{
    if (!m_recording.exchange(false, std::memory_order_acq_rel)) { return; }
    emit recordingChanged(false);
}

void TxAudioRecorder::clear()
{
    stop();
    m_written.store(0, std::memory_order_release);
    m_announcedFull.store(false, std::memory_order_release);
}

void TxAudioRecorder::feed(const float* samples, int frames) noexcept
{
    if (!samples || frames <= 0) { return; }
    if (!m_recording.load(std::memory_order_acquire)) { return; }

    const size_t cap = m_buffer.size();
    const size_t at  = m_written.load(std::memory_order_acquire);
    if (at >= cap) {
        // Full. Announce once, then stay quiet — emitting per block
        // would flood the event loop for as long as the operator keeps
        // talking.
        if (!m_announcedFull.exchange(true, std::memory_order_acq_rel)) {
            QMetaObject::invokeMethod(this, [this]() { emit recordingFull(); },
                                      Qt::QueuedConnection);
        }
        return;
    }

    const size_t room = cap - at;
    const size_t n = std::min(room, static_cast<size_t>(frames));
    std::memcpy(m_buffer.data() + at, samples, n * sizeof(float));
    m_written.store(at + n, std::memory_order_release);
}

QByteArray TxAudioRecorder::toWav() const
{
    const int frames = recordedFrames();
    if (frames <= 0) { return {}; }

    const quint32 dataBytes = static_cast<quint32>(frames) * 2;   // 16-bit mono

    QByteArray wav;
    wav.reserve(static_cast<int>(dataBytes) + 44);
    wav.append("RIFF", 4);
    appendLe32(wav, 36 + dataBytes);
    wav.append("WAVE", 4);
    wav.append("fmt ", 4);
    appendLe32(wav, 16);                                   // PCM chunk size
    appendLe16(wav, 1);                                    // PCM
    appendLe16(wav, 1);                                    // mono
    appendLe32(wav, static_cast<quint32>(m_sampleRate));
    appendLe32(wav, static_cast<quint32>(m_sampleRate) * 2); // byte rate
    appendLe16(wav, 2);                                    // block align
    appendLe16(wav, 16);                                   // bits
    wav.append("data", 4);
    appendLe32(wav, dataBytes);

    for (int i = 0; i < frames; ++i) {
        // Clamp before scaling. A sample above 1.0 — which the transmit
        // chain can legitimately produce before the limiter — would
        // otherwise wrap to full-scale negative and put a click in the
        // recording exactly where the operator was loudest.
        const float v = std::clamp(m_buffer[static_cast<size_t>(i)],
                                   -1.0f, 1.0f);
        appendLe16(wav, static_cast<quint16>(
                       static_cast<qint16>(std::lround(v * 32767.0f))));
    }
    return wav;
}

bool TxAudioRecorder::saveWav(const QString& path, QString* error) const
{
    const QByteArray wav = toWav();
    if (wav.isEmpty()) {
        if (error) { *error = QStringLiteral("Nothing recorded yet"); }
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) { *error = f.errorString(); }
        return false;
    }
    return f.write(wav) == wav.size();
}

} // namespace NereusSDR
