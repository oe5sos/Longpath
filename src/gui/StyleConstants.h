// src/gui/StyleConstants.h

// =================================================================
// src/gui/StyleConstants.h  (NereusSDR)
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
//                 Theme palette imported from AetherSDR `src/gui/ComboStyle.h` /
//                 `HGauge.h` / `SliceColors.h` and inline QColor calls
//                 in `MainWindow.cpp` / `VfoWidget.cpp`.
// =================================================================

#pragma once
#include <QString>
#include <QWidget>

namespace NereusSDR::Style {

// ── Entblaut, 2026-08-15 ──────────────────────────────────────────────
//
// „nach zeus sieht das aber nicht aus" — OE5SOS, mit einem Screenshot,
// auf dem dreißig Knöpfe einen leuchtend blauen Rahmen tragen: 5K, 4.4K,
// SQL, RIT, XIT, TUNE, MOX, VOX, MON, LEV, EQ, CFC, 2-Tone, PS-A. Bei
// Zeus ist ein inaktiver Knopf fast unsichtbar; sichtbar ist der eine,
// der an ist.
//
// Die Ursache war kBorder = #205070 — ein kräftiges Blau als Rahmen für
// ALLES. Dazu der durchgehende Blaustich in den Flächen: #0f0f1a,
// #0a0a18, #203040, #1a2a3a haben alle mehr Blau als Rot.
//
// Die alten Werte stehen daneben, weil das der Umbauzustand ist: was
// noch als Hex-Literal in einem Widget steht, folgt hier NICHT mit und
// bleibt alt. Diese Ungleichheit ist beabsichtigt und nützlich — sie
// zeigt genau, welche Stelle noch Arbeit braucht. Siehe
// docs/design/HAUSSTIL.md.
constexpr auto kAppBg           = "#08080a";   // war #0f0f1a
constexpr auto kPanelBg         = "#0c0c0e";   // war #0a0a18
constexpr auto kTextPrimary     = "#c4c4c9";   // war #c8d8e8
constexpr auto kTextSecondary   = "#8e8e93";
constexpr auto kTextTertiary    = "#76767a";
constexpr auto kTextScale       = "#5c5c60";
constexpr auto kTextInactive    = "#3d3d41";
// NereusSDR-original — used in 5+ places for AGC-T / pan / similar labels;
// sits between kTextSecondary (#8090a0) and kTextScale (#607080).
constexpr auto kLabelMid        = "#828288";
constexpr auto kAccent          = "#4a7ba8";
constexpr auto kTitleText       = "#a8a8ae";

// Borders & Surfaces
//
// kBorder ist der wichtigste Wert hier: er umrandet jeden Knopf im
// Programm. #205070 machte daraus dreißig blaue Rechtecke. #2c2c31 ist
// gerade noch da und tritt nicht mehr nach vorne — der ausgewählte
// Knopf soll auffallen, nicht alle anderen.
constexpr auto kButtonBg        = "#1a1a1e";   // war #1a2a3a
constexpr auto kButtonHover     = "#26262b";   // war #203040
constexpr auto kButtonAltHover  = "#2f3138";   // war #204060
constexpr auto kBorder          = "#2c2c31";   // war #205070
constexpr auto kBorderSubtle    = "#1f1f23";   // war #203040
constexpr auto kInsetBg         = "#08080a";   // war #0a0a18
constexpr auto kInsetBorder     = "#232329";   // war #1e2e3e
constexpr auto kGroove          = "#1f1f23";   // war #203040

// Title Bar Gradient
constexpr auto kTitleGradTop    = "#26262b";
constexpr auto kTitleGradMid    = "#1c1c20";
constexpr auto kTitleGradBot    = "#141417";
constexpr auto kTitleBorder     = "#0d0d0f";

// Active/Checked Button States
constexpr auto kGreenBg         = "#1c3a2a";
constexpr auto kGreenText       = "#6fa384";
constexpr auto kGreenBorder     = "#2c5c44";
constexpr auto kBlueBg          = "#254a72";
constexpr auto kBlueText        = "#cfe2f5";
constexpr auto kBlueBorder      = "#2f5c86";
constexpr auto kBlueHover       = "#2d5885";
constexpr auto kAmberBg         = "#33280f";
constexpr auto kAmberText       = "#c2924f";
// Die gedaempfte Messwertstufe. Sie stand bis 2026-08-17 dreimal als
// nacktes #6b5630 im Baum -- dateilokal in HGauge.cpp:13, noch einmal
// in InstrumentPainter.cpp und als Literal in VfoWidget. Eine Farbe mit
// drei Wohnorten und keinem Namen ist genau die, die beim naechsten
// Feinschliff an zwei Stellen mitgezogen wird und an einer nicht.
constexpr auto kAmberDim        = "#6b5630";
constexpr auto kAmberBorder     = "#6b5426";
constexpr auto kAmberWarn       = "#a8853f";
constexpr auto kRedBg           = "#7a2c2e";
constexpr auto kRedText         = "#f0dcdc";
constexpr auto kRedBorder       = "#a86b6d";

// ── Instrumente ───────────────────────────────────────────────────────
//
// S-Meter, SWR-Zifferblatt, Gain Reduction. Ein Instrument bekommt seine
// Wirkung aus Licht, nicht aus Farbe.
//
// Vorher war der S-Meter-Bogen zur Hälfte blau und zur Hälfte rot, und
// die Teilung kodierte damit nichts — es war eine Skala, die zufällig
// zweifarbig war. Rot bedeutet in diesem Programm „Achtung"; es bedeutete
// oben rechts im Bild dauerhaft „Skalenteil".
//
// Jetzt: ein cremeweißer Bogen über die ganze Länge, darunter ein
// entsättigtes Glimmen, und die Grenze als kurze Marke statt als halber
// Bogen. Die Information bleibt, die Fläche verschwindet.
//
// Der Glimmton ist absichtlich keiner, den man benennen würde. Sobald
// man ihn „olivgrün" nennen kann, ist er zu kräftig.
constexpr auto kInstrumentFace    = "#c8c8c0";  // Bogen, Zeiger, Teilung
constexpr auto kInstrumentGlowHi  = "#5c5842";  // Mitte des Glimmens
constexpr auto kInstrumentGlowLo  = "#33322a";  // Rand des Glimmens
constexpr auto kInstrumentLimit   = "#a86b6d";  // wo es zu viel wird

// ── Spektrum und Gitter ───────────────────────────────────────────────
//
// Die zwei größten Flächen im Fenster. Solange die Kurve neonzyan und
// die Frequenzskala knallgelb ist, nützt eine dezente Palette drumherum
// nichts — zwei Drittel des Bildes schreien weiter.
//
// Die Kurve gehört zur Akzentfamilie (kAccent #4a7ba8), ist aber eine
// Spur heller: sie ist das, worauf man den ganzen Abend schaut, und
// muss vor dem Rauschteppich stehen, ohne zu leuchten.
//
// Das Gitter kommt ohne Alpha aus dieser Datei — die Deckung setzt der
// Malcode, weil fein und grob dieselbe Farbe bei anderer Deckung sind.
// Eine Farbe hier zu verdoppeln, nur um zwei Alphawerte zu tragen, wäre
// ein Wert zu viel in der Theme-Datei.
//
// Die Bandkante bleibt rot und wird hier NICHT gedämpft: sie sagt, wo
// das Senden aufhört, erlaubt zu sein. Eine Sicherheitsmarke leiser zu
// drehen, weil sie auffällt, hieße das Merkmal abzuschaffen.
// Warm, nicht blau. Der Hausstil trennt nach Aufgabe, nicht nach
// Geschmack: „Blau = anfassbar. Warm = gemessen." Die Spektrumkurve ist
// das Gemessene schlechthin, und sie stand als einziger Messwert im
// Programm auf einem Blauton. HAUSSTIL.md führt beide in einer Zeile:
// „Messwert / Kurve #c2924f".
//
// Die Rolle heißt weiter „trace" und nicht „measured", damit eine
// Theme-Datei die Kurve getrennt vom übrigen Bernstein setzen kann —
// nur ihr Ausgangston ist jetzt der Messwert-Ton.
constexpr auto kSpectrumTrace     = "#c2924f";  // Kurve und Füllung
constexpr auto kSpectrumGrid      = "#8a8f96";  // Gitterlinien (Alpha im Code)
constexpr auto kSpectrumGridText  = "#9aa0a8";  // Frequenz- und dBm-Skala

// Gauge Fill Zones
// ── Telling one equaliser band from another ──────────────────────────
//
// Eight hues so that overlapping bands on one curve can be told apart.
// This is not decoration: with eight filters on top of each other, hue
// is the only thing that says which handle belongs to which curve.
//
// Grey at both ends on purpose — those slots hold the high-pass and
// low-pass slopes, which are shapes rather than bumps and should not
// compete for attention with the shaping bands in the middle.
//
// Four of the eight are NereusSDR's existing accents. Three are new
// (2026-08-11): the palette needs six distinguishable hues between the
// greys and NereusSDR only had four, so a coral, a blue and a violet
// were added, spaced to stay apart on #0a0a18 and chosen to sit with
// the accents rather than beside them.
constexpr auto kEqBand0 = "#8090a0";   // grey — kTextSecondary, HP slot
constexpr auto kEqBand1 = "#ff8850";   // coral, new
constexpr auto kEqBand2 = "#ffb800";   // amber — kAmberText
constexpr auto kEqBand3 = "#00ff88";   // green — kGreenText
constexpr auto kEqBand4 = "#00b4d8";   // teal  — kAccent
constexpr auto kEqBand5 = "#4a80ff";   // blue, new
constexpr auto kEqBand6 = "#a070e0";   // violet, new
constexpr auto kEqBand7 = "#8090a0";   // grey — kTextSecondary, LP slot

constexpr auto kGaugeNormal     = "#4a7ba8";
constexpr auto kGaugeWarning    = "#a8853f";
constexpr auto kGaugeDanger     = "#a86b6d";
constexpr auto kGaugePeak       = "#c4c4c9";

// Disabled
constexpr auto kDisabledBg      = "#141417";
constexpr auto kDisabledText    = "#4e4e53";
constexpr auto kDisabledBorder  = "#232327";

// Overlay
constexpr auto kOverlayBtnBg    = "rgba(20, 30, 45, 240)";
constexpr auto kOverlayPanelBg  = "rgba(15, 15, 26, 220)";
constexpr auto kOverlayBtnHover = "rgba(0, 112, 192, 180)";
constexpr auto kOverlayBorder   = "#2c2c31";

// Sizes
constexpr int kTitleBarH        = 16;
constexpr int kButtonH          = 22;
constexpr int kOverlayBtnW      = 68;
constexpr int kOverlayBtnH      = 22;
constexpr int kSliderGrooveH    = 4;
constexpr int kSliderHandleW    = 10;
constexpr int kSliderHandleH    = 10;
constexpr int kAppletPanelW     = 260;

// Status Bar
constexpr int kStatusBarH       = 46;
constexpr auto kStatusBarBg     = "#0a0a0c";
constexpr auto kStatusBarBorder = "#1f1f23";
constexpr auto kStatusSep       = "#2c2c31";

// Shared Stylesheet Fragments
inline QString buttonBaseStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background: %1; border: 1px solid %2; border-radius: 6px;"
        "  color: %3; font-size: 10px; font-weight: bold; padding: 2px 4px;"
        "}"
        "QPushButton:hover { background: %4; }"
    ).arg(kButtonBg, kBorder, kTextPrimary, kButtonAltHover);
}

