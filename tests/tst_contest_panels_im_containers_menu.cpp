// tests/tst_contest_panels_im_containers_menu.cpp  (Longpath)
//
// Betreiber am 2026-11-04, mit dem Containers-Menue auf dem Bild: "finde
// bei longpath jetzt nichts vom contestprogramm". Die Panels des
// Contestprogramms standen nur unter Tools > Contest logbook panels;
// hier, wo ALLE Panels mit Haken stehen, hat er sie gesucht.
//
// Dieser Pruefstand prueft am echten Hauptfenster: im Containers-Menue
// gibt es je Panel des Contestprogramms einen abhakbaren Eintrag, und
// ein Haken holt das Panel als eigenes Fenster, OHNE das Logbuch-Fenster
// zu zeigen. Alte Fassung: die Eintraege fehlen, Fall 1 faellt um.
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
#include <QMenu>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "gui/MainWindow.h"
#include "ContestFenster.h"
#include "FensterFrist.h"

using namespace Longpath;

class TestContestPanelsImContainersMenu : public QObject
{
    Q_OBJECT

private:
    MainWindow* m_mw{nullptr};
    QTemporaryDir m_ordner;

    // Wie tst_contestfenster: ein leeres Log oeffnet modal die
    // Einstellungen und greift nach rigctld und Rotoren.
    static bool belegeLogVor(const QString& ordner)
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                    QStringLiteral("vorbelegung"));
        db.setDatabaseName(ordner + QStringLiteral("/contestprogramm.sqlite"));
        if (!db.open()) { return false; }
        QSqlQuery q(db);
        bool ok = q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT)"));
        const QList<QPair<QString, QString>> werte = {
            {QStringLiteral("own_callsign"), QStringLiteral("OE5SOS")},
            {QStringLiteral("own_grid"), QStringLiteral("JN67UT")},
            {QStringLiteral("rigctld_host"), QString()},
            {QStringLiteral("rotor1_enabled"), QStringLiteral("0")},
            {QStringLiteral("rotor2_enabled"), QStringLiteral("0")},
            {QStringLiteral("esm_enabled"), QStringLiteral("0")},
            {QStringLiteral("spectrum_enabled"), QStringLiteral("0")},
        };
        for (const auto& kv : werte) {
            q.prepare(QStringLiteral("INSERT OR REPLACE INTO settings(key, value) VALUES(?, ?)"));
            q.addBindValue(kv.first);
            q.addBindValue(kv.second);
            ok = ok && q.exec();
        }
        db.close();
        QSqlDatabase::removeDatabase(QStringLiteral("vorbelegung"));
        return ok;
    }

    QMenu* containersMenu() const
    {
        for (QMenu* m : m_mw->findChildren<QMenu*>()) {
            if (m->title().remove(QLatin1Char('&')) == QStringLiteral("Containers")) {
                return m;
            }
        }
        return nullptr;
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", m_ordner.path().toUtf8());
        QVERIFY(belegeLogVor(m_ordner.path()));

        m_mw = new MainWindow();
        m_mw->resize(1280, 800);
        m_mw->show();
        QVERIFY2(QTest::qWaitForWindowExposed(m_mw, kFensterFrist),
                 "das Hauptfenster muss ueberhaupt erscheinen");
        QTest::qWait(300);
    }

    // ── Fall 1: je Panel ein Eintrag mit Haken im Containers-Menue ───────
    void jedesPanelStehtImContainersMenu()
    {
        QMenu* menu = containersMenu();
        QVERIFY2(menu, "das Containers-Menue muss da sein");
        const QList<ContestFenster::PanelEintrag> panels = ContestFenster::panels();
        QVERIFY(panels.size() >= 8);
        for (const ContestFenster::PanelEintrag& e : panels) {
            QAction* a = m_mw->findChild<QAction*>(
                QStringLiteral("actContainersContestPanel_%1").arg(e.id));
            QVERIFY2(a, qPrintable(QStringLiteral("Eintrag fuer '%1' fehlt im Containers-Menue").arg(e.id)));
            QVERIFY(a->isCheckable());
            QCOMPARE(a->text(), e.titel);
            QVERIFY2(menu->actions().contains(a), "der Eintrag muss IM Containers-Menue haengen");
        }
    }

    // ── Fall 2: der Haken holt das Panel als eigenes Fenster ─────────────
    void derHakenHoltDasPanelAlsFenster()
    {
        QAction* chat = m_mw->findChild<QAction*>(
            QStringLiteral("actContainersContestPanel_chat"));
        QVERIFY(chat);
        chat->setChecked(true);
        QTest::qWait(300);

        ContestFenster* bruecke = m_mw->contestFensterForTest();
        QVERIFY2(bruecke, "der Haken muss das Logbuch laden");
        QVERIFY(bruecke->istPanelSchwebend(QStringLiteral("chat")));
        QWidget* fenster = bruecke->panelFenster(QStringLiteral("chat"));
        QVERIFY(fenster && fenster->isWindow());
        QTRY_VERIFY_WITH_TIMEOUT(fenster->isVisible(), 3000);
        QVERIFY2(!bruecke->fenster()->isVisible(), "das Logbuch-Fenster bleibt zu");

        // Beim Aufklappen zieht das Menue den Haken nach.
        QMenu* menu = containersMenu();
        QVERIFY(menu);
        emit menu->aboutToShow();
        QVERIFY(chat->isChecked());

        chat->setChecked(false);
        QTest::qWait(200);
        QVERIFY(!bruecke->istPanelSchwebend(QStringLiteral("chat")));
    }
};

QTEST_MAIN(TestContestPanelsImContainersMenu)
#include "tst_contest_panels_im_containers_menu.moc"
