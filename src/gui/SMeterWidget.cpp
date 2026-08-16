// =================================================================
// src/gui/SMeterWidget.cpp  (NereusSDR)
// =================================================================
// Source attribution (AetherSDR, GPLv3):
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 section 5 requirements.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Ported in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//                 Layout from AetherSDR src/gui/SMeterWidget.{h,cpp} [@0cd4559].
//                 Tasks 34/35/36 commit: constructor + setPowerScale +
//                 paintEvent arc/scale/tick/needle/label rendering ported.
//                 contextMenuEvent stubbed (Task 38 implements body).
//                 Peak hold decay logic preserved from upstream (animateNeedle,
//                 updatePeakHoldValue, setPeakHoldEnabled/TimeMs/DecayRate,
//                 resetPeak). Peak hold context-menu wiring (Task 38) deferred.
//   2026-05-19  Task 37: added testAdvanceTime(int ms) + testSetPeakHoldTimeMs(int ms) +
//                 testPeakLevel() test seams; m_testTimeOffsetMs member for synthetic
//                 time injection into updatePeakHoldValue(). No production behavior
//                 changed; decay constants (Fast 20/Medium 10/Slow 5 dB/s) already
//                 ported verbatim from AetherSDR src/gui/SMeterWidget.cpp:675-681 [@0cd4559].
//   2026-05-19  Task 38: right-click context menu implemented. buildContextMenu()
//                 factory builds the TX Mode / RX Mode / Peak Hold submenus.
//                 contextMenuEvent() delegates + exec()s. txModeToLabel() and
//                 rxModeToLabel() helpers convert enum to the label strings used
//                 by setTxMode() / setRxMode(). AppSettings persistence added to
//                 setTxMode() (SMeter_TxSelect int 0..3), setRxMode()
//                 (SMeter_RxSelect int 0..3), setPeakHoldEnabled()
//                 (PeakHoldEnabled "True"/"False"), and setPeakDecayRate(QString)
//                 (PeakDecayRate "Fast"/"Medium"/"Slow").
//                 NereusSDR-native: AetherSDR has no contextMenuEvent; it uses
//                 an inline settings strip in AppletPanel (removed by Task 40).
//   2026-05-19  Task 47: constructor now loads the 4 persisted keys from
//                 AppSettings (SMeter_TxSelect, SMeter_RxSelect,
//                 PeakHoldEnabled, PeakDecayRate) and applies them via the
//                 existing public setters so the widget starts in the saved
//                 state across restarts.
//                 NereusSDR-native (AetherSDR has no AppSettings persistence).
//   2026-05-24  Task 13 (Phase 3P-III): RadioModel* constructor overload +
//                 connectToRadioModel() helper + txScaleIs2kWForTesting() seam.
//                 SMeterWidget now subscribes to RadioModel::externalAmpOperateChanged
//                 (cross-vendor aggregator) rather than PgxlConnection::statusUpdated
//                 directly when constructed via the RadioModel* overload.
//                 Existing QWidget* constructor and MainWindow wiring unchanged.
// =================================================================
#include "SMeterWidget.h"

#include "gui/widgets/SignalReading.h"
#include "gui/styles/ThemeQss.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QtMath>
#include <QFontMetrics>

#include "StyleConstants.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"

namespace NereusSDR {

// --- Construction ------------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:13-47 [@0cd4559]

SMeterWidget::SMeterWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(minimumSizeHint());
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // Performance: paintEvent fills rect() opaquely as the first step
    // (line ~443).  Tell Qt this widget needs no transparent compositing
    // pass under it — Qt skips the parent backing-store rebuild and saves
    // one IOSurface memmove per paint.  SMeterWidget paints at 30 Hz
    // (needle animation) + on peak decay (20 Hz), so this fires frequently.
    setAttribute(Qt::WA_OpaquePaintEvent);

    m_needleFraction = dbmToFraction(m_levelDbm);
    m_targetNeedleFraction = m_needleFraction;
    m_peakHoldDecayStartDbm = m_peakHoldDbm;

    m_needleAnimation.setTimerType(Qt::PreciseTimer);
    m_needleAnimation.setInterval(kNeedleAnimationIntervalMs);
    connect(&m_needleAnimation, &QTimer::timeout, this, &SMeterWidget::animateNeedle);

    // Peak hold decay: drops 0.5 dB every 50 ms after a new peak
    m_peakDecay.setInterval(50);
    connect(&m_peakDecay, &QTimer::timeout, this, [this]() {
        m_peakDbm -= 0.5f;
        if (m_peakDbm < m_levelDbm) {
            m_peakDbm = m_levelDbm;
            m_peakDecay.stop();
        }
        updateNeedleTarget();
        update();
    });

    // Hard reset peak hold every 10 seconds
    m_peakReset.setInterval(10000);
    m_peakReset.start();
    connect(&m_peakReset, &QTimer::timeout, this, [this]() {
        m_peakDbm = m_levelDbm;
        updateNeedleTarget();
        update();
    });

    // Load persisted mode and peak-hold settings from AppSettings.
    // Defaults match the member initialisers: TxSelect=0 (Power),
    // RxSelect=0 (Signal), PeakHoldEnabled=true, PeakDecayRate=Medium.
    // Task 47 - NereusSDR-native (AetherSDR has no AppSettings persistence).
    auto& s = AppSettings::instance();

    const int txSel = s.value("SMeter_TxSelect", 0).toInt();
    const QString txLabels[] = { "Power", "SWR", "Level", "Compression" };
    setTxMode(txLabels[qBound(0, txSel, 3)]);

