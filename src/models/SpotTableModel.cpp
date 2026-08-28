// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - SpotTableModel implementation. 8-column QAbstractTableModel
// over a bounded QVector<DxSpot>; bandForFreq() maps MHz to band labels
// using the IARU Region 2 amateur band edges (160m..2m).
//
// Ported from AetherSDR src/gui/DxClusterDialog.cpp:75-204 [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task D2. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". Implementation
//                                    extracted from
//                                    DxClusterDialog.cpp:75-204 verbatim:
//                                    extractMode (known mode-token set
//                                    of 20 entries, first-or-last
//                                    word match), data (DisplayRole +
//                                    TextAlignmentRole + ForegroundRole
//                                    + UserRole-on-ColFreq for sort key),
//                                    headerData (8 column labels), addSpot
//                                    (prepend + cap-trim), addSpots
//                                    (reverse-prepend keeps newest first
//                                    + cap-trim), freqAtRow (bounds-checked
//                                    accessor), clear (model-reset), and
//                                    bandForFreq (160m..2m IARU lookup).
//                                    Foreground colours (DxCall accent
//                                    #00B4D8 cyan, Freq #E0D060
//                                    yellow-ish) preserved verbatim.
//                                    AI tooling: Anthropic Claude Code.

#include "SpotTableModel.h"
#include "core/AppSettings.h"
#include "core/Maidenhead.h"

#include <QColor>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTime>

namespace Longpath {

// From AetherSDR src/gui/DxClusterDialog.cpp:75-88 [@0cd4559]
QString SpotTableModel::extractMode(const QString& comment)
{
    static const QSet<QString> known = {
        "CW", "SSB", "USB", "LSB", "AM", "FM", "FT8", "FT4",
        "JS8", "RTTY", "PSK31", "PSK63", "PSK", "OLIVIA",
        "JT65", "JT9", "SAM", "NFM", "DIGU", "DIGL"
    };
    QStringList words = comment.split(' ', Qt::SkipEmptyParts);
    if (!words.isEmpty() && known.contains(words.first().toUpper()))
        return words.first().toUpper();
    if (!words.isEmpty() && known.contains(words.last().toUpper()))
        return words.last().toUpper();
    return {};
}

// From AetherSDR src/gui/DxClusterDialog.cpp:90-126 [@0cd4559]
QVariant SpotTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_spots.size())
        return {};

    const auto& spot = m_spots[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColTime:      return spot.utcTime.toString("HH:mm");
        case ColFreq:      return QString::number(spot.freqMhz * 1000.0, 'f', 1);
        case ColDxCall:    return spot.dxCall;
        case ColMode:      return extractMode(spot.comment);
        case ColComment:   return spot.comment;
        case ColSpotter:   return spot.spotterCall;
        case ColBand:      return bandForFreq(spot.freqMhz);
        case ColReference: return spot.reference;
        case ColEntity:    return spot.entity;
        case ColDistance:  return formatDistance(spot);
        case ColBearing:   return formatBearing(spot);
        case ColSource:    return spot.source;
        }
    }
    if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColFreq)
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        if (index.column() == ColTime)
            return QVariant(Qt::AlignCenter);
    }
    if (role == Qt::ForegroundRole) {
        if (index.column() == ColDxCall) {
            // NereusSDR-native (2026-08-27): the fixed accent color
            // collides with the default watchlist highlight color
            // (both #00b4d8), making the call sign unreadable in a
            // matched row -- found on first live test. Swap to the
            // dark text already used elsewhere in this dialog for
            // text-on-accent-background (kStartBtnStyle,
            // kFilterPillStyle:checked in SpotHubDialog.cpp) instead
            // of inventing a new color; the highlight color itself
            // stays the operator's choice via the swatch button.
            if (m_watchColor.isValid() && matchesWatchTerms(spot))
                return QColor(0x0f, 0x0f, 0x1a);
            return QColor(0x00, 0xb4, 0xd8);  // accent
        }
        if (index.column() == ColFreq)
            return QColor(0xe0, 0xd0, 0x60);  // yellow-ish
    }
    // NereusSDR-native (2026-08-26): watchlist row highlight. Returned
    // for every column of a matching row (the view queries per-cell),
    // so the whole row tints -- not just one field. The color itself
    // is operator-chosen (see SpotTableModel.h); an invalid m_watchColor
    // means "no highlight configured" and this is a no-op.
    if (role == Qt::BackgroundRole && m_watchColor.isValid() && matchesWatchTerms(spot))
        return m_watchColor;
    // Store freq in UserRole for sorting
    if (role == Qt::UserRole && index.column() == ColFreq)
        return spot.freqMhz;

    return {};
}

// From AetherSDR src/gui/DxClusterDialog.cpp:128-143 [@0cd4559]
QVariant SpotTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColTime:      return "Time";
    case ColFreq:      return "Freq (kHz)";
    case ColDxCall:    return "DX Call";
    case ColMode:      return "Mode";
    case ColComment:   return "Comment";
    case ColSpotter:   return "Spotter";
    case ColBand:      return "Band";
    case ColReference: return "Ref";
    case ColEntity:    return "Entity";
    case ColDistance:  return "Dist";
    case ColBearing:   return "Brg";
    case ColSource:    return "Source";
    }
    return {};
}

// From AetherSDR src/gui/DxClusterDialog.cpp:145-156 [@0cd4559]
void SpotTableModel::addSpot(const DxSpot& spot)
{
    beginInsertRows({}, 0, 0);
    m_spots.prepend(spot);
    endInsertRows();

    if (m_spots.size() > m_maxSpots) {
        beginRemoveRows({}, m_maxSpots, m_spots.size() - 1);
        m_spots.resize(m_maxSpots);
        endRemoveRows();
    }
}

