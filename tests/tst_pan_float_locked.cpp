// SPDX-License-Identifier: GPL-3.0-or-later
//
// Das Abloesen des Panadapters ist gesperrt — und der Rueckweg nicht.
//
// Grund (2026-08-21): Panadapter abloesen, Fenster schliessen — SIGSEGV,
// nachgestellt und mit Stapel belegt. Die Spur endet in Qt selbst; drei
// Kuren wurden versucht und durch Messung verworfen.
//
// Gesperrt werden DARF es, weil der Grund weggefallen ist, aus dem der
// Betreiber es benutzt hat: er loeste ab, um den Panadapter groesser zu
// ziehen. Das ging nur deshalb nicht direkt, weil die Griffleisten drei
// Pixel breit waren. Seit sie sechs haben, zieht man den Rand.
//
// Zwei Dinge muessen zugleich gelten, und beide stehen hier:
//   1. Kein Weg fuehrt mehr ins Abloesen — auch nicht das Menue.
//   2. Ein bereits abgeloester Panadapter laesst sich IMMER zurueck-
//      holen. Sonst waere jemand, dessen gespeicherte Anordnung einen
//      schwebenden Panadapter enthaelt, dauerhaft eingesperrt.

#include <QtTest>
#include <QMenu>
#include <QPushButton>

#include "gui/PanadapterApplet.h"

using namespace Longpath;

class TstPanFloatLocked : public QObject
{
    Q_OBJECT

private:
    static QPushButton* floatButton(PanadapterApplet& a)
    {
        for (QPushButton* b : a.findChildren<QPushButton*>()) {
            const QString t = b->text();
            if (t == QString::fromUtf8("↗")
                || t == QString::fromUtf8("↙")) {
                return b;
            }
        }
        return nullptr;
    }

private slots:
    /// Eingedockt: der Knopf ist grau und sagt, warum.
    void dockedTheDetachButtonIsLockedAndExplainsItself()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.setFloatingIndicator(false);

        QPushButton* b = floatButton(applet);
        QVERIFY2(b, "Kein Abloese-Knopf gefunden");
        QVERIFY2(!b->isEnabled(),
                 "Der Abloese-Knopf ist anklickbar — er stuerzt aber ab");
        QVERIFY2(b->toolTip().contains(QString::fromUtf8("gesperrt")),
                 qPrintable(QStringLiteral(
                     "Der Knopf sagt nicht, warum er grau ist: '%1'")
                     .arg(b->toolTip())));
        QVERIFY2(b->toolTip().contains(QString::fromUtf8("Rand")),
                 "Der Knopf nennt nicht den Weg, der stattdessen geht");
    }

    /// Und ein Klick darauf loest nichts aus — auch nicht auf Umwegen.
    void clickingItEmitsNothing()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.setFloatingIndicator(false);

        QSignalSpy spy(&applet, &PanadapterApplet::floatRequested);
        QPushButton* b = floatButton(applet);
        QVERIFY(b);
        b->click();                       // grau, also folgenlos
        emit b->clicked();                // und selbst am Knopf vorbei
        QCOMPARE(spy.count(), 0);
    }

    /// Auch das Menue fuehrt nicht mehr hinein.
    void theMenuEntryIsLockedToo()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        QMenu* m = applet.buildContextMenuForTesting();
        QVERIFY(m);

        QAction* floatAct = nullptr;
        for (QAction* a : m->actions()) {
            if (a->text().contains(QStringLiteral("Float"))) { floatAct = a; }
        }
        QVERIFY2(floatAct, "Kein 'Float this pan' im Menue");
        QVERIFY2(!floatAct->isEnabled(),
                 "Das Menue loest noch ab, obwohl der Knopf gesperrt ist — "
                 "eine Sperre mit einer Hintertuer ist keine");
    }

    /// ABER: schwebt einer, muss man ihn zurueckholen koennen.
    ///
    /// Das ist die Haelfte, die man beim Sperren leicht vergisst — und
    /// dann sitzt jemand mit einer gespeicherten Anordnung fest.
    void aFloatingPanCanAlwaysComeBack()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.setFloatingIndicator(true);

        QPushButton* b = floatButton(applet);
        QVERIFY(b);
        QVERIFY2(b->isEnabled(),
                 "Ein abgeloester Panadapter kaeme nicht mehr zurueck");

        QSignalSpy back(&applet, &PanadapterApplet::dockRequested);
        b->click();
        QCOMPARE(back.count(), 1);
    }
};

QTEST_MAIN(TstPanFloatLocked)
#include "tst_pan_float_locked.moc"
