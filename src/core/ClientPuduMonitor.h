#pragma once

// =================================================================
// src/core/ClientPuduMonitor.h  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/core/ClientPuduMonitor.{h,cpp} at 31b29583
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 §5.
//
// Asked for at the bench in the plainest terms after a full day of
// almost-right: "copy everything from Aether, A to Z, same structure."
// This is that copy — the record-then-listen monitor exactly as
// AetherSDR runs it: capture up to 30 seconds of the processed chain's
// output into a preallocated buffer, auto-play it back the moment
// recording stops, keep live RX muted from record start to playback
// end, drop a WAV in /tmp for offline inspection, and play through a
// pull-mode QAudioSink with a 300 ms ring so scheduler hiccups never
// chop the audio.
//
// Three named divergences, each forced by an AetherSDR subsystem that
// does not exist here (same device as EqHost — the borrowed logic is
// untouched, the missing infrastructure is adapted at the edge):
//
//   1. AudioDeviceNegotiator::formatLadder → a local four-rung ladder
//      (Int16@24k, Int16@48k, Float@48k, device preferred). Same
//      open-don't-ask policy: every rung is tried with a real
//      QAudioSink::start(), never isFormatSupported() alone.
//   2. AudioSummaryLogger → qCInfo/qCWarning(lcAudio) one-liners.
//   3. AudioOutputRouter device-follow → setOutputDevice() is kept and
//      public, but nothing drives it yet; playback prefers the system
//      default output exactly as upstream does when unrouted.
//
// The capture format is upstream's: int16 stereo 24 kHz. NereusSDR's
// post-strip tap is float mono 48 kHz; the FEEDING side converts (see
// MainWindow's adapter lambda), so this file stays comparable against
// its upstream.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Ported to NereusSDR by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork). Namespace AetherSDR →
//                 NereusSDR; includes rebased; divergences 1-3 above.
// =================================================================

#include <QAudio>
#include <QAudioDevice>
#include <QBuffer>
#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>

#include <atomic>
#include <cstdint>

class QAudioSink;

namespace NereusSDR {

// PUDU monitor — captures up to 30 seconds of post-DSP TX audio (the
// output of the full client-side chain) into an in-memory buffer, then
// plays it back so the user can hear what their chain is producing
// without keying the radio.  On stop a WAV snapshot is dropped into
// /tmp for offline inspection; playback itself reads the in-memory
// buffer, never the file.
//
// Threading:
//  - feedTxPostDsp() runs on the audio worker thread.  Single writer,
//    lock-free via atomics.  Bails out early when not recording.
//  - Everything else (start/stop, playback, signals) runs on the UI
//    thread.  Audio thread hands off via Qt queued invoke when the
//    30-second cap is reached.
class ClientPuduMonitor : public QObject {
    Q_OBJECT

public:
    explicit ClientPuduMonitor(QObject* parent = nullptr);
    ~ClientPuduMonitor() override = default;

    // UI-thread transitions.  All idempotent — calling start while
    // already started, or stop while already stopped, is a no-op.
    void startRecording();
    void stopRecording();
    void startPlayback();
    void stopPlayback();

    bool isRecording()  const noexcept { return m_recording.load(std::memory_order_acquire); }
    bool isPlaying()    const noexcept { return m_playing; }
    bool hasRecording() const noexcept { return m_recordedBytes > 0; }
    int  recordedMs()   const noexcept;

    // Audio output device the playback sink opens.  When null, falls
    // back to QMediaDevices::defaultAudioOutput().  (Divergence 3: kept
    // for parity, currently unrouted.)
    void setOutputDevice(const QAudioDevice& dev) { m_outputDevice = dev; }

    // Audio thread — appends int16 stereo 24 kHz PCM into the buffer
    // while recording.  No-op otherwise.  Stops itself and queues the
    // UI-thread auto-stop handler once the 30-s cap is reached.
    void feedTxPostDsp(const QByteArray& int16stereo) noexcept;

    // Sample-rate / channel-count assumed throughout the class.  Kept
    // as constants so the WAV writer and playback chunk sizes can
    // reference them directly.
    static constexpr int kSampleRate  = 24000;
    static constexpr int kChannels    = 2;
    static constexpr int kBytesPerFrame = kChannels * 2;
    static constexpr int kMaxSeconds  = 30;
    static constexpr int kMaxBytes    = kMaxSeconds * kSampleRate * kBytesPerFrame;

signals:
    // State transitions for the UI.
    void recordingStarted();
    void recordingStopped(int durationMs);
    void playbackStarted();
    void playbackStopped();
    // Ask MainWindow to toggle the live RX audio feed off/on.  True
    // = disconnect live RX (so we don't hear over our capture or
    // playback), false = restore it.  Fired at record start and
    // playback end; held across the record→auto-play transition.
    void muteRxRequested(bool mute);

private slots:
    void onAutoStop();
    void onPlaybackSinkState(QAudio::State state);

private:
    bool preparePlaybackPcm(int sinkRateHz);
    void writeWavFile();

    QByteArray m_buffer;      // preallocated kMaxBytes capture buffer
    QByteArray m_playPcm;     // playback payload (possibly resampled)
    QBuffer    m_playBuffer;  // pull-mode source the sink drains

    std::atomic<bool> m_recording{false};
    std::atomic<int>  m_writeBytes{0};
    int  m_recordedBytes{0};
    int  m_recordedMs{0};
    bool m_playing{false};

    QElapsedTimer m_recElapsed;
    QAudioSink*   m_playSink{nullptr};
    QAudioDevice  m_outputDevice;
};

} // namespace NereusSDR
