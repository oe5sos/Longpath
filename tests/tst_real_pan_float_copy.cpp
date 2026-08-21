// SPDX-License-Identifier: GPL-3.0-or-later
//
// NOCH NICHT IM GATTER — absichtlich nicht in tests/CMakeLists.txt
// eingetragen. Der Test stellt den gemeldeten Fehler bisher NICHT
// nach: floatPanadapter() greift in dieser Umgebung gar nicht
// (schwebende Fenster: 0), also misst alles darunter nichts. Ein
// registrierter Test, der aus dem falschen Grund rot ist, macht das
// Gatter wertlos.
//
// Er bleibt liegen, weil die Fragestellung richtig ist und die
// naechste Sitzung dort weitermacht.
//
// Beim Abloesen eines Panadapters darf im Hauptfenster keiner
// zurueckbleiben.
//
// Der Betreiber, 2026-08-21, mit Bildschirmfoto: „sobald ich den
// pandapter veraendern moechte kommt eine kopie." Auf dem Bild steht
// „DISCONNECTED" doppelt und leicht versetzt, und die Knopfreihe
// S/B/-/+ gibt es zweimal.
//
// Dieser Test beantwortet EINE Frage, und nur die: gibt es wirklich
// zwei Panadapter, oder ist es einer und daneben stehengebliebene
// Pixel? Davon haengt die Richtung der Behebung ab — ein zweites
// Widget waere ein Fehler in der Verwaltung, stehengebliebene Pixel
// einer in der Darstellung (macOS, natives Fenster, QRhi).
//
// Raten waere hier besonders teuer: PanadapterStack.cpp haelt bereits
// zwei verworfene Kuren fest, beide aus Vermutungen entstanden.

#include <QtTest>
#include <QSet>

#include "gui/MainWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumWidget.h"

using namespace Longpath;

class TstRealPanFloatCopy : public QObject
{
    Q_OBJECT

private slots:
    void afterFloatingOnlyOnePanadapterIsVisible()
    {
        auto* mw = new MainWindow();
        mw->resize(1800, 1100);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(300);

        auto* stack = mw->findChild<PanadapterStack*>();
        QVERIFY2(stack, "Kein PanadapterStack im Hauptfenster");

        // ── Zaehlen, aber richtig ────────────────────────────────
        //
        // Der erste Anlauf zaehlte mit findChildren(mw) und kam auf
        // „einer bleibt zurueck". Falsch: das schwebende Fenster wird
        // mit window() als Elternteil gebaut, haengt also im
        // Objektbaum UNTER dem Hauptfenster. findChildren(mw) findet
        // den abgeloesten Panadapter damit mit — und die Runde ueber
        // die Top-Level-Fenster zaehlte ihn ein zweites Mal.
        //
        // Gezaehlt wird jetzt nach ZEIGERN, und jedem Panadapter wird
        // sein eigenes Fenster (window()) zugeordnet. Das ist die
        // Frage, um die es geht: in welchem Fenster steht er?
        QSet<SpectrumWidget*> seen;
        for (QWidget* top : QApplication::topLevelWidgets()) {
            for (SpectrumWidget* s : top->findChildren<SpectrumWidget*>()) {
                if (s->isVisible()) { seen.insert(s); }
            }
        }

        int inMain = 0, elsewhereCount = 0;
        for (SpectrumWidget* s : seen) {
            if (s->window() == mw) { ++inMain; } else { ++elsewhereCount; }
        }

        qInfo() << "schwebende Fenster:" << stack->floatingCountForTesting();
        qInfo() << "sichtbare Panadapter insgesamt:" << seen.size()
                << " davon im Hauptfenster:" << inMain
                << " in eigenen Fenstern:" << elsewhereCount;

        QVERIFY2(seen.size() == 1,
                 qPrintable(QStringLiteral(
                     "%1 sichtbare Panadapter statt einem — es gibt "
                     "wirklich zwei Widgets, nicht nur stehengebliebene "
                     "Pixel").arg(seen.size())));
        QVERIFY2(inMain == 0,
                 qPrintable(QStringLiteral(
                     "Nach dem Abloesen steht noch %1 Panadapter im "
                     "Hauptfenster").arg(inMain)));

        mw->close();
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        delete mw;
    }
};

QTEST_MAIN(TstRealPanFloatCopy)
#include "tst_real_pan_float_copy.moc"
