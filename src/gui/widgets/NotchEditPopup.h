// SPDX-License-Identifier: GPL-3.0-or-later
//
// NotchEditPopup — ein Notch-Filter anfassen, ohne ihn zu treffen.
//
// Der Betreiber, 2026-08-21: „es waere auch gut, wenn man am
// notchfilter klickt und dann diese bearbeite kann und auch loeschen."
//
// Bis dahin ging Bearbeiten nur mit der Maus am Balken: ziehen fuer die
// Mitte, an der Kante ziehen fuer die Breite, und eine Liste fester
// Breiten im Rechtsklick-Menue. Das ist schnell, wenn man trifft — und
// unbrauchbar, wenn der Balken zwei Pixel breit ist oder das Geraet
// getrennt ist (dann schluckt der Panadapter jeden Linksklick).
//
// Hier stehen dieselben drei Groessen als Zahlen: Mitte, Breite,
// aktiv. Dazu der Loeschknopf, weil „bearbeiten" und „wegwerfen" beim
// selben Ding zusammengehoeren.
//
// Bewusst KEIN eigener Zustand: das Fenster schickt Wuensche los und
// nimmt entgegen, was das Modell daraus gemacht hat. Ein Notch, dessen
// Breite WDSP heraufsetzt (autoincr, nbp.c:122-125), soll hier die
// Zahl zeigen, die wirklich gilt — nicht die, die getippt wurde.

#pragma once

#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;

namespace Longpath {

class NotchEditPopup : public QWidget {
    Q_OBJECT

public:
    explicit NotchEditPopup(QWidget* parent = nullptr);

    /// Auf einen Notch stellen und neben der Maus aufgehen lassen.
    void showFor(int id, double centreHz, double widthHz, bool active,
                 const QPoint& globalPos);

    int notchId() const { return m_id; }

    /// Die Zahlen nachfuehren, wenn das Modell etwas anderes daraus
    /// gemacht hat als gewuenscht. Loest keine Signale aus.
    void applyFromModel(int id, double centreHz, double widthHz, bool active);

signals:
    void centreRequested(int id, double hz);
    void widthRequested(int id, double hz);
    void activeRequested(int id, bool active);
    void removeRequested(int id);

private:
    void buildUi();

    int             m_id {-1};
    bool            m_loading {false};
    QDoubleSpinBox* m_centre {nullptr};
    QSpinBox*       m_width {nullptr};
    QCheckBox*      m_active {nullptr};
};

} // namespace Longpath
