#pragma once

// =================================================================
// src/core/TxAudioRecorder.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. The idea is AetherSDR's — its channel-strip
// monitor (src/core/ClientPuduMonitor.{h,cpp} @3a1f59e, GPLv3, same
// licence as this project) captures post-DSP transmit audio and plays
// it back so the operator can hear their own processing without keying.
// None of its code is reused: AetherSDR taps a FlexRadio DAX int16
// stream and plays through its own RX sink, where this taps WDSP's
// post-modulator float output. The shape of the answer is theirs.
//
// Record yourself, then listen to yourself.
//
// The live monitor answers "how do I sound"; this answers "how do I
// sound compared with a minute ago". Nobody can hold the timbre of
// their own voice in their head across a knob change, so a comparison
// has to be against a recording, not against a memory.
//
// Thirty seconds, in memory, one preallocated buffer. Long enough for
// a call and a test count, short enough that it costs nothing and can
// never fill a disk. Nothing is allocated once recording starts —
// feed() runs on the audio thread, where an allocation is a dropout.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include <QByteArray>
#include <QObject>
#include <QString>

#include <atomic>
#include <vector>

namespace NereusSDR {

class TxAudioRecorder : public QObject {
    Q_OBJECT
public:
    explicit TxAudioRecorder(QObject* parent = nullptr);

    static constexpr int kMaxSeconds = 30;

    // Sample rate of what will be fed. Changing it reallocates, so it
    // is refused while recording rather than resizing the buffer under
    // the audio thread's feet.
    void setSampleRate(int hz);
    int  sampleRate() const { return m_sampleRate; }

    bool isRecording() const
    { return m_recording.load(std::memory_order_acquire); }

    // How much is captured, in seconds.
    double recordedSeconds() const;
    bool   hasRecording() const { return recordedFrames() > 0; }
    int    recordedFrames() const
    { return static_cast<int>(m_written.load(std::memory_order_acquire)); }

    void start();
    void stop();
    void clear();

    // Audio thread. Mono float. Silently drops the tail once full and
    // reports through recordingFull() so the UI can stop itself — the
    // alternative, wrapping, would quietly replace the beginning of a
    // recording the operator is still speaking into.
    void feed(const float* samples, int frames) noexcept;

    // A 16-bit mono WAV of what was captured. Empty if nothing is.
    QByteArray toWav() const;
    bool saveWav(const QString& path, QString* error = nullptr) const;

signals:
    void recordingChanged(bool on);
    // The thirty seconds are used up. Emitted once per recording, from
    // the audio thread via a queued connection.
    void recordingFull();

private:
    std::vector<float> m_buffer;
    int m_sampleRate{48000};

    std::atomic<bool>     m_recording{false};
    std::atomic<size_t>   m_written{0};
    // Set when the buffer fills, so recordingFull() is emitted once and
    // not once per audio block for the rest of the session.
    std::atomic<bool>     m_announcedFull{false};
};

} // namespace NereusSDR
