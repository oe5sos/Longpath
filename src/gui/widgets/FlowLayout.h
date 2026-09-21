// =================================================================
// src/gui/widgets/FlowLayout.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Betreiber 2026-09-21: "Logbuch laesst sich nicht sehr verkleinern."
// Die beiden Zeilen ueber der Tabelle (Knoepfe; Filter mit sechzehn
// Elementen) lagen in QHBoxLayouts, deren Mindestbreite die Summe aller
// Elemente ist -- 1107 px fuer das ganze Fenster, gemessen. Dieses Layout
// legt seine Elemente von links nach rechts und bricht um, sobald die
// Breite nicht reicht: die Mindestbreite ist das breiteste EINZELNE
// Element, die Hoehe waechst mit der Zahl der Zeilen (heightForWidth).
// Bei genuegend Breite sieht es aus wie vorher.
//
// Eigene Umsetzung des bekannten Musters (Qt-Beispiel "Flow Layout"),
// nicht dessen Code: Abstaende aus dem Stil, Dehn-Platzhalter werden
// ignoriert (in einer umbrechenden Zeile haben sie keinen Sinn).
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#pragma once

#include <QLayout>
#include <QList>
#include <QStyle>

namespace Longpath {

class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = -1,
                        int hSpacing = -1, int vSpacing = -1);
    ~FlowLayout() override;

    void addItem(QLayoutItem* item) override;
    // Ein Unter-Layout (etwa Beschriftung + Feld) als EIN Element, das
    // beim Umbruch zusammenbleibt.
    void addLayout(QLayout* layout);
    int  count() const override { return m_items.size(); }
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;

    Qt::Orientations expandingDirections() const override { return {}; }
    bool  hasHeightForWidth() const override { return true; }
    int   heightForWidth(int width) const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;
    void  setGeometry(const QRect& rect) override;

    int horizontalSpacing() const;
    int verticalSpacing() const;

private:
    int doLayout(const QRect& rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem*> m_items;
    int m_hSpace;
    int m_vSpace;
};

} // namespace Longpath
