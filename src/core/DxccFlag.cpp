// =================================================================
// src/core/DxccFlag.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see DxccFlag.h for why this is curated rather
// than derived.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "DxccFlag.h"

#include <QHash>

namespace NereusSDR {

namespace {

// cty.dat primary prefix → ISO 3166-1 alpha-2.
//
// Covers the entities a European station actually works day to day.
// Deliberately incomplete: adding a guess for every one of the ~340
// DXCC entities would put wrong flags on the rare ones, which are
// exactly the contacts an operator looks at closely.
//
// Where an entity is politically part of another country, the flag
// shown is the sovereign one (Sardinia → Italy, Corsica → France),
// because that is the flag a person would expect to see.
const QHash<QString, QString>& table()
{
    static const QHash<QString, QString> t = {
        // ── Europe ───────────────────────────────────────────────
        {QStringLiteral("OE"), QStringLiteral("AT")},  // Austria
        {QStringLiteral("ON"), QStringLiteral("BE")},
        {QStringLiteral("LZ"), QStringLiteral("BG")},
        {QStringLiteral("9A"), QStringLiteral("HR")},
        {QStringLiteral("5B"), QStringLiteral("CY")},
        {QStringLiteral("OK"), QStringLiteral("CZ")},
        {QStringLiteral("OZ"), QStringLiteral("DK")},
        {QStringLiteral("ES"), QStringLiteral("EE")},
        {QStringLiteral("OH"), QStringLiteral("FI")},
        {QStringLiteral("OH0"),QStringLiteral("AX")},  // Aland
        {QStringLiteral("F"),  QStringLiteral("FR")},
        {QStringLiteral("TK"), QStringLiteral("FR")},  // Corsica
        {QStringLiteral("DL"), QStringLiteral("DE")},
        {QStringLiteral("SV"), QStringLiteral("GR")},
        {QStringLiteral("HA"), QStringLiteral("HU")},
        {QStringLiteral("TF"), QStringLiteral("IS")},
        {QStringLiteral("EI"), QStringLiteral("IE")},
        {QStringLiteral("I"),  QStringLiteral("IT")},
        {QStringLiteral("IS0"),QStringLiteral("IT")},  // Sardinia
        {QStringLiteral("YL"), QStringLiteral("LV")},
        {QStringLiteral("HB0"),QStringLiteral("LI")},
        {QStringLiteral("LY"), QStringLiteral("LT")},
        {QStringLiteral("LX"), QStringLiteral("LU")},
        {QStringLiteral("9H"), QStringLiteral("MT")},
        {QStringLiteral("ER"), QStringLiteral("MD")},
        {QStringLiteral("3A"), QStringLiteral("MC")},
        {QStringLiteral("ZA"), QStringLiteral("AL")},
        {QStringLiteral("PA"), QStringLiteral("NL")},
        {QStringLiteral("LA"), QStringLiteral("NO")},
        {QStringLiteral("SP"), QStringLiteral("PL")},
        {QStringLiteral("CT"), QStringLiteral("PT")},
        {QStringLiteral("CT3"),QStringLiteral("PT")},  // Madeira
        {QStringLiteral("YO"), QStringLiteral("RO")},
        {QStringLiteral("UA"), QStringLiteral("RU")},
        {QStringLiteral("YU"), QStringLiteral("RS")},
        {QStringLiteral("OM"), QStringLiteral("SK")},
        {QStringLiteral("S5"), QStringLiteral("SI")},
        {QStringLiteral("EA"), QStringLiteral("ES")},
        {QStringLiteral("EA6"),QStringLiteral("ES")},  // Balearics
        {QStringLiteral("EA8"),QStringLiteral("ES")},  // Canaries
        {QStringLiteral("SM"), QStringLiteral("SE")},
        {QStringLiteral("HB"), QStringLiteral("CH")},
        {QStringLiteral("UR"), QStringLiteral("UA")},
        {QStringLiteral("G"),  QStringLiteral("GB")},
        {QStringLiteral("GM"), QStringLiteral("GB")},  // Scotland
        {QStringLiteral("GW"), QStringLiteral("GB")},  // Wales
        {QStringLiteral("GI"), QStringLiteral("GB")},  // N. Ireland
        {QStringLiteral("GJ"), QStringLiteral("JE")},
        {QStringLiteral("GU"), QStringLiteral("GG")},
        {QStringLiteral("GD"), QStringLiteral("IM")},
        {QStringLiteral("EU"), QStringLiteral("BY")},
        {QStringLiteral("E7"), QStringLiteral("BA")},
        {QStringLiteral("Z3"), QStringLiteral("MK")},
        {QStringLiteral("4O"), QStringLiteral("ME")},
        {QStringLiteral("ZB2"),QStringLiteral("GI")},

        // ── Americas ─────────────────────────────────────────────
        {QStringLiteral("K"),  QStringLiteral("US")},
        {QStringLiteral("KL"), QStringLiteral("US")},  // Alaska
        {QStringLiteral("KH6"),QStringLiteral("US")},  // Hawaii
        {QStringLiteral("VE"), QStringLiteral("CA")},
        {QStringLiteral("XE"), QStringLiteral("MX")},
        {QStringLiteral("PY"), QStringLiteral("BR")},
        {QStringLiteral("LU"), QStringLiteral("AR")},
        {QStringLiteral("CE"), QStringLiteral("CL")},
        {QStringLiteral("CX"), QStringLiteral("UY")},
        {QStringLiteral("HK"), QStringLiteral("CO")},
        {QStringLiteral("YV"), QStringLiteral("VE")},
        {QStringLiteral("OA"), QStringLiteral("PE")},
        {QStringLiteral("CP"), QStringLiteral("BO")},
        {QStringLiteral("ZP"), QStringLiteral("PY")},
        {QStringLiteral("HC"), QStringLiteral("EC")},
        {QStringLiteral("CO"), QStringLiteral("CU")},
        {QStringLiteral("KP4"),QStringLiteral("PR")},

        // ── Asia ─────────────────────────────────────────────────
        {QStringLiteral("JA"), QStringLiteral("JP")},
        {QStringLiteral("BY"), QStringLiteral("CN")},
        {QStringLiteral("BV"), QStringLiteral("TW")},
        {QStringLiteral("HL"), QStringLiteral("KR")},
        {QStringLiteral("VU"), QStringLiteral("IN")},
        {QStringLiteral("9V"), QStringLiteral("SG")},
        {QStringLiteral("HS"), QStringLiteral("TH")},
        {QStringLiteral("YB"), QStringLiteral("ID")},
        {QStringLiteral("DU"), QStringLiteral("PH")},
        {QStringLiteral("9M2"),QStringLiteral("MY")},
        {QStringLiteral("4X"), QStringLiteral("IL")},
        {QStringLiteral("TA"), QStringLiteral("TR")},
        {QStringLiteral("EP"), QStringLiteral("IR")},
        {QStringLiteral("A6"), QStringLiteral("AE")},
        {QStringLiteral("A7"), QStringLiteral("QA")},
        {QStringLiteral("9K"), QStringLiteral("KW")},
        {QStringLiteral("HZ"), QStringLiteral("SA")},
        {QStringLiteral("UN"), QStringLiteral("KZ")},
        {QStringLiteral("4L"), QStringLiteral("GE")},
        {QStringLiteral("EK"), QStringLiteral("AM")},
        {QStringLiteral("4J"), QStringLiteral("AZ")},

        // ── Africa ───────────────────────────────────────────────
        {QStringLiteral("ZS"), QStringLiteral("ZA")},
        {QStringLiteral("SU"), QStringLiteral("EG")},
        {QStringLiteral("CN"), QStringLiteral("MA")},
        {QStringLiteral("7X"), QStringLiteral("DZ")},
        {QStringLiteral("3V"), QStringLiteral("TN")},
        {QStringLiteral("5Z"), QStringLiteral("KE")},
        {QStringLiteral("5N"), QStringLiteral("NG")},
        {QStringLiteral("ET"), QStringLiteral("ET")},
        {QStringLiteral("V5"), QStringLiteral("NA")},
        {QStringLiteral("CT3M"),QStringLiteral("PT")},

        // ── Oceania ──────────────────────────────────────────────
        {QStringLiteral("VK"), QStringLiteral("AU")},
        {QStringLiteral("ZL"), QStringLiteral("NZ")},
        {QStringLiteral("KH2"),QStringLiteral("GU")},
        {QStringLiteral("3D2"),QStringLiteral("FJ")},
    };
    return t;
}

} // namespace

QString dxccIsoCode(const QString& primaryPrefix)
{
    return table().value(primaryPrefix.trimmed().toUpper());
}

QString dxccFlagEmoji(const QString& primaryPrefix)
{
    const QString iso = dxccIsoCode(primaryPrefix);
    if (iso.size() != 2) { return {}; }

    // Regional indicator symbols: 'A' maps to U+1F1E6, and a pair of
    // them is rendered as a single flag glyph by the system font.
    const char32_t pair[2] = {
        static_cast<char32_t>(0x1F1E6 + (iso.at(0).toLatin1() - 'A')),
        static_cast<char32_t>(0x1F1E6 + (iso.at(1).toLatin1() - 'A')),
    };
    return QString::fromUcs4(pair, 2);
}

} // namespace NereusSDR
