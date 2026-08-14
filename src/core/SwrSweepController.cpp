// =================================================================
// src/core/SwrSweepController.cpp  (NereusSDR)
// =================================================================
//
// See SwrSweepController.h and the design doc
// docs/architecture/2026-08-13-swr-sweep-analyzer-design.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-13 — Created by Ralph Martin Fischer (OE5SOS),
//                 AI-assisted implementation via Anthropic Claude
//                 (Cowork).
// =================================================================

#include "SwrSweepController.h"

#include "core/MoxController.h"
#include "core/LogCategories.h"

#include <QDateTime>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// SwrSweepResult
// ---------------------------------------------------------------------------

quint64 SwrSweepResult::resonanceHz() const
{
    quint64 f = 0;
    double best = 1e9;
    for (const SwrSweepPoint& p : points) {
        if (p.swr > 0.0 && p.swr < best) {
            best = p.swr;
            f = p.freqHz;
        }
    }
    return f;
}

double SwrSweepResult::minSwr() const
{
    double best = 0.0;
    for (const SwrSweepPoint& p : points) {
        if (p.swr > 0.0 && (best <= 0.0 || p.swr < best)) {
            best = p.swr;
        }
    }
    return best;
}

int SwrSweepResult::validPoints() const
{
    int n = 0;
    for (const SwrSweepPoint& p : points) {
        if (p.swr > 0.0) { ++n; }
    }
    return n;
}

// ---------------------------------------------------------------------------
// SwrSweepPlan
// ---------------------------------------------------------------------------

SwrSweepPlan SwrSweepPlan::forBand(Band b)
{
    SwrSweepPlan plan;
    plan.band = b;
    // 60 m is channelized (US block) — excluded in v1, design doc
    // §Deferred. GEN / WWV / XVTR / SWL are not TX bands at all.
    if (b == Band::Band60m) {
        return plan;
    }
    double lo = 0.0;
    double hi = 0.0;
    if (!bandRangeHz(b, lo, hi)) {
        return plan;
    }
    plan.startHz = static_cast<quint64>(lo);
    plan.stopHz  = static_cast<quint64>(hi);
    return plan;
}

bool SwrSweepPlan::clipToGuard(const safety::BandPlanGuard& guard,
                               safety::Region region, DSPMode mode)
{
    if (!isValid() || points < kMinPoints || points > kMaxPoints) {
        return false;
    }
    // Probe every planned point once; keep the contiguous valid run
    // containing the band middle (regional edges only ever trim the
    // ends, so first-valid..last-valid is that run).
    int firstValid = -1;
    int lastValid  = -1;
    for (int i = 0; i < points; ++i) {
        const auto f = static_cast<std::int64_t>(freqAt(i));
        if (guard.isValidTxFreq(region, f, mode, /*extended=*/false)) {
            if (firstValid < 0) {
                firstValid = i;
            }
            lastValid = i;
        }
    }
    if (firstValid < 0 || (lastValid - firstValid + 1) < kMinPoints) {
        return false;
    }
    const quint64 newStart = freqAt(firstValid);
    const quint64 newStop  = freqAt(lastValid);
    startHz = newStart;
    stopHz  = newStop;
    points  = lastValid - firstValid + 1;
    return true;
}

quint64 SwrSweepPlan::freqAt(int index) const
{
    if (points <= 1) {
        return startHz;
    }
    const double t = static_cast<double>(index)
                     / static_cast<double>(points - 1);
    return startHz
           + static_cast<quint64>(t * static_cast<double>(stopHz - startHz)
                                  + 0.5);
}

// ---------------------------------------------------------------------------
// SwrSweepController
// ---------------------------------------------------------------------------

SwrSweepController::SwrSweepController(QObject* parent)
    : QObject(parent)
{
    m_stepTimer.setSingleShot(true);
    connect(&m_stepTimer, &QTimer::timeout, this, [this]() {
        switch (m_state) {
        case State::Keying:
            // Carrier is up and settled — measure the first point.
            stepToNextPoint();
            break;
        case State::Settling:
            beginMeasure();
            break;
        case State::Measuring:
            closePoint();
            break;
        case State::Finishing: {
            // Tune released and settled: restore the operator's TX
            // frequency and hand out the result.
            if (m_txFreqRestoreFn) {
                m_txFreqRestoreFn();
            }
            m_state = State::Idle;
            const SwrSweepResult result = m_result;
            m_result = SwrSweepResult{};
            emit sweepFinished(result);
            break;
        }
        case State::Idle:
            break;
        }
    });

    m_watchdog.setSingleShot(true);
    connect(&m_watchdog, &QTimer::timeout, this, [this]() {
        if (m_state == State::Measuring || m_state == State::Settling
            || m_state == State::Keying) {
            abortSweep(QStringLiteral(
                "No PA telemetry for %1 ms — link or radio gone")
                    .arg(m_telemetryTimeoutMs));
        }
    });
}

