// tests/tst_dock_ablage_nimmt_fenster_mit.cpp  (Longpath)
//
// Betreiber am 2026-11-03: "wenn ich longpath in die taskleiste legen will,
// bleiben die fenster offen. das ist falsch."
//
// Die abgeloesten Applets, der schwebende Panadapter, Rotor/Log und die
// Seitenflaeche sind Qt::Tool-Fenster mit dem Hauptfenster als Elternteil.
// macOS nimmt sie beim Minimieren des Elternfensters NICHT mit: das
// Hauptfenster geht ins Dock, die Kinder bleiben am Schreibtisch stehen.
//
// Dieser Pruefstand loest ein Applet aus der Spalte, wie es die Hand tut
// (Ablöseknopf in der Kopfleiste), minimiert das Hauptfenster und erwartet,
// dass das abgeloeste Fenster verschwindet -- und beim Zurueckholen wieder
// da ist. Alte Fassung: das Fenster bleibt sichtbar, Fall 1 faellt um.
//
// Bewusst nicht abgeraeumt (wie tst_real_mainwindow_detach): MainWindow
// startet Arbeitsfaeden, deren geordnetes Ende an einer laufenden
// Ereignisschleife haengt; in einem Testlauf, der gleich darauf endet, ist
// das Aufraeumen selbst die Fehlerquelle.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-03 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QPushButton>

#include "gui/MainWindow.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/GridCellWidget.h"
#include "gui/applets/AppletWidget.h"
#include "FensterFrist.h"

using namespace Longpath;

class TestDockAblageNimmtFensterMit : public QObject
{
    Q_OBJECT

private:
    MainWindow* m_mw{nullptr};
    AppletFloatingWindow* m_abgeloest{nullptr};

    // Ein Applet aus der Spalte loesen, ueber den Knopf in der Kopfleiste.
    AppletFloatingWindow* loeseEinApplet()
    {
        auto* panel = m_mw->findChild<AppletPanelWidget*>();
        if (!panel) { return nullptr; }
        QPushButton* detach = nullptr;
        for (GridCellWidget* c : panel->findChildren<GridCellWidget*>()) {
            if (!c->titleBar() || !c->titleBar()->isVisible()) { continue; }
            for (QPushButton* b : c->titleBar()->findChildren<QPushButton*>()) {
                if (b->text() == QStringLiteral("↗") && b->isVisible()) {
                    detach = b; break;
                }
            }
            if (detach) { break; }
        }
        if (!detach) { return nullptr; }
        const int vorher = m_mw->findChildren<AppletFloatingWindow*>().size();
        QTest::mouseClick(detach, Qt::LeftButton);
        QTest::qWait(200);
        const QList<AppletFloatingWindow*> wins =
            m_mw->findChildren<AppletFloatingWindow*>();
        if (wins.size() <= vorher) { return nullptr; }
        return wins.last();
    }

private slots:

    void initTestCase()
    {
        m_mw = new MainWindow();
        m_mw->resize(1280, 800);
        m_mw->show();
        QVERIFY2(QTest::qWaitForWindowExposed(m_mw, kFensterFrist),
                 "das Hauptfenster muss ueberhaupt erscheinen");
        QTest::qWait(300);   // Applets bauen sich beim Anzeigen auf

        m_abgeloest = loeseEinApplet();
        QVERIFY2(m_abgeloest, "ein Applet muss sich abloesen lassen");
        QTRY_VERIFY_WITH_TIMEOUT(m_abgeloest->isVisible(), 3000);
    }

    // ── Fall 1: minimieren nimmt das Fenster mit ─────────────────────────
    void minimierenNimmtDasAbgeloesteFensterMit()
    {
        QVERIFY(m_abgeloest->isVisible());
        QCOMPARE(m_mw->schwebendeFensterImDockForTest(), 0);

        m_mw->showMinimized();
        QCoreApplication::processEvents();

        QVERIFY2(!m_abgeloest->isVisible(),
                 "Das abgeloeste Applet steht nach dem Minimieren noch am "
                 "Schreibtisch -- genau der Befund des Betreibers vom "
                 "2026-11-03");
        QCOMPARE(m_mw->schwebendeFensterImDockForTest(), 1);
    }

    // ── Fall 2: zurueckholen bringt es wieder ────────────────────────────
    void zurueckholenBringtEsWieder()
    {
        QVERIFY(!m_abgeloest->isVisible());

        m_mw->showNormal();
        QCoreApplication::processEvents();

        QTRY_VERIFY2_WITH_TIMEOUT(m_abgeloest->isVisible(),
                 "Nach dem Zurueckholen aus dem Dock muss das abgeloeste "
                 "Applet wieder stehen", 3000);
        QCOMPARE(m_mw->schwebendeFensterImDockForTest(), 0);
    }

    // ── Fall 3: was der Betreiber selbst zu hatte, kommt NICHT zurueck ───
    void wasVorherZuWarBleibtZu()
    {
        m_abgeloest->hide();
        QVERIFY(!m_abgeloest->isVisible());

        m_mw->showMinimized();
        QCoreApplication::processEvents();
        QCOMPARE(m_mw->schwebendeFensterImDockForTest(), 0);

        m_mw->showNormal();
        QCoreApplication::processEvents();
        QTest::qWait(100);
        QVERIFY2(!m_abgeloest->isVisible(),
                 "Ein vom Betreiber geschlossenes Fenster darf das Dock "
                 "nicht wieder oeffnen");
    }
};

QTEST_MAIN(TestDockAblageNimmtFensterMit)
#include "tst_dock_ablage_nimmt_fenster_mit.moc"
