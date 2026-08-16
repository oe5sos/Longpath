#pragma once

// =================================================================
// src/gui/widgets/SignalReading.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Wann eine Pegelangabe eine Messung ist -- und was dasteht, wenn sie
// keine ist.
//
// ── Der Befund ───────────────────────────────────────────────────────
//
// 2026-08-16: ohne Verbindung stand an drei Stellen "-395 dBm" --
// Zifferblatt, Zahlenfeld und VFO-Leiste. Das ist kein schwaches
// Signal, das ist gar keins: -395 dBm liegt zweihundert Dezibel unter
// dem thermischen Rauschen eines jeden Empfaengers.
//
// docs/design/HAUSSTIL.md Regel 7:
//
//     Unbekannt ist ein Strich, keine Null. "--" statt "0.00" -- eine
//     Null sieht aus wie eine Messung.
//
// Eine Zahl sieht ebenso aus wie eine Messung, und -395 sogar wie eine
// sehr genaue. Genau davor warnt die Regel.
//
// ── Warum am Wert und nicht am Verbindungszustand ────────────────────
//
// Der naheliegende Weg waere ein Schalter gewesen, den jemand bei
// Verbindungsverlust umlegt. Dann hat die Frage "gilt diese Zahl?" zwei
// Besitzer -- den Wert und den Schalter --, und sie koennen
// auseinanderlaufen: eine stehengebliebene Messung nach einem Abbruch
// saehe weiterhin gueltig aus, und ein vergessener Aufruf beim
// Wiederverbinden liesse den Strich stehen, obwohl Daten fliessen.
//
// Der Wert traegt es selbst. Ein Pegel unterhalb der Schwelle IST keine
// Messung, gleich woher er kommt -- Sentinel, Abbruch, oder eine
// Rechnung, die auf Stille den Logarithmus von Null gebildet hat.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-16 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include <QString>

#include <cmath>

namespace NereusSDR {
namespace SignalReading {

/// Unterhalb dessen ist es keine Messung mehr.
///
/// -200 dBm mit Absicht grosszuegig: das thermische Rauschen in einem
/// schmalen Bin liegt um -163 dBm, ein sehr stiller Empfaenger meldet
/// vielleicht -150. Wer -180 misst, soll die Zahl trotzdem sehen --
/// die Schwelle faengt Unmoegliches ab, nicht Unwahrscheinliches.
constexpr float kFloorDbm = -200.0f;

/// Ist das eine Messung? Nicht endlich und nicht ueber dem Boden heisst
/// nein.
inline bool isMeasurement(float dbm)
{
    return std::isfinite(dbm) && dbm > kFloorDbm;
}

/// Der Strich aus HAUSSTIL Regel 7. Zwei Geviertstriche, damit er als
/// Angabe lesbar ist und nicht als Trennzeichen.
inline QString noReadingText()
{
    return QStringLiteral("——");
}

/// Fertiger Text: entweder die gerundete Zahl mit Einheit, oder der
/// Strich OHNE Einheit. "-- dBm" waere wieder eine halbe Behauptung --
/// es gibt keine Dezibel, wenn es keine Messung gibt.
inline QString text(float dbm, const QString& unit = QStringLiteral("dBm"))
{
    if (!isMeasurement(dbm)) { return noReadingText(); }
    return QStringLiteral("%1 %2")
        .arg(static_cast<double>(dbm), 0, 'f', 0)
        .arg(unit);
}

} // namespace SignalReading
} // namespace NereusSDR
