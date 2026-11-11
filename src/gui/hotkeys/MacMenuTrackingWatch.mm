// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/hotkeys/MacMenuTrackingWatch.mm  (Longpath)
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

#include "gui/hotkeys/MacMenuTrackingWatch.h"

#import <AppKit/AppKit.h>

#include <utility>

namespace Longpath {

MacMenuTrackingWatch::MacMenuTrackingWatch(std::function<void()> onTrackingBegan, QObject* parent)
    : QObject(parent)
{
    // No queue: the block runs in the post, on the main thread. The main queue
    // would wait out the very tracking this is here to notice.
    const std::function<void()> callback = std::move(onTrackingBegan);
    id observer = [[NSNotificationCenter defaultCenter]
        addObserverForName:NSMenuDidBeginTrackingNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification*) {
                    callback();
                }];
    m_observer = CFBridgingRetain(observer);
}

MacMenuTrackingWatch::~MacMenuTrackingWatch()
{
    id observer = CFBridgingRelease(m_observer);
    [[NSNotificationCenter defaultCenter] removeObserver:observer];
}

void MacMenuTrackingWatch::postForTest()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:NSMenuDidBeginTrackingNotification
                                                        object:nil];
}

} // namespace Longpath
