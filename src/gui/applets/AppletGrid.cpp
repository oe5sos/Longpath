// =================================================================
// src/gui/applets/AppletGrid.cpp  (NereusSDR)
// =================================================================
// Siehe AppletGrid.h — Schritt 1 des freien Rasters.
// =================================================================

#include "gui/applets/AppletGrid.h"

#include <QResizeEvent>

#include "gui/applets/AppletWidget.h"
#include "gui/applets/GridCellWidget.h"

#include <QGridLayout>

namespace Longpath {

namespace {
/// Abstand zwischen zwei FELDERN. Im Zielbild 9 px; Schritt 1 laesst
/// ihn auf 0, weil die Applets bisher ohne Abstand untereinander
/// standen und „sichtbar aendert sich nichts" die Zusicherung ist.
/// Der Abstand kommt mit Schritt 2, wenn es wirklich mehrere Spalten
/// gibt und ein Feld auch als Feld zu erkennen sein muss.
constexpr int kCellGap = 0;
} // namespace

AppletGrid::AppletGrid(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(kCellGap);
}

QString AppletGrid::nextId()
{
    return QStringLiteral("cell%1").arg(m_nextId++);
}

QString AppletGrid::addCell(const QString& id)
{
    const QString use = id.isEmpty() ? nextId() : id;
    if (m_meta.contains(use)) { return use; }

    auto* w = new GridCellWidget(use, this);
    m_cells.append(w);

    GridCell c;
    c.id = use;
    m_meta.insert(use, c);

    relayout();
    return use;
}

void AppletGrid::removeCell(const QString& id)
{
    GridCellWidget* w = cell(id);
    if (!w) { return; }
    // Die Inhalte zuerst aushaengen — sonst stuerben sie als Kinder des
    // Feldes, und der Aufrufer haelt Zeiger auf Geloeschtes. Dieselbe
    // Ruecksicht wie bei AppletFloatingWindow::releaseApplet.
    for (QWidget* c : w->widgets()) { w->removeWidget(c); }
    m_cells.removeOne(w);
    m_meta.remove(id);
    m_layout->removeWidget(w);
    w->setParent(nullptr);
    w->deleteLater();
    relayout();
}

GridCellWidget* AppletGrid::cell(const QString& id) const
{
    for (GridCellWidget* w : m_cells) {
        if (w && w->cellId() == id) { return w; }
    }
    return nullptr;
}

QList<GridCellWidget*> AppletGrid::cells() const
{
    return m_cells;
}

GridCellWidget* AppletGrid::cellFor(QWidget* w) const
{
    if (!w) { return nullptr; }
    for (GridCellWidget* c : m_cells) {
        if (c && c->contains(w)) { return c; }
    }
    return nullptr;
}

GridCellWidget* AppletGrid::appendInOwnCell(QWidget* content)
{
    if (!content) { return nullptr; }
    if (GridCellWidget* have = cellFor(content)) { return have; }
    const QString id = addCell();
    GridCellWidget* w = cell(id);
    if (w) { w->addWidget(content); }
    return w;
}

void AppletGrid::takeWidget(QWidget* content)
{
    GridCellWidget* w = cellFor(content);
    if (!w) { return; }
    w->removeWidget(content);
    // Ein leeres Feld ist ein Loch im Raster, das niemand angelegt hat.
    // Wer ein leeres Feld haben will, legt es an — dann traegt es eine
    // eigene Kennung und bleibt stehen.
    if (w->isEmpty()) { removeCell(w->cellId()); }
}

QList<AppletWidget*> AppletGrid::applets() const
{
    QList<AppletWidget*> out;
    for (GridCellWidget* w : m_cells) {
        if (w) { out += w->applets(); }
    }
    return out;
}

void AppletGrid::setColumns(int n)
{
    m_columnsExplicit = true;
    const int use = qMax(1, n);
    if (use == m_columns) { return; }
    m_columns = use;
    relayout();
}

void AppletGrid::clearExplicitColumns()
{
    if (!m_columnsExplicit) { return; }
    m_columnsExplicit = false;
    const int use = columnsForWidth(width());
    if (use == m_columns) { return; }
    m_columns = use;
    relayout();
}

// Schritt 2 des freien Rasters: Spalten nach Breite.
//
// Die Umbrueche liegen bei 1100 und 1600 (Betreiber, 2026-08-18). Sie
// wirken NUR, solange niemand die Spaltenzahl ausdruecklich gesetzt hat
// — siehe setColumns().
//
// Kein eigenes Umbau-Konto: relayout() setzt die Felder ohnehin aus
// Anzeigefolge und Spaltenzahl, und setColumns kehrt bei gleicher Zahl
// frueh zurueck. Eine Fenstergroessenaenderung, die keine Schwelle
// ueberschreitet, kostet damit einen Vergleich.
void AppletGrid::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (m_columnsExplicit) { return; }
    const int use = columnsForWidth(width());
    if (use == m_columns) { return; }
    m_columns = use;
    relayout();
}

