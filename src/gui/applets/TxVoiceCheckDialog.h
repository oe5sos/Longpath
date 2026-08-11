#pragma once

// =================================================================
// src/gui/applets/TxVoiceCheckDialog.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Hear yourself, record fifteen seconds, get three sentences and a
// button that sets the equaliser.
//
// The shape of this window is the point of it. Setting up transmit
// audio is a job with about forty controls and no feedback loop: the
// only person who cannot hear the result is the operator. Software
// usually answers that with more controls. This answers it with one
// button and a paragraph, and puts every number behind "Advanced" for
// the operator who wants to argue with it.
//
// So there are two surfaces over one measurement:
//
//   Simple    Speak for fifteen seconds. Read what it found, worst
//             first, in sentences. Press Apply, or don't.
//
//   Advanced  The ten measured band levels against the target curve,
//             the suggestion per band, the noise floor, crest factor,
//             hum and sibilance, and the recording itself as a WAV.
//
// Nothing here can transmit, and nothing here monitors live any more.
// Both takes come from TxWorkerThread's pre/post-strip taps — plain
// audio-domain taps, not paths, and nothing touches the radio.
//
// Why the raw microphone for the ANALYSIS: measuring the chain's own
// output and then recommending an EQ to fix it gives advice that
// changes every time it is taken. The compressor would also have
// flattened the spectrum being measured, and the 2.7 kHz transmit
// filter would have removed the bands the sibilance check needs.
//
// ── Speak, then listen (2026-08-11, the AetherSDR way) ───────────────
//
// The live "Hear myself" path is GONE from this window, asked for at
// the bench in plain words after it never got past "almost right":
// real-time self-monitoring through a block-batched network path
// carries latency and seams, and AetherSDR — whose workflow the bench
// wants 1:1 — never does it. Its PUDU monitor records the processed
// chain output and plays it back (ClientPuduMonitor @31b29583); this
// window now runs exactly that loop, with NereusSDR's own parts:
//
//   Record 15 s  → both worker taps captured simultaneously —
//                  PRE-strip for the analysis, POST-strip for the ear.
//                  The band is muted for the whole cycle so the take
//                  is captured against silence (Aether's muteRx).
//   Countdown 0  → the processed take AUTO-PLAYS. Speak, then listen,
//                  no third click.
//   A/B buttons  → raw vs processed of the same words, from memory
//                  through a QAudioSink — off the real-time path, so
//                  it cannot crackle and carries no latency.
//
// The taps sit BEFORE WDSP's DEXP, ALC and TX bandpass on purpose:
// the earlier sip1 source sat behind all three, which made every take
// sound dull and pumped regardless of what the strip did. What plays
// back is what the strip does to the voice — full bandwidth. (The
// on-air MON path for real transmissions is untouched by all this.)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-08-11 — Second recorder on the post-strip monitor tap +
//                 raw/processed playback (AetherSDR PUDU-monitor shape,
//                 no code reused). By Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
//   2026-08-11 — Rebuilt on the AetherSDR record-then-listen contract:
//                 live monitoring removed, takes moved to the worker's
//                 pre/post-strip taps (audio domain, pre-WDSP),
//                 band-quiet across the record/playback cycle,
//                 auto-play at countdown end. By Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/TxAudioRecorder.h"
#include "core/VoiceAnalyzer.h"

#include <QDialog>
#include <QMetaObject>
#include <QPointer>

#include <atomic>

class QAudioSink;
class QBuffer;
class QLabel;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTimer;
class QWidget;

namespace NereusSDR {

class RadioModel;

class TxVoiceCheckDialog : public QDialog {
    Q_OBJECT
public:
    // embedded=true turns the dialog into a plain widget for hosting
    // inside the channel strip's tab bar (2026-08-11, asked for at the
    // bench: one window for the change-record-listen loop). Same code,
    // two homes; embedded drops the Close button, the host closes.
    explicit TxVoiceCheckDialog(RadioModel* radio,
                                QWidget* parent = nullptr,
                                bool embedded = false);
    ~TxVoiceCheckDialog() override;

    // How long to speak for. Long enough that the long-term average is
    // of a voice rather than of one sentence's vowels, short enough
    // that an operator will actually do it. VoiceAnalyzer needs three
    // seconds of speech and this leaves room for the pauses.
    static constexpr int kRecordSeconds = 15;

    // Every current transmit path feeds WDSP at 48 kHz — see the
    // in_size / out_size note in TxChannel.h, where 48 kHz in is the
    // constant and only the output rate varies by protocol.
    static constexpr int kMicRateHz = 48000;

private:
    void buildUi();

