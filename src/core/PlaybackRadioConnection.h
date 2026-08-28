#pragma once

// no-port-check: NereusSDR/Longpath-original RadioConnection subclass.
// No Thetis precedent exists for this pattern — see the design doc's
// own §6/§7.3 finding ("No precedent exists for this pattern... nobody
// has built a RadioConnection subclass that isn't discovery-driven").
// Thetis's own playback mechanism (PlayWave.wplay(), a DSP-internal
// buffer overwrite) has no analogue here; this class is Longpath-
// original architecture, not a port.

// =================================================================
// src/core/PlaybackRadioConnection.h  (NereusSDR/Longpath)
// =================================================================
//
// A file-backed `RadioConnection` — Phase 3M-C, "Option A" from the
// design doc (operator decision, 2026-08-25): reads a previously
// recorded I/Q file (IqRecorder, Phase 3M-B) and re-emits it as
// `iqDataReceived(hwReceiverIndex, samples)`, entering the exact same
// feedIqData -> audio/FFT fan-out real hardware uses. No downstream
// consumer needs to know or care that the samples came from a file,
// not a radio.
//
// Design doc: docs/architecture/phase3m-recording-design.md §6, §7.3
// Plan doc:   docs/architecture/phase3m-recording-plan.md §Phase C
//
// ── Non-discovery construction (plan doc C.3) ────────────────────────
//
// This connection is never produced by RadioConnection::create() —
// there is no ProtocolVersion for "a WAV file", and inventing one
// would misrepresent what this is (a dev/review tool, not a wire
// protocol). A caller constructs it directly and calls openRecording()
// with a file path *before* connectToRadio() — connectToRadio() is
// still implemented (it's a pure virtual on the base class every
// sibling honours), but here it just starts emitting from whatever
// openRecording() already loaded; the RadioInfo it receives is
// ignored, since there is nothing to discover.
//
// ── Receive-only (matches every other read-only connection here) ────
//
// Every TX/PTT/mic-jack/PureSignal pure virtual is a one-line no-op —
// a recording has no transmitter to key.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "RadioConnection.h"
#include "core/audio/IqRecorder.h"   // IqRecordingInfo + readIqRecordingDescription
#include "core/audio/WavFile.h"      // WavStereoData

#include <QTimer>

namespace Longpath {

class PlaybackRadioConnection : public RadioConnection {
    Q_OBJECT

public:
    explicit PlaybackRadioConnection(QObject* parent = nullptr);
    ~PlaybackRadioConnection() override;

    // Distinct from every real protocolVersion() (P1=1, P2=2, SunSdr=3)
    // and from the base class default (1) — a real value a test or a
    // future "what kind of connection is this" UI check can key off of
    // without reaching for dynamic_cast.
    int protocolVersion() const override { return 0; }

    // Plan doc C.4: stop cleanly at end-of-file (the default — matches
    // how a file player normally behaves) or loop back to the start.
    // Not resolved in the design doc; this is the small, low-stakes
    // implementation choice the plan doc left for here.
    enum class EndOfFileBehavior { Stop, Loop };
    void setEndOfFileBehavior(EndOfFileBehavior b) { m_eofBehavior = b; }
    EndOfFileBehavior endOfFileBehavior() const { return m_eofBehavior; }

    // ── Non-discovery construction (plan doc C.1, C.3) ───────────────
    //
    // Loads the WAV + its readIqRecordingDescription() sidecar JSON
    // (IqRecorder's own format, Phase 3M-B — see that class for the
    // field layout) and prepares playback, but does NOT start emitting
    // yet and does NOT touch ConnectionState — call connectToRadio()
    // (with any RadioInfo; it's ignored) to actually start. Returns
    // false and sets *error if the file can't be read or isn't a
    // recognizable recording.
    bool openRecording(const QString& wavPath, QString* error = nullptr);

    bool isRecordingLoaded() const { return m_data.ok; }
    const IqRecordingInfo& recordingInfo() const { return m_meta; }

    // Test-only accessors, same pattern as every other RadioConnection
    // in this project (P1/P2/SunSdr all have a *ForTest() seam).
    qint64 frameCursorForTest() const { return m_frameCursor; }
    int    emittedBlockCountForTest() const { return m_emittedBlocks; }

public slots:
    void init() override;
    // RadioInfo is ignored — see the class comment. A recording must
    // already be loaded via openRecording() or this fails exactly like
    // a real radio that can't be reached: emits connectFailed() and
    // stays Disconnected.
    void connectToRadio(const Longpath::RadioInfo& info) override;
    void disconnect() override;

    // No-ops: playback replays a fixed recording, it isn't a tunable
    // receiver. Accepted (not rejected/warned) because a generic
    // caller wiring the same signal path it uses for a real radio
    // should not need a special case for this connection type.
    void setReceiverFrequency(int receiverIndex, quint64 frequencyHz) override;
    void setActiveReceiverCount(int count) override;
    void setSampleRate(int sampleRate) override;

    // ── Safe no-ops: receive-only, see the class comment ─────────────
    void setTxFrequency(quint64 frequencyHz) override;
    void setAttenuator(int dB) override;
    void setPreamp(bool enabled) override;
    void setTxDrive(int level) override;
    void setMox(bool enabled) override;
    void setAntennaRouting(AntennaRouting routing) override;
    void sendTxIq(const float* iq, int n) override;
    void setTrxRelay(bool enabled) override;
    void setMicBoost(bool on) override;
    void setLineIn(bool on) override;
    void setMicTipRing(bool tipHot) override;
    void setMicBias(bool on) override;
    void setLineInGain(int gain) override;
    void setUserDigOut(quint8 dig) override;
    void setPuresignalRun(bool run) override;
    void setMicPTTDisabled(bool disabled) override;
    void setMicXlr(bool xlrJack) override;
    void setWatchdogEnabled(bool enabled) override;

private slots:
    void onPlaybackTick();

private:
    // 10 ms tick, matching AudioEngine's own drain-timer cadence
    // (CLAUDE.md "Data Flow": "10ms timer drain") — no Thetis/hardware
    // precedent to follow here (design doc §7.3: this is Longpath-
    // original), so this reuses an already-vetted real-time pacing
    // interval already proven to work smoothly in this codebase,
    // rather than inventing a new one. Block size is derived from the
    // recording's own sample rate so playback runs at the same real-
    // time pace it was captured at, regardless of what rate that was.
    static constexpr int kTickMs = 10;

    void emitNextBlock();

    QTimer*        m_playbackTimer{nullptr};
    WavStereoData  m_data;    // interleaved I,Q,I,Q… loaded by openRecording()
    IqRecordingInfo m_meta;
    qint64         m_frameCursor{0};
    int            m_blockFrames{0};  // derived from m_data.sampleRate * kTickMs/1000
    int            m_emittedBlocks{0};
    EndOfFileBehavior m_eofBehavior{EndOfFileBehavior::Stop};
};

} // namespace Longpath
