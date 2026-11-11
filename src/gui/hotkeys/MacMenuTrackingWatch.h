// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/hotkeys/MacMenuTrackingWatch.h  (Longpath)
// =================================================================
//
// Nur macOS. Meldet sich, sobald irgendein Menue dieses Prozesses
// aufklappt -- auch das Apfelmenue, an dem kein QMenu haengt. Dann faellt
// eine gehaltene Sprechtaste, weil ihr Loslassen ins Menue ginge.
//
// Uebernommen aus dem Upstream NereusSDR (`src/gui/hotkeys/
// MacMenuTrackingWatch.h/.mm` @ 9036ffc04, J.J. Boyd KG4VCF), unveraendert
// bis auf Namensraum und Kopf.
//
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan.
//                AI-assisted implementation via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-10 - Uebernommen fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.
#pragma once

#include <QObject>

#include <functional>

namespace Longpath {

class MacMenuTrackingWatch : public QObject {
public:
    MacMenuTrackingWatch(std::function<void()> onTrackingBegan, QObject* parent);
    ~MacMenuTrackingWatch() override;

    static void postForTest();

private:
    const void* m_observer{nullptr};
};

} // namespace Longpath