int AppletGrid::positionOf(const QString& id) const
{
    for (int i = 0; i < m_cells.size(); ++i) {
        if (m_cells.at(i) && m_cells.at(i)->cellId() == id) { return i; }
    }
    return -1;
}

void AppletGrid::moveCell(const QString& id, int toPosition)
{
    const int from = positionOf(id);
    if (from < 0) { return; }
    const int to = qBound(0, toPosition, m_cells.size() - 1);
    if (from == to) { return; }
    m_cells.move(from, to);
    relayout();
    emit layoutChanged();
}

QList<GridCell> AppletGrid::arrangement() const
{
    QList<GridCell> out;
    for (int i = 0; i < m_cells.size(); ++i) {
        GridCellWidget* w = m_cells.at(i);
        if (!w) { continue; }
        GridCell c = m_meta.value(w->cellId());
        c.id  = w->cellId();
        // Ort aus der Anzeigefolge. Ab Schritt 3 fuehrt der Ort und
        // nicht die Folge; bis dahin waeren zwei Quellen fuer dieselbe
        // Aussage genau die Doppelung, die diese Woche zweimal
        // aufgefallen ist.
        c.row = i / m_columns;
        c.col = i % m_columns;
        c.title = w->title();
        c.applets.clear();
        for (AppletWidget* a : w->applets()) {
            if (a) { c.applets << a->appletId(); }
        }
        out.append(c);
    }
    return out;
}

void AppletGrid::applyArrangement(const QList<GridCell>& cells)
{
    // Nur die Anordnung, nicht die Inhalte: welches Applet wohin
    // gehoert, weiss der Aufrufer (er haelt die Kennungskarte). Hier
    // werden Felder umsortiert, angelegt und beschriftet.
    QList<GridCellWidget*> ordered;
    for (const GridCell& c : cells) {
        GridCellWidget* w = cell(c.id);
        if (!w) { continue; }
        m_meta.insert(c.id, c);
        w->setTitle(c.title);
        ordered.append(w);
    }
    // Was die Aufnahme nicht nennt, bleibt hinten stehen statt zu
    // verschwinden: nach einem Update kennt sie die neuen Felder nicht.
    for (GridCellWidget* w : m_cells) {
        if (!ordered.contains(w)) { ordered.append(w); }
    }
    m_cells = ordered;
    relayout();
}

void AppletGrid::relayout()
{
    for (int i = 0; i < m_cells.size(); ++i) {
        GridCellWidget* w = m_cells.at(i);
        if (!w) { continue; }
        const GridCell c = m_meta.value(w->cellId());
        m_layout->addWidget(w, i / m_columns, i % m_columns,
                            c.rowSpan, c.colSpan);
    }
    // Die letzte Zeile bekommt den Rest der Hoehe, damit die Felder
    // oben stehen und nicht auseinandergezogen werden — dasselbe, was
    // der abschliessende addStretch() im alten QVBoxLayout tat.
    const int rows = m_columns > 0
                         ? (m_cells.size() + m_columns - 1) / m_columns
                         : 0;
    for (int r = 0; r < rows; ++r) { m_layout->setRowStretch(r, 0); }
    m_layout->setRowStretch(rows, 1);
}

} // namespace Longpath
