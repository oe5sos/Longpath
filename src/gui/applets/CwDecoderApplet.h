// =================================================================
// src/gui/applets/CwDecoderApplet.h  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR -- GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       -- per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a structural derivative of AetherSDR's CW decode panel
//   (the control set on PanadapterApplet [@e944ec49] -- decoded text,
//   detected pitch and speed, lock-pitch / lock-speed toggles, a pitch
//   band, Clear -- backed by CwDecoder). The widget tree itself is new
//   Longpath code against this project's own AppletWidget base class
//   (the same shape as RttyDecoderApplet), but the set of controls and
//   what each one does is carried over. No Thetis equivalent exists
//   (see CwDecoder.h).
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 SS5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude. The pitch band
//                 follows the slice's CW pitch (SliceModel, Thetis-
//                 sourced) with +/- 150 Hz around it, the way AetherSDR's
//                 setKnownParameters pads a known pitch; the operator
//                 does not set a second pitch here. The cost gate keeps
//                 AetherSDR's default (0.70) without the sensitivity
//                 slider, and confidence dims the text instead of
//                 colouring it green/yellow/orange/red.
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

// Native CW (Morse) decoder display. Visible only when the active slice's
// mode is DSPMode::CWL or DSPMode::CWU (MainWindow gates it on the same
// availability axis as RttyDecoderApplet / RadeApplet).
//
// Controls:
//   1. Status row  -- detected pitch and speed (monospace), a LOCK capsule
//   2. Text output -- read-only, monospace, capped scrollback
//   3. Lock Pitch / Lock Speed toggles, Clear
class CwDecoderApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit CwDecoderApplet(RadioModel* model, QWidget* parent = nullptr);
    ~CwDecoderApplet() override;

    QString appletId()    const override { return QStringLiteral("CwDecoder"); }
    QString appletTitle() const override { return QStringLiteral("CW Decoder"); }
    void    syncFromModel() override;

    // Bound from MainWindow on construction and again on every active-slice
    // change (mirrors RttyDecoderApplet::setSlice).
    void setSlice(SliceModel* slice);

    // For tests.
    CwDecoder*      decoderForTest() const { return m_decoder; }
    QPlainTextEdit* textForTest() const { return m_textOutput; }
    QLabel*         statsForTest() const { return m_stats; }
    QPushButton*    lockPitchForTest() const { return m_lockPitchBtn; }
    QPushButton*    lockSpeedForTest() const { return m_lockSpeedBtn; }

private slots:
    void onTextDecoded(const QString& text, float cost);
    void onStatsUpdated(float pitchHz, float speedWpm);

private:
    void buildUI();
    void updateAudioTap();
    void applyPitchBandFromSlice();

    QPointer<SliceModel> m_slice;
    CwDecoder* m_decoder{nullptr};
    std::unique_ptr<AudioTapRing> m_tapRing;
    QTimer* m_pumpTimer{nullptr};
    QMetaObject::Connection m_pitchConn;

    QLabel*         m_stats{nullptr};
    QLabel*         m_lockCapsule{nullptr};
    QPlainTextEdit* m_textOutput{nullptr};
    int             m_charCount{0};
    QPushButton*    m_lockPitchBtn{nullptr};
    QPushButton*    m_lockSpeedBtn{nullptr};
    QPushButton*    m_clearBtn{nullptr};
};

} // namespace Longpath
