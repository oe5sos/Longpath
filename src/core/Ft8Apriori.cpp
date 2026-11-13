// =================================================================
// src/core/Ft8Apriori.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Verfahren und Maskengrenzen stammen aus WSJT-X
// [v2.7.0]; die Fundstellen stehen in Ft8Apriori.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-13 — Created in C++20/Qt6 for Longpath by Martin Fischer
//                 (OE5SOS), AI-assisted via Anthropic Claude (Claude Code).
// =================================================================
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/Ft8Apriori.h"

extern "C" {
#include "ft8/constants.h"
#include "ft8/message.h"
}

static_assert(Longpath::kApBits == FTX_LDPC_N,
              "kApBits muss FTX_LDPC_N sein -- sonst zeigt die Maske "
              "an der Codewortlaenge vorbei.");

namespace Longpath {

namespace {

/// Ein Bit aus der gepackten Nutzlast. Das hoechstwertige Bit des
/// ersten Bytes ist Bit 0 -- dieselbe Zaehlung wie `pack_bits` in
/// ft8_lib, und dieselbe, auf die sich die Maskengrenzen beziehen.
uint8_t bitAus(const uint8_t* nutzlast, int i)
{
    return static_cast<uint8_t>((nutzlast[i / 8] >> (7 - (i % 8))) & 1);
}

// ── Die Grenzen, Bit fuer Bit aus dem Vorbild ──────────────────────
//
// WSJT-X zaehlt ab 1, hier wird ab 0 gezaehlt; jede Grenze ist darum
// um eins kleiner als dort.
//
//   ft8b.f90:319  apmask(1:29)  -> 0..28   Rufzeichen 1 UND R1
//   ft8b.f90:359  apmask(1:58)  -> 0..57   dazu Rufzeichen 2 und R2
//   ft8b.f90:321  apmask(75:77) -> 74..76  der Nachrichtentyp
//
// Dass Typ 2 das R1 mitnimmt (29 Bits, nicht 28) ist kein Versehen des
// Vorbilds, sondern seine Aussage: wer mir antwortet, setzt dort keine
// Wiederholungsfahne.
constexpr int kBisRuf1   = 29;   ///< 0..28
constexpr int kBisRuf2   = 58;   ///< 0..57
constexpr int kTypVon    = 74;   ///< 74..76

/// Der Nachrichtentyp einer Standardnachricht: i3 = 1, also 0,0,1.
/// WSJT-X setzt ihn fest statt aus der Scheinnachricht zu lesen
/// (ft8b.f90:322-323) -- er haengt nicht an den Rufzeichen.
constexpr uint8_t kTypBits[3] = { 0, 0, 1 };

}  // namespace

ApVorgabe baueApVorgabe(ApTyp typ, const QString& meinRuf,
                        const QString& seinRuf)
{
    ApVorgabe v;

    const QString mein = meinRuf.trimmed().toUpper();
    const QString sein = seinRuf.trimmed().toUpper();
    if (mein.size() < 3) { return v; }
    if (typ == ApTyp::MeinRufUndSeiner && sein.size() < 3) { return v; }

    // Ohne Gegenueber ein Platzhalter, genau wie WSJT-X
    // (`ft8apset.f90:21`, hiscall='KA1ABC'): die Scheinnachricht muss
    // vollstaendig sein, damit die Packung greift; gebraucht werden
    // davon nur die vorderen Bits.
    const QString gegen = sein.isEmpty() ? QStringLiteral("KA1ABC") : sein;

    const QString schein = mein + QLatin1Char(' ') + gegen
                           + QStringLiteral(" RRR");
    const QByteArray roh = schein.toLatin1();

    ftx_message_t nachricht{};
    if (ftx_message_encode(&nachricht, nullptr, roh.constData())
        != FTX_MESSAGE_RC_OK) {
        // Nicht packbar heisst: mindestens ein Rufzeichen ist kein
        // Standardrufzeichen und steht gehasht in der Nachricht --
        // nicht als die 28 Bit, auf die sich die Masken beziehen.
        // WSJT-X steigt an derselben Stelle aus (ft8apset.f90:46).
        return v;
    }

    // Gegenprobe: nur eine Standardnachricht (i3=1) hat das Bitfeld,
    // das die Masken voraussetzen. `ftx_message_encode` faellt bei
    // Sonderrufzeichen auf Typ 4 zurueck, und der hat ein anderes.
    const uint8_t* p = nachricht.payload;
    for (int k = 0; k < 3; ++k) {
        if (bitAus(p, kTypVon + k) != kTypBits[k]) { return v; }
    }

    const int bisRuf = (typ == ApTyp::MeinRufUndSeiner) ? kBisRuf2
                                                        : kBisRuf1;
    for (int i = 0; i < bisRuf; ++i) {
        v.bits[static_cast<size_t>(i)]  = bitAus(p, i);
        v.maske[static_cast<size_t>(i)] = 1;
    }
    for (int k = 0; k < 3; ++k) {
        v.bits[static_cast<size_t>(kTypVon + k)]  = kTypBits[k];
        v.maske[static_cast<size_t>(kTypVon + k)] = 1;
    }

    v.brauchbar = true;
    return v;
}

}  // namespace Longpath