inline QString greenCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kGreenBg, kGreenText, kGreenBorder);
}

inline QString blueCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kBlueBg, kBlueText, kBlueBorder);
}

inline QString amberCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kAmberBg, kAmberText, kAmberBorder);
}

inline QString redCheckedStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kRedBg, kRedText, kRedBorder);
}

// DSP toggle — brighter green than the kGreenBg action buttons.
// Used by VfoWidget DSP tab and SpectrumOverlayPanel DSP toggles.
// Source: NereusSDR-original. Distinct semantic from action-button
// "checked" state because DSP toggles communicate "feature on" not
// "action engaged."
constexpr auto kDspToggleBg     = "#1b3527";
constexpr auto kDspToggleBorder = "#33684c";
constexpr auto kDspToggleText   = "#7fae91";

inline QString dspToggleStyle()
{
    return QStringLiteral(
        "QPushButton:checked { background: %1; color: %2; border: 1px solid %3; }"
    ).arg(kDspToggleBg, kDspToggleText, kDspToggleBorder);
}

inline QString sliderHStyle()
{
    return QStringLiteral(
        "QSlider::groove:horizontal {"
        "  height: 4px; background: %1; border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  width: 10px; height: 10px; margin: -3px 0;"
        "  background: %2; border-radius: 5px;"
        "}"
    ).arg(kGroove, kAccent);
}