    const int rxSel = s.value("SMeter_RxSelect", 0).toInt();
    const QString rxLabels[] = { "Signal", "Sig Avg", "Signal Peak", "Max Bin" };
    setRxMode(rxLabels[qBound(0, rxSel, 3)]);

    const bool peakOn = s.value("PeakHoldEnabled", QString("True")).toString() == "True";
    setPeakHoldEnabled(peakOn);

    const QString decayRate = s.value("PeakDecayRate", QString("Medium")).toString();
    setPeakDecayRate(decayRate);
}

// RadioModel* overload: delegates to the QWidget* constructor then wires the
// cross-vendor external-amp aggregator signals.
// NereusSDR-native. Task 13 (Phase 3P-III).
SMeterWidget::SMeterWidget(RadioModel* model, QWidget* parent)
    : SMeterWidget(parent)
{
    if (model) {
        connectToRadioModel(model);
    }
}

// Subscribe to RadioModel's cross-vendor external-amp signals.
// Called from the RadioModel* constructor overload only.
// NereusSDR-native; no upstream equivalent. Task 13.
void SMeterWidget::connectToRadioModel(RadioModel* model)
{
    // externalAmpOperateChanged(bool): flip the TX power scale to 2 kW when
    // any external amp enters OPERATE; revert to barefoot scale when all amps
    // leave OPERATE. Mirrors the logic MainWindow wires for the PGXL path.
    connect(model, &RadioModel::externalAmpOperateChanged, this,
            [this](bool inOp) {
        setPowerScale(/*maxWatts=*/0, inOp);
    });

    // externalAmpFwdSwrUpdated(int, float): forward RF-Kit power telemetry to
    // the TX needle. Mirrors how MainWindow feeds ampMetersChanged to the
    // PGXL-sourced S-meter needle.
    connect(model, &RadioModel::externalAmpFwdSwrUpdated, this,
            [this](int fwd, float swr) {
        setTxMeters(static_cast<float>(fwd), swr);
    });
}

// --- Public interface --------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:51-143 [@0cd4559]

void SMeterWidget::setLevel(float dbm)
{
    // Sub-perceivable change guard.  MeterPoller pushes this 10 times a
    // second; radio noise causes the dBm reading to wiggle by a few tenths
    // even on a quiet band.  Below 0.5 dB the needle position is the same
    // pixel and the S-unit text doesn't change.  Skipping the animation
    // rebuild + repaint for those sub-threshold ticks was the largest
    // single contributor to the macOS paint-pipeline saturation observed
    // in the 2026-05-24 bench profile.
    if (std::abs(dbm - m_levelDbm) < 0.5f) {
        return;
    }
    m_levelDbm = dbm;

    // Peak hold (existing needle/triangle behavior)
    if (dbm > m_peakDbm) {
        m_peakDbm = dbm;
        m_peakDecay.start();
    }

    // Configurable peak hold line
    if (m_peakHoldEnabled) {
        if (dbm > m_peakHoldDbm) {
            m_peakHoldDbm = dbm;
            m_peakHoldDecayStartDbm = dbm;
            m_peakHoldTimer.start();
            m_peakHoldTimerRunning = true;
        }
        updatePeakHoldValue();
    }

    updateNeedleTarget();

    if (!m_transmitting) {
        update();
    }
}

void SMeterWidget::setTxMeters(float fwdPower, float swr)
{
    m_txPower = fwdPower;
    m_txSwr = swr;

    updateNeedleTarget();

    // Repaint whenever TX power data arrives - either because moxChanged set
    // m_transmitting, or because RF power is flowing regardless (e.g. VOX,
    // hardware-keyed CW, or interlock race where setTransmitting arrives late).
    if (m_transmitting || m_txPower > 0.5f) {
        update();
    }
}

void SMeterWidget::setMicMeters(float micLevel, float compLevel, float micPeak, float compPeak)
{
    Q_UNUSED(micLevel);
    Q_UNUSED(compLevel);
    m_micLevel = micPeak;
    // compPeak is raw dBFS from COMPPEAK. Silence gate at -30.
    const float comp = (compPeak > -30.0f) ? qBound(-25.0f, compPeak, 0.0f) : 0.0f;
    m_compLevel = comp;

    updateNeedleTarget();

    if (m_transmitting && (m_txMode == TxMode::Level || m_txMode == TxMode::Compression)) {
        update();
    }
}

void SMeterWidget::setTransmitting(bool tx)
{
    m_transmitting = tx;
    if (!tx) {
        // Clear TX values immediately on un-key so the RX reading becomes the
        // animation target as soon as transmit ends.
        m_txPower = 0.0f;
        m_txSwr   = 1.0f;
    }
    updateNeedleTarget();
    update();
}

void SMeterWidget::setTxMode(const QString& mode)
{
    if (mode == "Power")            m_txMode = TxMode::Power;
    else if (mode == "SWR")         m_txMode = TxMode::SWR;
    else if (mode == "Level")       m_txMode = TxMode::Level;
    else if (mode == "Compression") m_txMode = TxMode::Compression;

    // Persist TX mode selection.
    // Key SMeter_TxSelect (int 0..3): Power=0, SWR=1, Level=2, Compression=3.
    // NereusSDR-native key per design doc ss5.4.2.
    AppSettings::instance().setValue("SMeter_TxSelect",
                                     static_cast<int>(m_txMode));
    updateNeedleTarget();
    update();
}

