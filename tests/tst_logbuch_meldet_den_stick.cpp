// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ohne Stick keine zweite Sicherung -- und in Longpath sah man das nicht.
// Am 07.11. stand „Zweiter Sicherungsordner nicht erreichbar" stundenlang
// nur im Protokoll: eigenstaendig zeigt das Logbuch es in seiner
// Statuszeile, und die ist in Longpath nicht zu sehen.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-08 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include <QDir>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "ContestFenster.h"

using namespace Longpath;

class TestLogbuchMeldetDenStick : public QObject
{
    Q_OBJECT

private:
    static bool belegeLogVor(const QString& ordner, const QString& stick)
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
            {QStringLiteral("backup_mirror_dir"), stick},
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
    void ohneStickMeldetEsSichBeimOeffnen()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        const QString stick = QStringLiteral("/Volumes/gibt-es-nicht-4711/Contest-Log-Sicherung");
        QVERIFY(belegeLogVor(ordner.path(), stick));

        ContestFenster fenster;
        QSignalSpy warnung(&fenster, &ContestFenster::sicherungsWarnung);
        QString grund;
        QVERIFY2(fenster.oeffne(&grund), qPrintable(grund));
        QTRY_COMPARE_WITH_TIMEOUT(warnung.size(), 1, 5000);
        QVERIFY2(warnung.first().first().toString().contains(stick),
                 qPrintable(warnung.first().first().toString()));
        fenster.schliesse();
    }

    // Der Startcheck nennt den fehlenden Stick -- und er ist von Longpath aus
    // zu oeffnen, obwohl das Menue des Logbuchs dort fehlt.
    void derStartcheckGehtAuf()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QVERIFY(belegeLogVor(ordner.path(), QStringLiteral("/Volumes/gibt-es-nicht-4711/x")));

        ContestFenster fenster;
        QString grund;
        QVERIFY2(fenster.oeffne(&grund), qPrintable(grund));
        QVERIFY(fenster.zeigeStartcheck());
        QWidget* bereit = fenster.startcheckForTest();
        QVERIFY(bereit);
        QTRY_VERIFY_WITH_TIMEOUT(bereit->isVisible(), 5000);
        fenster.schliesse();
    }

    void mitStickBleibtEsStill()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        qputenv("CONTESTPROGRAMM_DATA_DIR", ordner.path().toUtf8());
        QTemporaryDir stick;
        QVERIFY(stick.isValid());
        QVERIFY(belegeLogVor(ordner.path(), stick.path()));

        ContestFenster fenster;
        QSignalSpy warnung(&fenster, &ContestFenster::sicherungsWarnung);
        QString grund;
        QVERIFY2(fenster.oeffne(&grund), qPrintable(grund));
        QTest::qWait(500);
        QCOMPARE(warnung.size(), 0);
        fenster.schliesse();
    }
};

QTEST_MAIN(TestLogbuchMeldetDenStick)
#include "tst_logbuch_meldet_den_stick.moc"
