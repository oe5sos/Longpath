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
// Nothing here can transmit. The self-monitor runs the chain off air —
// see TxChannel::writesToRadio() — and the analysis reads the raw
// microphone before WDSP touches it, which is a tap and not a path.
//
// Why the raw microphone and not the monitor output: measuring the
// chain's own output and then recommending an EQ to fix it gives advice
// that changes every time it is taken. The compressor would also have
// flattened the spectrum being measured, and the 2.7 kHz transmit
// filter would have removed the bands the sibilance check needs.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "core/TxAudioRecorder.h"
#include "core/VoiceAnalyzer.h"

#include <QDialog>
#include <QMetaObject>
#include <QPointer>

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSlider;
class QTableWidget;
class QTimer;
class QWidget;

namespace NereusSDR {

class RadioModel;

class TxVoiceCheckDialog : public QDialog {
    Q_OBJECT
public:
    explicit TxVoiceCheckDialog(RadioModel* radio, QWidget* parent = nullptr);
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

    void setSelfMonitor(bool on);
    void startRecording();
    void stopRecording();
    void runAnalysis();
    void applySuggestion();
    void saveRecording();

    void showFindings();
    void fillAdvancedTable();
    void refreshButtons();

    // QPointer, not a raw pointer: this window outlives nothing,
    // but the destructor restores the monitor through the radio
    // model, and teardown order between two children of the main
    // window is not something to bet a crash on.
    QPointer<RadioModel> m_radio;

    TxAudioRecorder m_recorder;
    VoiceAnalysis   m_result;
    bool            m_haveResult{false};

    // Restored on the way out. An operator who opens this window, turns
    // the monitor on to listen, and closes it again should not be left
    // with a monitor they did not ask for and cannot see.
    bool m_monitorWasOn{false};
    bool m_restoreMonitor{false};

    QMetaObject::Connection m_micTap;

    QCheckBox*    m_listenBox{nullptr};
    QSlider*      m_volume{nullptr};
    QPushButton*  m_recordBtn{nullptr};
    QProgressBar* m_progress{nullptr};
    QTimer*       m_countdown{nullptr};
    int           m_secondsLeft{0};

    QLabel*       m_findings{nullptr};
    QPushButton*  m_applyBtn{nullptr};
    QPushButton*  m_saveBtn{nullptr};

    QPushButton*  m_advancedBtn{nullptr};
    QWidget*      m_advanced{nullptr};
    QTableWidget* m_bands{nullptr};
    QLabel*       m_numbers{nullptr};
};

} // namespace NereusSDR
