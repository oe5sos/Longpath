#pragma once

// =================================================================
// src/gui/applets/AppletGrid.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Schritt 1 des freien Rasters ─────────────────────────────────────
//
// Entwurf: docs/architecture/2026-08-17-freies-raster-vorschlag.md,
// „Schritt 1 — Der Behaelter, einspaltig. Sichtbar aendert sich nichts;
// die Applets liegen nur in Zellen statt in einer Box. Damit ist die
// Umstellung von Reihenfolge auf Zellen erledigt, BEVOR irgendetwas
// Sichtbares davon abhaengt."
//
// Das ist der Sinn dieser Klasse und der Grund, warum sie mit einer
// Spalte anfaengt: der Umbau von „Reihenfolge" auf „Ort" ist die
// riskante Aenderung, und sie soll allein stattfinden.
//
// ── Reihenfolge ODER Ort, nicht beides ───────────────────────────────
//
// AppletPanelWidget kannte nur einen Index je Applet. Ein Raster kennt
// Zeile, Spalte und Spannweite. Solange die Spaltenzahl 1 ist, sind die
// beiden dasselbe — die Zeile IST der Index. Genau deshalb laesst sich
// der Umbau ohne sichtbare Aenderung machen und pruefen.
//
// ── Ein Feld ist ein Behaelter ───────────────────────────────────────
//
// Festlegung des Betreibers, siehe GridCell.h. Die Zellen halten eine
// LISTE von Applets, auch wenn Schritt 1 nur eines hineinlegt. Das
// Datenmodell steht fertig, bevor es benutzt wird — die Alternative
// waere eine zweite Wanderung der gespeicherten Anordnung, und was eine
// Anordnungswanderung still verlieren kann, hat der Kennungsfehler vom
// 2026-08-18 gezeigt.
//
// ── Was noch NICHT hier steht ────────────────────────────────────────
//
// Spalten nach Breite (Schritt 2), Spannweiten samt Profil (Schritt 3),
// Density (Schritt 4). Die Schnittstelle traegt sie schon — setColumns()
// und die Spannweiten in GridCell —, aber niemand ruft sie.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-18 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/GridCell.h"

#include <QHash>
#include <QList>
#include <QWidget>

class QGridLayout;

namespace NereusSDR {

class AppletWidget;
class GridCellWidget;

class AppletGrid : public QWidget {
    Q_OBJECT

public:
    explicit AppletGrid(QWidget* parent = nullptr);

    // ── Felder ───────────────────────────────────────────────────────

    /// Legt ein Feld an und liefert seine Kennung. `id` leer heisst
    /// „vergib eine"; die vergebene ist fortlaufend und stabil, damit
    /// das Profil sie wiederfindet.
    QString addCell(const QString& id = QString());

    /// Raeumt ein Feld ab. Die Applets darin werden AUSGEHAENGT, nicht
    /// geloescht — der Aufrufer bekommt sie ueber applets() vorher.
    void removeCell(const QString& id);

    GridCellWidget* cell(const QString& id) const;
    QList<GridCellWidget*> cells() const;

    /// Das Feld, in dem dieses Widget steht, oder nullptr.
    GridCellWidget* cellFor(QWidget* w) const;

    // ── Inhalte ──────────────────────────────────────────────────────

    /// Legt das Widget in ein NEUES Feld am Ende. Der Weg von
    /// Schritt 1: ein Inhalt, ein Feld.
    GridCellWidget* appendInOwnCell(QWidget* w);

    /// Nimmt das Widget aus seinem Feld. Ein Feld, das dabei leer wird,
    /// verschwindet — ein leeres Feld ist ein Loch im Raster, das
    /// niemand angelegt hat.
    void takeWidget(QWidget* w);

    /// Alle Applets, in Anzeigefolge: Feld fuer Feld, im Feld von oben
    /// nach unten.
    QList<AppletWidget*> applets() const;

    // ── Anordnung ────────────────────────────────────────────────────

    int  columns() const { return m_columns; }
    void setColumns(int n);

    /// Die Stelle eines Feldes in der Anzeigefolge (0-basiert), oder -1.
    int  positionOf(const QString& id) const;

    /// Verschiebt ein Feld an eine andere Stelle der Anzeigefolge.
    void moveCell(const QString& id, int toPosition);

    /// Die ganze Anordnung, fuers Profil und fuer Tests.
    ///
    /// NICHT layout(): das gehoert QWidget und meint etwas anderes.
    /// Zwei Bedeutungen fuer denselben Namen an derselben Klasse sind
    /// genau die Sorte Verwechslung, die diese Woche zweimal Zeit
    /// gekostet hat.
    QList<GridCell> arrangement() const;
    void applyArrangement(const QList<GridCell>& cells);

signals:
    /// Ein Feld hat den Platz gewechselt. Einmal am Ende der Geste,
    /// nicht bei jedem Bildpunkt.
    void layoutChanged();

private:
    void relayout();
    QString nextId();

    QGridLayout* m_layout{nullptr};
    /// Die Felder in ANZEIGEFOLGE. Ort und Spannweite ergeben sich in
    /// Schritt 1 aus dieser Folge und der Spaltenzahl; ab Schritt 3
    /// steht der Ort im Feld selbst.
    QList<GridCellWidget*> m_cells;
    QHash<QString, GridCell> m_meta;
    int m_columns{1};
    int m_nextId{1};
};

} // namespace NereusSDR
