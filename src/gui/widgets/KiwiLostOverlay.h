// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port.
//
//   2026-11-25 — Neu angelegt (Betreiber: "deutlich anzeigen"): ein Kiwi,
//                der weg ist, soll nicht wie ein haengendes Bild aussehen.

#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;

namespace Longpath {

// Liegt ueber dem Bild eines Kiwi-Streifens: das Bild abgedunkelt, gross der
// Grund und ein Knopf, um einen anderen Kiwi zu waehlen. Entscheidet nichts
// selbst -- der Aufrufer setzt die Ueberschrift (leer = weg).
class KiwiLostOverlay : public QWidget {
    Q_OBJECT

public:
    explicit KiwiLostOverlay(QWidget* parent);

    // title leer: Anzeige weg. detail steht als Hinweis beim Ueberfahren.
    void setLost(const QString& title, const QString& detail = QString());
    bool isLost() const { return !m_title.isEmpty(); }
    QString title() const { return m_title; }
    QPushButton* chooseButtonForTest() const { return m_choose; }

signals:
    void chooseRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_title;
    QLabel* m_label{nullptr};
    QPushButton* m_choose{nullptr};
};

} // namespace Longpath