double SwrSweepController::swrFromWatts(double fwdW, double revW)
{
    if (fwdW < kMinFwdW) {
        return 0.0;   // bridge noise, not a measurement
    }
    if (revW < 0.0) {
        revW = 0.0;
    }
    if (revW >= fwdW) {
        return 99.0;
    }
    const double gamma = std::sqrt(revW / fwdW);
    if (gamma >= 0.999) {
        return 99.0;
    }
    const double swr = (1.0 + gamma) / (1.0 - gamma);
    return swr > 99.0 ? 99.0 : swr;
}

void SwrSweepController::setTimingsForTest(int tuneSettleMs,
                                           int telemetryTimeoutMs)
{
    m_tuneSettleMs       = tuneSettleMs;
    m_telemetryTimeoutMs = telemetryTimeoutMs;
}

bool SwrSweepController::startSweep(const SwrSweepPlan& plan)
{
    if (m_state != State::Idle) {
        emit reasonChanged(QStringLiteral("Sweep already running"));
        return false;
    }
    if (!plan.isValid() || plan.points < SwrSweepPlan::kMinPoints
        || plan.points > SwrSweepPlan::kMaxPoints) {
        emit reasonChanged(QStringLiteral("Sweep plan invalid"));
        return false;
    }
    if (!m_mox || !m_txFrequencyFn || !m_txFreqRestoreFn) {
        emit reasonChanged(QStringLiteral("Sweep backend not wired"));
        return false;
    }
    if (m_readyFn && !m_readyFn()) {
        emit reasonChanged(QStringLiteral(
            "Not connected / not powered — sweep refused"));
        return false;
    }
    if (m_mox->state() != MoxState::Rx) {
        emit reasonChanged(QStringLiteral(
            "Radio is transmitting — sweep refused"));
        return false;
    }

    m_plan   = plan;
    m_result = SwrSweepResult{};
    m_result.band = plan.band;
    m_result.startedAt = QDateTime::currentDateTime();
    if (m_antennaLabelFn) {
        m_result.antennaLabel = m_antennaLabelFn();
    }
    m_index = 0;
    m_highSwrRun = 0;
    m_deadRun = 0;

    // Key the carrier through the fully-gated TUNE path. Every
    // existing safety check (band plan, CW block, TX inhibit) runs
    // inside; a refusal leaves MoxController in Rx.
    m_mox->setTune(true);
    if (m_mox->state() == MoxState::Rx) {
        emit reasonChanged(QStringLiteral(
            "TUNE was refused (band plan / safety gate)"));
        return false;
    }

    m_state = State::Keying;
    m_lastTelemetryMs = QDateTime::currentMSecsSinceEpoch();
    m_watchdog.start(m_telemetryTimeoutMs);
    m_stepTimer.start(m_tuneSettleMs);
    emit sweepStarted(m_plan);
    emit progressChanged(0, m_plan.points);
    qCInfo(lcConnection).nospace()
        << "SWR sweep started: " << bandLabel(m_plan.band)
        << " " << m_plan.startHz << ".." << m_plan.stopHz
        << " Hz, " << m_plan.points << " points";
    return true;
}

void SwrSweepController::abortSweep(const QString& reason)
{
    if (m_state == State::Idle || m_state == State::Finishing) {
        return;
    }
    qCInfo(lcConnection) << "SWR sweep aborted:" << reason;
    finish(false, reason);
}

void SwrSweepController::stepToNextPoint()
{
    if (m_index >= m_plan.points) {
        finish(true, QString());
        return;
    }
    const quint64 f = m_plan.freqAt(m_index);
    m_txFrequencyFn(f);
    m_state = State::Settling;
    m_stepTimer.start(m_plan.settleMs);
}

void SwrSweepController::beginMeasure()
{
    m_accFwd = 0.0;
    m_accRev = 0.0;
    m_accN   = 0;
    m_accFwdRawPeak = 0;
    m_state  = State::Measuring;
    m_stepTimer.start(m_plan.dwellMs);
}

