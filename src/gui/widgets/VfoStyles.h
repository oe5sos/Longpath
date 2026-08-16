#pragma once

// =================================================================
// src/gui/widgets/VfoStyles.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Stylesheet constants ported verbatim from AetherSDR
//                 `src/gui/VfoWidget.cpp:134-177`.
// =================================================================

#include <QColor>
#include <QStringView>

namespace NereusSDR {

// ── VFO Flag Style Strings ─────────────────────────────────────────────────
// Ported verbatim from AetherSDR src/gui/VfoWidget.cpp:134-177.
// All strings are ASCII-only, so inline constexpr QStringView is safe.

// From AetherSDR src/gui/VfoWidget.cpp:137 — verbatim port
inline constexpr QStringView kBgStyle {
    u"QWidget#VfoWidgetRoot { background: transparent; border: none; }"
};

// From AetherSDR src/gui/VfoWidget.cpp:140 — verbatim port
inline constexpr QStringView kFlatBtn {
    u"QPushButton { background: transparent; border: none; "
    u"font-size: 13px; font-weight: bold; padding: 0 6px; margin: 0; }"
};

// From AetherSDR src/gui/VfoWidget.cpp:144 — verbatim port
inline constexpr QStringView kTabLblNormal {
    u"QLabel { background: transparent; border: none; "
    u"border-bottom: 2px solid transparent; "
    u"color: #708090; font-size: 13px; font-weight: bold; padding: 3px 0; }"
};

// From AetherSDR src/gui/VfoWidget.cpp:149 — verbatim port
inline constexpr QStringView kTabLblActive {
    u"QLabel { background: transparent; border: none; "
    u"border-bottom: 2px solid #00b4d8; "
    u"color: #00b4d8; font-size: 13px; font-weight: bold; padding: 3px 0; }"
};

// From AetherSDR src/gui/VfoWidget.cpp:154 — verbatim port
inline constexpr QStringView kDisabledBtn {
    u"QPushButton:disabled { background-color: #1a1a2a; color: #556070; "
    u"border: 1px solid #2a3040; }"
};

// From AetherSDR src/gui/VfoWidget.cpp:158 — verbatim port
inline constexpr QStringView kDspToggle {
    u"QPushButton { background: #1a2a3a; border: 1px solid #304050; border-radius: 2px; "
    u"color: #c8d8e8; font-size: 13px; font-weight: bold; padding: 2px 4px; }"
    u"QPushButton:checked { background: #1a6030; color: #ffffff; border: 1px solid #20a040; }"
    u"QPushButton:hover { border: 1px solid #0090e0; }"
};

// From AetherSDR src/gui/VfoWidget.cpp:164 — verbatim port
inline constexpr QStringView kModeBtn {
    u"QPushButton { background: #1a2a3a; border: 1px solid #304050; border-radius: 2px; "
    u"color: #c8d8e8; font-size: 13px; font-weight: bold; padding: 3px; }"
    u"QPushButton:checked { background: #0070c0; color: #ffffff; border: 1px solid #0090e0; }"
    u"QPushButton:hover { border: 1px solid #0090e0; }"
};

// From AetherSDR src/gui/VfoWidget.cpp:170 — verbatim port
inline constexpr QStringView kSliderStyle {
    u"QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
    u"QSlider::handle:horizontal { background: #c8d8e8; width: 12px; margin: -4px 0; border-radius: 6px; }"
    u"QSlider::groove:vertical { background: #1a2a3a; width: 4px; border-radius: 2px; }"
    u"QSlider::handle:vertical { background: #c8d8e8; height: 12px; margin: 0 -4px; border-radius: 6px; }"
};

// From AetherSDR src/gui/VfoWidget.cpp:176 — verbatim port
inline constexpr QStringView kLabelStyle {
    u"QLabel { background: transparent; border: none; color: #8aa8c0; font-size: 13px; }"
};

// ── VFO Flag Color Constants ───────────────────────────────────────────────
// Drawn from inline QColor calls in AetherSDR src/gui/VfoWidget.cpp.
// QColor is NOT constexpr in Qt6; use inline const.

inline const QColor kHdrRxBlue    { 0x44, 0x88, 0xff }; // RX antenna label text color
inline const QColor kHdrTxRed     { 0xff, 0x44, 0x44 }; // TX antenna label text color
inline const QColor kFilterCyan   { 0x00, 0xc8, 0xff }; // filter-width label text color
inline const QColor kSliceBadgeBlue{ 0x00, 0x70, 0xc0 }; // slice letter badge background
// ── Aussteuerung ist ein Messwert ───────────────────────────────────
//
// War Tuerkis unter S9 und Signalgruen darueber. Beides falsch: Gruen
// (Salbei) ist im Hausstil der Ton fuer einen ZUSTAND -- an oder aus,
// ein Laempchen. Was ein Pegelbalken zeigt, ist gemessen, und Gemessenes
// steht in Bernstein, genau wie die Frequenz.
//
// Zwei Stufen bleiben, damit S9 als Schwelle ablesbar ist: darunter
// gedaempft, darueber voll. Derselbe Ton, zwei Helligkeiten -- kein
// zweiter Farbbegriff fuer dieselbe Groesse.
inline const QColor kMeterCyan    { 0x8a, 0x6c, 0x3c }; // Messwert, gedaempft (unter S9)
inline const QColor kMeterGreen   { 0xc2, 0x92, 0x4f }; // Messwert, voll (ab S9)
inline const QColor kLabelMuted   { 0x68, 0x88, 0xa0 }; // muted label text
inline const QColor kBodyText     { 0xc8, 0xd8, 0xe8 }; // default body text
inline const QColor kBgDark       { 0x10, 0x10, 0x1c }; // flag dark bg used by custom paint

} // namespace NereusSDR
