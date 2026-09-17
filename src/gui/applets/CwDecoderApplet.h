// =================================================================
// src/gui/applets/CwDecoderApplet.h  (Longpath)
// =================================================================
//
// Longpath-original. The panel for the native CW decoder, built the
// same way as RttyDecoderApplet (the control set and the tap plumbing
// are that applet's; the decoder underneath is the Zeus port in
// core/CwDecoderCore.h). Visible only while the active slice is in CWL
// or CWU -- the same availability axis RADE and the RTTY decoder use.
//
// What is on it, top to bottom:
//   1. Status row  -- tone capsule (TON / KEIN TON), tracked tone Hz,
//                     WPM and SNR (monospace)
//   2. Signal      -- one HGauge, the SNR in dB (0..40)
//   3. Text        -- read-only, monospace, capped scrollback
//   4. Controls    -- receive pitch (TON: [<] 600 Hz [>], 25 Hz steps,
//                     the RX applet's STEP idiom), Leeren
//
// The pitch starts at the CWPitch setting the CW filter is centred on
// (Thetis cw_pitch, default 600), so out of the box the decoder looks
// where the filter looks; the arrows let the operator move it and the
// Goertzel bank searches +-125 Hz around it either way. Not persisted:
// the next start follows CWPitch again.
// no-port-check: Longpath-original applet; the decoder logic it hosts
// is ported and cited in core/CwDecoderCore.{h,cpp}.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-17 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================
#pragma once

#include "AppletWidget.h"

#include <QPointer>
#include <memory>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;

namespace Longpath {

class RadioModel;
class SliceModel;
class CwDecoder;
class AudioTapRing;
class HGauge;
class TriBtn;

class CwDecoderApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit CwDecoderApplet(RadioModel* model, QWidget* parent = nullptr);
    ~CwDecoderApplet() override;

    QString appletId()    const override { return QStringLiteral("CwDecoder"); }
    QString appletTitle() const override { return QStringLiteral("CW Decoder"); }
    void    syncFromModel() override;

    // Bound from MainWindow on construction and again on every active-slice
    // change (rebindRttyRadeAvailability), exactly like the RTTY decoder.
    void setSlice(SliceModel* slice);

    // For tests: the decoder this applet drives.
    CwDecoder* decoderForTest() const { return m_decoder; }

private slots:
    void onTextDecoded(const QString& text, float confidence);
    void onStatsUpdated(float wpm, float snrDb, bool tonePresent, float trackedHz);

private:
    void buildUI();
    void updateAudioTap();
    static int cwPitchFromSettings();

    QPointer<SliceModel> m_slice;
    CwDecoder* m_decoder{nullptr};
    std::unique_ptr<AudioTapRing> m_tapRing;
    QTimer* m_pumpTimer{nullptr};

    // Row 1 -- status
    QLabel* m_toneCapsule{nullptr};
    QLabel* m_trackedHz{nullptr};
    QLabel* m_wpmValue{nullptr};
    QLabel* m_snrValue{nullptr};
    // Row 2 -- signal
    HGauge* m_signalGauge{nullptr};
    // Row 3 -- text
    QPlainTextEdit* m_textOutput{nullptr};
    int m_charCount{0};
    // Row 4 -- controls: TON: [<] [600 Hz] [>], the RX applet's STEP idiom
    TriBtn*      m_pitchDown{nullptr};
    QLabel*      m_pitchLabel{nullptr};
    TriBtn*      m_pitchUp{nullptr};
    int          m_pitchHz{600};
    QPushButton* m_clearBtn{nullptr};
    void setPitch(int hz);
};

} // namespace Longpath
