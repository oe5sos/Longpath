// SPDX-License-Identifier: GPL-3.0-or-later
//
// Wer das Hauptfenster schliesst, schliesst ALLES.
//
// Der Betreiber, live am 2026-08-22: "weiters bleibt das fenster nach
// dem schliessen am dektop" — mit einem Bild, auf dem der abgeloeste
// Panadapter allein ueber dem Schreibtisch stand, waehrend das
// Hauptfenster weg war. Der Wasserfall darin war roter Schrott: das
// Fenster lebte weiter und zeichnete aus einem Speicher, den niemand
// mehr fuellte.
//
// Zwei Wege fuehren zum Schliessen, und BEIDE muessen aufraeumen:
//   1. der rote Knopf am Hauptfenster  -> MainWindow::closeEvent
//   2. Beenden / Cmd+Q                 -> QApplication::closeAllWindows()
//
// Weg 2 war der undichte: Qt schickt dem Schwebefenster ein
// Schliessereignis, unser closeEvent bat um Rueckkehr in die Anordnung
// und reichte das Loeschen per deleteLater NACH — nur laeuft beim
// Beenden keine Runde mehr, in der ein Nachgereichtes ankaeme.

#include <QtTest>
#include <QApplication>
#include <QPointer>

#include "gui/MainWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/PanFloatingWindow.h"

using namespace Longpath;

class TstClosingTakesTheFloatAlong : public QObject
{
    Q_OBJECT

private:
    static QList<PanFloatingWindow*> floatsOnScreen()
    {
        QList<PanFloatingWindow*> out;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (auto* f = qobject_cast<PanFloatingWindow*>(w)) {
                if (f->isVisible()) { out.append(f); }
            }
        }
        return out;
    }

private slots:
    // REIHENFOLGE BEACHTEN: quittingClosesItToo() steht zuerst, weil
    // theTeardownLeavesNothingBehind() prepareForShutdown() ausloest —
    // und das ist ENDGUELTIG (destroy(true,true) reisst auf macOS die
    // native View ab). Danach im selben Prozess ein zweites
    // GPU-Spektrum aufzubauen ist ausserhalb des Vertrags und stuerzt
    // ab; im Betrieb folgt darauf das Prozessende, nichts anderes.
    // Gemessen: einzeln bestehen beide Faelle, zusammen in der
    // umgekehrten Reihenfolge nicht.
    void quittingClosesItToo()
    {
        // Der Weg des Betreibers, im ECHTEN Hauptfenster: abloesen,
        // dann beenden. Ein blosser PanadapterStack wuerde hier
        // luegen — die Abbau-Fahne setzt MainWindow::closeEvent.
        auto* mw = new MainWindow();
        mw->resize(1200, 800);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));

        PanadapterStack* stack = mw->findChild<PanadapterStack*>();
        QVERIFY2(stack, "Kein Panadapter-Stapel im Hauptfenster");
        stack->floatPanadapter(QStringLiteral("pan-0"));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QCOMPARE(floatsOnScreen().count(), 1);

        // Genau das, was Qt bei Cmd+Q tut.
        QApplication::closeAllWindows();

        // UND DANN NICHTS MEHR. Keine processEvents-Schleife: beim
        // echten Beenden laeuft keine Runde mehr, in der ein
        // nachgereichtes deleteLater ankaeme. Genau diese Zeile
        // trennt die Pruefung vom Selbstbetrug — mit einer
        // Ereignisrunde besteht sie auch OHNE die Abbau-Fahne
        // (nachgemessen 2026-08-22), weil das Zurueckdocken das
        // Fenster dann noch rechtzeitig abraeumt.

        QVERIFY2(floatsOnScreen().isEmpty(),
                 "Nach dem Beenden steht der abgeloeste Panadapter noch "
                 "am Schreibtisch — genau das Bild vom 2026-08-22");
    }

    void theTeardownLeavesNothingBehind()
    {
        auto* stack = new PanadapterStack;
        stack->resize(900, 600);
        stack->show();
        QVERIFY(QTest::qWaitForWindowExposed(stack));

        stack->addPanadapter(QStringLiteral("pan-0"));
        stack->applyLayout(QStringLiteral("1"), {QStringLiteral("pan-0")});
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        stack->floatPanadapter(QStringLiteral("pan-0"));
        for (int i = 0; i < 8; ++i) { QCoreApplication::processEvents(); }

        QCOMPARE(floatsOnScreen().count(), 1);
        QPointer<PanFloatingWindow> ghost = floatsOnScreen().first();

        stack->shutDownFloating();
        // KEINE zusaetzliche Ereignisrunde: beim Beenden gibt es die
        // auch nicht. Was hier noch steht, steht beim Betreiber am
        // Schreibtisch.
        QVERIFY2(floatsOnScreen().isEmpty(),
                 "Ein abgeloester Panadapter ueberlebt den Abbau");
        QVERIFY2(ghost.isNull(),
                 "Das Fenster ist nur versteckt, nicht fort");

        delete stack;
    }

};

QTEST_MAIN(TstClosingTakesTheFloatAlong)
#include "tst_closing_takes_the_float_along.moc"
