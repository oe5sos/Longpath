// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/core/hotkeys/TuneCoalescer.h  (Longpath)
// =================================================================
//
// Sammelt Abstimmschritte einer festgehaltenen Taste und gibt sie
// hoechstens alle 20 ms weiter, ohne einen zu verlieren. Uebernommen aus
// dem Upstream NereusSDR (`src/core/hotkeys/TuneCoalescer.h`
// @ 9036ffc04, J.J. Boyd KG4VCF), unveraendert bis auf Namensraum und
// Kopf. Der Wert 20 ms stammt dort aus AetherSDR
// (MainWindow_Controllers.cpp:2295 [@1e0718ad]).
//
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-10 - Uebernommen fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.


#include <QtGlobal>

namespace Longpath {

class TuneCoalescer {
public:
    static constexpr qint64 kMinIntervalMs = 20;  // Spec V5. AetherSDR MainWindow_Controllers.cpp:2295 [@1e0718ad]

    // Adds steps; returns the steps to send now, or 0 if the interval has not passed.
    int add(int steps, qint64 nowMs)
    {
        m_pending += steps;
        return takeDue(nowMs);
    }

    // Called by the timer.
    int takeDue(qint64 nowMs)
    {
        if (m_pending == 0 || (m_hasSent && nowMs - m_lastSendMs < kMinIntervalMs)) {
            return 0;
        }
        const int due = m_pending;
        m_pending = 0;
        m_lastSendMs = nowMs;
        m_hasSent = true;
        return due;
    }

    // Lock or refusal. The last send still counts toward the interval.
    void drop() { m_pending = 0; }

    int pending() const { return m_pending; }

private:
    int m_pending{0};
    qint64 m_lastSendMs{0};
    bool m_hasSent{false};
};

} // namespace Longpath
