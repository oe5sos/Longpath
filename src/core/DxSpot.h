// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - DX spot value type
//
// Ported from AetherSDR src/core/DxClusterClient.h:13-23 [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B1. Extracted DxSpot
//                                    from AetherSDR's DxClusterClient.h
//                                    so that multiple spot-ingest clients
//                                    (SpotCollector, RBN, WSJT-X, POTA,
//                                    FreeDV, PSK) can share the type
//                                    without pulling in DX-cluster code.
//                                    Added "source" comment to list the
//                                    full set of expected source labels.
//                                    AI tooling: Anthropic Claude Code.
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task B4. Added
//                                    Q_DECLARE_METATYPE so DxSpot can flow
//                                    through QSignalSpy in the
//                                    tst_wsjtx_decoder test (the WSJT-X
//                                    parser tests are the first ones that
//                                    spy on `spotReceived(DxSpot)` rather
//                                    than calling a parser seam
//                                    synchronously).
//   2026-08-26  AI (Anthropic Claude Code)  NereusSDR-native extension
//                                    (SpotHub POTA improvement pass, no
//                                    upstream equivalent). Added
//                                    `reference` and `entity` so
//                                    activation-style sources (POTA)
//                                    don't have to flatten their park
//                                    reference into the free-text
//                                    `comment` string. `entity` is the
//                                    reference's location prefix (POTA
//                                    "US-4558" -> "US"), giving
//                                    SpotTableModel/BandFilterProxy a
//                                    clean column to filter on
//                                    (mirrors DX-cluster's DXCC concept
//                                    for activation spots). Both are
//                                    empty for sources that have no
//                                    such reference (Cluster, RBN,
//                                    WSJT-X, SpotCollector, FreeDV,
//                                    PSK).
//   2026-09-03  AI (Anthropic Claude Code)  SOTA is connected now (see
//                                    SotaClient.h) -- the note above
//                                    about it being "scoped but not yet
//                                    connected" no longer applies. SOTA
//                                    references split on '/' rather
//                                    than POTA's '-' (SotaClient derives
//                                    `entity` accordingly); `grid` comes
//                                    from a lat/lon-to-Maidenhead
//                                    conversion instead of a ready-made
//                                    field, since SOTA's API gives
//                                    coordinates, not a locator.
//   2026-08-27  AI (Anthropic Claude Code)  NereusSDR-native extension
//                                    (operator-requested follow-up).
//                                    Added `grid` (Maidenhead locator
//                                    of the spotted park, "grid6" from
//                                    the POTA API -- already present
//                                    in every live spot, no extra
//                                    lookup needed) so SpotTableModel
//                                    can show distance/bearing from
//                                    the operator's own grid using the
//                                    existing Maidenhead.h helpers
//                                    (same maths FreeDVReporterDialog
//                                    already uses for station
//                                    distance/heading).

#pragma once

#include <QMetaType>
#include <QString>
#include <QTime>

namespace Longpath {

// From AetherSDR src/core/DxClusterClient.h:13-23 [@0cd4559]
struct DxSpot {
    QString spotterCall;     // W3LPL
    double  freqMhz{0.0};    // 14.025 (converted from kHz)
    QString dxCall;          // JA1ABC
    QString comment;         // "CW big signal"
    QTime   utcTime;         // 18:24 UTC
    QString source;          // "Cluster", "RBN", "WSJT-X", "SpotCollector", "POTA", "FreeDV", "PSK"
    QString color;           // #AARRGGBB for radio spot color (optional)
    int     snr{0};          // signal-to-noise ratio (dB)
    int     lifetimeSec{0};  // 0 = use source default from AppSettings
    QString reference;       // park/summit reference, e.g. "US-4558" (empty if source has none)
    QString entity;          // reference's location prefix, e.g. "US" (empty if source has none)
    QString grid;            // Maidenhead locator of the spot, e.g. "DM52ph" (empty if source has none)
};

}  // namespace Longpath

Q_DECLARE_METATYPE(Longpath::DxSpot)
