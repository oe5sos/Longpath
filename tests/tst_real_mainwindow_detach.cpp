// =================================================================
// tests/tst_real_mainwindow_detach.cpp  (Longpath)
// =================================================================
//
// Der erste Test, der das ECHTE Hauptfenster baut.
//
// ── Warum es ihn gibt ───────────────────────────────────────────────
//
// Am 2026-08-20 hat der Betreiber viermal gemeldet, das Abloesen gehe
// nicht, und zuletzt: „test selbst, im richtigen app". Er hatte jedes
// Mal recht, und der Grund war immer derselbe.
//
// In tests/ gab es 705 Pruefungen und KEINE EINZIGE, die MainWindow
// anlegt. Jede Fensterpruefung baute ein AppletFloatingWindow von
// Hand — und ging damit an allem vorbei, was im echten Programm auf
// dem Weg dorthin liegt: dem Rasterfeld mit seiner Kopfleiste, dem
// Weiterreichen der Signale, detachApplet, ensureOnVisibleScreen, der
// Sichtbarkeitspruefung. Genau dort sassen die Fehler.
//
// Ein Ersatzaufbau prueft den Ersatzaufbau. Diese Pruefung geht den
// Weg, den eine Hand geht: Hauptfenster anzeigen, den Knopf in der
// Kopfleiste suchen, druecken, nachmessen.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QPushButton>
#include <QScreen>

#include "gui/MainWindow.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/GridCellWidget.h"
#include "gui/applets/AppletWidget.h"

using namespace Longpath;

class TestRealMainWindowDetach : public QObject
{
    Q_OBJECT

private slots:

    void clickingDetachInTheRealAppGivesASmallWindow()
    {
        // Bewusst nicht abgeraeumt: MainWindow startet Arbeitsfaeden
        // (SpectrumThread und andere), deren geordnetes Ende an einer
        // laufenden Ereignisschleife haengt. In einem Testlauf, der
        // gleich darauf endet, ist das Aufraeumen selbst die
        // Fehlerquelle — nicht das, was hier geprueft wird.
        auto* mwp = new MainWindow();
        MainWindow& mw = *mwp;
        mw.resize(1280, 800);
        mw.show();
        QVERIFY2(QTest::qWaitForWindowExposed(&mw),
                 "das Hauptfenster muss ueberhaupt erscheinen");
        QTest::qWait(300);   // Applets bauen sich beim Anzeigen auf

        auto* panel = mw.findChild<AppletPanelWidget*>();
        QVERIFY2(panel, "es muss eine Applet-Spalte geben");

        const QList<GridCellWidget*> cells =
            panel->findChildren<GridCellWidget*>();
        QVERIFY2(!cells.isEmpty(),
                 "in der Spalte muss mindestens ein Applet stehen");

        // Den Knopf so suchen, wie eine Hand ihn sucht.
        QPushButton* detach = nullptr;
        GridCellWidget* used = nullptr;
        for (GridCellWidget* c : cells) {
            if (!c->titleBar() || !c->titleBar()->isVisible()) { continue; }
            for (QPushButton* b : c->titleBar()->findChildren<QPushButton*>()) {
                if (b->text() == QStringLiteral("↗") && b->isVisible()) {
                    detach = b; used = c; break;
                }
            }
            if (detach) { break; }
        }
        QVERIFY2(detach,
                 "in einer sichtbaren Kopfleiste MUSS ein Ablöseknopf "
                 "stehen — bis 2026-08-20 stand er in einer Kopfleiste, "
                 "die kein Applet mehr benutzt");
        Q_UNUSED(used);

        const int before = mw.findChildren<AppletFloatingWindow*>().size();
        QTest::mouseClick(detach, Qt::LeftButton);
        QTest::qWait(400);

        const QList<AppletFloatingWindow*> wins =
            mw.findChildren<AppletFloatingWindow*>();
        QVERIFY2(wins.size() == before + 1,
                 "der Druck MUSS genau ein neues Fenster ergeben");

        AppletFloatingWindow* win = wins.last();
        QVERIFY2(win->isVisible(), "und es muss zu sehen sein");

        const QSize avail = win->screen() ? win->screen()->availableSize()
                                          : QSize(1440, 900);
        QVERIFY2(win->height() <= (avail.height() * 2) / 3 + 4,
                 qPrintable(QStringLiteral(
                     "das Fenster fuellt den Schirm: %1 hoch, erlaubt "
                     "waeren %2").arg(win->height())
                     .arg((avail.height() * 2) / 3)));
        QVERIFY2(win->width() <= avail.width() / 2 + 4,
                 qPrintable(QStringLiteral(
                     "das Fenster ist zu breit: %1, erlaubt waeren %2")
                     .arg(win->width()).arg(avail.width() / 2)));

        // Und es muss sich von Hand klein ziehen lassen.
        QVERIFY2(win->minimumSizeHint().height() <= 220,
                 qPrintable(QStringLiteral(
                     "die Untergrenze ist zu hoch (%1) — dann zieht der "
                     "Inhalt das Fenster wieder auf")
                     .arg(win->minimumSizeHint().height())));

        // Rahmenlos, mit eigener Leiste und Anfasser.
        QVERIFY2(win->windowFlags() & Qt::FramelessWindowHint,
                 "rahmenlos, sonst stehen zwei Leisten uebereinander");
        QVERIFY2(win->findChild<QWidget*>() != nullptr, "Inhalt vorhanden");
    }
};

QTEST_MAIN(TestRealMainWindowDetach)
#include "tst_real_mainwindow_detach.moc"
