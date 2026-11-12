// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/gui/hotkeys/HotKeyDispatcher.h  (Longpath)
// =================================================================
//
// Die einzige Stelle, die eine Tastenhandlung ausfuehrt. Sie findet den
// Empfaenger, wendet die Sperren an, buendelt Schritte und fuehrt Buch
// ueber gehaltene Sprechtasten; dann ruft sie HotKeyTargets. Nur
// Hauptfaden.
//
// Uebernommen aus dem Upstream NereusSDR (`src/gui/hotkeys/
// HotKeyDispatcher.h` @ 9036ffc04, J.J. Boyd KG4VCF). Ohne Punkt- und
// Strichtasten (CW-Senden ist zurueckgestellt) und ohne die Pruefung auf
// fremde Geraete am selben Funkgeraet, die Longpath nicht kennt.
//
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan.
//                AI-assisted implementation via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-10 - Uebernommen und gekuerzt fuer Longpath von Martin
//                Fischer (OE5SOS), KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.

#include "core/hotkeys/HotKeyAction.h"
#include "core/hotkeys/TuneCoalescer.h"
#include "gui/hotkeys/HotKeyTargets.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <functional>

namespace Longpath {

class HotKeyBindings;
class SliceModel;

class HotKeyDispatcher : public QObject {
    Q_OBJECT

public:
    HotKeyDispatcher(HotKeyBindings* bindings, HotKeyTargets* targets, QObject* parent = nullptr);

    // Eine Drehen-Handlung macht je Druck einen Schritt, zehn bei
    // tune.up10 und tune.down10.
    void press(const QString& id, const HotKeySource& source, QChar pin = {});
    void release(const QString& id, const HotKeySource& source);
    void releaseAll(const HotKeySource& source);    // Sicherungen, eine Quelle
    void releaseAllHolds();                         // der Ausschalter

    // Ob die Taste jetzt etwas zu tun hat (siehe HotKeyTargets::takesKey).
    bool takesKey(const QString& id) const;

    // Eine Sprechtaste faellt, wenn ihr Loslassen nicht ankommen kann --
    // und ueber den Ruhezustand des Rechners kann es das nicht. Solange
    // etwas gehalten wird, vergleicht ein Takt die Wanduhr mit einer
    // Uhr, die im Ruhezustand steht. Was die Wanduhr gewonnen hat, war
    // Schlaf. Werte aus Upstream.
    static constexpr qint64 kSleepGapMs = 3000;
    static constexpr int kSleepTickMs = 1000;

    bool enabled() const;                           // der Ausschalter

    bool transmitHoldOn() const { return !m_holdSources.isEmpty() && m_holdKeyed; }
    bool holdsTransmit(const HotKeySource& source) const { return m_holdSources.contains(source); }

    using ClockMs = std::function<qint64()>;
    void setSleepClocksForTest(ClockMs wallClock, ClockMs awakeClock);
    bool sleepWatchRunning() const { return m_sleepTimer.isActive(); }

public slots:
    void onTransmitWentOff();                       // loescht den Anspruch der Buchfuehrung
    void flushTune();                               // Slot des Taktgebers; oeffentlich fuer Pruefstaende
    void checkForSleep();                           // Slot des Taktgebers; oeffentlich fuer Pruefstaende

signals:
    void holdsDroppedBySleep();
    void transmitHoldChanged(bool on);
    void transmitKeyWentOff();

private:
    // Schritte, die auf einen Empfaenger warten. Abstimmen und Pegel
    // getrennt, damit eine Lautstaerketaste das Abstimmen nicht bremst.
    struct TurnQueue {
        QPointer<SliceModel> slice;
        bool isTune{false};
        TuneCoalescer coalescer;
        double inFlightHz{0.0};
        qint64 inFlightAtMs{0};
        bool hasInFlight{false};
    };

    bool admit(const HotKeyAction& action, QChar pin, SliceModel*& slice);
    void execute(const HotKeyAction& action, SliceModel* slice);
    void addTurnSteps(const HotKeyAction& action, SliceModel* slice, int steps);
    void sendTurnSteps(TurnQueue& queue, int steps);
    void sendTune(TurnQueue& queue, int steps);
    void dropTune(const SliceModel* slice);
    void dropAllTurns();
    void armFlushTimer();

    void startHold(const HotKeyAction& action, const HotKeySource& source);
    void endTransmitHold(const HotKeySource& source);
    void updateSleepWatch();
    void noteSleepClocks();
    void noteTransmitHold();

    HotKeyBindings* m_bindings;
    HotKeyTargets* m_targets;
    QTimer m_flushTimer;
    QList<TurnQueue> m_turnQueues;

    // Die Buchfuehrung der Sprechtaste. m_holdKeyed: eine gehaltene Taste
    // hat den Sender getastet, und er ist seither nicht ausgegangen --
    // also darf das letzte Loslassen ihn abschalten. Ohne diesen Anspruch
    // (etwa ein eingerastetes MOX) bleibt der Sender, wie er ist.
    QList<HotKeySource> m_holdSources;
    bool m_holdKeyed{false};
    bool m_transmitHoldOn{false};

    QTimer m_sleepTimer;
    ClockMs m_wallClock;
    ClockMs m_awakeClock;
    qint64 m_lastWallMs{0};
    qint64 m_lastAwakeMs{0};
};

} // namespace Longpath