// From AetherSDR src/gui/DxClusterDialog.cpp:158-173 [@0cd4559]
void SpotTableModel::addSpots(const QVector<DxSpot>& spots)
{
    if (spots.isEmpty()) return;
    int count = spots.size();
    beginInsertRows({}, 0, count - 1);
    // Prepend in reverse so newest is at index 0
    for (int i = count - 1; i >= 0; --i)
        m_spots.prepend(spots[i]);
    endInsertRows();

    if (m_spots.size() > m_maxSpots) {
        beginRemoveRows({}, m_maxSpots, m_spots.size() - 1);
        m_spots.resize(m_maxSpots);
        endRemoveRows();
    }
}

// From AetherSDR src/gui/DxClusterDialog.cpp:175-180 [@0cd4559]
double SpotTableModel::freqAtRow(int row) const
{
    if (row >= 0 && row < m_spots.size())
        return m_spots[row].freqMhz;
    return 0.0;
}

// From AetherSDR src/gui/DxClusterDialog.cpp:182-187 [@0cd4559]
void SpotTableModel::clear()
{
    beginResetModel();
    m_spots.clear();
    endResetModel();
}

// From AetherSDR src/gui/DxClusterDialog.cpp:189-204 [@0cd4559]
QString SpotTableModel::bandForFreq(double mhz)
{
    if (mhz >= 1.8   && mhz <= 2.0)    return "160m";
    if (mhz >= 3.5   && mhz <= 4.0)    return "80m";
    if (mhz >= 5.0   && mhz <= 5.5)    return "60m";
    if (mhz >= 7.0   && mhz <= 7.3)    return "40m";
    if (mhz >= 10.1  && mhz <= 10.15)  return "30m";
    if (mhz >= 14.0  && mhz <= 14.35)  return "20m";
    if (mhz >= 18.068 && mhz <= 18.168) return "17m";
    if (mhz >= 21.0  && mhz <= 21.45)  return "15m";
    if (mhz >= 24.89 && mhz <= 24.99)  return "12m";
    if (mhz >= 28.0  && mhz <= 29.7)   return "10m";
    if (mhz >= 50.0  && mhz <= 54.0)   return "6m";
    if (mhz >= 144.0 && mhz <= 148.0)  return "2m";
    return "";
}

// NereusSDR-native (2026-08-27).
void SpotTableModel::setOurGridSquare(const QString& grid)
{
    m_ourGrid = grid;
    if (rowCount() > 0) {
        emit dataChanged(index(0, ColDistance), index(rowCount() - 1, ColBearing),
                          {Qt::DisplayRole});
    }
}

// NereusSDR-native (2026-08-27). Reuses the exact same haversine
// distance/bearing maths FreeDVReporterDialog already uses for
// station distance/heading (src/core/Maidenhead.h), applied to
// DxSpot::grid instead of a FreeDV station's grid square. Empty
// whenever either grid is unknown.
QString SpotTableModel::formatDistance(const DxSpot& spot) const
{
    if (m_ourGrid.isEmpty() || spot.grid.isEmpty()) {
        return {};
    }
    const double km = calculateDistanceKm(m_ourGrid, spot.grid);
    if (km <= 0.0) {
        return {};
    }
    // Mirrors FreeDVReporterDialog::formatDistance's miles/km toggle so
    // both features honor the same operator preference.
    const bool miles = AppSettings::instance()
        .value("FreeDvReporter/DistanceMiles", "False").toString() == "True";
    const double value = miles ? km * 0.621371 : km;
    return QString::number(static_cast<int>(value + 0.5));
}

QString SpotTableModel::formatBearing(const DxSpot& spot) const
{
    if (m_ourGrid.isEmpty() || spot.grid.isEmpty()) {
        return {};
    }
    const double deg = calculateBearingInDegrees(m_ourGrid, spot.grid);
    // A spot exactly on top of the operator's own grid (deg == 0 from
    // calculateBearingInDegrees's atan2 branch) is indistinguishable
    // from "unknown" here, but that's the same ambiguity
    // FreeDVReporterDialog::formatHeading already accepts.
    if (deg <= 0.0) {
        return {};
    }
    static const char* const kCardinal[] = {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
    };
    const int idx = static_cast<int>((deg / 360.0) * 16 + 0.5) % 16;
    return QString("%1\xC2\xB0 %2")
        .arg(static_cast<int>(deg + 0.5), 3, 10, QLatin1Char('0'))
        .arg(kCardinal[idx]);
}

// NereusSDR-native (2026-08-26). Normalizes to trimmed, non-empty terms
// so matchesWatchTerms() doesn't need to re-check per row.
void SpotTableModel::setWatchTerms(const QStringList& terms)
{
    m_watchTerms.clear();
    for (const QString& term : terms) {
        const QString trimmed = term.trimmed();
        if (!trimmed.isEmpty())
            m_watchTerms.append(trimmed);
    }
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, ColCount - 1), {Qt::BackgroundRole});
}

// NereusSDR-native (2026-08-26). Case-insensitive exact match against
// DxCall or Reference -- exact rather than substring so a watched call
// like "OE5" doesn't also light up every "OE5xyz/P" spot.
bool SpotTableModel::matchesWatchTerms(const DxSpot& spot) const
{
    if (m_watchTerms.isEmpty())
        return false;
    for (const QString& term : m_watchTerms) {
        if (spot.dxCall.compare(term, Qt::CaseInsensitive) == 0)
            return true;
        if (!spot.reference.isEmpty() && spot.reference.compare(term, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

} // namespace Longpath
