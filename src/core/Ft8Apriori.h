// =================================================================
// src/core/Ft8Apriori.h  (Longpath)
// =================================================================
//
// Longpath-original; das Verfahren stammt aus WSJT-X (K1JT und die
// WSJT-Gruppe, GPLv3), siehe die Fundstellen unten. Kein Quelltext
// von dort — Fortran laesst sich ohnehin nicht abschreiben.
//
// ── Was man weiss, bevor man hinhoert (2026-11-13) ──────────────────
//
// Betreiber: FT8 und FT4 sollen laufen „mit allem was dazu gehoert."
//
// Wer auf mein CQ antwortet, schreibt MEIN Rufzeichen in die
// Nachricht. Das steht fest, bevor ein einziges Bit entziffert ist.
// Achtundzwanzig der siebenundsiebzig Bits sind damit keine Frage
// mehr, sondern eine Angabe — und ein Dekoder, der das weiss, holt
// genau die schwachen Schlusszeichen heraus (RR73, 73), an denen ein
// QSO sonst unvollendet liegen bleibt.
//
// `ft8_lib` kann das von sich aus nicht. Die Kette nimmt es seit
// 2026-11-13 entgegen (`ftx_decode_candidate_ap`); hier steht, WAS
// man ihr mitgibt.
//
// ── Der Trick mit der Scheinnachricht ───────────────────────────────
//
// Die 77 Bit einer Standardnachricht selbst zusammenzusetzen hiesse,
// die Rufzeichenpackung ein zweites Mal zu schreiben — und das ist
// die Stelle, an der zwei Fassungen eines Tages auseinanderlaufen.
// Stattdessen wird eine vollstaendige Scheinnachricht
// „MEINRUF SEINRUF RRR" durch `ftx_message_encode` geschickt. Was
// herauskommt, traegt die Rufzeichen an genau den Stellen, an denen
// sie auch in der echten Nachricht stehen. Dasselbe tut WSJT-X
// (`lib/ft8/ft8apset.f90:38`, msg=mycall//hiscall//' RRR').
//
// ── Das Bitfeld einer Standardnachricht (i3=1) ──────────────────────
//
//     Bit  0..27   Rufzeichen 1 — der GERUFENE
//     Bit  28      R1
//     Bit 29..56   Rufzeichen 2 — der RUFER
//     Bit  57      R2
//     Bit  58      R
//     Bit 59..73   Locator oder Rapport
//     Bit 74..76   Nachrichtentyp (i3)
//
// Mein Rufzeichen steht also VORNE, wenn man mir antwortet. Genau
// darauf beruhen die beiden Masken.
//
// ── Warum das nicht raten ist ───────────────────────────────────────
//
// Weil der CRC entscheidet. Eine Maske, die danebenliegt, erzeugt ein
// Codewort, das die Pruefsumme nicht besteht, und die Nachricht faellt
// durch wie jedes Rauschen. Falsche Treffer kosten Rechenzeit, keine
// Richtigkeit. Siehe `ftx_decode_candidate_ap`.
//
// Fundstellen in WSJT-X [v2.7.0]:
//   lib/ft8/ft8apset.f90:1-62   — die Scheinnachricht und apsym
//   lib/ft8/ft8b.f90:300-360    — die Masken je Typ
//   lib/ft8/ft8b.f90:241        — apmag = groesster Betrag * 1.01
//   lib/ft8/decode174_91.f90:55 — wie die Maske im Dekoder wirkt
//
// =================================================================
// Modification history (Longpath):
//   2026-11-13 — Created in C++20/Qt6 for Longpath by Martin Fischer
//                 (OE5SOS), AI-assisted via Anthropic Claude (Claude Code).
// =================================================================
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <array>
#include <cstdint>

namespace Longpath {

/// Die Laenge eines FT8-Codeworts. Muss zu FTX_LDPC_N passen; der
/// Pruefstand haelt beide gegeneinander, damit ein Upstream-Wechsel
/// nicht still danebengreift.
inline constexpr int kApBits = 174;

/// Welche Gewissheit in einen Durchgang gegeben wird.
enum class ApTyp {
    /// Nur mein Rufzeichen: „jemand ruft MICH." Der Fall, der beim
    /// Warten auf eine Antwort zaehlt.
    MeinRuf,
    /// Mein Rufzeichen UND das der Gegenstation: das laufende QSO.
    /// Die staerkste Gewissheit -- und die, die den Schluss rettet.
    MeinRufUndSeiner,
};

/// Die bekannten Bits und ihre Maske, fertig fuer
/// `ftx_decode_candidate_ap`.
struct ApVorgabe {
    std::array<uint8_t, kApBits> bits{};
    std::array<uint8_t, kApBits> maske{};
    /// Falsch, wenn die Vorgabe nicht gebaut werden konnte -- kein
    /// Rufzeichen, nicht packbar, oder (bei MeinRufUndSeiner) kein
    /// Gegenueber. Dann NICHT dekodieren: eine leere Maske waere ein
    /// zweiter Durchgang ohne jeden Gewinn.
    bool brauchbar{false};
};

/// Die Vorgabe bauen. `meinRuf` immer noetig, `seinRuf` nur fuer
/// `ApTyp::MeinRufUndSeiner`.
///
/// Nicht-Standard-Rufzeichen (Schraegstriche, zu lang) geben eine
/// unbrauchbare Vorgabe zurueck: sie stehen gehasht in der Nachricht,
/// nicht als die 28 Bit, auf die sich die Masken beziehen. WSJT-X
/// steigt an derselben Stelle aus (`ft8apset.f90:46`, `msg.ne.msgchk`).
ApVorgabe baueApVorgabe(ApTyp typ, const QString& meinRuf,
                        const QString& seinRuf = QString());

}  // namespace Longpath
