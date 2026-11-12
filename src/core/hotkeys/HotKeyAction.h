// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/core/hotkeys/HotKeyAction.h  (Longpath)
// =================================================================
//
// Eine Handlung, die eine Taste ausloesen kann.
//
// Uebernommen aus dem Upstream NereusSDR (`src/core/hotkeys/
// HotKeyAction.h` @ 9036ffc04, Fassung vom 2026-11-09, J.J. Boyd
// KG4VCF). Dort tragen auch MIDI, Drehknoepfe und Stream-Deck-Tasten
// dieselbe Liste; Longpath nimmt vorerst nur die Tastatur. Die Liste
// ist trotzdem so geblieben, dass ein Knopf spaeter dieselben Kennungen
// rufen kann.
//
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-10 - Uebernommen fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.

#include <QLoggingCategory>
#include <QString>
#include <QStringList>

namespace Longpath {

Q_DECLARE_LOGGING_CATEGORY(lcHotKeys)

// Druecken: einmal ausloesen. Halten: an, solange die Taste unten ist
// (Sprechtaste). Drehen: Schritte, die sich beim Festhalten der Taste
// aufsummieren (Abstimmen, Lautstaerke).
enum class HotKeyGesture { Press, Hold, Turn };

// Worauf die Handlung wirkt. Slice: der Empfaenger, den das Fenster
// gerade bedient. TransmitSlice: der Sender. Pan: das Panorama im
// Vordergrund. Window: das Programm als Ganzes.
enum class HotKeyScope { Slice, TransmitSlice, Pan, Window };

struct HotKeyAction {
    QString id;      // fest, steht in den Einstellungen und in Exportdateien
    QString name;    // so sieht es der Betreiber
    QString group;
    HotKeyGesture gesture;
    bool transmits;  // tastet den Sender
    HotKeyScope scope;
    QStringList defaultKeys;  // in der kanonischen Schreibweise von HotKeyChord
};

} // namespace Longpath
