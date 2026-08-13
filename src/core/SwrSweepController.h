// =================================================================
// src/core/SwrSweepController.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original feature (no Thetis equivalent — Thetis has no
// band-sweep SWR analyzer). Built entirely on ported, bench-proven
// primitives: MoxController::setTune (BandPlanGuard-gated),
// RadioConnection::setTxFrequency (Alex TX-LPF follows inside the
// connection), and the handlePaTelemetry fwd/rev watt feed.
//
// Design doc: docs/architecture/2026-08-13-swr-sweep-analyzer-design.md
// Operator request (OE5SOS): "alle Antennen per Knopfdruck
// analysieren, Kurve wie ein SWR-Messgerät, nur genauer — reine
// Analyse."
//
// The controller TUNEs at the operator's tune power, steps the TX
// frequency across a band INSIDE the band-plan edges, averages the
// directional-coupler telemetry at every step, and emits an SWR curve.
// Nothing is adjusted; this is measurement only.
//
// Dependency pattern mirrors TwoToneController: non-owning injected
// pointers plus std::function seams, so the whole state machine runs
// under test with synthetic telemetry and no radio.
//
// Thread model: main thread only (QTimer-driven state machine).
// ingestTelemetry is called from RadioModel::handlePaTelemetry, which
// already runs on the main thread.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-13 — Created by Ralph Martin Fischer (OE5SOS),
//                 AI-assisted implementation via Anthropic Claude
//                 (Cowork).
// =================================================================

#pragma once

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QDateTime>

#include <functional>

#include "models/Band.h"
#include "core/safety/BandPlanGuard.h"
#include "core/WdspTypes.h"

namespace NereusSDR {

class MoxController;

/// One measured point of a sweep. swr <= 0 marks an invalid sample
/// (forward power below the bridge's honest floor at that step).
struct SwrSweepPoint {
    quint64 freqHz{0};
    double  swr{0.0};
    double  fwdW{0.0};
};

/// A finished (or aborted) sweep with everything the chart needs.
struct SwrSweepResult {
    Band                    band{Band::GEN};
    QString                 antennaLabel;
    QDateTime               startedAt;
    QVector<SwrSweepPoint>  points;
    bool                    completed{false};   // false = aborted
    QString                 abortReason;        // empty when completed

    /// Frequency of the minimum valid SWR, 0 when no valid point.
    quint64 resonanceHz() const;
    double  minSwr() const;
};

/// What to sweep. Build via forBand() and clip via clipToGuard().
struct SwrSweepPlan {
    Band    band{Band::GEN};
    quint64 startHz{0};
    quint64 stopHz{0};
    int     points{51};
    int     settleMs{120};
    int     dwellMs{220};

    /// Seed from the IARU Region-2 band table (Band.cpp edges).
    /// Returns an invalid plan (startHz == 0) for GEN / WWV / XVTR /
    /// SWL bands and for 60 m (channelized — excluded in v1, see
    /// design doc).
    static SwrSweepPlan forBand(Band b);

    /// Clip start/stop to the first/last frequency the band-plan guard
    /// allows for (region, mode) at this plan's point spacing, so a
    /// Region-1 40 m sweep ends at 7.200 MHz instead of failing. Returns
    /// false when fewer than kMinPoints points survive.
    bool clipToGuard(const safety::BandPlanGuard& guard,
                     safety::Region region, DSPMode mode);

    bool isValid() const { return startHz > 0 && stopHz > startHz; }
    quint64 freqAt(int index) const;

    static constexpr int kMinPoints = 11;
    static constexpr int kMaxPoints = 401;
};

// ---------------------------------------------------------------------------
// SwrSweepController — the TUNE-step-measure state machine.
//
// States: Idle → Keying(300 ms tune settle) → per point
// [Settling(settleMs) → Measuring(dwellMs)] → Finishing(300 ms) → Idle.
// finish() runs on EVERY exit path: tune off, settle, TX freq restore.
// ---------------------------------------------------------------------------
class SwrSweepController : public QObject
{
    Q_OBJECT

public:
    explicit SwrSweepController(QObject* parent = nullptr);

