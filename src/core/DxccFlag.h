#pragma once

// =================================================================
// src/core/DxccFlag.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// A DXCC entity is not a country. Alaska, Hawaii and the continental
// United States are three entities under one flag; Sardinia is a
// separate entity but Italian; Sovereign Military Order of Malta has
// no national flag at all. So this is a curated lookup, not a
// derivation — and an entity that is not in the table gets NO flag
// rather than a guessed one. A wrong flag beside a callsign is worse
// than a blank space: it is confidently incorrect, and the operator
// has no reason to doubt it.
//
// Flags are emoji (two regional-indicator code points), so nothing has
// to be bundled or licensed and the system font renders them.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include <QString>

namespace NereusSDR {

// Emoji flag for a cty.dat primary prefix ("G", "OE", "VK"), or an
// empty string when the entity has no unambiguous national flag or is
// not in the table.
QString dxccFlagEmoji(const QString& primaryPrefix);

// ISO 3166-1 alpha-2 for the prefix, or empty. Exposed separately
// because a future flag-image path would want the code, not the emoji.
QString dxccIsoCode(const QString& primaryPrefix);

} // namespace NereusSDR
