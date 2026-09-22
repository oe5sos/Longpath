// SPDX-License-Identifier: GPL-3.0-or-later
//
// Sichern, ohne auf das Beenden zu warten (2026-09-17).
//
// Betreiber: "wichtig ist, dass sich das programm immer automatisch
// sichert. sollte ein stromausfall oder sonstiges sein, sollte man
// immer auf die daten zurueck greifen koennen!"
//
// Drei Dinge werden hier festgehalten:
//   1. isDirty(): ein setValue() macht die Einstellungen "ungesichert",
//      ein save() macht sie wieder gesichert, ein setValue() mit dem
//      alten Wert aendert nichts daran (sonst schriebe der Autosave
//      jede Minute, obwohl nichts passiert ist).
//   2. Die Tageskopie: beim ersten save() eines Tages liegt danach
//      "<Datei>.<JJJJ-MM-TT>" neben der Datei -- mit dem Stand von
//      VOR dem Schreiben, das ist der, auf den man zurueckgreift.
//   3. Die Rotation: mehr als kDailyBackupsToKeep Tageskopien bleiben
//      nicht liegen, die aeltesten gehen zuerst; Handkopien ohne
//      Datumsmuster ("…settings.vor-…") bleiben unberuehrt.

#include <QtTest>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include "core/AppSettings.h"

using namespace Longpath;

class TestAppSettingsAutosave : public QObject
{
    Q_OBJECT
private slots:
    void dirtyFollowsChangesNotWrites()
    {
        QTemporaryDir tmp;
        AppSettings s(tmp.filePath(QStringLiteral("Longpath.settings")));
        QVERIFY(!s.isDirty());
        s.setValue(QStringLiteral("Test/Key"), QStringLiteral("eins"));
        QVERIFY(s.isDirty());
        s.save();
        QVERIFY(!s.isDirty());
        s.setValue(QStringLiteral("Test/Key"), QStringLiteral("eins"));   // derselbe Wert
        QVERIFY2(!s.isDirty(), "ein unveraenderter Wert darf nicht als Aenderung zaehlen");
        s.setValue(QStringLiteral("Test/Key"), QStringLiteral("zwei"));
        QVERIFY(s.isDirty());
        s.save();
        s.remove(QStringLiteral("Test/Key"));
        QVERIFY(s.isDirty());
        s.remove(QStringLiteral("Test/Gibtesnicht"));
        s.save();
        QVERIFY(!s.isDirty());
    }

    void firstSaveOfTheDayKeepsYesterdaysState()
    {
        QTemporaryDir tmp;
        const QString path = tmp.filePath(QStringLiteral("Longpath.settings"));
        AppSettings s(path);
        s.setValue(QStringLiteral("Test/Key"), QStringLiteral("gestern"));
        s.save();                                    // erste Datei
        // Vor dem ersten Schreiben eines Tages gab es die Datei schon:
        // jetzt aendern und nochmal schreiben -> die Tageskopie traegt
        // den Stand von VOR diesem Schreiben.
        QFile::remove(path + QLatin1Char('.')
                      + QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")));
        s.setValue(QStringLiteral("Test/Key"), QStringLiteral("heute"));
        s.save();
        const QStringList backups = s.dailyBackups();
        QCOMPARE(backups.size(), 1);
        QVERIFY(backups.first().endsWith(
            QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));
        AppSettings copy(backups.first());
        copy.load();
        QCOMPARE(copy.value(QStringLiteral("Test/Key")).toString(), QStringLiteral("gestern"));
        // Ein zweites save() am selben Tag ueberschreibt die Tageskopie NICHT.
        s.setValue(QStringLiteral("Test/Key"), QStringLiteral("spaeter"));
        s.save();
        AppSettings again(backups.first());
        again.load();
        QCOMPARE(again.value(QStringLiteral("Test/Key")).toString(), QStringLiteral("gestern"));
    }

    void onlyTheNewestDailyBackupsStay()
    {
        QTemporaryDir tmp;
        const QString path = tmp.filePath(QStringLiteral("Longpath.settings"));
        AppSettings s(path);
        s.setValue(QStringLiteral("Test/Key"), QStringLiteral("x"));
        s.save();
        // Zwanzig alte Tageskopien und eine Handkopie hinlegen.
        for (int i = 1; i <= 20; ++i) {
            const QString d = QDate::currentDate().addDays(-i)
                                  .toString(QStringLiteral("yyyy-MM-dd"));
            QFile f(path + QLatin1Char('.') + d);
            QVERIFY(f.open(QIODevice::WriteOnly)); f.write("alt"); f.close();
        }
        QFile hand(path + QStringLiteral(".vor-reparatur"));
        QVERIFY(hand.open(QIODevice::WriteOnly)); hand.write("hand"); hand.close();

        s.setValue(QStringLiteral("Test/Key"), QStringLiteral("y"));
        s.save();   // legt die heutige an und raeumt auf
        const QStringList backups = s.dailyBackups();
        QCOMPARE(backups.size(), AppSettings::kDailyBackupsToKeep);
        // Die aelteste verbliebene ist juenger als die aelteste geloeschte.
        QVERIFY(backups.first().endsWith(
            QDate::currentDate().addDays(-(AppSettings::kDailyBackupsToKeep - 1))
                .toString(QStringLiteral("yyyy-MM-dd"))));
        QVERIFY2(QFileInfo::exists(path + QStringLiteral(".vor-reparatur")),
                 "Handkopien des Betreibers bleiben liegen");
    }
};

QTEST_MAIN(TestAppSettingsAutosave)
#include "tst_app_settings_autosave.moc"
