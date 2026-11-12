// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/core/hotkeys/HotKeyActionList.h  (Longpath)
// =================================================================
//
// Die eine Liste aller Handlungen, die eine Taste ausloesen kann.
//
// Uebernommen aus dem Upstream NereusSDR (`src/core/hotkeys/
// HotKeyActionList.h` @ 9036ffc04, J.J. Boyd KG4VCF).
//
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-10 - Uebernommen fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.

#include "core/hotkeys/HotKeyAction.h"

#include <QList>

namespace Longpath {

class HotKeyActionList {
public:
    static const QList<HotKeyAction>& all();
    static const HotKeyAction* find(const QString& id);  // nullptr, wenn unbekannt

    // Die Plaetze des Sprachspeichers: voice.msg1 … voice.msg10 und
    // voice.rec1 … voice.rec10. -1 fuer jede andere Kennung.
    static int voiceSlotOf(const QString& id);
};

} // namespace Longpath
