// SPDX-License-Identifier: GPL-3.0-or-later
//
// „er soll mal technisch funktionieren! layout und funktionen koennen
//  danach erweitert werden. wie gesagt, das sollte nur eine option
//  sein, die man ein und ausblenden kann." (Betreiber, 2026-10-15)
//
// Der schwebende Panadapter war gebaut (PanFloatingWindow, seit
// Sub-Epic D), aber als BEDIENUNG unfertig — und das ist dieselbe
// Fehlerklasse wie diese Woche schon viermal:
//
//   * Im Menue stand „Float active pan…": eine Handlung in EINE
//     Richtung. Sie loeste ab und konnte nie wieder andocken. Ob
//     gerade etwas schwebte, sagte das Menue nicht.
//   * Zurueck ging es nur ueber den ↙ im Kopf des Schwebefensters —
//     also nur, wenn man das Fenster schon gefunden hatte.
//
// Dieser Lauf haelt den Schalter nach: an loest ab, aus dockt an, und
// der Haken sagt die Wahrheit, auch wenn jemand den anderen Weg nimmt.
//
// Gegenprobe (2026-10-15): mit der alten, nicht ankreuzbaren Aktion
// faellt derSchalterIstAnkreuzbarUndBeidseitig schon an
// „nicht ankreuzbar".

#include <QtTest>
#include <QAction>
#include <QMenu>
#include <QMenuBar>

#include "gui/MainWindow.h"
#include "gui/PanadapterStack.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstPanadapterSchalter : public QObject
{
    Q_OBJECT

private:
    /// Nicht mw->menuBar(): MainWindow haengt seine Leiste per
    /// setMenuWidget() in eine eigene TitleBar um, und menuBar() legt
    /// danach eine neue, LEERE an.
    static QAction* schalter(MainWindow* mw)
    {
        for (QMenuBar* bar : mw->findChildren<QMenuBar*>()) {
            for (QAction* a : bar->actions()) {
                if (!a->menu()) { continue; }
                if (!a->text().contains(QLatin1String("View"))) { continue; }
                for (QAction* e : a->menu()->actions()) {
                    if (e->text().contains(QLatin1String("Panadapter"))) {
                        return e;
                    }
                }
            }
        }
        return nullptr;
    }

private slots:
    void derSchalterIstAnkreuzbarUndBeidseitig()
    {
        auto* mw = new MainWindow();
        mw->resize(1700, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        QAction* a = schalter(mw);
        QVERIFY2(a, "kein Panadapter-Schalter im View-Menue");
        QVERIFY2(a->isCheckable(),
                 "nicht ankreuzbar — dann sagt er nie, ob etwas schwebt");
        QVERIFY2(a->isEnabled(), "ausgegraut");

        auto* stack = mw->findChild<PanadapterStack*>();
        QVERIFY(stack);
        const QString pan = stack->activePanId();
        QVERIFY(!pan.isEmpty());
        QVERIFY2(!stack->isPanFloating(pan), "steht schon schwebend da");
        QVERIFY2(!a->isChecked(), "Haken steht, obwohl nichts schwebt");

        // AN: der Panadapter bekommt sein eigenes Fenster.
        a->setChecked(true);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QVERIFY2(stack->isPanFloating(pan),
                 "der Schalter hat nichts abgeloest");

        // AUS: und wieder zurueck. Das ist der Weg, den es vorher gar
        // nicht gab.
        a->setChecked(false);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QVERIFY2(!stack->isPanFloating(pan),
                 "der Schalter dockt nicht wieder an — eine Einbahn");

        delete mw;
    }

    /// Fuenf Mal hin und her. Der Panadapter traegt ein QRhiWidget,
    /// dessen Zeichenkontext am FENSTER haengt; beim Umhaengen muss er
    /// abgebaut und neu gebunden werden (siehe die lange Begruendung in
    /// PanadapterStack::floatPanadapter). Ein Weg, der beim ersten Mal
    /// geht und beim dritten haengt, ist nicht „technisch
    /// funktionsfaehig" — also wird er mehrfach gefahren.
    void fuenfMalHinUndHer()
    {
        auto* mw = new MainWindow();
        mw->resize(1700, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        auto* stack = mw->findChild<PanadapterStack*>();
        QVERIFY(stack);
        const QString pan = stack->activePanId();
        QVERIFY(!pan.isEmpty());

        for (int runde = 1; runde <= 5; ++runde) {
            stack->floatPanadapter(pan);
            for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
            QVERIFY2(stack->isPanFloating(pan),
                     qPrintable(QStringLiteral("Runde %1: nicht abgeloest")
                                    .arg(runde)));

            stack->dockPanadapter(pan);
            for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
            QVERIFY2(!stack->isPanFloating(pan),
                     qPrintable(QStringLiteral("Runde %1: nicht angedockt")
                                    .arg(runde)));
        }

        // Und danach ist der Panadapter noch da — nicht nur „nicht
        // schwebend", sondern wirklich im Fenster.
        QVERIFY2(stack->panadapter(pan),
                 "der Panadapter ist beim Hin und Her verlorengegangen");

        delete mw;
    }

    /// Und der Haken folgt dem ANDEREN Weg: wer den ↗ im Panelkopf
    /// nimmt, soll das Menue nicht luegen sehen.
    void derHakenFolgtAuchDemAnderenWeg()
    {
        auto* mw = new MainWindow();
        mw->resize(1700, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        QAction* a = schalter(mw);
        QVERIFY(a);
        auto* stack = mw->findChild<PanadapterStack*>();
        QVERIFY(stack);
        const QString pan = stack->activePanId();

        // Der Weg am Menue vorbei — genau das, was ↗ und der
        // Rechtsklick tun.
        stack->floatPanadapter(pan);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QVERIFY2(a->isChecked(),
                 "der Haken blieb leer, obwohl der Panadapter schwebt");

        stack->dockPanadapter(pan);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QVERIFY2(!a->isChecked(),
                 "der Haken blieb stehen, obwohl wieder angedockt ist");

        delete mw;
    }
};

QTEST_MAIN(TstPanadapterSchalter)
#include "tst_panadapter_schalter.moc"
