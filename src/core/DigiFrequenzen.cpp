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
};

// Region 1 (IARU), also das, was hier gilt. Die Liste fuehrt nur die
// Baender, auf denen es eine allgemein benutzte Anrufstelle GIBT --
// 60 m hat FT8, aber kein FT4, und die Rundfunkbaender haben nichts
// von beidem. Lieber eine Luecke als eine erfundene Zahl: wer auf
// einem Band ohne Eintrag Digital waehlt, behaelt seine Frequenz.
constexpr Stelle kStellen[] = {
    { Band::Band160m,  1'840'000,          0 },
    { Band::Band80m,   3'573'000,  3'575'000 },
    { Band::Band60m,   5'357'000,          0 },
    { Band::Band40m,   7'074'000,  7'047'500 },
    { Band::Band30m,  10'136'000, 10'140'000 },
    { Band::Band20m,  14'074'000, 14'080'000 },
    { Band::Band17m,  18'100'000, 18'104'000 },
    { Band::Band15m,  21'074'000, 21'140'000 },
    { Band::Band12m,  24'915'000, 24'919'000 },
    { Band::Band10m,  28'074'000, 28'180'000 },
    { Band::Band6m,   50'313'000, 50'318'000 },
};

} // namespace

qint64 anruffrequenz(Band band, Digimodus modus)
{
    for (const Stelle& s : kStellen) {
        if (s.band != band) { continue; }
        return modus == Digimodus::Ft8 ? s.ft8Hz : s.ft4Hz;
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
        const qint64 ziel = (modus == Digimodus::Ft8) ? s.ft8Hz : s.ft4Hz;
        if (ziel == 0) { continue; }
        const qint64 abstand = skalenfrequenzHz > ziel
                                   ? skalenfrequenzHz - ziel
                                   : ziel - skalenfrequenzHz;
        if (abstand <= spielraumHz) { return true; }
    }
    return false;
}

} // namespace Longpath::DigiFrequenzen