    // ── Dependency injection (all non-owning; set before startSweep) ──
    void setMoxController(MoxController* mox) { m_mox = mox; }
    /// Pushes one TX frequency to the wire (RadioModel marshals to the
    /// connection thread). Required.
    void setTxFrequencyFn(std::function<void(quint64)> fn)
    { m_txFrequencyFn = std::move(fn); }
    /// Restores the operator's TX frequency after the sweep
    /// (RadioModel::pushTxFrequencyFromTxSlice). Required.
    void setTxFreqRestoreFn(std::function<void()> fn)
    { m_txFreqRestoreFn = std::move(fn); }
    /// Answers whether a sweep may start at all (connected, powered).
    void setReadyFn(std::function<bool()> fn)
    { m_readyFn = std::move(fn); }
    /// Names the currently selected TX antenna for the result label.
    void setAntennaLabelFn(std::function<QString()> fn)
    { m_antennaLabelFn = std::move(fn); }

    // ── Sweep lifecycle ───────────────────────────────────────────────
    /// Starts a sweep. Returns false (with reasonChanged emitted) when
    /// a gate refuses. The plan must already be guard-clipped.
    bool startSweep(const SwrSweepPlan& plan);
    void abortSweep(const QString& reason);
    bool isSweeping() const { return m_state != State::Idle; }

    /// Telemetry feed — RadioModel::handlePaTelemetry calls this with
    /// the same raw-scaled watts it hands SwrProtectionController.
    /// Ignored outside the Measuring state.
    void ingestTelemetry(double fwdW, double revW);

    // ── Tuning constants ─────────────────────────────────────────────
    /// Mirrors TwoToneController::kTuneReleaseSettleMs (Thetis
    /// console.cs:44740 [v2.10.3.13] Task.Delay(300) around TUNE
    /// transitions).
    static constexpr int kTuneSettleMs = 300;
    /// Below this forward power the bridge reading is noise, not a
    /// measurement; the point is recorded as invalid. NereusSDR-native
    /// threshold — see design doc §Safety.
    static constexpr double kMinFwdW = 0.5;
    /// Three consecutive points at or above this SWR abort the sweep:
    /// that is an open feedline or no antenna, not a bad antenna.
    static constexpr double kAbortSwr = 25.0;
    static constexpr int    kAbortSwrRun = 3;
    /// Telemetry silence longer than this aborts (link or radio gone).
    static constexpr int kTelemetryTimeoutMs = 2000;

    /// Pure SWR math, exposed for tests: gamma = sqrt(rev/fwd),
    /// swr = (1+gamma)/(1-gamma), capped at 99; returns 0 (invalid)
    /// when fwdW < kMinFwdW; rev clamped into [0, fwd].
    static double swrFromWatts(double fwdW, double revW);

    // ── Test seam ────────────────────────────────────────────────────
    /// Compresses every internal wait to 0/1 ms so the state machine
    /// runs synchronously-ish under QSignalSpy::wait.
    void setTimingsForTest(int tuneSettleMs, int telemetryTimeoutMs);

signals:
    void sweepStarted(const NereusSDR::SwrSweepPlan& plan);
    void pointReady(int index, quint64 freqHz, double swr, double fwdW);
    void progressChanged(int done, int total);
    void sweepFinished(const NereusSDR::SwrSweepResult& result);
    /// Human-readable refusal / abort reason for the status line.
    void reasonChanged(const QString& reason);

private:
    enum class State { Idle, Keying, Settling, Measuring, Finishing };

    void stepToNextPoint();
    void beginMeasure();
    void closePoint();
    void finish(bool completed, const QString& reason);

    MoxController* m_mox{nullptr};
    std::function<void(quint64)> m_txFrequencyFn;
    std::function<void()>        m_txFreqRestoreFn;
    std::function<bool()>        m_readyFn;
    std::function<QString()>     m_antennaLabelFn;

    State          m_state{State::Idle};
    SwrSweepPlan   m_plan;
    SwrSweepResult m_result;
    int            m_index{0};
    int            m_highSwrRun{0};

    // Per-point accumulation while Measuring.
    double m_accFwd{0.0};
    double m_accRev{0.0};
    int    m_accN{0};
    qint64 m_lastTelemetryMs{0};

    QTimer m_stepTimer;      // single-shot, drives every state change
    QTimer m_watchdog;       // telemetry-silence abort

    int m_tuneSettleMs{kTuneSettleMs};
    int m_telemetryTimeoutMs{kTelemetryTimeoutMs};
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::SwrSweepPlan)
Q_DECLARE_METATYPE(NereusSDR::SwrSweepResult)
