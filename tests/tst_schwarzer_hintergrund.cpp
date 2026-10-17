// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall.
//
// Keine Linien im Hintergrund (2026-10-14).
//
// Der Betreiber, mit einem Bild: „in Longpath unter Click to Connect ganz
// unten ist nach wie vor dieser blaue Punkt und diese vertikale und
// horizontale Linie, die gehoert auf jeden Fall entfernt. Ich moechte im
// Hintergrund keine Linien haben, das muss ein schwarzer Bildschirm
// sein. Keinen Rahmen hier setzen, mit keinen Begrenzungen und so
// weiter."
//
// NACHGEMESSEN in seinem Bild, nicht geraten:
//
//   blauer Punkt   x 1105..1135, y 1133..1160, Farbe (53,119,163)
//                  -> das ist `kAccent` (#4a7ba8) durch die
//                     Bildkompression, rund, ~14 pt
//   waagerechte    y 1131, Farbe (70,69,72), ueber die ganze Breite
//   Linie
//
// Das war der GRIFF und die RINNE eines `QSlider` („zoomBar", feste
// Hoehe 20, ueber die ganze Breite unter dem Spektrum) und der
// HAARSTRICH der Splittergriffe (`border-top`/`border-left: 1px solid
// kBorder`).
//
// Beides ist weg. Der Zoom bleibt auf zwei geprueften Wegen erreichbar
// (Cmd/Ctrl + Mausrad; Ziehen an der Frequenzskala), und die Griffe
// bleiben sechs Pixel breit -- greifbar, nur nicht mehr dauerhaft
// sichtbar.

#include <QtTest/QtTest>

#include <QSlider>
#include <QSplitter>

#include "gui/MainWindow.h"
#include "gui/StyleConstants.h"

using namespace Longpath;

class TstSchwarzerHintergrund : public QObject {
    Q_OBJECT

private slots:
    void derSplitterGriffZiehtKEINENStrichMehr()
    {
        const QString stil = Style::splitterStyle();
        QVERIFY2(!stil.contains(QStringLiteral("border-top")),
                 qPrintable(stil));
        QVERIFY2(!stil.contains(QStringLiteral("border-left")),
                 qPrintable(stil));
        // Und er hat keinen eigenen Grund mehr -- er nimmt die Farbe
        // dessen an, worueber er liegt.
        QVERIFY2(stil.contains(QStringLiteral("background: transparent")),
                 qPrintable(stil));
    }

    void derGriffBLEIBTGreifbarUndZeigtSichBeimBeruehren()
    {
        // Die Greifbarkeit war der Grund fuer den Strich (ein Griff von
        // drei Pixeln ist auf einem Retina-Schirm nicht zu treffen).
        // Sechs Pixel sind sechs Pixel, ob man sie sieht oder nicht --
        // das darf beim Aufraeumen nicht mit verlorengehen.
        QCOMPARE(Style::kSplitterHandlePx, 6);
        const QString stil = Style::splitterStyle();
        QVERIFY2(stil.contains(QStringLiteral("handle:hover")), qPrintable(stil));
        QVERIFY2(stil.contains(QString::fromLatin1(Style::kAccent)),
                 qPrintable(stil));
    }

    void esGibtKEINENZoomSchieberMehrUnterDemSpektrum()
    {
        QStandardPaths::setTestModeEnabled(true);
        MainWindow mw;
        mw.resize(1600, 1000);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, 20000));
        QCoreApplication::processEvents();

        // Der Schieber war waagerecht, 20 px hoch und lief ueber fast
        // die ganze Breite des Spektrumbereichs. Genau so einen darf es
        // nicht mehr geben.
        QStringList gefunden;
        for (QSlider* sl : mw.findChildren<QSlider*>()) {
            if (!sl->isVisible()) { continue; }
            if (sl->orientation() != Qt::Horizontal) { continue; }
            if (sl->height() > 24 && sl->height() < 16) { continue; }
            if (sl->width() < mw.width() / 2) { continue; }
            gefunden << QStringLiteral("%1 %2x%3")
                            .arg(sl->objectName()).arg(sl->width()).arg(sl->height());
        }
        QVERIFY2(gefunden.isEmpty(),
                 qPrintable(QStringLiteral("breiter waagerechter Schieber "
                                           "wieder da: %1")
                                .arg(gefunden.join(QStringLiteral(", ")))));
    }

    void derGriffDesSchiebersWarDASBlaueImBild()
    {
        // Festgehalten, damit die Messung nicht verlorengeht: die Farbe
        // aus Martins Bild gegen die Hausfarbe. (53,119,163) gegen
        // kAccent #4a7ba8 = (74,123,168) -- der Abstand ist die
        // Bildkompression, nicht eine andere Farbe.
        const QColor haus(QString::fromLatin1(Style::kAccent));
        const QColor gemessen(53, 119, 163);
        QVERIFY2(qAbs(haus.red()   - gemessen.red())   < 30
                 && qAbs(haus.green() - gemessen.green()) < 30
                 && qAbs(haus.blue()  - gemessen.blue())  < 30,
                 qPrintable(QStringLiteral("Hausfarbe %1, im Bild %2")
                                .arg(haus.name(), gemessen.name())));
    }
};

QTEST_MAIN(TstSchwarzerHintergrund)
#include "tst_schwarzer_hintergrund.moc"