    void setRxQuiet(bool on);
    void startRecording();
    void stopRecording();
    // Playback of the last take, from memory through a QAudioSink.
    // processed=false plays the raw microphone, true plays the
    // post-strip/EQ monitor signal. Pressing the active button again
    // stops. See the header note for why this exists.
    void playRecording(bool processed);
    void stopPlayback();
    void runAnalysis();
    void applySuggestion();       // WDSP's own equaliser
    void applyToStrip();          // the channel strip, whole chain
    void saveRecording();

    void showFindings();
    void fillAdvancedTable();
    void refreshButtons();

    // ── Saying why nothing is happening ──────────────────────────────
    //
    // Hearing yourself needs six things to be true at once: a radio
    // connection, a running transmit pump, mic blocks arriving, the
    // right mic source selected, the mic not muted, and the monitor
    // mixed into the speakers. Shipping one checkbox over six invisible
    // preconditions means that when it does not work the operator has
    // nowhere to start — which is exactly what happened.
    //
    // So the taps are always listening while this window is open, and
    // the watchdog reports the first broken link rather than the last.
    void startLevelWatch();
    void stopLevelWatch();
    void updateLevel();
    QString micSourceName() const;

    QMetaObject::Connection m_levelTap;

    // Written on the transmit worker thread, read on the GUI thread.
    std::atomic<unsigned> m_micBlocks{0};
    // Peak since the last read, as an integer so it can be atomic
    // without a compare-exchange loop: millionths of full scale.
    std::atomic<unsigned> m_micPeakMicros{0};

    // True once the taps are on a live transmit channel. The window
    // can be opened before the radio is connected, and until this fix
    // it stayed dead for the rest of the session — the meter frozen at
    // -60 and the status line still saying "not connected" while a
    // recording ran perfectly well beside it.
    bool    m_tapsArmed{false};
    QTimer* m_levelTimer{nullptr};
    int     m_watchTicks{0};
    // Loudest peak seen since the window opened, so a failed
    // analysis can say whether there was ever any signal at all.
    double  m_peakSeenDb{-60.0};
    // Block counts at the moment recording started, so the report can
    // say what arrived during the take rather than since the window
    // opened.
    unsigned m_micBlocksAtStart{0};

    // Hosted inside the strip window's tab bar rather than standing
    // alone. Only buildUi() reads it (no Close button when embedded).
    bool m_embedded{false};

    // QPointer, not a raw pointer: this window outlives nothing,
    // but the destructor restores the monitor through the radio
    // model, and teardown order between two children of the main
    // window is not something to bet a crash on.
    QPointer<RadioModel> m_radio;

    TxAudioRecorder m_recorder;
    // The same take through the channel strip, recorded simultaneously
    // from the worker's POST-STRIP tap (2026-08-11 — audio domain,
    // full bandwidth, before WDSP's DEXP/ALC/TX-filter; the earlier
    // sip1 source sat behind all of those and made every take sound
    // dull and pumped). The analysis never reads this one; only the
    // play button and the auto-play do.
    TxAudioRecorder m_procRecorder;
    QMetaObject::Connection m_procTap;
    VoiceAnalysis   m_result;
    bool            m_haveResult{false};

    // Playback state. The sink is rebuilt per play — a QAudioSink is
    // cheap and rebuilding sidesteps every stale-device question.
    QAudioSink* m_playSink{nullptr};
    QBuffer*    m_playBuf{nullptr};
    QByteArray  m_playPcm;
    bool        m_playingProcessed{false};

    // The band-quiet gate (setRxQuiet). True from record start until
    // playback ends — the AetherSDR record/listen contract.
    bool m_rxQuiet{false};

    QMetaObject::Connection m_micTap;

    QLabel*       m_liveStatus{nullptr};
    QProgressBar* m_levelBar{nullptr};
    QPushButton*  m_recordBtn{nullptr};
    QPushButton*  m_playRawBtn{nullptr};
    QPushButton*  m_playProcBtn{nullptr};
    QProgressBar* m_progress{nullptr};
    QTimer*       m_countdown{nullptr};
    int           m_secondsLeft{0};

    QLabel*       m_findings{nullptr};
    QPushButton*  m_applyBtn{nullptr};
    QPushButton*  m_stripBtn{nullptr};
    QPushButton*  m_saveBtn{nullptr};

    QPushButton*  m_advancedBtn{nullptr};
    QWidget*      m_advanced{nullptr};
    QTableWidget* m_bands{nullptr};
    QLabel*       m_numbers{nullptr};
};

} // namespace NereusSDR