// NereusSDR extends upstream's 2-mode dispatch to 4 modes.
// Upstream (AetherSDR src/gui/SMeterWidget.cpp:133-144 [@0cd4559]):
//   "S-Meter" -> SMeter, anything else -> SMeterPeak.
// NereusSDR adds "Sig Avg" (SignalAverage) and "Max Bin" (MaxBin)
// per design doc ss5.4.1.
// Context menu uses "Signal" / "Sig Avg" / "Signal Peak" / "Max Bin"
// per design doc ss5.4.2; "S-Meter" is the legacy external-caller form.
void SMeterWidget::setRxMode(const QString& mode)
{
    if (mode == "S-Meter" || mode == "Signal") {
        m_rxMode = RxMode::SMeter;
        m_source = "S-Meter";
    } else if (mode == "Sig Avg") {
        m_rxMode = RxMode::SignalAverage;
        m_source = "Sig Avg";
    } else if (mode == "Signal Peak") {
        m_rxMode = RxMode::SMeterPeak;
        m_source = "S-Meter Peak";
    } else if (mode == "Max Bin") {
        m_rxMode = RxMode::MaxBin;
        m_source = "Max Bin";
    } else {
        // Default covers "S-Meter Peak" and any unrecognized string
        m_rxMode = RxMode::SMeterPeak;
        m_source = "S-Meter Peak";
    }

    // Persist RX mode selection.
    // Key SMeter_RxSelect (int 0..3): Signal=0, SignalAverage=1, SignalPeak=2, MaxBin=3.
    // Range widened from AetherSDR's 0..1 per design doc ss5.4.2.
    AppSettings::instance().setValue("SMeter_RxSelect",
                                     static_cast<int>(m_rxMode));
    updateNeedleTarget();
    update();
}

// --- Needle target -----------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:146-178 [@0cd4559]

void SMeterWidget::updateNeedleTarget()
{
    updatePeakHoldValue();

    if (m_transmitting) {
        m_targetNeedleFraction = txValueToFraction(currentTxValue());
    } else if (m_rxMode == RxMode::SMeterPeak) {
        m_targetNeedleFraction = dbmToFraction(m_peakDbm);
    } else {
        // SMeter, SignalAverage, and MaxBin all use m_levelDbm as target;
        // the *source* of that value differs (caller feeds the right WDSP meter).
        m_targetNeedleFraction = dbmToFraction(m_levelDbm);
    }

    const bool needleAtTarget = qAbs(m_targetNeedleFraction - m_needleFraction) <= kNeedleSnapEpsilon;
    if (needleAtTarget) {
        m_needleFraction = m_targetNeedleFraction;
    }

    const bool peakHoldAnimating = m_peakHoldEnabled
        && m_peakHoldTimerRunning
        && m_peakHoldTimer.elapsed() > m_peakHoldTimeMs
        && m_peakHoldDbm > m_levelDbm + 0.01f;

    if (needleAtTarget && !peakHoldAnimating) {
        if (m_needleAnimation.isActive()) {
            m_needleAnimation.stop();
        }
        return;
    }

    if (!m_needleAnimation.isActive()) {
        m_needleElapsed.restart();
        m_needleAnimation.start();
    }
}

// From AetherSDR src/gui/SMeterWidget.cpp:181-211 [@0cd4559]
void SMeterWidget::animateNeedle()
{
    const qint64 elapsedMs = m_needleElapsed.restart();
    if (elapsedMs <= 0) {
        return;
    }

    updatePeakHoldValue();

    const float delta = m_targetNeedleFraction - m_needleFraction;
    const float elapsedSeconds = static_cast<float>(elapsedMs) / 1000.0f;
    const float timeConstant = (delta >= 0.0f) ? kNeedleAttackTimeSeconds
                                                : kNeedleReleaseTimeSeconds;
    const float alpha = 1.0f - std::exp(-elapsedSeconds / timeConstant);
    const bool needleAtTarget = qAbs(delta) <= kNeedleSnapEpsilon;
    if (needleAtTarget) {
        m_needleFraction = m_targetNeedleFraction;
    } else {
        m_needleFraction += delta * alpha;
    }

    const bool peakHoldAnimating = m_peakHoldEnabled
        && m_peakHoldTimerRunning
        && m_peakHoldTimer.elapsed() > m_peakHoldTimeMs
        && m_peakHoldDbm > m_levelDbm + 0.01f;

    if (needleAtTarget && !peakHoldAnimating) {
        m_needleAnimation.stop();
    }

    // Visual-change guard.  The animation tick computes a new needle
    // fraction every 33 ms, but when the needle is settled or moving by
    // sub-pixel amounts the repaint is wasted work (paintEvent is the
    // single most expensive widget paint in the app per profile).  Skip
    // the update() unless the needle moved by at least 0.005 (1 pixel
    // on a 200 px arc) OR the peak hold marker is animating its decay.
    if (std::abs(m_needleFraction - m_lastDrawnNeedleFraction) > 0.005f
        || peakHoldAnimating) {
        m_lastDrawnNeedleFraction = m_needleFraction;
        update();
    }
}

// From AetherSDR src/gui/SMeterWidget.cpp:214-231 [@0cd4559]
// m_testTimeOffsetMs is 0 in production; testAdvanceTime() sets it for unit tests.
void SMeterWidget::updatePeakHoldValue()
{
    if (!m_peakHoldEnabled || !m_peakHoldTimerRunning) {
        return;
    }

    const qint64 elapsedMs = m_peakHoldTimer.elapsed() + m_testTimeOffsetMs;
    if (elapsedMs <= m_peakHoldTimeMs) {
        return;
    }

    const float decayElapsedSeconds =
        static_cast<float>(elapsedMs - m_peakHoldTimeMs) / 1000.0f;
    m_peakHoldDbm = m_peakHoldDecayStartDbm - (m_peakDecayDbPerSec * decayElapsedSeconds);
    if (m_peakHoldDbm <= m_levelDbm) {
        m_peakHoldDbm = m_levelDbm;
    }
}