inline QString sliderVStyle()
{
    return QStringLiteral(
        "QSlider::groove:vertical {"
        "  width: 4px; background: %1; border-radius: 2px;"
        "}"
        "QSlider::handle:vertical {"
        "  height: 10px; width: 16px; margin: 0 -6px;"
        "  background: %2; border-radius: 5px;"
        "}"
    ).arg(kGroove, kAccent);
}

inline QString insetValueStyle()
{
    return QStringLiteral(
        "QLabel {"
        "  font-size: 10px; background: %1; border: 1px solid %2;"
        "  border-radius: 6px; padding: 1px 2px; color: %3;"
        "}"
    ).arg(kInsetBg, kInsetBorder, kTextPrimary);
}

inline QString titleBarStyle()
{
    return QStringLiteral(
        "background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        " stop:0 %1, stop:0.5 %2, stop:1 %3);"
        " border-bottom: 1px solid %4;"
    ).arg(kTitleGradTop, kTitleGradMid, kTitleGradBot, kTitleBorder);
}

// ── Setup Dialog Page Stylesheets ─────────────────────────────────────────────
// Flat string constants used by SetupPage subclasses that build their own
// layouts directly (CatNetwork, Keyboard, Diagnostics pages).

constexpr auto kPageStyle =
    "QWidget { background: #0f0f1a; color: #c8d8e8; }";

