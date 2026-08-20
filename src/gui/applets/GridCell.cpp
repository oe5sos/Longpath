// =================================================================
// src/gui/applets/GridCell.cpp  (NereusSDR)
// =================================================================
// Siehe GridCell.h — ein Feld ist ein Behaelter, kein Widget.
// =================================================================

#include "gui/applets/GridCell.h"

namespace Longpath {

QVariantMap GridCell::toVariant() const
{
    QVariantMap m;
    m.insert(QStringLiteral("row"),     row);
    m.insert(QStringLiteral("col"),     col);
    m.insert(QStringLiteral("rowSpan"), rowSpan);
    m.insert(QStringLiteral("colSpan"), colSpan);
    m.insert(QStringLiteral("applets"), applets);
    if (!title.isEmpty()) { m.insert(QStringLiteral("title"), title); }
    if (locked)           { m.insert(QStringLiteral("locked"), true); }
    return m;
}

GridCell GridCell::fromVariant(const QString& id, const QVariantMap& m)
{
    GridCell c;
    c.id      = id;
    c.row     = m.value(QStringLiteral("row")).toInt();
    c.col     = m.value(QStringLiteral("col")).toInt();
    // Vorgabe 1, nicht 0: eine Aufnahme ohne Spannweite meint ein
    // einfaches Feld. Ein 0 breites Feld waere unsichtbar, und der
    // Bediener suchte danach.
    c.rowSpan = qMax(1, m.value(QStringLiteral("rowSpan"), 1).toInt());
    c.colSpan = qMax(1, m.value(QStringLiteral("colSpan"), 1).toInt());
    c.applets = m.value(QStringLiteral("applets")).toStringList();
    c.title   = m.value(QStringLiteral("title")).toString();
    c.locked  = m.value(QStringLiteral("locked"), false).toBool();
    return c;
}

} // namespace Longpath
