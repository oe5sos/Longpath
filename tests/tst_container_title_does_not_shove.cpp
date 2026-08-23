// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Titelleiste eines Behaelters schiebt den Inhalt NICHT.
//
// Der Betreiber am 2026-08-23: "das öffnen und zittern im rx1 panel
// ist noch nicht behoben" — und praezisiert: "ist nur beim ersten mal
// trennen des filter."
//
// Nachgestellt am laufenden Programm: vor dem Klick auf den
// Trennen-Pfeil keine Titelleiste, danach "RX1 Main Panel" — und die
// ganze rechte Spalte zwoelf Punkte tiefer. Ursache: die Leiste
// erscheint beim Ueberfahren (Thetis-Vorbild) und lag in der
// ANORDNUNG. Wer dort liegt, schiebt beim Erscheinen alles darunter.
//
// Beim ERSTEN Mal faellt es auf, danach ist die Leiste schon da —
// genau die Beobachtung des Betreibers.

#include <QtTest>
#include <QLabel>

#include "gui/containers/ContainerWidget.h"

using namespace Longpath;

class TstContainerTitleDoesNotShove : public QObject
{
    Q_OBJECT

private slots:
    void showingTheTitleLeavesTheContentWhereItIs()
    {
        ContainerWidget c;
        auto* content = new QLabel(QStringLiteral("Inhalt"));
        c.setContent(content);
        c.resize(320, 240);
        c.show();
        QVERIFY(QTest::qWaitForWindowExposed(&c));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        const QPoint before = content->mapTo(&c, QPoint(0, 0));

        // Wie beim Ueberfahren der obersten Zeile.
        if (QWidget* bar = c.findChild<QWidget*>(QStringLiteral("containerTitleBar"))) {
            bar->setVisible(true);
        } else {
            // Ohne Namen: die Leiste ist das erste Kind mit fester Hoehe 22.
            for (QWidget* w : c.findChildren<QWidget*>()) {
                if (w->height() == 22 && w->parent() == &c) { w->setVisible(true); }
            }
        }
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        const QPoint after = content->mapTo(&c, QPoint(0, 0));
        qInfo() << "Inhalt vorher" << before << "nachher" << after;

        QVERIFY2(before == after,
                 qPrintable(QStringLiteral(
                     "Der Inhalt ist von y=%1 auf y=%2 gerutscht, als die "
                     "Titelleiste erschien — genau das Zittern, das der "
                     "Betreiber beim ersten Trennen sieht")
                     .arg(before.y()).arg(after.y())));
    }
};

QTEST_MAIN(TstContainerTitleDoesNotShove)
#include "tst_container_title_does_not_shove.moc"
