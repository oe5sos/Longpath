// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/core/audio/AnsageAufbereitung.h  (Longpath)
// =================================================================
//
// no-port-check: Longpath-original.
//
// Eine Ansage contesttauglich machen (Betreiber, 2026-11-13: „ein voice
// memo aufnehmen, welches fuer den Kenwood vor allem fuer den Contest
// gut funktioniert").
//
// Drei Dinge, die eine rohe Mikrofonaufnahme im Contest falsch macht:
//
//   * Stille vorn kostet bei jedem CQ Zeit, Stille hinten haelt den
//     Sender auf, waehrend die Gegenstation schon antwortet.
//   * Der Tastendruck, der die Aufnahme startet und beendet, ist im
//     eingebauten Mikrofon zu hoeren -- und ginge mit auf Sendung.
//   * Jede Ansage hat einen anderen Pegel. Am 590 geht der Ton roh ueber
//     USB hinaus (keine Sprachbearbeitung von Longpath dazwischen), also
//     treibt jede Ansage die ALC anders.
//
// kuerze() schneidet die Stille ab und laesst einen kurzen Rand stehen.
// Sprache ist, was mindestens kHaltMs am Stueck laut ist: ein
// Tastenklick (10–30 ms) ist es nicht. fuerSendung() nimmt die Tiefen
// unter kHochpassHz heraus (Rumpeln, Ploppen: sie fressen Aussteuerung,
// die das Sendefilter ohnehin wegwirft) und bringt die Sprache auf einen
// festen Pegel mit Spitzenreserve.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-13 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QVector>

namespace Longpath::AnsageAufbereitung {

inline constexpr int   kRahmenMs      = 10;     // Messfenster
inline constexpr int   kHaltMs        = 80;     // so lange laut = Sprache
inline constexpr int   kVorMs         = 80;     // Rand vor dem ersten Wort
inline constexpr int   kNachMs        = 200;    // Rand nach dem letzten Wort
inline constexpr int   kBlendeMs      = 10;     // weiche Kanten, kein Knacks
inline constexpr float kZuLeiseDb     = -30.0f; // Spitze darunter: zu leise
inline constexpr float kZielSpracheDb = -16.0f; // Effektivwert der Sprache
inline constexpr float kZielSpitzeDb  = -1.0f;  // Spitze hoechstens
inline constexpr float kMaxHubDb      = 30.0f;  // mehr wird nicht angehoben
inline constexpr float kHochpassHz    = 150.0f;

struct Befund {
    bool  sprache{false};      // ueberhaupt Sprache gefunden
    int   anfang{0};           // erste Probe der Sprache
    int   ende{0};             // eine hinter der letzten Probe der Sprache
    float spitzeDb{-120.0f};   // Spitze innerhalb der Sprache, dBFS
    float spracheDb{-120.0f};  // Effektivwert der lauten Rahmen, dBFS
    int   amAnschlag{0};       // Proben mit |x| >= 0,99 (uebersteuert)
};

/// Misst, wo die Sprache steht und wie laut sie ist.
Befund miss(const float* s, int n, int rateHz);
inline Befund miss(const QVector<float>& s, int rateHz)
{
    return miss(s.constData(), int(s.size()), rateHz);
}

/// Schneidet die Stille vorn und hinten ab (mit Rand und weichen
/// Kanten). Ohne Sprache kommt die Aufnahme unveraendert zurueck.
QVector<float> kuerze(const QVector<float>& s, int rateHz, Befund* befund = nullptr);

/// Die Verstaerkung (linear), die die Sprache auf kZielSpracheDb bringt,
/// ohne dass die Spitze kZielSpitzeDb ueberschreitet.
float verstaerkung(const Befund& b);

/// Hochpass 2. Ordnung (Butterworth), Probe fuer Probe.
class Hochpass
{
public:
    void setze(int rateHz, float grenzHz = kHochpassHz);
    void zuruecksetzen() { m_x1 = m_x2 = m_y1 = m_y2 = 0.0; }
    float operator()(float x);

private:
    double m_b0{1.0}, m_b1{0.0}, m_b2{0.0}, m_a1{0.0}, m_a2{0.0};
    double m_x1{0.0}, m_x2{0.0}, m_y1{0.0}, m_y2{0.0};
};

/// Die Verstaerkung fuer den Sendeweg: gemessen NACH dem Hochpass, weil
/// der die Spitzen veraendert.
float verstaerkungFuerSendung(const QVector<float>& s, int rateHz);

/// Hochpass + Verstaerkung, begrenzt auf ±1: so geht die Ansage am 590
/// hinaus, und so klingt sie beim Anhoeren.
QVector<float> fuerSendung(const QVector<float>& s, int rateHz);

} // namespace Longpath::AnsageAufbereitung
