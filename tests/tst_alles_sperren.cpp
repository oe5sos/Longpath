// tests/tst_alles_sperren.cpp  (Longpath)
//
// Vorschlag 6 der Vereinfachungen vom 2026-11-04 (Betreiber: "entscheide
// selbst"): ein Griff fuer alle Schloesser. Containers > "Lock every
// window in place" sperrt jedes schwebende Fenster, "Unlock every window"
// gibt alles wieder frei. Die Einzelschloesser bleiben.
//
// Dieser Pruefstand loest ein Applet aus der Spalte (wie die Hand), ruft
// die beiden Befehle und liest das Schloss in der Kopfleiste des
// schwebenden Fensters. Alte Fassung: die Befehle gibt es nicht.
//
// Bewusst nicht abgeraeumt (wie tst_dock_ablage_nimmt_fenster_mit).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-04 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QAction>
#include <QPushButton>

#include "gui/MainWindow.h"
#include "gui/WindowChrome.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/GridCellWidget.h"
#include "gui/applets/AppletWidget.h"
#include "FensterFrist.h"

using namespace Longpath;

class TestAllesSperren : public QObject
{
    Q_OBJECT

private:
    MainWindow* m_mw{nullptr};
    AppletFloatingWindow* m_abgeloest{nullptr};

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
        QTest::qWait(300);
        m_abgeloest = loeseEinApplet();
        QVERIFY2(m_abgeloest, "ein Applet muss sich abloesen lassen");
        QTRY_VERIFY_WITH_TIMEOUT(m_abgeloest->isVisible(), 3000);
    }

    void sperrenUndFreigebenGreifenAmSchwebendenFenster()
    {
        auto* tb = m_abgeloest->findChild<WindowTitleBar*>();
        QVERIFY2(tb, "das schwebende Fenster traegt eine Kopfleiste mit Schloss");
        tb->setLocked(false);
        QVERIFY(!tb->isLocked());

        QAction* sperren = m_mw->findChild<QAction*>(QStringLiteral("actLockEveryWindow"));
        QAction* frei    = m_mw->findChild<QAction*>(QStringLiteral("actUnlockEveryWindow"));
        QVERIFY2(sperren && frei, "beide Befehle muessen im Containers-Menue stehen");

        sperren->trigger();
        QVERIFY2(tb->isLocked(), "nach 'Lock every window' muss das Schloss zu sein");
        QVERIFY(m_abgeloest->property("longpathWindowLocked").toBool());

        frei->trigger();
        QVERIFY2(!tb->isLocked(), "nach 'Unlock every window' muss das Schloss offen sein");
    }

    void dieZahlStimmt()
    {
        // Alles schon offen: nichts umzustellen. Dann alles zu: mindestens
        // das abgeloeste Fenster.
        QCOMPARE(m_mw->sperreAlleFensterForTest(false), 0);
        QVERIFY(m_mw->sperreAlleFensterForTest(true) >= 1);
        QCOMPARE(m_mw->sperreAlleFensterForTest(true), 0);
        QVERIFY(m_mw->sperreAlleFensterForTest(false) >= 1);
    }
};

QTEST_MAIN(TestAllesSperren)
#include "tst_alles_sperren.moc"
