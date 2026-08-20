// =================================================================
// tests/tst_applet_window_size.cpp  (Longpath)
// =================================================================
//
// Ein abgeloestes Applet darf den Bildschirm nicht fuellen.
//
// ── Warum es diese Pruefung gibt ────────────────────────────────────
//
// Der Betreiber, 2026-08-20: „es geht immer ein neues fenster
// bildschirm fuellend auf, dass ist falsch". Er hat es zweimal sagen
// muessen, weil mein erster Anlauf nur die Mindesthoehe des Applets
// auf null setzte. Das half beim RX-Applet — gemessen 300x368 — und
// half bei den anderen nicht: deren Anordnung zieht ihre Untergrenze
// aus den KINDERN, und die bleibt, egal was man dem Applet selbst
// sagt.
//
// Die Loesung ist ein Rollbereich zwischen Fenster und Applet. Er hat
// selbst kaum eine Untergrenze; was nicht hineinpasst, wird gerollt,
// statt das Fenster aufzuziehen. Dieselbe Loesung, die die
// Applet-Spalte schon benutzt.
//
// Die Pruefung misst deshalb nicht ein Applet, sondern mehrere — und
// prueft die Eigenschaft, die WIRKLICH traegt: die Untergrenze des
// Fensters muss klein sein. Solange die klein ist, kann keine
// Anordnung das Fenster aufziehen, auch keine, die es heute noch
// nicht gibt.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QPointer>
#include <QScreen>

#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/DvkApplet.h"
#include "gui/applets/RxApplet.h"
#include "gui/applets/VaxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TestAppletWindowSize : public QObject
{
    Q_OBJECT

private slots:

    void noAppletWindowFillsTheScreen()
    {
        RadioModel model;
        model.addSlice();

        QList<QPair<QString, AppletWidget*>> cases;
        cases << qMakePair(QStringLiteral("Rx"),
                           static_cast<AppletWidget*>(
                               new RxApplet(model.slices().value(0), &model)));
        cases << qMakePair(QStringLiteral("Dvk"),
                           static_cast<AppletWidget*>(
                               new DvkApplet(&model, nullptr)));
        cases << qMakePair(QStringLiteral("Vax"),
                           static_cast<AppletWidget*>(
                               new VaxApplet(&model, nullptr)));

        for (auto& c : cases) {
            auto* win = new AppletFloatingWindow(c.second, c.first, 0, nullptr);
            win->show();
            QVERIFY(QTest::qWaitForWindowExposed(win));
            win->applyDefaultSize();

            const QSize avail = win->screen() ? win->screen()->availableSize()
                                              : QSize(1440, 900);

            QVERIFY2(win->height() <= (avail.height() * 2) / 3 + 2,
                     qPrintable(c.first + QStringLiteral(
                         ": das Fenster ist zu hoch — hoechstens zwei "
                         "Drittel des Schirms")));
            QVERIFY2(win->width() <= avail.width() / 2 + 2,
                     qPrintable(c.first + QStringLiteral(
                         ": das Fenster ist zu breit — hoechstens die "
                         "halbe Schirmbreite")));

            // DIE Eigenschaft: eine kleine Untergrenze. Sie ist der
            // Grund, warum keine Anordnung das Fenster aufziehen kann.
            QVERIFY2(win->minimumSizeHint().height() <= 200,
                     qPrintable(c.first + QStringLiteral(
                         ": die Untergrenze ist zu hoch — dann zieht der "
                         "Inhalt das Fenster auf, sobald er waechst")));
            QVERIFY2(win->minimumSizeHint().width() <= 300,
                     qPrintable(c.first + QStringLiteral(
                         ": die Untergrenze ist zu breit")));

            delete win;
        }
    }

    // Der Rollbereich BESITZT sein Widget. Wer beim Andocken nur
    // layout()->removeWidget() ruft, findet das Applet dort nicht —
    // es bliebe Kind des Rollbereichs und stuerbe mit dem Fenster,
    // mitten im Andocken.
    void dockingBackKeepsTheAppletAlive()
    {
        RadioModel model;
        model.addSlice();
        auto* rx = new RxApplet(model.slices().value(0), &model);
        QPointer<RxApplet> alive(rx);

        auto* win = new AppletFloatingWindow(rx, QStringLiteral("Rx"),
                                             0, nullptr);
        AppletWidget* back = win->releaseApplet();
        delete win;

        QVERIFY2(!alive.isNull(),
                 "das Applet MUSS das Andocken ueberleben — sonst ist "
                 "es beim Zurueckdocken weg");
        QCOMPARE(back, static_cast<AppletWidget*>(rx));
        QVERIFY2(rx->parent() == nullptr,
                 "und ohne Elternteil zurueckkommen");
        delete rx;
    }
};

QTEST_MAIN(TestAppletWindowSize)
#include "tst_applet_window_size.moc"
