// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - SpotTableModel: QAbstractTableModel wrapping a bounded
// QVector<DxSpot> with 8 columns (Time, Freq, DxCall, Comment, Spotter,
// Band, Mode, Source). Newest spot at row 0; bounded at 500 (default).
// addSpot / addSpots / clear / setMaxSpots / freqAtRow API. Used by the
// DX cluster + spot-collector + RBN + WSJT-X + POTA + FreeDV + PSK
// dialogs (Phase F SpotHubDialog).
//
// Ported from AetherSDR src/gui/DxClusterDialog.h:33-58 [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task D2. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". Both
//                                    SpotTableModel and BandFilterProxy
//                                    lived inline in AetherSDR's
//                                    DxClusterDialog.h:33-75 (with
//                                    implementations in
//                                    DxClusterDialog.cpp:75-226);
//                                    extracted to standalone src/models/
//                                    files so the SpotHubDialog (Phase F)
//                                    can reuse them. 8-column enum
//                                    (ColTime, ColFreq, ColDxCall,
//                                    ColComment, ColSpotter, ColBand,
//                                    ColMode, ColSource, ColCount)
//                                    preserved verbatim. Public surface
//                                    (addSpot, addSpots, clear,
//                                    setMaxSpots, freqAtRow, extractMode,
//                                    rowCount, columnCount, data,
//                                    headerData) preserved verbatim.
//                                    Default cap of 500 spots
//                                    (m_maxSpots{500}) preserved
//                                    verbatim. AI tooling: Anthropic
//                                    Claude Code.
//   2026-08-26  AI (Anthropic Claude Code)  NereusSDR-native extension
//                                    (SpotHub POTA improvement pass, no
//                                    upstream equivalent). Appends
//                                    ColReference / ColEntity (10
//                                    columns total, was 8) showing the
//                                    new DxSpot::reference / ::entity
//                                    fields; empty for sources that
//                                    don't set them. Inserted before
//                                    ColSource so ColSource stays the
//                                    last column (SpotHubDialog's Spot
//                                    List table stretches the last
//                                    section). Also adds
//                                    setWatchTerms()/setWatchColor():
//                                    a chaser "watchlist" of callsigns
//                                    and/or park/summit references
//                                    that, when matched against a
//                                    row's DxCall or Reference
//                                    (case-insensitive), tints that
//                                    row's BackgroundRole with an
//                                    operator-chosen color (picked via
//                                    a swatch button in SpotHubDialog,
//                                    same pattern as the existing
//                                    per-source spot-color pickers --
//                                    color choice stays the operator's,
//                                    not hardcoded here).
//   2026-08-27  AI (Anthropic Claude Code)  NereusSDR-native extension
//                                    (operator-requested follow-up).
//                                    Appends ColDistance / ColBearing,
//                                    computed from DxSpot::grid and an
//                                    operator grid square set via
//                                    setOurGridSquare() -- using the
//                                    existing Maidenhead.h helpers
//                                    (already shared with
//                                    FreeDVStationModel and the rotor
//                                    dial, see that header). Empty for
//                                    rows without a grid or before the
//                                    operator's grid is known. Mirrors
//                                    the FreeDvReporter/DistanceMiles
//                                    and FreeDvReporter/
//                                    DirectionAsCardinal AppSettings
//                                    keys so both features honor the
//                                    same unit/format preference
//                                    rather than introducing a second
//                                    one.

#pragma once

#include <QAbstractTableModel>
#include <QColor>
#include <QStringList>
#include <QVector>

#include "core/DxSpot.h"

namespace Longpath {

// From AetherSDR src/gui/DxClusterDialog.h:33-58 [@0cd4559]
class SpotTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { ColTime, ColFreq, ColDxCall, ColComment, ColSpotter, ColBand, ColMode, ColReference, ColEntity, ColDistance, ColBearing, ColSource, ColCount };
    static QString extractMode(const QString& comment);

    explicit SpotTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex& = {}) const override { return m_spots.size(); }
    int columnCount(const QModelIndex& = {}) const override { return ColCount; }
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void addSpot(const DxSpot& spot);
    void addSpots(const QVector<DxSpot>& spots);
    void clear();
    void setMaxSpots(int max) { m_maxSpots = max; }
    double freqAtRow(int row) const;

    // NereusSDR-native watchlist highlight (see modification history
    // above). `terms` is matched case-insensitively against each row's
    // DxCall and Reference; an invalid `color` (default) disables
    // highlighting even if terms are set.
    void setWatchTerms(const QStringList& terms);
    void setWatchColor(const QColor& color) { m_watchColor = color; }

    // NereusSDR-native (2026-08-27). The operator's own Maidenhead
    // locator ("User/GridSquare" in AppSettings), needed to compute
    // ColDistance/ColBearing. Empty (the default) means those columns
    // stay blank. Set once from AppSettings when the Spot List tab is
    // built and again whenever the Settings tab saves a new grid --
    // see SpotHubDialog. Refreshes every already-inserted row so a
    // grid change updates existing rows, not just future ones.
    void setOurGridSquare(const QString& grid);

private:
    static QString bandForFreq(double mhz);
    bool matchesWatchTerms(const DxSpot& spot) const;
    QString formatDistance(const DxSpot& spot) const;
    QString formatBearing(const DxSpot& spot) const;

    QVector<DxSpot> m_spots;
    int m_maxSpots{500};
    QStringList m_watchTerms;
    QColor m_watchColor;
    QString m_ourGrid;
};

} // namespace Longpath