// Test-only: advance synthetic time offset by ms and apply pending decay.
// Not called in production.  Each call to testAdvanceTime accumulates; call
// resetPeak() or re-enable peak hold between test cases to reset state.
void SMeterWidget::testAdvanceTime(int ms)
{
    m_testTimeOffsetMs += static_cast<qint64>(ms);
    updatePeakHoldValue();
}

// --- sUnitsText --------------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:233-242 [@0cd4559]

QString SMeterWidget::sUnitsText() const
{
    if (m_levelDbm <= S0_DBM) return "S0";
    if (m_levelDbm <= S9_DBM) {
        const int s = qRound((m_levelDbm - S0_DBM) / DB_PER_S);
        return QString("S%1").arg(qBound(0, s, 9));
    }
    const int over = qRound(m_levelDbm - S9_DBM);
    return QString("S9+%1").arg(over);
}

// --- Mapping -----------------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:246-258 [@0cd4559]

float SMeterWidget::dbmToFraction(float dbm) const
{
    // S0 to S9 occupies the left 60% of the arc
    // S9 to S9+60 occupies the right 40%
    const float clamped = qBound(S0_DBM, dbm, MAX_DBM);

    if (clamped <= S9_DBM) {
        // Linear within S0..S9 -> 0.0..0.6
        return 0.6f * (clamped - S0_DBM) / (S9_DBM - S0_DBM);
    }
    // Linear within S9..S9+60 -> 0.6..1.0
    return 0.6f + 0.4f * (clamped - S9_DBM) / (MAX_DBM - S9_DBM);
}

// From AetherSDR src/gui/SMeterWidget.cpp:260-276 [@0cd4559]
float SMeterWidget::txValueToFraction(float value) const
{
    switch (m_txMode) {
    case TxMode::Power:
        return qBound(0.0f, value / m_powerScaleMax, 1.0f);
    case TxMode::SWR:
        // 1.0-3.0
        return qBound(0.0f, (value - 1.0f) / 2.0f, 1.0f);
    case TxMode::Level:
        // -40 to +5
        return qBound(0.0f, (value + 40.0f) / 45.0f, 1.0f);
    case TxMode::Compression:
        // Gain reduction: 0 = none, -25 = heavy compression
        return qBound(0.0f, (value + 25.0f) / 25.0f, 1.0f);
    }
    return 0.0f;
}

// From AetherSDR src/gui/SMeterWidget.cpp:278-287 [@0cd4559]
float SMeterWidget::currentTxValue() const
{
    switch (m_txMode) {
    case TxMode::Power:       return m_txPower;
    case TxMode::SWR:         return m_txSwr;
    case TxMode::Level:       return m_micLevel;
    case TxMode::Compression: return m_compLevel;
    }
    return 0.0f;
}

// --- Paint -------------------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:291-640 [@0cd4559]

