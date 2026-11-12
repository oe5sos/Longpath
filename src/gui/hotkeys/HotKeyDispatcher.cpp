// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/hotkeys/HotKeyDispatcher.cpp  (Longpath)
// =================================================================
//
// Uebernommen aus dem Upstream NereusSDR (`src/gui/hotkeys/
// HotKeyDispatcher.cpp` @ 9036ffc04, J.J. Boyd KG4VCF). Gekuerzt um
// Punkt-/Strichtasten, Drehknoepfe und die Pruefung auf fremde Geraete;
// der Sprachspeicher geht ueber runVoiceAction.
//
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan.
//                AI-assisted implementation via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-10 - Uebernommen und gekuerzt fuer Longpath von Martin
//                Fischer (OE5SOS), KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.

#include "gui/hotkeys/HotKeyDispatcher.h"

#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "models/SliceModel.h"

#include <QDateTime>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <utility>

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <time.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace Longpath {

namespace {

// How long after a send the next tune still builds on that target rather than
// the slice's own frequency. Long enough for a remote slice to catch up, short
// enough that a drag or a typed frequency soon after is what the next key
// starts from. Upstream NereusSDR's choice, not a value from a reference.
constexpr qint64 kInFlightWindowMs = 250;

// A clock that stops while the machine sleeps. Qt's own can't serve: QElapsedTimer
// reads std::chrono::steady_clock, which keeps counting through sleep on macOS
// (libc++ src/chrono.cpp, CLOCK_MONOTONIC_RAW) and on Windows (QueryPerformanceCounter;
// Microsoft, "Acquiring high-resolution time stamps").
qint64 awakeClockMs()
{
#if defined(Q_OS_MAC)
    // man clock_gettime: CLOCK_UPTIME_RAW "does not increment while the system is asleep".
    return static_cast<qint64>(clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1000000ULL);
#elif defined(Q_OS_WIN)
    // Microsoft, QueryUnbiasedInterruptTime: "does not include time the system spends
    // in sleep or hibernation". Units of 100 ns. MinGW declares it too
    // (mingw-w64 realtimeapiset.h, _WIN32_WINNT >= 0x0601). The page does not mention
    // connected standby.
    ULONGLONG ticks = 0;
    QueryUnbiasedInterruptTime(&ticks);
    return static_cast<qint64>(ticks / 10000ULL);
#elif defined(Q_OS_LINUX)
    // clock_gettime(2): CLOCK_MONOTONIC "does not count time that the system is suspended".
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<qint64>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
#else
#error "The hot key sleep failsafe needs a clock that stops during sleep on this platform."
#endif
}

const QString& idTxHold()
{
    static const QString kId = QStringLiteral("tx.hold");
    return kId;
}

bool isTuneAction(const QString& id)
{
    return id.startsWith(QLatin1String("tune."));
}

// Signed steps for one use of a Turn action.
int stepsOfTurnAction(const QString& id)
{
    if (id == QLatin1String("tune.up10")) {
        return 10;
    }
    if (id == QLatin1String("tune.down10")) {
        return -10;
    }
    return id.endsWith(QLatin1String(".down")) ? -1 : 1;
}

// V6, and the plan's Lock case. Lock itself, DSP and audio stay usable.
bool refusedWhenLocked(const QString& id)
{
    return isTuneAction(id) || id.startsWith(QLatin1String("band.")) || id.startsWith(QLatin1String("mode."));
}

} // namespace

HotKeyDispatcher::HotKeyDispatcher(HotKeyBindings* bindings, HotKeyTargets* targets, QObject* parent)
    : QObject(parent)
    , m_bindings(bindings)
    , m_targets(targets)
{
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setTimerType(Qt::PreciseTimer);
    m_flushTimer.setInterval(static_cast<int>(TuneCoalescer::kMinIntervalMs));
    connect(&m_flushTimer, &QTimer::timeout, this, &HotKeyDispatcher::flushTune);

    m_wallClock = [] { return QDateTime::currentMSecsSinceEpoch(); };
    m_awakeClock = [] { return awakeClockMs(); };
    m_sleepTimer.setInterval(kSleepTickMs);
    connect(&m_sleepTimer, &QTimer::timeout, this, &HotKeyDispatcher::checkForSleep);

    connect(m_bindings, &HotKeyBindings::enabledChanged, this, [this](bool on) {
        if (!on) {
            releaseAllHolds();
            dropAllTurns();
        }
    });
}

bool HotKeyDispatcher::enabled() const
{
    return m_bindings->enabled();
}

void HotKeyDispatcher::press(const QString& id, const HotKeySource& source, QChar pin)
{
    if (!m_bindings->enabled()) {
        return;
    }
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (action == nullptr) {
        qCWarning(lcHotKeys) << "Unknown hot key action:" << id;
        return;
    }
    if (action->gesture == HotKeyGesture::Hold) {
        startHold(*action, source);
        return;
    }
    SliceModel* slice = nullptr;
    if (!admit(*action, pin, slice)) {
        return;
    }
    if (action->gesture == HotKeyGesture::Turn) {
        addTurnSteps(*action, slice, stepsOfTurnAction(id));
    } else {
        execute(*action, slice);
    }
}

void HotKeyDispatcher::release(const QString& id, const HotKeySource& source)
{
    // No off switch check: what is held is in the ledger, and letting go must
    // always work. Turning the switch off has already emptied it.
    if (id == idTxHold()) {
        endTransmitHold(source);
    }
}

void HotKeyDispatcher::releaseAll(const HotKeySource& source)
{
    endTransmitHold(source);
}

void HotKeyDispatcher::releaseAllHolds()
{
    const QList<HotKeySource> holding = m_holdSources;
    for (const HotKeySource& source : holding) {
        endTransmitHold(source);
    }
}

bool HotKeyDispatcher::takesKey(const QString& id) const
{
    return m_bindings->enabled() && m_targets->takesKey(id);
}

void HotKeyDispatcher::setSleepClocksForTest(ClockMs wallClock, ClockMs awakeClock)
{
    m_wallClock = std::move(wallClock);
    m_awakeClock = std::move(awakeClock);
    noteSleepClocks();
}

void HotKeyDispatcher::noteSleepClocks()
{
    m_lastWallMs = m_wallClock();
    m_lastAwakeMs = m_awakeClock();
}

void HotKeyDispatcher::checkForSleep()
{
    if (!m_sleepTimer.isActive()) {
        return;
    }
    const qint64 wallGained = m_wallClock() - m_lastWallMs;
    const qint64 awakeGained = m_awakeClock() - m_lastAwakeMs;
    noteSleepClocks();
    // The wall clock can also be moved forward by itself: a time sync, or someone
    // setting the clock. Epoch time ignores a daylight change unless the machine's
    // clock itself jumps. A jump past the threshold during a hold drops that hold,
    // which fails toward key-up. Moved back, it reads as no sleep.
    if (wallGained - awakeGained <= kSleepGapMs) {
        return;
    }
    qCInfo(lcHotKeys) << "Holds dropped: the machine slept for" << wallGained - awakeGained << "ms";
    // As the other failsafes: holds drop, and a latched MOX or a TUNE stays on.
    releaseAllHolds();
    emit holdsDroppedBySleep();
}

void HotKeyDispatcher::updateSleepWatch()
{
    const bool holding = !m_holdSources.isEmpty();
    if (holding == m_sleepTimer.isActive()) {
        return;
    }
    if (holding) {
        noteSleepClocks();
        m_sleepTimer.start();
    } else {
        m_sleepTimer.stop();
    }
}

void HotKeyDispatcher::onTransmitWentOff()
{
    m_holdKeyed = false;
    noteTransmitHold();
    emit transmitKeyWentOff();
}

void HotKeyDispatcher::noteTransmitHold()
{
    const bool on = transmitHoldOn();
    if (on != m_transmitHoldOn) {
        m_transmitHoldOn = on;
        emit transmitHoldChanged(on);
    }
}

void HotKeyDispatcher::flushTune()
{
    m_turnQueues.removeIf([](const TurnQueue& queue) { return queue.slice.isNull(); });

    const qint64 now = m_targets->nowMs();
    bool stillPending = false;
    for (TurnQueue& queue : m_turnQueues) {
        if (queue.coalescer.pending() == 0) {
            continue;
        }
        if (queue.isTune && (queue.slice.isNull() || queue.slice->locked())) {
            queue.coalescer.drop();
            queue.hasInFlight = false;
            continue;
        }
        const int due = queue.coalescer.takeDue(now);
        if (due != 0) {
            sendTurnSteps(queue, due);
        }
        stillPending = stillPending || queue.coalescer.pending() != 0;
    }
    if (stillPending) {
        armFlushTimer();
    }
}

bool HotKeyDispatcher::admit(const HotKeyAction& action, QChar pin, SliceModel*& slice)
{
    const QString reason = m_targets->unavailableReason(action.id);
    if (!reason.isEmpty()) {
        m_targets->toast(reason);
        return false;
    }
    if (action.scope != HotKeyScope::Slice) {
        return true;
    }

    const QChar letter = pin.isNull() ? m_bindings->pinFor(action.id) : pin.toUpper();
    if (letter.isNull()) {
        slice = m_targets->windowSlice();
        if (slice == nullptr) {
            return false;
        }
    } else {
        slice = m_targets->sliceByLetter(letter);
        if (slice == nullptr) {
            m_targets->toast(QStringLiteral("Slice %1 isn't open.").arg(letter));
            return false;
        }
    }

    // Upstream V6: Abstimmen auf einem gesperrten Empfaenger tut nichts
    // und sagt es. Sperre selbst, DSP und Ton bleiben bedienbar.
    if (slice->locked() && refusedWhenLocked(action.id)) {
        dropTune(slice);
        m_targets->toast(QStringLiteral("Slice %1 is locked.").arg(m_targets->letterOf(slice)));
        return false;
    }
    return true;
}

void HotKeyDispatcher::execute(const HotKeyAction& action, SliceModel* slice)
{
    // Der Sprachspeicher zuerst: Senden, Aufnehmen und Stopp gehen an
    // dieselbe Stelle, gleich welcher Geltungsbereich darunter steht.
    if (action.id.startsWith(QLatin1String("voice."))) {
        m_targets->runVoiceAction(action.id);
        return;
    }
    switch (action.scope) {
    case HotKeyScope::Slice:
        m_targets->runSliceAction(action.id, slice, 1);
        break;
    case HotKeyScope::Pan:
    case HotKeyScope::Window:
        m_targets->runWindowAction(action.id);
        break;
    case HotKeyScope::TransmitSlice:
        m_targets->runTransmitAction(action.id);
        break;
    }
}

void HotKeyDispatcher::addTurnSteps(const HotKeyAction& action, SliceModel* slice, int steps)
{
    const bool isTune = isTuneAction(action.id);
    auto queue = std::find_if(m_turnQueues.begin(), m_turnQueues.end(), [slice, isTune](const TurnQueue& q) {
        return q.slice == slice && q.isTune == isTune;
    });
    if (queue == m_turnQueues.end()) {
        m_turnQueues.append(TurnQueue{slice, isTune, {}, 0.0, 0, false});
        queue = std::prev(m_turnQueues.end());
    }

    const int due = queue->coalescer.add(steps, m_targets->nowMs());
    if (due != 0) {
        sendTurnSteps(*queue, due);
    }
    if (queue->coalescer.pending() != 0) {
        armFlushTimer();
    }
}

void HotKeyDispatcher::sendTurnSteps(TurnQueue& queue, int steps)
{
    if (queue.isTune) {
        sendTune(queue, steps);
    } else if (steps > 0) {
        m_targets->runSliceAction(QStringLiteral("volume.up"), queue.slice, steps);
    } else {
        m_targets->runSliceAction(QStringLiteral("volume.down"), queue.slice, -steps);
    }
}

void HotKeyDispatcher::sendTune(TurnQueue& queue, int steps)
{
    SliceModel* const slice = queue.slice;
    const int stepHz = slice->stepHz();
    if (stepHz <= 0) {
        qCWarning(lcHotKeys) << "Tune with no step size ignored:" << stepHz;
        queue.coalescer.drop();
        return;
    }

    const qint64 now = m_targets->nowMs();
    const bool burst = queue.hasInFlight && now - queue.inFlightAtMs <= kInFlightWindowMs;
    const double base = burst ? queue.inFlightHz : slice->frequency();
    const double step = stepHz;
    // Auf das Raster wie die Pfeiltasten im Panorama (SpectrumWidget::keyPressEvent).
    const double hz = std::round((base + steps * step) / step) * step;

    if (m_targets->tuneTo(slice, hz)) {
        queue.inFlightHz = hz;
        queue.inFlightAtMs = now;
        queue.hasInFlight = true;
    } else {
        // Refused, or already there at a band limit: start again from the slice.
        queue.coalescer.drop();
        queue.hasInFlight = false;
    }
}

void HotKeyDispatcher::dropTune(const SliceModel* slice)
{
    for (TurnQueue& queue : m_turnQueues) {
        if (queue.isTune && queue.slice == slice) {
            queue.coalescer.drop();
            queue.hasInFlight = false;
        }
    }
}

void HotKeyDispatcher::dropAllTurns()
{
    for (TurnQueue& queue : m_turnQueues) {
        queue.coalescer.drop();
        queue.hasInFlight = false;
    }
    m_flushTimer.stop();
}

void HotKeyDispatcher::armFlushTimer()
{
    if (!m_flushTimer.isActive()) {
        m_flushTimer.start();
    }
}

void HotKeyDispatcher::startHold(const HotKeyAction& action, const HotKeySource& source)
{
    if (action.id != idTxHold()) {
        qCWarning(lcHotKeys) << "No hold for:" << action.id;
        return;
    }
    // Doppelte zuerst: die Tastenwiederholung darf nichts neu versuchen
    // und nichts melden, was immer mit der Taste seither geschah.
    if (m_holdSources.contains(source)) {
        return;
    }
    const QString reason = m_targets->unavailableReason(action.id);
    if (!reason.isEmpty()) {
        m_targets->toast(reason);
        return;
    }
    if (!m_holdKeyed && !m_targets->transmitting()) {
        // Erst beanspruchen, damit ein Aus-Signal, das in keyHold selbst
        // faellt, nicht verloren geht.
        m_holdKeyed = true;
        if (!m_targets->keyHold(true)) {
            m_holdKeyed = false;
            return;
        }
    }
    m_holdSources.append(source);
    updateSleepWatch();
    noteTransmitHold();
}

void HotKeyDispatcher::endTransmitHold(const HotKeySource& source)
{
    const qsizetype removed = m_holdSources.removeAll(source);
    updateSleepWatch();
    if (removed == 0 || !m_holdSources.isEmpty() || !m_holdKeyed) {
        noteTransmitHold();
        return;
    }
    m_holdKeyed = false;
    noteTransmitHold();
    // TUNE hat waehrend des Haltens uebernommen: es bleibt nach dem
    // Loslassen an, wie der Knopf.
    if (!m_targets->tuneOn()) {
        m_targets->keyHold(false);
    }
}

} // namespace Longpath
