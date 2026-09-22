// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/widgets/FlowLayout.h  (Longpath)
// =================================================================
// Longpath-original. No Thetis port.
//
// Eine Zeile, die umbricht: Knoepfe und Felder laufen von links nach
// rechts und rutschen in die naechste Zeile, wenn der Platz ausgeht —
// wie Text. Der Betreiber (2026-09-21): das Logbuchfenster liess sich
// nicht schmaler machen als seine Werkzeugleiste, „diese kann aber
// anstatt 1-zeilig auch 2-zeilig werden".
//
// Zwei Dinge, die ein Werkzeugleisten-Layout braucht und ein einfacher
// Umbruch nicht haette:
//   * Was in seiner Zeile uebrig bleibt, bekommen die Elemente, die
//     sich waagrecht ausdehnen wollen (QSizePolicy::ExpandFlag —
//     Suchfeld, Datumsfelder) — und ein addStretch() als Luecke, mit der
//     ein „Clear" rechts bleibt, solange die Zeile es hergibt.
//   * Die Mindestbreite ist das breiteste EINZELNE Element, nicht die
//     Summe — genau darum geht es.
// Hoehe folgt der Breite (hasHeightForWidth), das umschliessende
// QVBoxLayout fragt danach.
// =================================================================
#pragma once

#include <QLayout>
#include <QList>
#include <QRect>

namespace Longpath {

class FlowLayout : public QLayout {
    Q_OBJECT
public:
    explicit FlowLayout(QWidget* parent = nullptr, int hSpacing = 6, int vSpacing = 6);
    ~FlowLayout() override;

    void addItem(QLayoutItem* item) override;
    // Eine dehnbare Luecke — wie QBoxLayout::addStretch().
    void addStretch();

    int  count() const override { return m_items.size(); }
    QLayoutItem* itemAt(int i) const override { return m_items.value(i); }
    QLayoutItem* takeAt(int i) override;

    Qt::Orientations expandingDirections() const override { return Qt::Horizontal; }
    bool  hasHeightForWidth() const override { return true; }
    int   heightForWidth(int width) const override;
    // Die Mindesthoehe bei dieser Breite IST die umgebrochene Hoehe —
    // ohne das rechnet das umschliessende QVBoxLayout mit einer Zeile
    // und die Nachbarn rutschen unter die Leiste (Kartenspalte bei
    // 640 px, 2026-09-22).
    int   minimumHeightForWidth(int width) const override { return heightForWidth(width); }
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    void  setGeometry(const QRect& rect) override;

    // Zeilen beim letzten setGeometry — fuer Pruefstaende.
    int rowsLaidOut() const { return m_rows; }

private:
    int layOut(const QRect& rect, bool apply) const;
    static bool wantsWidth(QLayoutItem* item);

    QList<QLayoutItem*> m_items;
    int m_hSpace;
    int m_vSpace;
    mutable int m_rows{0};
};

} // namespace Longpath