void SMeterWidget::paintEvent(QPaintEvent*)
{
    // ── Variante 1 aus smeter-beide.html: Balken + Zahlenblock ───────
    //
    // Loest das Zifferblatt ab. Aufbau von links nach rechts:
    // Empfaengerkuerzel, versenkter Kasten mit dem Balken, darunter die
    // Skalenzahlen; rechts die grosse Zahl in Monospace mit kleiner
    // Einheit, darunter zwei Zeilen Schluessel-Wert.
    //
    // Masse aus dem Entwurf, wo HAUSSTIL nichts anderes sagt.
    //
    // ── Zwei Abweichungen vom Entwurf, beide vom Betreiber ──────────
    //
    // 1. Die Skalenbeschriftung ist EINFARBIG. Im Entwurf stehen dBm/S0/
    //    S3/S7 in Grau und +10/+40/+60 in der Messwertfarbe -- wieder
    //    eine zweifarbige Skala ohne Bedeutung, dieselbe Sache wie das
    //    rote Skalenende, nur in Bernstein. Die obere Haelfte ist nicht
    //    mehr Messwert als die untere.
    //
    // 2. Grosse Zahl auf measured, Einheit auf text-secondary;
    //    Schluessel auf Skalenfarbe, Werte auf measured. Das CSS des
    //    Entwurfs hat es umgekehrt (Zahl auf --t1, Schluessel auf
    //    --data); die Anweisung des Betreibers geht vor, und sie ist
    //    stimmiger: der Wert ist die Messung, die Beschriftung ist eine
    //    Beschriftung.
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(Style::role("app-bg", Style::kAppBg)));

    const int w = width();
    const int h = height();

    const QColor cMeasured(Style::role("measured", Style::kAmberText));
    const QColor cScale(Style::role("text-scale", Style::kTextScale));
    const QColor cLabel(Style::role("text-secondary", Style::kTextSecondary));
    const QColor cBorder(Style::role("border", Style::kBorder));
    const QColor cInset(Style::role("inset-bg", Style::kInsetBg));

    // ── Der versenkte Kasten ────────────────────────────────────────
    constexpr int kPad = 12, kPadX = 14, kGap = 14, kRightW = 132;
    const QRect box(2, 2, w - 4, h - 4);
    p.setPen(QPen(cBorder, 1));
    p.setBrush(cInset);
    p.drawRoundedRect(box, 6, 6);   // HAUSSTIL: Radius 6, nie 3
    p.setBrush(Qt::NoBrush);

    const int innerL = box.left() + kPadX;
    const int innerT = box.top() + kPad;

    // ── Empfaengerkuerzel ───────────────────────────────────────────
    QFont fRx = p.font();
    fRx.setPixelSize(14);
    p.setFont(fRx);
    const QString rxLabel = m_transmitting ? QStringLiteral("TX")
                                           : QStringLiteral("RX");
    const QFontMetrics fmRx(fRx);
    p.setPen(cLabel);
    p.drawText(innerL, innerT + fmRx.ascent() + 3, rxLabel);

    // ── Daneben oder darunter ───────────────────────────────────────
    //
    // Gemessen am 2026-08-16: die Applet-Spalte ist beim Betreiber
    // 260 px breit (MainSplitterSizes 400,260), abzueglich Raender
    // weniger. Nebeneinander bliebe fuer den Balken zu wenig -- und
    // dann schoebe sich der Balken zusammen, statt dass die Anzeige den
    // Platz vernuenftig aufteilt.
    //
    // Also: unterhalb kStackBelowW wandert der Zahlenblock UNTER den
    // Balken, und der bekommt die volle Breite.
    const bool stacked = (w < kStackBelowW);
    const int barL = innerL + fmRx.horizontalAdvance(QStringLiteral("RX")) + kGap;
    const int barR = stacked ? (box.right() - kPadX)
                             : (box.right() - kPadX - kRightW - kGap);
    const int barW = std::max(40, barR - barL);
    constexpr int kBarH = 34;

    // ── Welcher Wert, welche Skala ──────────────────────────────────
    const float shown = m_transmitting
        ? txDisplayValue()
        : ((m_rxMode == RxMode::SMeterPeak) ? m_peakDbm : m_levelDbm);
    const bool haveReading =
        m_transmitting || SignalReading::isMeasurement(shown);

    // ── Der Balken ──────────────────────────────────────────────────
    const QRect bar(barL, innerT, barW, kBarH);
    p.setPen(QPen(cBorder, 1));
    p.setBrush(QColor(0, 0, 0));
    p.drawRoundedRect(bar, 4, 4);
    p.setBrush(Qt::NoBrush);

    if (haveReading) {
        const float frac = m_transmitting ? txDisplayFraction()
                                          : dbmToFraction(shown);
        const int fillW = int(qBound(0.0f, frac, 1.0f) * (barW - 6));
        if (fillW > 0) {
            // Senkrechter Verlauf im Messwert-Ton.
            QLinearGradient g(0, bar.top() + 4, 0, bar.bottom() - 4);
            QColor lo = cMeasured; lo.setAlpha(150);
            g.setColorAt(0.0, cMeasured);
            g.setColorAt(1.0, lo);
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawRoundedRect(QRect(bar.left() + 3, bar.top() + 4,
                                    fillW, kBarH - 8), 2, 2);
            p.setBrush(Qt::NoBrush);
        }
    }

    // ── Spitzenmarke ────────────────────────────────────────────────
    //
    // Kommt aus dem Zifferblatt zurueck, weil sie etwas zeigt, was der
    // Balken nicht kann: die Spitze der letzten Sekunden mit Nachlauf
    // (m_peakDbm, Abklingstufen Fast/Medium/Slow). Ein duenner Strich
    // statt eines Dreiecks -- im Balken ist dafuer kein Platz.
    if (!m_transmitting && m_rxMode == RxMode::SMeterPeak
        && SignalReading::isMeasurement(m_peakDbm)) {
        const float pf = qBound(0.0f, dbmToFraction(m_peakDbm), 1.0f);
        const int px = bar.left() + 3 + int(pf * (barW - 6));
        p.setPen(QPen(QColor(Style::role("instrument-face",
                                         Style::kInstrumentFace)), 1));
        p.drawLine(px, bar.top() + 4, px, bar.bottom() - 4);
    }

    // ── Segmentstriche: Aussparungen, keine aufgemalten Linien ──────
    //
    // In der Grundfarbe des Kastens ueber den Balken gezogen. Als
    // gemalte Linien wuerden sie bei jeder Aenderung des Verlaufs
    // mitwandern; als Aussparung bleiben sie stehen, wo die Teilung
    // ist, und der Balken laeuft dahinter durch.
    p.setPen(QPen(QColor(0x1c, 0x1d, 0x21), 1));
    for (int i = 1; i < 20; ++i) {
        const int x = bar.left() + int(double(i) / 20.0 * barW);
        p.drawLine(x, bar.top() + 5, x, bar.bottom() - 5);
    }

    // ── Skalenzahlen, EINFARBIG ─────────────────────────────────────
    QFont fSc = p.font();
    fSc.setPixelSize(10);
    fSc.setFamily(QStringLiteral("Menlo"));
    fSc.setStyleHint(QFont::Monospace);
    p.setFont(fSc);
    const QFontMetrics fmSc(fSc);
    p.setPen(cScale);
    // Die Skala folgt DEM, WAS DER BALKEN ZEIGT -- eine Quelle fuer
    // beides, wie das Kuerzel an der Zahl. Watt-Marken unter einem
    // Leistungsbalken, S-Stufen unter dem Empfangspegel. Eine Skala,
    // die der Groesse widerspricht, waere schlechter als gar keine.
    const QStringList scale = scaleLabels();
    const int scaleY = bar.bottom() + 5 + fmSc.ascent();
    for (int i = 0; i < scale.size(); ++i) {
        const double f = double(i) / (scale.size() - 1);
        int x = bar.left() + int(f * barW);
        if (i == 0) { x = bar.left(); }
        else if (i == scale.size() - 1) {
            x = bar.right() - fmSc.horizontalAdvance(scale[i]);
        } else {
            x -= fmSc.horizontalAdvance(scale[i]) / 2;
        }
        p.drawText(x, scaleY, scale[i]);
    }

    // ── Rechte Spalte: grosse Zahl, Einheit, zwei Zeilen ────────────
    const int rightR = box.right() - kPadX;
    const int blockTop = stacked ? (scaleY + 10) : innerT;

    QFont fBig = p.font();
    fBig.setPixelSize(30);
    fBig.setFamily(QStringLiteral("Menlo"));
    fBig.setStyleHint(QFont::Monospace);
    QFont fUnit = p.font();
    fUnit.setPixelSize(11);

    const QString bigText = haveReading
        ? QString::number(double(shown), 'f', 0)
        : SignalReading::noReadingText();
    const QString unitText = haveReading ? txUnitLabel() : QString{};

    const QFontMetrics fmBig(fBig), fmUnit(fUnit);
    const int unitW = unitText.isEmpty()
                        ? 0 : fmUnit.horizontalAdvance(unitText) + 3;
    p.setFont(fBig);
    p.setPen(haveReading ? cMeasured : cScale);
    const int bigY = blockTop + fmBig.ascent();
    p.drawText(rightR - unitW - fmBig.horizontalAdvance(bigText), bigY, bigText);
    if (!unitText.isEmpty()) {
        p.setFont(fUnit);
        p.setPen(cLabel);
        p.drawText(rightR - fmUnit.horizontalAdvance(unitText), bigY, unitText);
    }

    // Zwei Zeilen Schluessel-Wert. Der erste Platz nennt die aktive
    // QUELLE -- das ist ein Modus, kein zweiter Messwert, und er gehoert
    // deshalb an die Zahl. Der zweite ist ein FESTER Platz, der immer
    // eine Messung traegt: Rauschflur, in RADE stattdessen SNR. Ein
    // Platz, zwei Beschriftungen, immer ein Wert -- damit springt das
    // Panel nicht in der Hoehe, was bei einem festen Kopf ueber einem
    // scrollbaren Koerper auffiele.
    QFont fKv = p.font();
    fKv.setPixelSize(11);
    fKv.setFamily(QStringLiteral("Menlo"));
    fKv.setStyleHint(QFont::Monospace);
    p.setFont(fKv);
    const QFontMetrics fmKv(fKv);
    const int kvL = stacked ? innerL : (rightR - kRightW);

    struct Row { QString k, v; };
    const Row rows[2] = {
        { rxSourceAbbrev(), haveReading ? sUnitsText()
                                        : SignalReading::noReadingText() },
        { m_snrIsRade ? QStringLiteral("SNR") : QStringLiteral("NF"),
          SignalReading::isMeasurement(m_secondaryDbm)
              ? QStringLiteral("%1 dB").arg(double(m_secondaryDbm), 0, 'f', 0)
              : SignalReading::noReadingText() },
    };
    int kvY = bigY + 7 + fmKv.ascent();
    for (const Row& r : rows) {
        p.setPen(cScale);
        p.drawText(kvL, kvY, r.k);
        p.setPen(SignalReading::noReadingText() == r.v ? cScale : cMeasured);
        p.drawText(rightR - fmKv.horizontalAdvance(r.v), kvY, r.v);
        kvY += fmKv.height() + 3;
    }
}

