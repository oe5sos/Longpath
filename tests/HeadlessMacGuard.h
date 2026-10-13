#pragma once

// =================================================================
// tests/HeadlessMacGuard.h  (Longpath)
// =================================================================
//
// Ein Prüfstand, der das Hauptfenster baut, läuft auf einem Mac OHNE
// Bildschirm nicht.
//
// Gemessen am 2026-10-13: das Hauptfenster trägt mehrere QRhiWidget
// (Zeigerkranz, Wasserfall). Die Qt-Anbindungen „offscreen" und
// „minimal" melden beide erst
//
//     QRhiWidget: QRhi is not supported on this platform.
//     This plugin does not support setParent!
//
// und greifen gleich darauf in einen Zeiger 0x1 — EXC_BAD_ACCESS in
// objc_msgSend, also in Cocoa-Code, den es nur auf dem Mac gibt. Es
// ist eine Grenze der Anbindung und keine Frage unserer Fenster:
// dieselben Prüfstände laufen auf einem echten Schirm und unter Linux.
//
// Warum das überhaupt auffiel: diese fünf Prüfprogramme standen
// monatelang als veraltete Binärdateien im Bauordner und wurden bei
// jedem Lauf mitgezählt, ohne neu gebaut zu werden — ein falsches
// Grün, vor dem docs/development/fast-test-loop.md ausdrücklich warnt.
// Beim ersten vollständigen Neubau am 2026-10-13 stürzten sie alle
// fünf ab.
//
// Überspringen statt rot stehen lassen, und mit Grund: ein roter
// Prüfstand, den auf dieser Maschine niemand ausführen kann, ist ein
// kaputtes Werkzeug — er verdeckt die echten Roten. Mit einem Schirm
// laufen sie weiter; am Gerät des Betreibers gehören Prüfläufe mit
// sichtbaren Fenstern aber nicht hin (er hielt am 2026-09-30 eine
// Sonde für Longpath selbst).
//
// =================================================================
// Modification history (Longpath):
//   2026-10-13 — Original für Longpath von Martin Fischer,
//                 KI-gestützt über Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>
#include <QGuiApplication>

// In der ERSTEN Zeile jedes Falls, der ein MainWindow baut.
#define LONGPATH_SKIP_IF_HEADLESS_MAC_GUI()                                   \
    do {                                                                      \
        const QString _lp_platform = QGuiApplication::platformName();         \
        if (_lp_platform == QLatin1String("offscreen")                        \
            || _lp_platform == QLatin1String("minimal")) {                    \
            QSKIP("Hauptfenster + QRhiWidget stürzt ohne Schirm ab "          \
                  "(siehe tests/HeadlessMacGuard.h)");                        \
        }                                                                     \
    } while (false)
