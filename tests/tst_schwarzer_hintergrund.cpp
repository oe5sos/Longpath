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
        // Drei Merkmale, alle aus e31c8a42 abgelesen, nicht geraten:
        // `new QSlider(Qt::Horizontal, spectrumPane)` mit
        // `setFixedHeight(20)`, und er lief ueber fast die ganze
        // Breite. Entsprechend: waagerecht, 16..24 px hoch, NICHT in
        // einem Applet, und mindestens vier Fuenftel breit.
        //
        // Die drei Einschraenkungen sind am 2026-10-20 dazugekommen,
        // weil der Lauf auf Linux falsch anschlug (#279, Shard 2/4):
        // gemeldet war „400x18", und 18 px hoch im RxApplet ist ein
        // gewoehnlicher Empfangsregler, kein Zoom-Schieber. Unter xvfb
        // ist das Fenster schmaler als die hier gesetzten 1600, und
        // damit reichte „halbe Fensterbreite" bis in die Applets
        // hinein. Die alte Hoehenzeile konnte ausserdem nie greifen:
        // `> 24 && < 16` ist fuer jede Zahl falsch.
        // Applets und Titelleiste sind nicht der Hintergrund: dort
        // gehoeren Regler hin. Der Zoom-Schieber sass im spectrumPane,
        // also im Splitter darunter. Ohne die Titelleiste faengt der
        // Lauf sonst den Hauptlautstaerkeregler (masterSlider, 100x16)
        // mit -- in der Gegenprobe vom 2026-10-20 genau so gesehen.
        const auto gehoertNichtZumHintergrund = [](const QWidget* w) {
            for (const QObject* o = w->parent(); o != nullptr; o = o->parent()) {
                const QString klasse = QString::fromUtf8(o->metaObject()->className());
                if (klasse.endsWith(QLatin1String("Applet"))
                    || klasse.endsWith(QLatin1String("TitleBar"))) {
                    return true;
                }
            }
            return false;
        };
        const auto elternkette = [](const QWidget* w) {
            QStringList kette;
            for (const QObject* o = w->parent(); o != nullptr; o = o->parent()) {
                kette << QString::fromUtf8(o->metaObject()->className());
            }
            return kette.join(QLatin1String(" < "));
        };

        QStringList gefunden;
        for (QSlider* sl : mw.findChildren<QSlider*>()) {
            if (!sl->isVisible()) { continue; }
            if (sl->orientation() != Qt::Horizontal) { continue; }
            if (sl->height() < 16 || sl->height() > 24) { continue; }
            // Breit im Verhaeltnis zu SEINEM Bereich, nicht zum
            // Fenster: der Spektrumbereich ist nur ein Teil davon
            // (links liegen die Applets). Die erste Fassung dieser
            // Zeile mass gegen mw.width() und liess den Schieber aus
            // e31c8a42 in der Gegenprobe glatt durch.
            const QWidget* bereich = sl->parentWidget();
            if (bereich == nullptr || sl->width() * 5 < bereich->width() * 3) {
                continue;
            }
            if (gehoertNichtZumHintergrund(sl)) { continue; }
            // „ueber fast die ganze Breite" -- ein handlicher Regler
            // von 100 px ist nie gemeint gewesen.
            if (sl->width() < 200) { continue; }
            // Die Elternkette mit melden: beim naechsten Fund steht
            // sofort da, WER es ist -- der Name war leer, und das
            // allein hat hier einen Lauf gekostet.
            gefunden << QStringLiteral("%1 %2x%3 [%4]")
                            .arg(sl->objectName()).arg(sl->width()).arg(sl->height())
                            .arg(elternkette(sl));
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
