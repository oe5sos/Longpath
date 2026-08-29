// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - BandFilterProxy: QSortFilterProxyModel that hides spots
// whose band (as reported by SpotTableModel's ColBand DisplayRole) is in
// the m_hiddenBands set. setBandVisible(band, visible) toggles set
// membership and reinvalidates the filter. Empty / unknown band always
// shows.
//
// Ported from AetherSDR src/gui/DxClusterDialog.h:62-75 [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task D2. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". Class lived
//                                    inline in AetherSDR's
//                                    DxClusterDialog.h:62-75 (with
//                                    setBandVisible / filterAcceptsRow
//                                    in DxClusterDialog.cpp:208-226);
//                                    extracted to standalone src/models/
//                                    files so the SpotHubDialog (Phase
//                                    F) can reuse it. Public surface
//                                    (setBandVisible, isBandVisible)
//                                    and protected filterAcceptsRow
//                                    override preserved verbatim.
//                                    Filter looks up the band string
//                                    via SpotTableModel::ColBand
//                                    DisplayRole; empty band string
//                                    always shows. AI tooling:
//                                    Anthropic Claude Code.
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 Task F3. Adds source
//                                    filtering as a NereusSDR-native
//                                    extension (AetherSDR upstream
//                                    proxies bands only). New public
//                                    surface: setSourceVisible(source,
//                                    visible) and isSourceVisible(source)
//                                    mirror the band API. The proxy
//                                    looks up source via
//                                    SpotTableModel::ColSource
//                                    DisplayRole and applies both
//                                    band and source predicates in
//                                    filterAcceptsRow (AND semantics:
//                                    a row must pass both filters to
//                                    be visible). Empty source string
//                                    always shows (matches band
//                                    convention). The Spot List tab
//                                    drives this from 7 source-pill
//                                    QPushButtons (DX / RBN / JT / COL
//                                    / POT / FDR / PSK). AI tooling:
//                                    Anthropic Claude Code.
//   2026-08-26  AI (Anthropic Claude Code)  NereusSDR-native extension
//                                    (SpotHub POTA improvement pass, no
//                                    upstream equivalent). Adds
//                                    setEntityVisible()/isEntityVisible(),
//                                    a third predicate mirroring
//                                    band/source but over
//                                    SpotTableModel::ColEntity (the
//                                    location prefix of a POTA/SOTA
//                                    reference, e.g. "US"). Unlike the
//                                    fixed band/source lists, entities
//                                    are open-ended and discovered at
//                                    runtime, so SpotHubDialog builds
//                                    the filter UI dynamically as new
//                                    entities are seen rather than from
//                                    a fixed pill row. AND semantics
//                                    with the existing two predicates,
//                                    same as source joining band.
//   2026-08-27  AI (Anthropic Claude Code)  NereusSDR-native extension
//                                    (operator-requested follow-up).
//                                    Adds setModeVisible()/isModeVisible(),
//                                    a fourth predicate over
//                                    SpotTableModel::ColMode, same
//                                    "empty always shows" / dynamic-
//                                    discovery treatment as entity
//                                    (modes are open-ended in practice
//                                    -- extractMode() recognizes 20
//                                    tokens -- so SpotHubDialog
//                                    populates the filter menu as
//                                    modes are actually seen, same
//                                    pattern as the entity menu).
//   2026-08-27  AI (Anthropic Claude Code)  NereusSDR-native extension
//                                    (operator-requested follow-up,
//                                    pattern taken from SOTAwatch3's
//                                    free-text "FILTER..." box). Adds
//                                    setSearchText(): a single
//                                    substring predicate (case-
//                                    insensitive) over DxCall,
//                                    Reference, Comment, and Spotter,
//                                    ORed together -- unlike the other
//                                    four predicates (which are exact-
//                                    match hidden-sets ANDed with
//                                    everything else), this is a loose
//                                    quick-search meant to complement
//                                    the exact-match watchlist, not
//                                    replace it. Empty text always
//                                    shows, same convention as the
//                                    rest. Deliberately not persisted
//                                    to AppSettings -- it's a
//                                    transient quick-filter, not a
//                                    standing preference, matching
//                                    SOTAwatch3's own (non-persisted)
//                                    behavior.

#pragma once

#include <QSortFilterProxyModel>
#include <QSet>
#include <QString>
#include <QtGlobal>

namespace Longpath {

// From AetherSDR src/gui/DxClusterDialog.h:62-75 [@0cd4559]
class BandFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit BandFilterProxy(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setBandVisible(const QString& band, bool visible);
    bool isBandVisible(const QString& band) const { return !m_hiddenBands.contains(band); }

    // NereusSDR Task F3 extension. AetherSDR upstream filters bands
    // only; the SpotHub Spot List tab additionally pills by source
    // (Cluster / RBN / WSJT-X / SpotCollector / POTA / FreeDV / PSK).
    void setSourceVisible(const QString& source, bool visible);
    bool isSourceVisible(const QString& source) const { return !m_hiddenSources.contains(source); }

    // NereusSDR-native extension, added alongside the ColReference /
    // ColEntity columns (see modification history above).
    void setEntityVisible(const QString& entity, bool visible);
    bool isEntityVisible(const QString& entity) const { return !m_hiddenEntities.contains(entity); }

    // NereusSDR-native extension (2026-08-27), mirrors setEntityVisible.
    void setModeVisible(const QString& mode, bool visible);
    bool isModeVisible(const QString& mode) const { return !m_hiddenModes.contains(mode); }

    // NereusSDR-native extension (2026-08-27), see modification history
    // above. Not an exact-match hidden-set like the other predicates --
    // a single substring search across several columns.
    void setSearchText(const QString& text);
    QString searchText() const { return m_searchText; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

    // Qt 6.13 deprecated invalidateFilter() in favor of
    // beginFilterChange() / endFilterChange(Direction::Rows). Both APIs
    // exist in Qt 6.10+. NereusSDR has no explicit Qt minimum in
    // CMakeLists.txt, so we keep a fallback to invalidateFilter on
    // older Qt builds. NereusSDR-only compat shim; not an upstream port.
    void invalidateFilterCompat() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
        invalidateFilter();
#endif
    }

private:
    QSet<QString> m_hiddenBands;
    QSet<QString> m_hiddenSources;
    QSet<QString> m_hiddenEntities;
    QSet<QString> m_hiddenModes;
    QString m_searchText;
};

} // namespace Longpath