constexpr auto kGroupBoxStyle =
    "QGroupBox { border: 1px solid #304050; border-radius: 4px;"
    " margin-top: 8px; padding-top: 12px; font-weight: bold; color: #8aa8c0; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }";

constexpr auto kSecondaryLabelStyle =
    "QLabel { color: #8090a0; font-size: 11px; }";

constexpr auto kComboStyle =
    "QComboBox { background: #1a2a3a; border: 1px solid #304050;"
    " border-radius: 6px; color: #c8d8e8; font-size: 12px; padding: 2px 4px; }"
    "QComboBox::drop-down { border: none; }"
    "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8;"
    " selection-background-color: #00b4d8; }";

constexpr auto kCheckBoxStyle =
    "QCheckBox { color: #c8d8e8; font-size: 12px; }"
    "QCheckBox::indicator { width: 14px; height: 14px; background: #1a2a3a;"
    " border: 1px solid #304050; border-radius: 2px; }"
    "QCheckBox::indicator:checked { background: #00b4d8; border-color: #00b4d8; }";

constexpr auto kRadioButtonStyle =
    "QRadioButton { color: #c8d8e8; font-size: 12px; }"
    "QRadioButton::indicator { width: 14px; height: 14px; background: #08080a;"
    " border: 1px solid #2c2c31; border-radius: 7px; }"
    "QRadioButton::indicator:checked { background: #4a7ba8; border-color: #2f5c86; }";