// --- Power scale -------------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:642-656 [@0cd4559]
// Upstream parameter is named hasAmplifier. NereusSDR plan uses hasAmplifier
// but documents it as pgxlConnected in the design table. The third branch
// (wScale > 500 -> generalized formula) does NOT exist in AetherSDR upstream:
// upstream treats ANY maxWatts > 100 as Aurora (600/500). The design table's
// "generalized formula" row is subsumed by the Aurora branch at all tested
// wScale values. Preserving upstream behavior verbatim per source-first protocol.
void SMeterWidget::setPowerScale(int maxWatts, bool hasAmplifier)
{
    if (hasAmplifier) {
        m_powerScaleMax = 2000.0f;
        m_powerRedStart = 1500.0f;
    } else if (maxWatts > 100) {
        m_powerScaleMax = 600.0f;
        m_powerRedStart = 500.0f;
    } else {
        m_powerScaleMax = 120.0f;
        m_powerRedStart = 100.0f;
    }
    updateNeedleTarget();
    update();
}

// --- Peak hold configuration -------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:660-698 [@0cd4559]

void SMeterWidget::setPeakHoldEnabled(bool enabled)
{
    m_peakHoldEnabled = enabled;
    m_peakHoldDbm = m_levelDbm;
    m_peakHoldDecayStartDbm = m_levelDbm;
    m_peakHoldTimerRunning = false;

    // Persist peak hold enabled state.
    // Key PeakHoldEnabled ("True"/"False") per design doc ss5.4.2.
    AppSettings::instance().setValue("PeakHoldEnabled",
                                     enabled ? QString("True") : QString("False"));
    updateNeedleTarget();
    update();
}

void SMeterWidget::setPeakHoldTimeMs(int ms)
{
    m_peakHoldTimeMs = qBound(100, ms, 2000);
}

