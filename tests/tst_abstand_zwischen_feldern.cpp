// SPDX-License-Identifier: GPL-3.0-or-later
//
// „es sollte auch immer automatisch zwischen windows und windows ein
//  minimaler automatischer abstand sein, vielleicht 2 pixel. x und y,
//  dann sieht es besser aus und gleich." (Betreiber, 2026-10-15)
//
// „Gleich" ist der Auftrag, nicht „2". Gemessen am laufenden Programm
// (Automationsbruecke, 15 Felder in der Applet-Spalte) stand vorher:
//
//   zwischen zwei Feldern      6 px
//   links vom Fensterrand      6 px
//   rechts                     8 px   (Rinne fuer den Rollbalken)
//   oben zur Befehlsleiste     6 px
//   im Quelltext insgesamt     15 verschiedene setSpacing-Werte
//
// Dieser Lauf MISST den Abstand am echten Hauptfenster, statt eine
// Konstante gegen sich selbst zu pruefen: er liest die Geometrie der
// Felder und rechnet die Luecken aus. Faellt irgendwo wieder ein
// eigener Wert hinein, wird er rot.
//
// Gegenprobe (2026-10-15): mit den alten 6 px scheitert
// derAbstandIstUeberallDerselbe mit „6 px statt 2".

#include <QtTest>

#include "gui/MainWindow.h"
#include "gui/StyleConstants.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/GridCellWidget.h"

using namespace Longpath;

class TstAbstandZwischenFeldern : public QObject
{
    Q_OBJECT

private slots:
    void derAbstandIstUeberallDerselbe()
    {
        auto* mw = new MainWindow();
        // Echte Breite. Bei 800x800 schneidet der Schirm Spalten ab und
        // man misst Zufall (am 2026-10-14 genau so einmal passiert).
        mw->resize(1700, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(250);

        auto* panel = mw->findChild<AppletPanelWidget*>();
        QVERIFY2(panel, "keine Applet-Spalte — dann misst das hier nichts");

        QList<GridCellWidget*> felder;
        for (GridCellWidget* c : panel->findChildren<GridCellWidget*>()) {
            if (c->isVisible()) { felder << c; }
        }
        QVERIFY2(felder.size() >= 3,
                 qPrintable(QStringLiteral("nur %1 sichtbare Felder")
                                .arg(felder.size())));

        std::sort(felder.begin(), felder.end(),
                  [](GridCellWidget* a, GridCellWidget* b) {
                      return a->y() < b->y();
                  });

        const int soll = Style::kPanelGapPx;
        QStringList abweichend;
        for (int i = 1; i < felder.size(); ++i) {
            const int unten = felder[i - 1]->y() + felder[i - 1]->height();
            const int luecke = felder[i]->y() - unten;
            if (luecke != soll) {
                abweichend << QStringLiteral("%1 px statt %2 (Feld %3)")
                                  .arg(luecke).arg(soll).arg(i);
            }
        }
        qInfo() << "Felder:" << felder.size() << "Sollabstand:" << soll;

        QVERIFY2(abweichend.isEmpty(),
                 qPrintable(QStringLiteral(
                     "Ungleiche Abstaende zwischen den Feldern: %1")
                                .arg(abweichend.join(QLatin1String(", ")))));

        // Und der Rand LINKS ist derselbe Abstand. Ohne das sieht die
        // Spalte an den Feldern gleichmaessig aus und am Rand nicht.
        const int randLinks = felder.first()->x();
        QCOMPARE(randLinks, soll);

        delete mw;
    }
};

QTEST_MAIN(TstAbstandZwischenFeldern)
#include "tst_abstand_zwischen_feldern.moc"