// ── Eingabefelder sind Mulden, keine Flaechen ────────────────────────
//
// Gemeldet 2026-08-16 auf Startup & Preferences: Callsign, Grid Square,
// Process Priority und FFTW Wisdom waren "fast unsichtbar -- man sieht
// nicht mehr, dass das bedienbare Felder sind".
//
// Ursache war nicht zu viel Daempfung, sondern die falsche RICHTUNG.
// Die Werte #1a2a3a (Grund) und #304050 (Rand) wurden von themed()
// korrekt auf #1a1a1e und #2c2c31 gezogen -- zwei fast gleiche
// Grautoene. Vorher trennte der Rand durch ein kraeftiges Blau von
// selbst; ohne das liegt ein Feld auf dem Panel wie eine Flaeche.
//
// HAUSSTIL beschreibt es als VERSENKT: "Versenkt (Glas) #000000 +
// inset 0 2px 8px #000". Ein Feld gehoert DUNKLER als seine Umgebung,
// nicht heller -- dann traegt der hellere Rand von selbst.
//
// Qt-Stylesheets koennen keinen inneren Schatten. Der Grund liegt
// deshalb auf kInsetBg (#08080a, dunkler als das Panel #0c0c0e) und der
// Rand auf kBorder (#2c2c31), der gegen fast Schwarz deutlich steht.
//
// Ein leeres Feld ist nicht inaktiv. Es wartet auf eine Eingabe.
constexpr auto kLineEditStyle =
    "QLineEdit { background: #08080a; border: 1px solid #2c2c31;"
    " border-radius: 6px; color: #c4c4c9; font-size: 12px; padding: 2px 4px; }";

constexpr auto kSpinBoxStyle =
    "QSpinBox { background: #08080a; border: 1px solid #2c2c31;"
    " border-radius: 6px; color: #c4c4c9; font-size: 12px; padding: 2px 4px; }";

constexpr auto kDoubleSpinBoxStyle =
    "QDoubleSpinBox { background: #08080a; border: 1px solid #2c2c31;"
    " border-radius: 6px; color: #c4c4c9; font-size: 12px; padding: 2px 4px; }";

// Backwards-compat wrapper for places that prefer a function form.
inline QString doubleSpinBoxStyle() { return QString::fromLatin1(kDoubleSpinBoxStyle); }

constexpr auto kSliderStyle =
    "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
    "QSlider::handle:horizontal { background: #00b4d8; width: 12px;"
    " height: 12px; border-radius: 6px; margin: -4px 0; }";

constexpr auto kButtonStyle =
    "QPushButton { background: #1a2a3a; border: 1px solid #304050;"
    " border-radius: 6px; color: #c8d8e8; font-size: 12px; padding: 3px 10px; }"
    "QPushButton:hover { background: #203040; }"
    "QPushButton:pressed { background: #00b4d8; color: #0f0f1a; }";

