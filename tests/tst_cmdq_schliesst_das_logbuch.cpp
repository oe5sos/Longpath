// tests/tst_cmdq_schliesst_das_logbuch.cpp  (Longpath)
//
// Bei Cmd+Q und quit() kommt exit() noch innerhalb von closeEvent():
// ~ContestFenster laeuft nie (gemessen 2026-11-07). Am Abbau des
// Controllers haengt aber die letzte Log-Sicherung samt Kopie in den
// zweiten Ordner, und dort endet auch die Sperre auf dem Log.
//
// Der Pruefstand schliesst das echte Hauptfenster und zerstoert es
// bewusst NICHT, genau so viel erlebt der Betrieb bei Cmd+Q. Das
// Logbuch-Fenster geht auch in der alten Fassung zu, closeEvent raeumt es
// mit den anderen Fenstern ab. Der Controller aber bleibt stehen. Die
// Pruefung sitzt darum an der Sperre, die er haelt: alte Fassung, der
// zweite Oeffner wird abgewiesen.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-07 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QAction>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "gui/MainWindow.h"
#include "gui/applets/KombiApplet.h"
#include "ContestFenster.h"
#include "FensterFrist.h"

using namespace Longpath;

class TestCmdQSchliesstDasLogbuch : public QObject
{
    Q_OBJECT

private:
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

private slots:
    void dasSchliessenDesHauptfensters_macht_das_logbuch_zu()
    {
        QVERIFY(m_ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", m_ordner.path().toUtf8());
        QVERIFY(belegeLogVor(m_ordner.path()));

        // Bewusst nicht abgeraeumt, wie tst_dock_ablage_nimmt_fenster_mit.
        auto* fenster = new MainWindow();
        fenster->resize(1280, 800);
        fenster->show();
        QVERIFY2(QTest::qWaitForWindowExposed(fenster, kFensterFrist),
                 "das Hauptfenster muss ueberhaupt erscheinen");
        QTest::qWait(300);

        QAction* chat = fenster->findChild<QAction*>(
            QStringLiteral("actContainersContestPanel_chat"));
        QVERIFY(chat);
        chat->setChecked(true);
        QTest::qWait(300);

        ContestFenster* bruecke = fenster->contestFensterForTest();
        QVERIFY2(bruecke, "der Haken muss das Logbuch laden");
        QVERIFY2(bruecke->istOffen(), "ohne offenes Logbuch sagte die Pruefung nichts");

        // Das Protokollfeld in Combined, das schwebt: so stand es, wenn
        // Longpath beim Beenden abstuerzte. Das Gehaeuse wurde geloescht,
        // und ~MainWindow des Logbuchs griff in das tote Feld.
        fenster->holeContestInsKombiFeldForTest(QStringLiteral("unifiedlog"));
        QTest::qWait(100);
        QVERIFY(bruecke->panelInFremdemGehaeuse(QStringLiteral("unifiedlog")));
        fenster->detachAppletForTest(fenster->kombiFeldForTest(1));
        QTest::qWait(100);

        fenster->close();

        QVERIFY(!bruecke->istOffen());

        // Die Sperre ist frei: der Controller ist abgebaut, und mit ihm
        // ist die letzte Sicherung geschrieben. Bei Cmd+Q liefe das sonst
        // nie mehr.
        ContestFenster zweiter;
        QString grund;
        QVERIFY2(zweiter.oeffne(&grund), qPrintable(grund));
    }
};

QTEST_MAIN(TestCmdQSchliesstDasLogbuch)
#include "tst_cmdq_schliesst_das_logbuch.moc"