void SMeterWidget::setPeakDecayRate(DecayRate rate)
{
    switch (rate) {
    case DecayRate::Fast:   m_peakDecayDbPerSec = 20.0f; break;
    case DecayRate::Medium: m_peakDecayDbPerSec = 10.0f; break;
    case DecayRate::Slow:   m_peakDecayDbPerSec = 5.0f;  break;
    }
}

void SMeterWidget::setPeakDecayRate(const QString& rate)
{
    if (rate == "Fast")        setPeakDecayRate(DecayRate::Fast);
    else if (rate == "Slow")   setPeakDecayRate(DecayRate::Slow);
    else                       setPeakDecayRate(DecayRate::Medium);

    // Persist decay rate selection.
    // Key PeakDecayRate ("Fast"/"Medium"/"Slow") per design doc ss5.4.2.
    // Normalize the stored string to the canonical set.
    const QString canonical = (rate == "Fast")  ? QString("Fast")
                            : (rate == "Slow")  ? QString("Slow")
                                                : QString("Medium");
    AppSettings::instance().setValue("PeakDecayRate", canonical);
}

void SMeterWidget::resetPeak()
{
    m_peakHoldDbm = m_levelDbm;
    m_peakHoldDecayStartDbm = m_levelDbm;
    m_peakHoldTimerRunning = false;
    updateNeedleTarget();
    update();
}

// --- Context menu ------------------------------------------------------------
// NereusSDR-native UX per design doc ss5.4.2. AetherSDR does not have a
// contextMenuEvent; it uses an inline settings strip in AppletPanel (removed
// by Task 40). The menu structure matches the design doc exactly:
//   TX Mode (exclusive) -> Power / SWR / Level / Compression
//   RX Mode (exclusive) -> Signal / Sig Avg / Signal Peak / Max Bin
//   Peak Hold -> Enabled (toggle) / Decay (Fast/Medium/Slow) / Reset

void SMeterWidget::contextMenuEvent(QContextMenuEvent* ev)
{
    QMenu* menu = buildContextMenu(this);
    menu->exec(ev->globalPos());
    menu->deleteLater();
}

QMenu* SMeterWidget::buildContextMenu(QObject* parent)
{
    auto* menu = new QMenu(qobject_cast<QWidget*>(parent));

    // ---- TX Mode submenu (exclusive action group) ----------------------------
    QMenu* txMenu = menu->addMenu("TX Mode");
    auto* txGroup = new QActionGroup(menu);
    txGroup->setExclusive(true);

    struct TxEntry { const char* label; TxMode mode; };
    const TxEntry txEntries[] = {
        {"Power",       TxMode::Power},
        {"SWR",         TxMode::SWR},
        {"Level",       TxMode::Level},
        {"Compression", TxMode::Compression},
    };
    for (const auto& e : txEntries) {
        auto* a = txMenu->addAction(QLatin1String(e.label));
        a->setCheckable(true);
        a->setChecked(m_txMode == e.mode);
        txGroup->addAction(a);
        const TxMode capturedMode = e.mode;
        connect(a, &QAction::triggered, this, [this, capturedMode]() {
            setTxMode(txModeToLabel(capturedMode));
        });
    }

    // ---- RX Mode submenu (exclusive action group) ----------------------------
    // Labels per design doc ss5.4.2 (Signal / Sig Avg / Signal Peak / Max Bin).
    QMenu* rxMenu = menu->addMenu("RX Mode");
    auto* rxGroup = new QActionGroup(menu);
    rxGroup->setExclusive(true);

    struct RxEntry { const char* label; RxMode mode; };
    const RxEntry rxEntries[] = {
        {"Signal",       RxMode::SMeter},
        {"Sig Avg",      RxMode::SignalAverage},
        {"Signal Peak",  RxMode::SMeterPeak},
        {"Max Bin",      RxMode::MaxBin},
    };
    for (const auto& e : rxEntries) {
        auto* a = rxMenu->addAction(QLatin1String(e.label));
        a->setCheckable(true);
        a->setChecked(m_rxMode == e.mode);
        rxGroup->addAction(a);
        const RxMode capturedMode = e.mode;
        connect(a, &QAction::triggered, this, [this, capturedMode]() {
            setRxMode(rxModeToLabel(capturedMode));
        });
    }

    // ---- Peak Hold submenu ---------------------------------------------------
    QMenu* peakMenu = menu->addMenu("Peak Hold");

    // Enabled toggle
    auto* enabledA = peakMenu->addAction("Enabled");
    enabledA->setCheckable(true);
    enabledA->setChecked(m_peakHoldEnabled);
    connect(enabledA, &QAction::triggered, this,
            [this](bool checked) { setPeakHoldEnabled(checked); });

    // Decay sub-submenu (exclusive action group)
    QMenu* decayMenu = peakMenu->addMenu("Decay");
    auto* decayGroup = new QActionGroup(menu);
    decayGroup->setExclusive(true);

    struct DecayEntry { const char* label; DecayRate rate; float dbPerSec; };
    const DecayEntry decayEntries[] = {
        {"Fast",   DecayRate::Fast,   20.0f},
        {"Medium", DecayRate::Medium, 10.0f},
        {"Slow",   DecayRate::Slow,    5.0f},
    };
    for (const auto& e : decayEntries) {
        const QString labelWithRate = QString("%1 (%2 dB/s)")
            .arg(QLatin1String(e.label))
            .arg(static_cast<int>(e.dbPerSec));
        auto* a = decayMenu->addAction(labelWithRate);
        a->setCheckable(true);
        a->setChecked(qFuzzyCompare(m_peakDecayDbPerSec, e.dbPerSec));
        decayGroup->addAction(a);
        const QString rateName(QLatin1String(e.label));
        connect(a, &QAction::triggered, this, [this, rateName]() {
            setPeakDecayRate(rateName);
        });
    }

    peakMenu->addSeparator();

    // Reset transient action
    auto* resetA = peakMenu->addAction("Reset");
    connect(resetA, &QAction::triggered, this, &SMeterWidget::resetPeak);

    return menu;
}

