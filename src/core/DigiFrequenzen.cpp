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
// jeweils `Modes::FT2, IARURegions::ALL`.
//
// ── Ausser 40 m und 80 m: dort gilt, was am Band GESCHIEHT ──────
//
// Betreiber nach dem ersten Versuch auf 14,084: „nur rauschen
// gehoert". Nachgesehen bei PSK Reporter (`retrieve.pskreporter.info`,
// mode=FT2, letzte Stunde, 2026-11-17 19:50–20:46 MEZ, 241 Meldungen):
//
//     20 m  14,0845–14,0862 MHz  -> 14,084   wie Decodium
//     40 m   7,0533– 7,0537 MHz  ->  7,052   Decodium sagt 7,062
//     80 m   3,5788– 3,5802 MHz  ->  3,578   Decodium sagt 3,568
//     10 m  28,185              -> 28,184   wie Decodium
//
// Auf 7,062 und 3,568 sendete in dieser Stunde niemand; auf 7,052
// (SP6CA, UA3DIB, DB3BD, IK1PMV) und 3,578 (DO5HOK, IK1PMV, IV3NDR)
// lief europaeischer Betrieb. 7,052 fuehrt auch die Tafel von 4X4XM
// (qsl.net/4x4xm, „Digital Ham Communication Frequencies"). Wer auf
// der Liste des Programms sitzt und dort niemanden hoert, dem hilft
// die Liste nicht.
//
// Die uebrigen Baender: in dieser Stunde ohne eine Meldung, also nicht
// nachgeprueft -- Decodiums Zahl bleibt, bis das Band etwas anderes
// sagt.
//
// ── Alle Quellen nebeneinander (2026-11-17, „suche alle gueltigen") ──
//
//            Decodium-   DXZone-     Decodium-     am Band
//            Programm    Testliste   Handbuch*     (PSK, 20–21 Uhr)
//   160 m    1,843       1,843       –             –
//    80 m    3,568       3,578       3,580         3,578–3,580  -> 3,578
//    60 m    5,360       5,360       –             –
//    40 m    7,062 **    7,052       7,080         7,053        -> 7,052
//    30 m   10,144      10,144      10,137         –
//    20 m   14,084      14,084      14,080        14,084–14,086 -> 14,084
//    17 m   18,108      18,108      18,105         –
//    15 m   21,144      21,144      21,080         –
//    12 m   24,923      24,923      24,920        24,924        -> 24,923
//    10 m   28,184      28,184      28,180        28,185        -> 28,184
//     6 m   50,316       –          50,318         –
//     2 m  144,177       –           –             –
//
//   *  Decodium 4.0 Handbuch v1.0.262, 5.1.1 „Standard FT2 frequencies".
//      NICHT uebernommen: 14,080 / 28,180 / 50,318 sind die
//      FT4-Anrufstellen, das Programm selbst stellt andere Zahlen ein,
//      und am Band liegt der Betrieb auf denen des Programms.
//   ** Das FT2-Team (IU8LMC, IZ8XXE, ARI Caserta) hat 40 m von 7,052 auf
//      7,062 verlegt, „to align with the IARU Region 1 bandplan"
//      (vu3dxr.in, „FT2 moving to 7062 kHz"). Am 17.11. abends funkte
//      dort niemand, auf 7,052 schon. Darum stellt der Knopf 7,052 ein;
//      7,062 steht unten bei den WEITEREN Stellen, und wer dort hinstellt,
//      bekommt den FT2-Dekoder genauso.
//
// Jede Stelle liegt mindestens 2 kHz neben der FT4- und FT8-Stelle
// desselben Bandes -- `stehtSchonDort` mit seinen 500 Hz verwechselt
// sie also nicht (geprueft in tests/tst_ft2.cpp).
constexpr Stelle kStellen[] = {
    { Band::Band160m,  1'840'000,          0,  1'843'000 },
    { Band::Band80m,   3'573'000,  3'575'000,  3'578'000 },
    { Band::Band60m,   5'357'000,          0,  5'360'000 },
    { Band::Band40m,   7'074'000,  7'047'500,  7'052'000 },
    { Band::Band30m,  10'136'000, 10'140'000, 10'144'000 },
    { Band::Band20m,  14'074'000, 14'080'000, 14'084'000 },
    { Band::Band17m,  18'100'000, 18'104'000, 18'108'000 },
    { Band::Band15m,  21'074'000, 21'140'000, 21'144'000 },
    { Band::Band12m,  24'915'000, 24'919'000, 24'923'000 },
    { Band::Band10m,  28'074'000, 28'180'000, 28'184'000 },
    { Band::Band6m,   50'313'000, 50'318'000, 50'316'000 },
    // 2 m nur fuer FT2: Decodium `Modes::FT2, IARURegions::ALL`. FT8 und
    // FT4 haben hier (noch) keinen Eintrag -- nicht Teil dieses Auftrags.
    { Band::Band2m,            0,          0, 144'177'000 },
};

// ── Weitere gueltige FT2-Stellen ─────────────────────────────────────
//
// Auf diese stellt der Knopf NICHT, aber steht die Skala dort, erkennt
// Longpath FT2 (`stehtSchonDort`, und damit Ft8Applet::folgeDerSkala).
//
//   3,568 / 7,062  Decodium-Programm bzw. FT2-Team (siehe oben)
//   70,157         4 m, Decodium `IARURegions::R1`
//   432,177        70 cm, Decodium `IARURegions::ALL`
//   1296,177       23 cm, Decodium `IARURegions::ALL`
//
// Nicht aufgenommen: 222,177 MHz (Decodium `IARURegions::R2`, kein
// Amateurband in Region 1) und 1,839 (4X4XM, 160 m) -- dafuer gibt es
// keine zweite Quelle, und 1,840 ist die FT8-Stelle daneben.
constexpr qint64 kFt2Weitere[] = {
    3'568'000, 7'062'000, 70'157'000, 432'177'000, 1'296'177'000,
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
    if (modus == Digimodus::Ft2) {
        for (const qint64 ziel : kFt2Weitere) {
            const qint64 abstand = skalenfrequenzHz > ziel
                                       ? skalenfrequenzHz - ziel
                                       : ziel - skalenfrequenzHz;
            if (abstand <= spielraumHz) { return true; }
        }
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
