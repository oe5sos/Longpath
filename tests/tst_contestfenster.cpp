// tests/tst_contestfenster.cpp  (Longpath)
// no-port-check: Longpath-original test file.
//
// Das Contestprogramm als Fenster in Longpath (2026-11-04). Geprueft wird
// die Bruecke allein, ohne MainWindow: ein eigener Datenordner
// (CONTESTPROGRAMM_DATA_DIR), Fenster auf, zu, wieder auf -- und dass ein
// zweiter Schreiber auf demselben Log abgewiesen wird, so wie das
// eigenstaendige Programm abgewiesen wuerde, wenn das Fenster hier offen
// ist (dieselbe Einzelplatz-Wache, dasselbe Verzeichnis).

#include <QtTest>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QWidget>

#include "ContestFenster.h"
#include "FensterFrist.h"

using namespace Longpath;

class TestContestFenster : public QObject
{
    Q_OBJECT

private:
    // Ein leeres Log oeffnet beim ersten Mal modal die Einstellungen (kein
    // Rufzeichen) und greift nach rigctld und Rotoren. Hier wird das Log
    // vorbelegt wie in test_vorgabe_ueberlappt_nicht des Contestprogramms
    // -- ueber SQLite direkt, damit dieser Pruefstand keine Kopfdatei des
    // Contestprogramms braucht (siehe ContestFenster.h zu den Kollisionen).
    static bool belegeLogVor(const QString& ordner)
    {
        const QString name = QStringLiteral("vorbelegung");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
            db.setDatabaseName(ordner + QStringLiteral("/contestprogramm.sqlite"));
            if (!db.open()) { return false; }
            QSqlQuery q(db);
            if (!q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS settings "
                                       "(key TEXT PRIMARY KEY, value TEXT)"))) {
                return false;
            }
            const QList<QPair<QString, QString>> werte = {
                {QStringLiteral("own_callsign"),     QStringLiteral("OE5SOS")},
                {QStringLiteral("own_grid"),         QStringLiteral("JN67UT")},
                {QStringLiteral("rigctld_host"),     QString()},
                {QStringLiteral("rotor1_enabled"),   QStringLiteral("0")},
                {QStringLiteral("rotor2_enabled"),   QStringLiteral("0")},
                {QStringLiteral("esm_enabled"),      QStringLiteral("0")},
                {QStringLiteral("spectrum_enabled"), QStringLiteral("0")},
            };
            for (const auto& kv : werte) {
                q.prepare(QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
                q.addBindValue(kv.first);
                q.addBindValue(kv.second);
                if (!q.exec()) { return false; }
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(name);
        return true;
    }

private slots:
    void dasVerzeichnisFolgtDerUmgebung()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QCOMPARE(ContestFenster::datenverzeichnis(), ordner.path());
    }

    void aufZuUndWiederAuf()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QVERIFY(belegeLogVor(ordner.path()));

        ContestFenster bruecke;
        QSignalSpy auf(&bruecke, &ContestFenster::geoeffnet);
        QSignalSpy zu(&bruecke, &ContestFenster::geschlossen);
        QVERIFY(!bruecke.istOffen());

        QString grund;
        QVERIFY2(bruecke.oeffne(&grund), qPrintable(grund));
        QVERIFY(bruecke.istOffen());
        QWidget* fenster = bruecke.fensterForTest();
        QVERIFY(fenster);
        QVERIFY(QTest::qWaitForWindowExposed(fenster, kFensterFrist));
        QVERIFY(fenster->isVisible());
        QCOMPARE(auf.count(), 1);

        // Die Datenbank liegt im eigenen Ordner, nicht beim echten Log.
        QVERIFY(QFile::exists(ordner.path() + QStringLiteral("/contestprogramm.sqlite")));

        // Ein zweiter Aufruf holt nur nach vorn, baut nichts Zweites.
        QVERIFY(bruecke.oeffne(&grund));
        QCOMPARE(bruecke.fensterForTest(), fenster);
        QCOMPARE(auf.count(), 1);

        bruecke.schliesse();
        QVERIFY(!bruecke.istOffen());
        QVERIFY(bruecke.fensterForTest() == nullptr);

        QVERIFY2(bruecke.oeffne(&grund), qPrintable(grund));
        QVERIFY(bruecke.istOffen());
        QCOMPARE(auf.count(), 2);
        bruecke.schliesse();
    }

    void einZweiterSchreiberWirdAbgewiesen()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QVERIFY(belegeLogVor(ordner.path()));

        ContestFenster erste;
        QString grund;
        QVERIFY2(erste.oeffne(&grund), qPrintable(grund));

        // Dieselbe Wache, dasselbe Verzeichnis: so saehe es aus, wenn das
        // eigenstaendige Contestprogramm schon liefe.
        ContestFenster zweite;
        QVERIFY2(!zweite.oeffne(&grund),
                 "Zwei Schreiber auf demselben Log -- genau das darf nicht sein");
        QVERIFY2(grund.contains(QStringLiteral("already running")), qPrintable(grund));
        QVERIFY(!zweite.istOffen());

        erste.schliesse();
        // Nach dem Schliessen ist der Platz frei.
        QVERIFY2(zweite.oeffne(&grund), qPrintable(grund));
        zweite.schliesse();
    }
};

QTEST_MAIN(TestContestFenster)
#include "tst_contestfenster.moc"
