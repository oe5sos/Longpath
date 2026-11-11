// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/core/hotkeys/HotKeyChord.h  (Longpath)
// =================================================================
//
// Eine Tastenfolge als Text in Qts tragbarer Schreibweise
// („Ctrl+Shift+F1"). Uebernommen aus dem Upstream NereusSDR
// (`src/core/hotkeys/HotKeyChord.h` @ 9036ffc04, J.J. Boyd KG4VCF),
// unveraendert bis auf Namensraum und Kopf.
//
// Auf dem Mac steht „Ctrl" fuer die Befehlstaste: Qt vertauscht die
// beiden, wie bei jedem Menue-Kuerzel im Programm.
//
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-10 - Uebernommen fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.


#include <QString>

#include <optional>

namespace Longpath {

struct HotKeyChord {
    bool ctrl{false};
    bool shift{false};
    bool alt{false};
    bool meta{false};
    bool num{false};  // keypad
    QString key;      // a Qt portable key name; letters are upper case

    // Empty for text that is not one modifier list and one known key.
    static std::optional<HotKeyChord> parse(const QString& text);

    QString toString() const;  // canonical order Ctrl+Shift+Alt+Meta+Num+Key
    bool hasNoModifier() const;
    bool isLetterOrNumberOrArrow() const;  // looks at the key, not the modifiers
    bool operator==(const HotKeyChord&) const = default;
};

} // namespace Longpath
