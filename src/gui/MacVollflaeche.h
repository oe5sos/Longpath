#pragma once

// =================================================================
// src/gui/MacVollflaeche.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Volle Fläche — und trotzdem die drei Punkte links oben.
//
// „ich möchte übrigens, dass ich das auch einfach in die taskleiste
//  runter legen kann … bei apple sind links oben 3 farbige symbole,
//  rot, gelb und grün … ich meine eigentlich das hauptfenster!"
//
// ── Woran es lag ─────────────────────────────────────────────────────
//
// Das Hauptfenster läuft seit dem 2026-09-01 **randlos auf voller
// Bildschirmfläche** statt im Vollbild. Der Grund steht bei
// MainWindow::enterBorderlessFullSize und ist gut: `showFullScreen()`
// legt das Fenster in einen eigenen macOS-Space, und die schwebenden
// Werkzeugfenster folgten dorthin nicht zuverlässig — sie blieben
// unsichtbar auf dem Schreibtisch-Space zurück.
//
// Der Preis war aber: `Qt::FramelessWindowHint` erzeugt auf macOS ein
// Fenster mit `NSWindowStyleMaskBorderless`. So eines hat keinen
// Titelbalken, also auch keine drei Punkte — und AppKit lässt ein
// Fenster ohne Titelbalken nicht ins Dock zuklappen. Beides, was hier
// verlangt wurde, war damit unmöglich.
//
// ── Was stattdessen passiert ─────────────────────────────────────────
//
// Derselbe Anblick auf dem geraden Weg, den moderne Mac-Programme
// gehen: das Fenster behält seinen Titelbalken (bleibt also
// zuklappbar und trägt die drei Punkte), der Balken wird aber
// **durchsichtig** und der Inhalt **darunter durchgezogen**:
//
//   titlebarAppearsTransparent = YES
//   titleVisibility            = NSWindowTitleHidden
//   styleMask                 |= NSWindowStyleMaskFullSizeContentView
//
// Optisch ist das dasselbe wie randlos — kein Balken, kein Rahmen,
// der Inhalt beginnt oben am Rand. Nur liegen jetzt die echten drei
// Punkte darüber, und der gelbe tut, was er soll.
//
// Kein eigener Space: die Werkzeugfenster bleiben, wo sie sind. Das
// ist die ganze Begründung, aus der das randlose Fenster überhaupt
// gewählt wurde, und sie gilt hier weiter.
//
// ── Was der Aufrufer tun muss ────────────────────────────────────────
//
// Links oben Platz lassen. Die drei Punkte sitzen über dem Inhalt,
// und eine Menüzeile, die darunter beginnt, ist zur Hälfte nicht
// anklickbar. `kKnopfBreite` sagt, wieviel.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-10 — Created in C++20/Qt6/Objective-C++ for Longpath by
//                 Martin Fischer, AI-assisted via Anthropic Claude
//                 (Claude Code).
// =================================================================

#include <QtGlobal>

class QWidget;

namespace Longpath::MacVollflaeche {

// Wieviel Platz die drei Punkte links oben brauchen, in Punkten.
// 78 ist gemessen, nicht geraten: drei Knöpfe zu 14 px mit 20 px
// Abstand vom Rand und 8 px untereinander.
inline constexpr int kKnopfBreite = 78;

// Schaltet die Behandlung an oder aus. Auf allem ausser macOS
// passiert nichts und es wird `false` gemeldet -- der Aufrufer bleibt
// dann beim randlosen Fenster.
#ifdef Q_OS_MAC
bool setzeVollflaeche(QWidget* fenster, bool an);

// Ob diese Fassung die Behandlung überhaupt kann (also: macOS).
bool verfuegbar();
#else
// Ohne macOS gibt es die Behandlung nicht. Die Stummel stehen hier
// inline und nicht in einem eigenen Baustein, weil MacVollflaeche.mm
// nur auf dem Mac mitgebaut wird -- der Aufrufer darf trotzdem auf
// jeder Plattform fragen und bleibt dann beim randlosen Fenster.
inline bool setzeVollflaeche(QWidget*, bool) { return false; }
inline bool verfuegbar() { return false; }
#endif

} // namespace Longpath::MacVollflaeche