// Apply the canonical "dark page" stylesheet to a Setup page that lays
// itself out manually (i.e. doesn't inherit the SetupPage::addLabeledX
// helper-based widgets). Replaces the 4 byte-for-byte copies of
// applyDarkStyle() that previously lived in TransmitSetupPages,
// DisplaySetupPages, AppearanceSetupPages, GeneralOptionsPage.
//
// Per docs/architecture/ui-audit-polish-plan.md §A3.
//
// Two intentional drift corrections vs the local copies:
// - QGroupBox border uses kBorder (#205070), not the locals' #203040.
// - QGroupBox padding-top is 12 px, not the locals' 4 px (an 8 px
//   header-position shift, matching the rest of the app).
// All other rules match the locals byte-for-byte so callers see no
// other visual change.
//
// Issue #175 follow-up (2026-05-04): added QRadioButton rules.  Without
// these, calling applyDarkPageStyle() on a SetupPage subclass overwrote
// the QRadioButton::indicator style installed in SetupPage::init() via
// kRadioButtonStyle, leaving radio-button text + indicators in system
// default (black-on-black on dark theme).  Caught on PowerPage's new
// "Tune" group radios.  Mirrors kRadioButtonStyle (lines 241-245 above).
inline void applyDarkPageStyle(QWidget* w)
{
    if (!w) { return; }
    w->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; color: %2; }"
        "QGroupBox { color: %7; font-size: 11px;"
        "  border: 1px solid %3; border-radius: 4px;"
        "  margin-top: 8px; padding-top: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QLabel { color: %2; }"
        "QComboBox { background: %4; color: %2; border: 1px solid %3;"
        "  border-radius: 6px; padding: 2px 6px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: %4; color: %2;"
        "  selection-background-color: %5; }"
        "QSlider::groove:horizontal { background: %4; height: 4px;"
        "  border-radius: 2px; }"
        "QSlider::handle:horizontal { background: %5; width: 12px;"
        "  margin: -4px 0; border-radius: 6px; }"
        "QSlider::sub-page:horizontal { background: %5; border-radius: 2px; }"
        "QSpinBox, QDoubleSpinBox { background: %4; color: %2;"
        "  border: 1px solid %3; border-radius: 6px; padding: 1px 4px; }"
        "QCheckBox { color: %2; }"
        "QCheckBox::indicator { width: 14px; height: 14px; background: %4;"
        "  border: 1px solid %3; border-radius: 2px; }"
        "QCheckBox::indicator:checked { background: %5; border-color: %5; }"
        "QRadioButton { color: %2; }"
        "QRadioButton::indicator { width: 14px; height: 14px; background: %4;"
        "  border: 1px solid %3; border-radius: 7px; }"
        "QRadioButton::indicator:checked { background: %5; border-color: %5; }"
        "QLineEdit { background: %4; color: %2; border: 1px solid %3;"
        "  border-radius: 6px; padding: 2px 6px; }"
        "QPushButton { background: %4; color: %2; border: 1px solid %3;"
        "  border-radius: 6px; padding: 3px 12px; }"
        "QPushButton:hover { background: %6; }"
        "QPushButton:pressed { background: %5; color: %1; }"
    ).arg(kAppBg, kTextPrimary, kBorder, kButtonBg, kAccent, kButtonHover, kTextSecondary));
}

// ── TX / RX filter overlay palette ────────────────────────────────────────────
// Plan 4 D9 (Cluster E).  Used by SpectrumWidget::drawTxFilterOverlay() and
// SpectrumWidget::drawTxFilterWaterfallColumn().
// Colours are NereusSDR-original; no Thetis upstream equivalent (Thetis uses
// hard-coded GDI+ brushes without named constants).

// TX filter overlay — translucent orange.
// Customisable via ColorSwatchButton on Setup → Display → TX Display per
// docs/architecture/ui-audit-polish-plan.md §E1.D9b.
constexpr auto kTxFilterOverlayFill   = "rgba(255, 120, 60, 46)";
constexpr auto kTxFilterOverlayBorder = "#a8724a";
constexpr auto kTxFilterOverlayLabel  = "#c2924f";

// RX filter overlay — translucent cyan.
// Border reuses kAccent (#00b4d8); only the fill variant is new.
// Customisable on Setup → Display → Spectrum Defaults.
constexpr auto kRxFilterOverlayFill   = "rgba(0, 180, 216, 80)";

} // namespace NereusSDR::Style
