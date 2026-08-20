#pragma once

// =================================================================
// src/gui/widgets/TxSpectrumWidget.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// What is actually leaving the transmitter, and how wide it is.
//
// ── The one number in the channel strip that is about somebody else ──
//
// Everything else in the strip is taste. The gate, the compressor, the
// equaliser — get them wrong and you sound bad, which is your problem.
// Occupied bandwidth is whether the stations either side of you can
// use the band, and a wrong figure here does not merely mislead the
// operator, it misleads them in public.
//
// So this draws the measured spectrum with the −26 dBc and −60 dBc
// edges marked, and prints the width between them. TxSpectrumAnalysis
// does the arithmetic and has done since it was written; nothing was
// showing it.
//
// ── Why the edges are found by scanning inward ───────────────────────
//
// A real speech spectrum has dips in it. Walking OUTWARD from the peak
// stops at the first one and reports half the true width — flattering,
// in the direction that tells a splattering station it is clean. The
// analysis module scans inward from the ends for exactly that reason;
// see the note in TxSpectrumAnalysis.h and the test named
// a_dip_in_the_middle_does_not_end_the_measurement.
//
// ── It is not a substitute for a receiver ────────────────────────────
//
// This is the post-modulator siphon: what WDSP produced, before the
// power amplifier, the filters and the antenna. A clean reading here
// and a dirty signal on the air is entirely possible and means the
// problem is downstream. The widget says so rather than letting a
// green number stand for the whole transmitter.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/TxSpectrumAnalysis.h"

#include <QVector>
#include <QWidget>

#include <vector>

namespace Longpath {

class MicSpectrum;

class TxSpectrumWidget : public QWidget {
    Q_OBJECT
public:
    explicit TxSpectrumWidget(QWidget* parent = nullptr);

    // Where to read the transmitted audio from. Not owned. Null is a
    // valid state — the widget says there is no transmitter rather than
    // drawing an empty box.
    void setSource(const MicSpectrum* ring);

    // How much of the ring to analyse. Longer is steadier and slower to
    // react; two seconds is about three or four syllables, which is the
    // shortest window whose average means anything for speech.
    void setWindowSeconds(double s);

    // Freeze the display on the worst reading seen since the last
    // reset. A live curve during a call is unreadable; the peak-hold is
    // what you look at afterwards.
    void setHold(bool on);
    bool isHold() const noexcept { return m_hold; }
    void resetHold();

    // The most recent measurement, for whoever wants the number rather
    // than the picture.
    TxSpectrumAnalysis::Occupancy occupancy() const { return m_occ; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    // Emitted whenever a fresh measurement lands, so a status line
    // elsewhere can carry the width without polling.
    void measured(const Longpath::TxSpectrumAnalysis::Occupancy& occ);

protected:
    void paintEvent(QPaintEvent*) override;
    void timerEvent(QTimerEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private:
    void recompute();

    double xFor(double hz) const;
    double yFor(double db) const;

    const MicSpectrum* m_ring{nullptr};

    std::vector<float>  m_scratch;   // reused, so painting allocates nothing
    std::vector<double> m_mag;       // the live curve, dB relative to peak
    std::vector<double> m_held;      // the widest reading since reset
    TxSpectrumAnalysis::Occupancy m_occ;
    TxSpectrumAnalysis::Occupancy m_heldOcc;

    double m_windowSeconds{2.0};
    bool   m_hold{false};
    int    m_timer{0};
    // Frames the ring had at the last measurement. Equal means nothing
    // new arrived, which is how "the transmitter is idle" is told apart
    // from "the transmitter is running and silent" — different faults,
    // different remedies.
    unsigned long long m_lastSeen{0};
    bool   m_everMeasured{false};

    mutable double m_plotL{0.0}, m_plotR{0.0}, m_plotT{0.0}, m_plotB{0.0};
};

} // namespace Longpath
