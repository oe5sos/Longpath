// no-port-check: Longpath-original file; siehe den Kopf von DcsCode.h.

#include "core/DcsCode.h"

#include <array>

namespace Longpath {

namespace {

/// Generatorpolynom des Golay(23,12)-Codes:
/// x^11 + x^10 + x^6 + x^5 + x^4 + x^2 + 1.
constexpr uint32_t kGenerator = 0xC75;

/// Gewicht (Anzahl Einsen) eines Wortes.
int gewicht(uint32_t w)
{
    int n = 0;
    while (w) { w &= (w - 1); ++n; }
    return n;
}

/// Das Syndrom eines 23-Bit-Wortes: der Rest der Division durch das
/// Generatorpolynom, ueber GF(2).
uint32_t syndrom(uint32_t word23)
{
    uint32_t rest = word23 & 0x7FFFFF;
    for (int bit = 22; bit >= 11; --bit) {
        if (rest & (1u << bit)) {
            rest ^= kGenerator << (bit - 11);
        }
    }
    return rest & 0x7FF;
}

/// Ein Wort ringfoermig um `n` Stellen nach links drehen (23 Bit breit).
uint32_t drehen(uint32_t w, int n)
{
    w &= 0x7FFFFF;
    n %= kDcsWordBits;
    return ((w << n) | (w >> (kDcsWordBits - n))) & 0x7FFFFF;
}

} // namespace

uint32_t dcsGolayParity(uint32_t data12)
{
    // Systematische Kodierung: die Datenbits nach oben schieben und den
    // Rest der Polynomdivision als Pruefbits nehmen.
    return syndrom((data12 & 0xFFF) << 11);
}

uint32_t dcsEncode(uint32_t code9)
{
    // Sendereihenfolge: erst die 9 Codebits, dann die drei festen, dann
    // die 11 Pruefbits. Bit 0 ist das zuerst gesendete.
    const uint32_t data12 = (code9 & 0x1FF) | (kDcsFixedBits << kDcsCodeBits);
    const uint32_t parity = dcsGolayParity(data12);
    return (data12 & 0xFFF) | (parity << 12);
}

int dcsDecode(uint32_t word23, int* korrigiert)
{
    word23 &= 0x7FFFFF;
    if (korrigiert) { *korrigiert = 0; }

    // Fehlersuche nach dem klassischen Verfahren fuer zyklische Codes:
    // das Wort schrittweise drehen und sehen, ob das Syndrom ein
    // Fehlermuster mit hoechstens drei Bits ergibt -- dann liegen alle
    // Fehler im Pruefteil und lassen sich direkt abziehen.
    uint32_t korr = word23;
    bool gefunden = false;
    for (int i = 0; i < kDcsWordBits; ++i) {
        const uint32_t gedreht = drehen(word23, i);
        const uint32_t s = syndrom(gedreht);
        if (gewicht(s) <= 3) {
            const uint32_t repariert = gedreht ^ s;
            korr = drehen(repariert, kDcsWordBits - i);
            if (korrigiert) { *korrigiert = gewicht(s); }
            gefunden = true;
            break;
        }
    }
    if (!gefunden) {
        return -1;
    }
    if (syndrom(korr) != 0) {
        return -1;
    }

    // Die drei festen Bits sind der einzige Anker fuer die Wortgrenze --
    // das Syndrom taugt dafuer nicht, weil jede ringfoermige Verschiebung
    // eines Golay-Wortes wieder ein gueltiges Golay-Wort ist.
    const uint32_t fest = (korr >> kDcsCodeBits) & 0b111;
    if (fest != kDcsFixedBits) {
        return -1;
    }
    return static_cast<int>(korr & 0x1FF);
}

uint32_t dcsRotate(uint32_t word23, int n)
{
    return drehen(word23, n);
}

int dcsAgreement(uint32_t a, uint32_t b)
{
    return kDcsWordBits - gewicht((a ^ b) & 0x7FFFFF);
}

uint32_t dcsExpectedWord(int oktal, DcsPolarity polarity)
{
    const uint32_t wort = dcsEncode(dcsOctalToCode(oktal));
    return polarity == DcsPolarity::Inverted ? ((~wort) & 0x7FFFFF) : wort;
}

int dcsInversePartner(int oktal)
{
    const uint32_t invers = (~dcsEncode(dcsOctalToCode(oktal))) & 0x7FFFFF;
    for (int kandidat : dcsStandardCodes()) {
        const uint32_t w = dcsEncode(dcsOctalToCode(kandidat));
        for (int p = 0; p < kDcsWordBits; ++p) {
            if (drehen(w, p) == invers) {
                return kandidat;
            }
        }
    }
    return -1;
}

const QVector<int>& dcsStandardCodes()
{
    // Die 83 gebraeuchlichen Codes als Oktalzahlen. Dieselbe Liste, die
    // jedes Handfunkgeraet im Menue fuehrt.
    static const QVector<int> kCodes = {
         23,  25,  26,  31,  32,  36,  43,  47,  51,  53,  54,  65,  71,  72,
         73,  74, 114, 115, 116, 122, 125, 131, 132, 134, 143, 145, 152, 155,
        156, 162, 165, 172, 174, 205, 212, 223, 225, 226, 243, 244, 245, 246,
        251, 252, 255, 261, 263, 265, 266, 271, 274, 306, 311, 315, 325, 331,
        332, 343, 346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432,
        445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516, 523, 526,
        532, 546, 565, 606, 612, 624, 627, 631, 632, 654, 662, 664, 703, 712,
        723, 731, 732, 734, 743, 754};
    return kCodes;
}

uint32_t dcsOctalToCode(int oktal)
{
    // "023" sind drei Oktalziffern: 0, 2, 3 -> 000 010 011.
    uint32_t code = 0;
    int stelle = 0;
    int rest = oktal;
    while (stelle < 3) {
        code |= static_cast<uint32_t>(rest % 10) << (3 * stelle);
        rest /= 10;
        ++stelle;
    }
    return code & 0x1FF;
}

int dcsCodeToOctal(uint32_t code9)
{
    int oktal = 0;
    int faktor = 1;
    for (int stelle = 0; stelle < 3; ++stelle) {
        oktal += static_cast<int>((code9 >> (3 * stelle)) & 0b111) * faktor;
        faktor *= 10;
    }
    return oktal;
}

} // namespace Longpath