void SwrSweepController::ingestTelemetry(double fwdW, double revW,
                                         quint16 fwdRaw)
{
    if (m_state == State::Idle || m_state == State::Finishing) {
        return;
    }
    m_lastTelemetryMs = QDateTime::currentMSecsSinceEpoch();
    m_watchdog.start(m_telemetryTimeoutMs);
    if (m_state != State::Measuring) {
        return;
    }
    m_accFwd += fwdW;
    m_accRev += revW;
    // Peak rather than mean: the question the raw count answers is
    // "did the board EVER report anything", and an average over a
    // 220 ms dwell buries a single honest sample under the silence
    // either side of it.
    m_accFwdRawPeak = std::max(m_accFwdRawPeak, fwdRaw);
    ++m_accN;
}

void SwrSweepController::closePoint()
{
    SwrSweepPoint pt;
    pt.freqHz = m_plan.freqAt(m_index);
    if (m_accN > 0) {
        pt.fwdW = m_accFwd / m_accN;
        const double revW = m_accRev / m_accN;
        pt.swr = swrFromWatts(pt.fwdW, revW);
    }
    m_result.points.append(pt);
    m_result.maxFwdW   = std::max(m_result.maxFwdW, pt.fwdW);
    m_result.maxFwdRaw = std::max(m_result.maxFwdRaw, m_accFwdRawPeak);
    emit pointReady(m_index, pt.freqHz, pt.swr, pt.fwdW);
    emit progressChanged(m_index + 1, m_plan.points);

    // Open-feedline guard: a run of essentially-total reflection is
    // "no antenna", not "bad antenna" — stop burning the PA on it.
    if (pt.swr >= kAbortSwr) {
        if (++m_highSwrRun >= kAbortSwrRun) {
            finish(false, QStringLiteral(
                "SWR >= %1 on %2 consecutive points — open feedline?")
                    .arg(kAbortSwr).arg(kAbortSwrRun));
            return;
        }
    } else {
        m_highSwrRun = 0;
    }

    // ── Nothing is being measured ────────────────────────────────────
    //
    // The mirror of the guard above. A run of points with no usable
    // forward reading means the bridge is not telling us anything, and
    // carrying on means transmitting another forty-odd times to learn
    // the same nothing. 2026-08-14 that is exactly what happened: the
    // full plan was keyed at 1 W tune power and the verdict — "no valid
    // measurements, forward power too low?" — arrived at the end, with
    // a question mark.
    //
    // The reason names the watts, because "too low" is not actionable
    // and "0.2 W measured, 0.5 W needed" is.
    if (pt.swr <= 0.0) {
        if (++m_deadRun >= kAbortDeadRun) {
            finish(false, m_result.maxFwdW < kSilentBridgeW
                ? QStringLiteral(
                      "Der Richtkoppler meldet gar nichts: höchstens "
                      "%1 W über %2 Punkte, roher ADC-Höchstwert %3 "
                      "von 4095. Der rohe Wert ist das, was das Gerät "
                      "gesendet hat — bewegt der sich nicht, wenn du "
                      "die Leistung erhöhst, liegt es nicht an der "
                      "Skalierung und nicht am Sweep, sondern daran, "
                      "dass keine HF entsteht oder der Koppler nichts "
                      "meldet.")
                      .arg(m_result.maxFwdW, 0, 'f', 2)
                      .arg(kAbortDeadRun)
                      .arg(m_result.maxFwdRaw)
                : QStringLiteral(
                      "Zu wenig Vorlauf über %1 Punkte — höchstens %2 W "
                      "gemessen, ab %3 W ist die Anzeige eine Messung. "
                      "Tune-Leistung höher stellen.")
                      .arg(kAbortDeadRun)
                      .arg(m_result.maxFwdW, 0, 'f', 2)
                      .arg(kMinFwdW, 0, 'f', 1));
            return;
        }
    } else {
        m_deadRun = 0;
    }

    ++m_index;
    stepToNextPoint();
}

void SwrSweepController::finish(bool completed, const QString& reason)
{
    m_watchdog.stop();
    m_stepTimer.stop();
    m_result.completed  = completed;
    m_result.abortReason = reason;
    if (!reason.isEmpty()) {
        emit reasonChanged(reason);
    }
    // Release the carrier FIRST on every path, then settle before the
    // TX-frequency restore (mirrors the TwoToneController release
    // choreography and its 300 ms Thetis-derived settle).
    if (m_mox) {
        m_mox->setTune(false);
    }
    m_state = State::Finishing;
    m_stepTimer.start(m_tuneSettleMs);
}

} // namespace NereusSDR
