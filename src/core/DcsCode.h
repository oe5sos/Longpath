// no-port-check: Longpath-original file. Es gibt nichts zu portieren --
// weder Thetis noch WDSP kennen DCS, in keiner Richtung. Das Verfahren
// hier ist nach der oeffentlichen Beschreibung des Digital Coded
// Squelch gebaut (23-Bit-Golay-Wort bei 134,4 bit/s) und die
// Golay(23,12)-Arithmetik ist Mathematik, kein fremder Code. Dass der
// Generator stimmt, belegt der Pruefstand selbst: er rechnet den
// Mindestabstand ueber alle 4096 Codewoerter aus und verlangt 7.

#pragma once

// DCS (Digital Coded Squelch, bei Motorola "DPL") -- die Codewoerter.
//
// Ein DCS-Sender legt unter die Sprache einen fortlaufenden Datenstrom:
// immer dasselbe 23-Bit-Wort, wieder und wieder, mit 134,4 bit/s. Ein
// Wort dauert damit 23 / 134,4 = 171 ms.
//
// Aufbau des Wortes, vom zuerst gesendeten Bit an:
//
// Geschrieben, mit der hoechsten Stelle links:
//
//     P11..P1       11 Golay-Pruefbits      (Stellen 22..12)
//     1 0 0         drei feste Bits         (Stellen 11, 10, 9)
//     C9..C1        9 Bits Benutzercode     (Stellen 8..0, drei Oktalziffern)
//
// Gesendet wird ab der NIEDRIGSTEN Stelle: C1 zuerst, P11 zuletzt.
//
// Zwei Eigenschaften davon bestimmen den ganzen Bau des Detektors:
//
//   * Es gibt KEINE Synchronbits. Der Empfaenger muss die Wortgrenze
//     selbst finden.
//   * Golay(23,12) ist ein ZYKLISCHER Code: verschiebt man ein gueltiges
//     Codewort ringfoermig, ist das Ergebnis wieder ein gueltiges
//     Codewort. Die Pruefsumme taugt darum NICHT, um die Wortgrenze zu
//     finden -- sie stimmt in allen 23 Lagen. Der einzige Anker sind die
//     drei festen Bits.
//
// LSB zuerst: das zuerst gesendete Bit ist das niederwertigste. Wer das
// uebersieht, liest jeden Code rueckwaerts.

#include <QVector>

#include <cstdint>

namespace Longpath {

/// Laenge eines DCS-Wortes in Bit.
inline constexpr int kDcsWordBits = 23;
/// Bits des Benutzercodes (drei Oktalziffern).
inline constexpr int kDcsCodeBits = 9;
/// Die drei festen Bits zwischen Code und Pruefbits. Das Wort lautet
/// geschrieben P11..P1 - 100 - C9..C1, die festen Bits liegen also auf
/// den Stellen 11, 10, 9 und heissen dort 1, 0, 0 -- als Feld ueber
/// Stelle 9 gelesen ist das 0b100.
inline constexpr uint32_t kDcsFixedBits = 0b100;
/// Bitrate des Datenstroms.
inline constexpr double kDcsBitRateHz = 134.4;

/// Die Golay(23,12)-Pruefbits zu `data12`. Generatorpolynom 0xC75
/// (x^11 + x^10 + x^6 + x^5 + x^4 + x^2 + 1).
uint32_t dcsGolayParity(uint32_t data12);

/// Baut das vollstaendige 23-Bit-Wort zu einem 9-Bit-Benutzercode.
/// Bit 0 des Ergebnisses ist das zuerst gesendete Bit.
uint32_t dcsEncode(uint32_t code9);

/// Liest ein empfangenes 23-Bit-Wort. Liefert den 9-Bit-Benutzercode,
/// oder -1, wenn das Wort auch nach der Fehlerkorrektur keines ist oder
/// die drei festen Bits nicht stimmen.
///
/// Golay(23,12) korrigiert bis zu drei falsche Bits; `korrigiert` nimmt
/// auf Wunsch die Anzahl auf.
int dcsDecode(uint32_t word23, int* korrigiert = nullptr);

/// Die gebraeuchlichen DCS-Codes als Oktalzahlen (23, 25, ... 754).
const QVector<int>& dcsStandardCodes();

/// Oktaldarstellung ("023") eines 9-Bit-Codes.
uint32_t dcsOctalToCode(int oktal);
/// Der umgekehrte Weg.
int dcsCodeToOctal(uint32_t code9);

/// Ein Wort ringfoermig um `n` Stellen nach links drehen (23 Bit breit).
uint32_t dcsRotate(uint32_t word23, int n);

/// Wieviele der 23 Bits stimmen ueberein? 23 heisst gleich, 0 heisst
/// invers.
int dcsAgreement(uint32_t a, uint32_t b);

/// Die Polaritaet, mit der ein Sender das Wort auf den Traeger legt.
/// "Invertiert" heisst: jedes Bit gekippt. Geraete fuehren das als
/// N und I hinter dem Code ("023N", "023I").
enum class DcsPolarity : int { Normal = 0, Inverted = 1 };

/// Das Wort, das ein Sender mit diesem Code und dieser Polaritaet
/// ausgibt.
uint32_t dcsExpectedWord(int oktal, DcsPolarity polarity);

/// Welcher Code sieht invertiert aus wie `oktal` normal? Das ist keine
/// Eigenheit der Umsetzung, sondern von DCS selbst -- eine
/// veroeffentlichte Angabe lautet, "+023 erzeugt dasselbe Muster wie
/// -047", und genau das rechnet diese Funktion aus. Liefert -1, wenn es
/// keinen Partner in der Standardliste gibt.
int dcsInversePartner(int oktal);

} // namespace Longpath
