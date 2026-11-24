// SPDX-License-Identifier: GPL-3.0-or-later
//
// „ein Window, wo RX und TX zusammen sind" (Betreiber, 2026-11-24).
//
// Der Menüpunkt Containers → „RX + TX in einem Fenster" legt RX und TX in
// Combined 2, klappt es auf und blendet es ein. Beim ersten Versuch am
// laufenden Programm lagen RX und TX schon in Combined 2, das Feld blieb
// aber unsichtbar (setVisible() ist ein No-op, wenn der Stand schon
// „sichtbar" lautet; nur ensureVisible() blendet wirklich ein). Dieser Lauf
// fährt den Weg im echten MainWindow und prüft, was der Betreiber sieht.

#include <QtTest>
#include <QAction>

#include "gui/MainWindow.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/applets/AppletWidget.h"
#include "gui/applets/KombiApplet.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstRxTxEinFenster : public QObject
{
    Q_OBJECT

private slots:
    void rxUndTxStehenUntereinanderInCombined2()
    {
        auto* mw = new MainWindow();
        mw->resize(1700, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        // Der Zustand vom Betreiber-Rechner: Combined 2 steht in der Auswahl
        // schon auf „sichtbar" (Haken im Menü), die Hülle in der Spalte ist
        // aber zu. setVisible() ändert dann nichts; nur ensureVisible()
        // blendet sie ein.
        auto* vis = mw->findChild<AppletVisibilityController*>();
        QVERIFY(vis);
        vis->setVisible(QStringLiteral("Kombi2"), true);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        for (KombiApplet* k : KombiApplet::alle()) {
            if (k && k->appletId() == QLatin1String("Kombi2") && k->parentWidget()) {
                k->parentWidget()->hide();
            }
        }
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        QAction* a = mw->findChild<QAction*>(QStringLiteral("actRxTxOneWindow"));
        QVERIFY2(a, "kein Menüpunkt „RX + TX in einem Fenster“");
        a->trigger();
        for (int i = 0; i < 20; ++i) { QCoreApplication::processEvents(); }

        KombiApplet* feld2 = nullptr;
        for (KombiApplet* k : KombiApplet::alle()) {
            if (k && k->appletId() == QLatin1String("Kombi2")) { feld2 = k; }
        }
        QVERIFY2(feld2, "Combined 2 gibt es nicht");

        QWidget* rx = nullptr;
        QWidget* tx = nullptr;
        for (QWidget* w : feld2->seiten()) {
            const auto* ap = qobject_cast<const AppletWidget*>(w);
            if (!ap) { continue; }
            if (ap->appletId() == QLatin1String("Rx")) { rx = w; }
            if (ap->appletId() == QLatin1String("Tx")) { tx = w; }
        }
        QVERIFY2(rx, "RX liegt nicht in Combined 2");
        QVERIFY2(tx, "TX liegt nicht in Combined 2");

        QVERIFY2(feld2->aufgeklappt(), "Combined 2 ist nicht aufgeklappt");
        QVERIFY2(feld2->isVisible(),
                 "Combined 2 ist unsichtbar — das war der Fehler vom ersten Versuch");
        QVERIFY2(rx->isVisible(), "RX ist nicht zu sehen");
        QVERIFY2(tx->isVisible(), "TX ist nicht zu sehen");

        // Untereinander: gleiche Spalte, RX oberhalb von TX, ohne Überlappung.
        const QRect rr(rx->mapTo(mw, QPoint(0, 0)), rx->size());
        const QRect tr(tx->mapTo(mw, QPoint(0, 0)), tx->size());
        QVERIFY2(rr.bottom() <= tr.top() || tr.bottom() <= rr.top(),
                 "RX und TX überlappen sich");
        QVERIFY2(rr.width() > 0 && tr.width() > 0 && rr.height() > 0 && tr.height() > 0,
                 "RX oder TX hat keine Größe");

        // Zum Ansehen: LONGPATH_RXTX_BILD=<Ordner> schreibt zwei Bilder.
        const QString bildOrdner = qEnvironmentVariable("LONGPATH_RXTX_BILD");
        if (!bildOrdner.isEmpty()) {
            feld2->grab().save(bildOrdner + QStringLiteral("/combined2.png"));
            mw->grab().save(bildOrdner + QStringLiteral("/fenster.png"));
        }

        delete mw;
    }
};

QTEST_MAIN(TstRxTxEinFenster)
#include "tst_rx_tx_ein_fenster.moc"