// Convert TxMode enum to the label string accepted by setTxMode().
// NereusSDR-native; no upstream equivalent.
QString SMeterWidget::txModeToLabel(TxMode mode)
{
    switch (mode) {
    case TxMode::Power:       return "Power";
    case TxMode::SWR:         return "SWR";
    case TxMode::Level:       return "Level";
    case TxMode::Compression: return "Compression";
    }
    return "Power";
}

// Convert RxMode enum to the canonical label string accepted by setRxMode().
// Uses the context-menu display strings (Signal / Sig Avg / Signal Peak / Max Bin)
// per design doc ss5.4.2.
// NereusSDR-native; no upstream equivalent.
QString SMeterWidget::rxModeToLabel(RxMode mode)
{
    switch (mode) {
    case RxMode::SMeter:         return "Signal";
    case RxMode::SignalAverage:  return "Sig Avg";
    case RxMode::SMeterPeak:     return "Signal Peak";
    case RxMode::MaxBin:         return "Max Bin";
    }
    return "Signal";
}


// ── Der zweite Platz ────────────────────────────────────────────────

void SMeterWidget::setNoiseFloorDbm(float dbm)
{
    m_secondaryDbm = dbm;
    m_snrIsRade    = false;
    update();
}

void SMeterWidget::setRadeSnrDb(float db)
{
    // SNR verdraengt den Rauschflur, solange RADE laeuft -- beide
    // beantworten dieselbe Frage, und zwei Zeilen dafuer waeren eine zu
    // viel.
    m_secondaryDbm = db;
    m_snrIsRade    = true;
    update();
}

void SMeterWidget::clearSecondary()
{
    m_secondaryDbm = std::numeric_limits<float>::quiet_NaN();
    m_snrIsRade    = false;
    update();
}

QString SMeterWidget::rxSourceAbbrev() const
{
    switch (m_rxMode) {
    case RxMode::SignalAverage: return QStringLiteral("AVG");
    case RxMode::SMeterPeak:    return QStringLiteral("PK");
    case RxMode::MaxBin:        return QStringLiteral("MAX");
    case RxMode::SMeter:
    default:                    return QStringLiteral("SIG");
    }
}

// ── Die Sendeseite im selben Aufbau ─────────────────────────────────
//
// Der Entwurf kennt nur den Empfang. Damit das Panel beim Senden nicht
// leer dasteht, traegt derselbe Balken die gewaehlte TX-Groesse -- so
// wie es der Zeiger vorher auch tat.

float SMeterWidget::txDisplayValue() const
{
    switch (m_txMode) {
    case TxMode::Power:       return m_txPower;
    case TxMode::SWR:         return m_txSwr;
    case TxMode::Level:       return m_micLevel;
    case TxMode::Compression: return m_compLevel;
    }
    return 0.0f;
}

float SMeterWidget::txDisplayFraction() const
{
    switch (m_txMode) {
    case TxMode::Power:
        return m_powerScaleMax > 0.0f ? m_txPower / m_powerScaleMax : 0.0f;
    case TxMode::SWR:
        return qBound(0.0f, (m_txSwr - 1.0f) / 2.0f, 1.0f);
    case TxMode::Level:
    case TxMode::Compression:
        return qBound(0.0f, m_micLevel / 100.0f, 1.0f);
    }
    return 0.0f;
}

QString SMeterWidget::txUnitLabel() const
{
    if (!m_transmitting) { return QStringLiteral("dBm"); }
    switch (m_txMode) {
    case TxMode::Power:       return QStringLiteral("W");
    case TxMode::SWR:         return QString{};
    case TxMode::Level:
    case TxMode::Compression: return QStringLiteral("dB");
    }
    return QString{};
}


QStringList SMeterWidget::scaleLabels() const
{
    if (!m_transmitting) {
        return {QStringLiteral("dBm"), QStringLiteral("S0"),
                QStringLiteral("S3"),  QStringLiteral("S7"),
                QStringLiteral("+10"), QStringLiteral("+40"),
                QStringLiteral("+60")};
    }
    switch (m_txMode) {
    case TxMode::Power: {
        // Marken aus der LEBENDEN Skala, damit die Umskalierung je
        // angeschlossenem Geraet auch hier greift (Barefoot 100 W,
        // Aurora 600 W, PGXL 2 kW).
        QStringList out{QStringLiteral("W")};
        for (int i = 1; i <= 4; ++i) {
            out << QString::number(int(m_powerScaleMax * i / 4.0));
        }
        return out;
    }
    case TxMode::SWR:
        return {QStringLiteral("SWR"), QStringLiteral("1.0"),
                QStringLiteral("1.5"), QStringLiteral("2.0"),
                QStringLiteral("3.0")};
    case TxMode::Level:
    case TxMode::Compression:
        return {QStringLiteral("dB"), QStringLiteral("0"),
                QStringLiteral("25"), QStringLiteral("50"),
                QStringLiteral("75"), QStringLiteral("100")};
    }
    return {};
}

} // namespace NereusSDR
