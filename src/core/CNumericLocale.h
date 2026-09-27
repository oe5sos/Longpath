#pragma once

// =================================================================
// src/core/CNumericLocale.h  (Longpath)
// =================================================================
//
// Longpath-original.
// no-port-check: names third-party C files only to say which of them read
// numbers with sscanf/fscanf; nothing here is derived from them.
//
// ── Zahlen immer mit Punkt ───────────────────────────────────────────
//
// Befund 2026-09-27 (Nachtschicht): „TLE-Satz: 0 Satelliten, 95
// verworfen" in jedem Longpath, das aus dem Terminal gestartet wurde --
// und 95 geladen, 0 verworfen im selben Stand aus dem Dock.
//
// Qt ruft beim Anlegen der QApplication auf Unix setlocale(LC_ALL, "")
// auf. Mit LANG=de_AT.UTF-8 (jedes Terminal, jede deutsche Linux-
// Sitzung) ist das Dezimalzeichen der C-Bibliothek danach ein KOMMA.
// Alles, was Zahlen mit sscanf/fscanf/strtod liest, liest dann
// „26266.86626550" als 26266 und den Rest als Muell:
//
//   - third_party/sgp4/SGP4.cpp twoline2rv: sscanf("%lf") auf jeder
//     Bahndatenzeile -> alle Satelliten verworfen;
//   - third_party/wdsp/src/calcc.c: fscanf("%le") beim Zurueckholen einer
//     gespeicherten PureSignal-Korrektur, fir.c beim Einlesen einer
//     Impulsantwort aus Datei.
//
// Aus dem Dock (launchd, ohne LANG) blieb es beim „C"-Gebietsschema, und
// darum fiel es nicht auf.
//
// Qt empfiehlt genau das hier (QCoreApplication, „Locale Settings"):
// direkt nach dem Anlegen der Application setlocale(LC_NUMERIC, "C").
// Qts eigene Zahlenformate (QLocale, QString::number) haengen nicht am
// C-Gebietsschema; die Anzeige in der Oberflaeche bleibt unberuehrt.
//
// Modification history (Longpath):
//   2026-09-27 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <clocale>

namespace Longpath {

/// Das Dezimalzeichen der C-Bibliothek auf den Punkt zuruecksetzen.
/// Nach dem Anlegen der QApplication aufrufen (vorher setzt Qt es wieder
/// auf die Systemsprache).
inline void useCNumericLocale()
{
    std::setlocale(LC_NUMERIC, "C");
}

} // namespace Longpath
