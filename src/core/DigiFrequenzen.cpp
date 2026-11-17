// no-port-check: Longpath-eigene Tafel. Die Zahlen sind die
// veroeffentlichten Anrufstellen aus der Vorgabeliste von WSJT-X
// (`FrequencyList.cpp`, Default_Frequencies) -- Konvention der
// Betriebsart, kein Quelltext von dort.
// =================================================================
// src/core/DigiFrequenzen.cpp  (Longpath)
// =================================================================
//
// Siehe DigiFrequenzen.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include "core/DigiFrequenzen.h"

namespace Longpath::DigiFrequenzen {

namespace {

struct Stelle {
    Band    band;
    qint64  ft8Hz;   // 0 = gibt es nicht
    qint64  ft4Hz;   // 0 = gibt es nicht
    qint64  ft2Hz;   // 0 = gibt es nicht
};

// Region 1 (IARU), also das, was hier gilt. Die Liste fuehrt nur die
// Baender, auf denen es eine allgemein benutzte Anrufstelle GIBT --
// 60 m hat FT8, aber kein FT4, und die Rundfunkbaender haben nichts
// von beidem. Lieber eine Luecke als eine erfundene Zahl: wer auf
// einem Band ohne Eintrag Digital waehlt, behaelt seine Frequenz.
//
// ── FT2 (2026-11-17) ─────────────────────────────────────────
//
// WSJT-X Improved 3.1.0 bringt KEINE FT2-Anrufstellen mit (in
// `models/FrequencyList.cpp` steht kein `Modes::FT2`). Die Zahlen
// kommen darum aus Decodium 4.0, dem Programm, mit dem die Betriebsart
// entstanden ist: `models/FrequencyList.cpp`, Default_Frequencies,
// jeweils `Modes::FT2, IARURegions::ALL`. Jede liegt mindestens 2 kHz
// neben der FT4-Stelle desselben Bandes -- `stehtSchonDort` mit seinen
// 500 Hz verwechselt sie also nicht.
constexpr Stelle kStellen[] = {
    { Band::Band160m,  1'840'000,          0,  1'843'000 },
    { Band::Band80m,   3'573'000,  3'575'000,  3'568'000 },
    { Band::Band60m,   5'357'000,          0,  5'360'000 },
    { Band::Band40m,   7'074'000,  7'047'500,  7'062'000 },
    { Band::Band30m,  10'136'000, 10'140'000, 10'144'000 },
    { Band::Band20m,  14'074'000, 14'080'000, 14'084'000 },
    { Band::Band17m,  18'100'000, 18'104'000, 18'108'000 },
    { Band::Band15m,  21'074'000, 21'140'000, 21'144'000 },
    { Band::Band12m,  24'915'000, 24'919'000, 24'923'000 },
    { Band::Band10m,  28'074'000, 28'180'000, 28'184'000 },
    { Band::Band6m,   50'313'000, 50'318'000, 50'316'000 },
};

qint64 stelleFuer(const Stelle& s, Digimodus modus)
{
    switch (modus) {
    case Digimodus::Ft4: return s.ft4Hz;
    case Digimodus::Ft2: return s.ft2Hz;
    case Digimodus::Ft8: break;
    }
    return s.ft8Hz;
}

} // namespace

qint64 anruffrequenz(Band band, Digimodus modus)
{
    for (const Stelle& s : kStellen) {
        if (s.band != band) { continue; }
        return stelleFuer(s, modus);
    }
    return 0;
}

bool stehtSchonDort(qint64 skalenfrequenzHz, Digimodus modus,
                    qint64 spielraumHz)
{
    if (skalenfrequenzHz <= 0) { return false; }
    // Nicht ueber das Band gehen, sondern ueber die Tafel: liegt die
    // Frequenz nahe IRGENDEINER Anrufstelle dieser Betriebsart, steht
    // der Bediener schon richtig. Das Band erst zu bestimmen waere ein
    // Umweg und ginge auf einer Bandgrenze daneben.
    for (const Stelle& s : kStellen) {
        const qint64 ziel = stelleFuer(s, modus);
        if (ziel == 0) { continue; }
        const qint64 abstand = skalenfrequenzHz > ziel
                                   ? skalenfrequenzHz - ziel
                                   : ziel - skalenfrequenzHz;
        if (abstand <= spielraumHz) { return true; }
    }
    return false;
}

} // namespace Longpath::DigiFrequenzen

namespace Longpath::DigiFrequenzen {

Bildausschnitt bildausschnitt(qint64 skalenfrequenzHz)
{
    Bildausschnitt a;
    if (skalenfrequenzHz <= 0) { return a; }
    a.mitteHz = double(skalenfrequenzHz)
                + (kDurchlassVonHz + kDurchlassBisHz) / 2.0;
    a.breiteHz = (kDurchlassBisHz - kDurchlassVonHz) + 2.0 * kLuftHz;
    return a;
}

}  // namespace Longpath::DigiFrequenzen
