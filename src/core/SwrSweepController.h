// =================================================================
// src/core/SwrSweepController.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// Umfang der Uebernahme: EINE Konstante. kTuneSettleMs = 300 spiegelt
// das Task.Delay(300) um die TUNE-Umschaltung (console.cs:44743
// [@852bf0e]). Alles Weitere in dieser Datei ist NereusSDR-original --
// Thetis kennt keinen Bandwobbler. Dass eine geborgte Konstante
// trotzdem den vollen Kopf traegt, ist der Hausstand: FFTEngine.cpp
// steht mit derselben Begruendung in der PROVENANCE-Tabelle
// („constant reference only").
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-13 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-08-17 — Verbatim console.cs header + PROVENANCE row added; the
//                 borrowed constant had a cite but no attribution, which
//                 scripts/check-new-ports.py had been failing on.
//                 Martin Fischer, AI-assisted via Anthropic Claude.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

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
#include <QString>
#include <QTimer>
#include <QVector>
#include <QDateTime>

#include <functional>

#include "models/Band.h"
#include "core/safety/BandPlanGuard.h"

#include <optional>
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

    /// The highest per-point forward power the bridge reported during
    /// the sweep. Carried so a sweep that measured nothing can say WHY
    /// in watts instead of guessing out loud — see the note on
    /// kMinFwdW.
    double maxFwdW{0.0};

    /// The highest RAW forward ADC count seen, straight off the wire
    /// before any per-board scaling.
    ///
    /// ── Why the raw count and not just the watts ─────────────────────
    ///
    /// Three rounds of "0.01 W" told us nothing new each time, because
    /// watts are the far end of a chain — ADC count, board triplet,
    /// calibration — and a small number at the end does not say which
    /// link is at fault. The count does: it is what the radio actually
    /// sent. If it does not move when the power is raised, nothing
    /// downstream of the radio can be blamed, and the next test is a
    /// measurement instead of another round of the same sentence.
    quint16 maxFwdRaw{0};

    /// The forward ADC's idle reading, sampled with the transmitter OFF
    /// immediately before this sweep. Everything is judged against it,
    /// and a failed sweep can say "43 against an idle 41" instead of
    /// quoting a number with nothing to compare it to.
    quint16 baselineRaw{0};

    /// The same two numbers for the REVERSE channel.
    ///
    /// ── The failure that looks like success ──────────────────────────
    ///
    /// 2026-08-14, first sweep that ever completed: fifty-one of
    /// fifty-one points measured, SWR 1.00 at every one, a flat line
    /// along the bottom of the chart and "Resonanz bei 14.000 MHz" —
    /// the first point, because they were all identical.
    ///
    /// SWR is (1+γ)/(1−γ) with γ = √(rev/fwd). Reverse reading exactly
    /// zero gives exactly 1.00, always, everywhere. So a dead reverse
    /// channel does not look broken. It looks like a perfect antenna,
    /// which is the one wrong answer an operator has no reason to
    /// question — and would act on by not adjusting an antenna that
    /// needs it.
    ///
    /// Forward being dead produces no curve and gets noticed in
    /// seconds. Reverse being dead produces a beautiful one.
    quint16 maxRevRaw{0};
    quint16 baselineRevRaw{0};

    /// True when the reverse ADC never climbed clear of its own idle
    /// reading during the whole sweep. Then every SWR here is 1.00 by
    /// arithmetic rather than by measurement.
    bool reverseNeverMoved{false};

    /// Frequency of the minimum valid SWR, 0 when no valid point.
    quint64 resonanceHz() const;
    double  minSwr() const;

    /// How many points carry a real reading. `points.size()` counts the
    /// attempts; this counts the measurements, and the difference is
    /// the whole of what went wrong on 2026-08-14.
    int validPoints() const;
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

    // ── A range inside ONE band ──────────────────────────────────────
    //
    // A narrow look around a resonance: 14.000 to 14.120 rather than
    // the whole of 20 m, or the top half of 40 m at fine spacing.
    //
    // ── Why it no longer crosses band edges ──────────────────────────
    //
    // It used to. Type 1.8 to 30 MHz and it planned nine stretches with
    // eight holes between them, because most of 1.8–30 MHz is not ours
    // to transmit on. The result was nine short curves the chart had to
    // show as nine panels, and OE5SOS's verdict on 2026-08-15, after
    // several rounds of trying to make it read as one picture, was
    // plain: "es muss eine durchgehende linie sein, es muss der ganze
    // bereich gemessen werden" — and when told the transmitter may not
    // key between the bands, "diese Option soll es nicht mehr geben".
    //
    // He is right that the feature promised something it cannot give.
    // A sweep that keys the transmitter can only ever see the twelve
    // per cent of HF that is allocated to us; presenting that as a
    // range sweep invites the reading that the range was measured. The
    // instrument that sweeps 1.8 to 30 MHz unbroken is a vector
    // analyser, and this program reads its file.
    //
    // So a range must now lie inside a single allocated stretch.
    // Anything wider comes back invalid with `note` saying why, rather
    // than quietly measuring a twelfth of what was asked for.
    //
    // Returns an invalid plan when the range holds no allowed segment,
    // and when it holds more than one.
    static SwrSweepPlan forRange(double startHz, double stopHz,
                                 const safety::BandPlanGuard& guard,
                                 std::optional<safety::Region> region,
                                 DSPMode mode, int totalPoints);

    /// Why an invalid plan is invalid, in words the operator can act
    /// on. Empty on a valid plan. "Sweep plan invalid" told nobody
    /// anything, which is how the eleven-points-over-two-segments bug
    /// stayed hidden.
    QString note;

    /// Explicit frequency list. Empty for a plain band sweep, where the
    /// points are startHz..stopHz evenly. Non-empty for a range sweep,
    /// where they are grouped into the allowed segments and freqAt()
    /// reads straight out of here.
    QVector<quint64> freqs;

    /// The allowed segments a range sweep found, as [first, last] index
    /// pairs into freqs. One entry per band touched; empty for a band
    /// sweep. The chart uses it to break the curve at the holes instead
    /// of drawing a straight line across unmeasured spectrum.
    QVector<QPair<int, int>> segments;


    /// Clip start/stop to the first/last frequency the band-plan guard
    /// allows for (region, mode) at this plan's point spacing, so a
    /// Region-1 40 m sweep ends at 7.200 MHz instead of failing. Returns
    /// false when fewer than kMinPoints points survive.
    ///
    /// A region of std::nullopt means the operator has not said where he
    /// is. The clip then keeps only frequencies EVERY region permits —
    /// see safety/RegionSetting.h. The seed range comes from the IARU
    /// Region-2 table, so skipping the clip means transmitting on the US
    /// plan; that is not a defensible default and is no longer reachable.
    bool clipToGuard(const safety::BandPlanGuard& guard,
                     std::optional<safety::Region> region, DSPMode mode);

    bool isValid() const { return startHz > 0 && stopHz > startHz; }
    quint64 freqAt(int index) const;

    /// True when this plan has holes in it — a range sweep across more
    /// than one band. Callers that describe the result to the operator
    /// must not call it "3.5 to 28.0 MHz".
    bool isSegmented() const { return segments.size() > 1; }

    static constexpr int kMinPoints = 11;
    static constexpr int kMaxPoints = 401;

    /// Fewest points any one segment gets, however many segments there
    /// are.
    ///
    /// Was three, which is enough to see a trend and not enough to see
    /// a curve — OE5SOS, 2026-08-14: "die Linie sollte wie eine Kurve
    /// aussehen, mit vielen Punkten, rund!". Three points per band drew
    /// two straight strokes.
    ///
    /// kMinPoints is what the controller demands of a whole sweep
    /// before it will call it a measurement, so it is the natural floor
    /// for one band of one too. At 51 requested over nine bands that
    /// gives 99 points and about half a minute of transmitting instead
    /// of seventeen seconds — the cost of the answer being readable.
    static constexpr int kMinPerSegment = kMinPoints;
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
    /// Still needed, but only to READ the transmit state. Keying goes
    /// through setTuneFn — see the note there.
    void setMoxController(MoxController* mox) { m_mox = mox; }

    /// How to key and release TUNE.
    ///
    /// ── The bug this exists to fix ───────────────────────────────────
    ///
    /// The controller used to call MoxController::setTune() directly.
    /// MoxController runs the keying state machine and nothing else.
    /// The tune DRIVE LEVEL is pushed by RadioModel::setTune(), which
    /// also sets the tune-adjusted TX VFO and arms SWR protection, and
    /// then calls MoxController itself.
    ///
    /// So every sweep keyed the transmitter without ever pushing a
    /// drive level, and the radio transmitted on whatever was left
    /// over. Measured on the bench 2026-08-14, same tune power, same
    /// antenna, same band:
    ///
    ///     TUNE button   VOR 339 counts   Power 1 W    carrier visible
    ///     sweep         VOR  63 counts   Power 0 W    nothing
    ///
    /// A factor of fifty in power. Every sweep that day transmitted
    /// into almost nothing and reported, correctly, that the coupler
    /// had nothing to report — and the whole day went into suspecting
    /// the coupler, the board profile, the protocol and the scaling.
    /// The instrument was right every single time.
    ///
    /// Injected rather than calling RadioModel directly, because this
    /// class is deliberately testable without one. Defaults to the raw
    /// MoxController path so existing callers and tests keep working.
    void setTuneFn(std::function<void(bool)> fn) { m_tuneFn = std::move(fn); }

    /// sqrt(bridge_fwd / bridge_rev) for the connected board.
    ///
    /// ── Why SWR is computed from raw counts here ─────────────────────
    ///
    /// The scaled watts arrive with a per-board `adc_cal_offset`
    /// already subtracted — 32 counts forward and 28 reverse on an
    /// Anvelina — and clamped at zero. That offset is meant to remove a
    /// pedestal the ADC sits at with no drive.
    ///
    /// This radio has no pedestal: measured on the bench, idle reads
    /// VOR 0 · RÜCK 0. So the subtraction does not remove a pedestal,
    /// it removes SIGNAL — and on the reverse channel, where a real
    /// reading may be a few dozen counts, it removes all of it. Every
    /// reverse count at or below 28 becomes exactly zero watts, which
    /// becomes exactly SWR 1.00.
    ///
    /// That is why an 80 m sweep drew 1.00 across the bottom of the
    /// band where the operator's VNA says 2.5: not a curve, a floor.
    ///
    /// So the ratio is taken from the counts, against the idle baseline
    /// this sweep measured for itself, and only the two bridge
    /// constants are needed:
    ///
    ///     |Γ| = (revΔ / fwdΔ) · sqrt(bridge_fwd / bridge_rev)
    ///
    /// Everything else — the reference voltage, the 4095 — divides out.
    /// Default 1.0 covers the boards whose two bridges match and keeps
    /// every existing caller working.
    void setBridgeRatio(double sqrtFwdOverRev)
    { if (sqrtFwdOverRev > 0.0) { m_bridgeRatio = sqrtFwdOverRev; } }
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
    /// `fwdRaw` is the unscaled ADC count the board reported, carried
    /// only so a failed sweep can name it. Defaulted so the tests and
    /// any other caller need not care.
    void ingestTelemetry(double fwdW, double revW,
                         quint16 fwdRaw = 0, quint16 revRaw = 0);

    // ── Tuning constants ─────────────────────────────────────────────
    /// Mirrors TwoToneController::kTuneReleaseSettleMs (Thetis
    /// console.cs:44743 [@852bf0e] — `await Task.Delay(300);` nach dem
    /// Abschalten von TUN, bevor der Zweiton startet).
    ///
    /// Die Zeile stand als 44740 [v2.10.3.13] hier; gegen die
    /// vorliegende Arbeitskopie nachgesehen und auf 44743 [@852bf0e]
    /// gestellt. Der WERT ist unveraendert 300 — nur die Zeilennummer
    /// war zwischen den Fassungen verrutscht.
    static constexpr int kTuneSettleMs = 300;
    /// Fallback floor in watts, used only when no raw ADC count is
    /// available (the tests feed watts alone).
    ///
    /// ── Why this is no longer the main test ──────────────────────────
    ///
    /// Half a watt is a sensible floor for a 100 W radio and nonsense
    /// for a QRP one. The operator's SOTA rig is an ANAN-10E: ten watts
    /// maximum, tuning at one. A fixed 0.5 W gate throws away half of
    /// everything such a radio can produce, and no amount of raising
    /// the power fixes it because there is no power to raise.
    ///
    /// The real question was never "how many watts" but "did the bridge
    /// respond at all", and that is answered in ADC counts against the
    /// radio's own idle reading. See kMinRawRise.
    static constexpr double kMinFwdW = 0.5;

    /// How far the forward ADC must climb above the radio's OWN idle
    /// reading before the point counts as a measurement.
    ///
    /// Measured against a baseline taken with the transmitter off,
    /// moments before the sweep — so it adapts to the board, the
    /// coupler and the day, and needs no per-model table. Sixty counts
    /// out of 4095 is about one and a half percent of full scale: far
    /// above the handful of counts an idle ADC wanders by, far below
    /// what any real carrier produces.
    ///
    /// This is what makes a QRP sweep possible. A 10 W radio's coupler
    /// is scaled for a 10 W radio; at one watt it produces a small
    /// reading, not no reading, and a small reading well clear of the
    /// noise is a perfectly good measurement.
    static constexpr quint16 kMinRawRise = 60;

    /// The same test for the REVERSE channel, and it must be far
    /// smaller.
    ///
    /// ── Measured on the bench, 2026-08-14 ────────────────────────────
    ///
    /// TUNE at 3 W into OE5SOS's antenna, counts read live off the
    /// coupler:
    ///
    ///     idle      VOR   0  ·  RÜCK  0
    ///     keyed     VOR 339  ·  RÜCK 38
    ///
    /// Both channels work. But 38 is below the 60-count rise that
    /// forward needs, so the same threshold applied to both would call
    /// this healthy reverse channel dead, drop the trace and tell the
    /// operator his coupler is broken.
    ///
    /// Which is obvious once seen: forward carries the whole transmit
    /// power and swings hundreds of counts, reverse carries only what
    /// the antenna sends back. On a GOOD antenna that is deliberately
    /// almost nothing — a 1.1 SWR reflects a quarter of one percent of
    /// the power. Requiring reverse to swing as hard as forward is
    /// requiring the antenna to be bad before the measurement is
    /// believed.
    ///
    /// Ten counts is a few times the idle scatter and comfortably under
    /// the 38 a well-matched antenna produced here.
    static constexpr quint16 kMinRevRise = 10;

    /// The lowest tune power worth keying for.
    ///
    /// ── Why this number exists ───────────────────────────────────────
    ///
    /// 2026-08-14, OE5SOS's ANAN with Tune Pwr at 1 W: the sweep ran to
    /// completion, transmitted at all fifty-one points, and reported
    /// "no valid measurements (forward power too low?)". Every point
    /// had been thrown away by kMinFwdW, and the operator found out
    /// after seventeen seconds of pointless transmission — from a
    /// sentence ending in a question mark.
    ///
    /// A directional coupler is a fixed attenuator into a diode
    /// detector. It has a floor, and 1 W is under it. This is the same
    /// fact stated where it can be checked BEFORE anything is keyed.
    /// Five watts is the practical figure for an ANAN's bridge with
    /// room above the 0.5 W floor for the reflected side to mean
    /// something too.
    static constexpr int kMinUsefulTuneW = 5;

    /// Consecutive points with no usable reading before giving up. The
    /// mirror of kAbortSwrRun: that one catches an open feedline, this
    /// one catches a bridge that is not reading at all. Without it a
    /// sweep with a dead bridge transmits the full plan and reports
    /// failure at the end.
    static constexpr int kAbortDeadRun = 5;

    /// Below this, the bridge is not reading LOW — it is not reading.
    ///
    /// ── Two faults that look alike and are not ───────────────────────
    ///
    /// 2026-08-14, second run: tune power raised to 5 W, and the sweep
    /// reported at most 0.01 W forward. On an ORIONMKII triplet that is
    /// about 41 ADC counts where five watts should give roughly 536 —
    /// not a reading that is too small to use, but no reading at all.
    ///
    /// "Raise the tune power" is the right advice at 0.3 W and actively
    /// misleading at 0.01 W, where the operator will keep winding the
    /// power up against a fault that is somewhere else entirely: the
    /// carrier, the drive, or the telemetry. The two get different
    /// sentences.
    static constexpr double kSilentBridgeW = 0.05;
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
    // Baseline comes FIRST and before the carrier: it is only a
    // baseline if the transmitter is off while it is taken.
    enum class State { Idle, Baseline, Keying, Settling, Measuring,
                       Finishing };

    void stepToNextPoint();
    void beginMeasure();
    void closePoint();
    /// Which of the three "measured nothing" faults this run hit, in
    /// words the operator can act on. See the definition — the fork is
    /// made on ADC counts, not on watts, and the reason matters.
    QString deadRunReason() const;
    void finish(bool completed, const QString& reason);
    /// Baseline window has elapsed: latch the idle reading and key.
    /// Split out because keying can be refused, and that refusal now
    /// happens here rather than inside startSweep().
    void beginKeying();

    MoxController* m_mox{nullptr};
    std::function<void(bool)> m_tuneFn;
    double m_bridgeRatio{1.0};
    /// Key or release through the injected path, falling back to the
    /// bare MoxController when nothing was injected.
    void keyTune(bool on);
    std::function<void(quint64)> m_txFrequencyFn;
    std::function<void()>        m_txFreqRestoreFn;
    std::function<bool()>        m_readyFn;
    std::function<QString()>     m_antennaLabelFn;

    State          m_state{State::Idle};
    SwrSweepPlan   m_plan;
    SwrSweepResult m_result;
    int            m_index{0};
    int            m_highSwrRun{0};
    int            m_deadRun{0};

    // Per-point accumulation while Measuring.
    double  m_accFwd{0.0};
    double  m_accRev{0.0};
    int     m_accN{0};
    quint16 m_accFwdRawPeak{0};
    quint16 m_accRevRawPeak{0};

    // Idle-reading accumulation during State::Baseline.
    double  m_baseAcc{0.0};
    double  m_baseRevAcc{0.0};
    int     m_baseN{0};
    quint16 m_baselineRaw{0};
    quint16 m_baselineRevRaw{0};
    // Whether any caller supplied raw ADC counts at all. Without it,
    // a caller that passes watts only (every test written before the
    // baseline landed) has baseline 0 and peak 0, which satisfies
    // "reverse never rose above idle" and gets flagged as a dead
    // channel. Absence of data is not evidence of a fault.
    bool    m_sawAnyRaw{false};
    qint64 m_lastTelemetryMs{0};

    QTimer m_stepTimer;      // single-shot, drives every state change
    QTimer m_watchdog;       // telemetry-silence abort

    int m_tuneSettleMs{kTuneSettleMs};
    int m_telemetryTimeoutMs{kTelemetryTimeoutMs};
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::SwrSweepPlan)
Q_DECLARE_METATYPE(NereusSDR::SwrSweepResult)
