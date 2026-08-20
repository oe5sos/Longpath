// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_pan_options_button.cpp  (Longpath)
// =================================================================
// Der Zahnrad-Knopf am Panadapter — sichtbar und wirksam.
//
// Der Betreiber, 2026-08-20: „weiters fehlt auch die option button um
// beim pandapter gleich etwas zu aendern wie zb bild, groesse usw."
//
// Bild, Deckkraft und Grundfarbe lagen nur unter Setup -> Display,
// drei Klicks und einen Dialog von der Flaeche entfernt, die man
// gerade ansieht.
//
// Geprueft wird der WEG, nicht der Zustand — die Lehre aus dem
// Ablöseknopf, den es gab, den aber niemand erreichen konnte.
//
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>

#include "gui/PanadapterApplet.h"

using namespace Longpath;

class TestPanOptionsButton : public QObject
{
    Q_OBJECT
private slots:
    void theGearIsVisibleInTheHeader()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        applet.resize(800, 400);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet));

        QPushButton* gear = nullptr;
        for (QPushButton* b : applet.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("⚙")) { gear = b; break; }
        }
        QVERIFY2(gear, "in der Kopfleiste MUSS ein Zahnrad stehen");
        QVERIFY2(gear->isVisible(), "und es muss zu sehen sein");
        QVERIFY2(gear->isEnabled(), "und anklickbar");
        QVERIFY2(!gear->toolTip().isEmpty(),
                 "mit einem Hinweis, was es tut");
    }

    // Die Eintraege muessen wirklich etwas ausloesen.
    void itsEntriesActuallyFire()
    {
        PanadapterApplet applet(QStringLiteral("A"));

        QSignalSpy imgSpy(&applet,
                          &PanadapterApplet::backgroundImageRequested);
        QSignalSpy opaSpy(&applet,
                          &PanadapterApplet::backgroundOpacityRequested);
        QSignalSpy colSpy(&applet,
                          &PanadapterApplet::backgroundColourRequested);
        QSignalSpy setSpy(&applet,
                          &PanadapterApplet::displaySetupRequested);

        QMenu* m = applet.buildDisplayMenuForTesting();
        QVERIFY(m);

        auto fire = [m](const QString& text) -> bool {
            for (QAction* a : m->actions()) {
                if (a->text() == text) { a->trigger(); return true; }
                if (a->menu()) {
                    for (QAction* sub : a->menu()->actions()) {
                        if (sub->text() == text) { sub->trigger(); return true; }
                    }
                }
            }
            return false;
        };

        QVERIFY2(fire(QStringLiteral("Hintergrundbild entfernen")),
                 "der Eintrag zum Entfernen MUSS im Menue stehen");
        QCOMPARE(imgSpy.count(), 1);
        QVERIFY2(imgSpy.at(0).at(0).toString().isEmpty(),
                 "Entfernen heisst: leerer Pfad");

        QVERIFY2(fire(QStringLiteral("40 %")),
                 "die Deckkraft MUSS waehlbar sein");
        QCOMPARE(opaSpy.count(), 1);
        QCOMPARE(opaSpy.at(0).at(0).toInt(), 40);

        QVERIFY2(fire(QStringLiteral("Grundfarbe…")), "Grundfarbe fehlt");
        QCOMPARE(colSpy.count(), 1);

        QVERIFY2(fire(QStringLiteral("Alle Anzeige-Einstellungen…")),
                 "der Weg in den Setup-Dialog MUSS bleiben — ein Menue, "
                 "das alles kann, waere wieder der Dialog, nur schlechter");
        QCOMPARE(setSpy.count(), 1);

        m->deleteLater();
    }
};
QTEST_MAIN(TestPanOptionsButton)
#include "tst_pan_options_button.moc"
