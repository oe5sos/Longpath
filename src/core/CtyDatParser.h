// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - CtyDatParser: AD1C / K1EA cty.dat country-file parser.
//
// Ported from AetherSDR src/core/CtyDatParser.h [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task C1. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". DxccEntity
//                                    fields (primaryPrefix, name,
//                                    continent, cqZone, ituZone) and
//                                    the public surface (loadFromFile,
//                                    loadFromResource,
//                                    resolvePrimaryPrefix,
//                                    entityByPrefix, entityCount,
//                                    isLoaded) follow upstream
//                                    byte-for-byte. AI tooling:
//                                    Anthropic Claude Code.

#pragma once

#include <QString>
#include <QHash>
#include <QSet>

namespace Longpath {

// From AetherSDR src/core/CtyDatParser.h:9-15 [@0cd4559]
struct DxccEntity {
    QString primaryPrefix;   // e.g. "G"
    QString name;            // e.g. "England"
    QString continent;       // e.g. "EU"
    int     cqZone{0};
    int     ituZone{0};

    // Entity centre, from cty.dat fields 5 and 6. Captured but
    // previously discarded by the header regex; the rotator dial needs
    // them to show a bearing the moment a callsign is typed, before any
    // network lookup could return a real locator. Country-level
    // accuracy only — good enough to know which way to turn, not good
    // enough to log.
    //
    // NOTE ON SIGN: cty.dat writes longitude POSITIVE WEST (so England
    // is +0.00, New York is +73.00). Everything else in this codebase —
    // and the Maidenhead helpers — uses positive east. The parser flips
    // it on read, so this field is positive east like everywhere else.
    double  latitude{0.0};
    double  longitude{0.0};
    bool    hasLatLon{false};
};

// From AetherSDR src/core/CtyDatParser.h:17-49 [@0cd4559]
//
// ---------------------------------------------------------------------------
// CtyDatParser
//
// Parses the AD1C cty.dat file and resolves callsigns to DXCC entities via
// longest-prefix matching.  Load once at startup; queries are O(prefix_len).
// ---------------------------------------------------------------------------
class CtyDatParser {
public:
    // Returns true on success.
    bool loadFromFile(const QString& path);
    bool loadFromResource(const QString& resourcePath);   // e.g. ":/cty.dat"

    // Resolve callsign -> primary prefix of matched entity ("G", "VK", …)
    // Returns empty string if no match.
    QString resolvePrimaryPrefix(const QString& callsign) const;

    // Look up entity details by primary prefix.
    const DxccEntity* entityByPrefix(const QString& primaryPrefix) const;

    int entityCount() const { return m_entityByPrefix.size(); }
    bool isLoaded()   const { return !m_entityByPrefix.isEmpty(); }

private:
    void parse(const QStringList& lines);

    // exact-match table:  "=VK9XX"  -> primaryPrefix
    QHash<QString, QString> m_exactMatch;
    // prefix table: "VK9X" -> primaryPrefix
    QHash<QString, QString> m_prefixTable;
    // entity details keyed by primaryPrefix
    QHash<QString, DxccEntity> m_entityByPrefix;
    int m_maxPrefixLen{0};
};

} // namespace Longpath
