// =================================================================
// tests/tst_settings_backup.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis source (tests for the port of):
//   Project Files/Source/Console/clsDBMan.cs (the backup half of DBMan)
//   original licence from Thetis source is included in the ported file
//   (src/core/SettingsBackup.h)
//
// SettingsBackup against a temporary settings file:
//   * takeBackup copies the file to backups/settings_backup_<epoch>.xml
//     and writes the {Description, Auto} sidecar; the flush hook runs
//     first; an empty description is a refusal
//   * a second copy in the same second gets the _1 suffix
//   * orderedBackups lists newest first, reads the sidecar, and falls
//     back to "Default"/not auto without one
//   * rename keeps the auto flag; same or empty description is a no-op
//   * remove takes the sidecar along and refuses paths outside the folder
//   * export copies, restore takes a "Before restore" copy and replaces
//     the live file
//   * the prune: everything of the last 7 days stays, one per week
//     beyond that, and only automatic copies (Auto, or described
//     "Startup"/"Shutdown") are ever removed; off by default
//
// no-port-check: tests a Thetis port (see core/SettingsBackup.h); registered in THETIS-PROVENANCE.md
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================
/*  clsDBMan.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2026 Original authors
Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "core/SettingsBackup.h"

using namespace Longpath;

namespace {

struct Rig {
    QTemporaryDir dir;
    QString settingsPath;

    Rig()
    {
        settingsPath = dir.path() + QStringLiteral("/Longpath.settings");
        writeSettings(QStringLiteral("<Settings><A>1</A></Settings>"));
    }

    void writeSettings(const QString& text) const
    {
        QFile f(settingsPath);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(text.toUtf8());
    }

    static QString read(const QString& path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) { return {}; }
        return QString::fromUtf8(f.readAll());
    }

    QString backupsDir() const { return dir.path() + QStringLiteral("/backups"); }

    // A backup as the program would have written it at `epoch`.
    QString plant(qint64 epoch, const QString& description, bool automatic,
                  bool withSidecar = true) const
    {
        QDir().mkpath(backupsDir());
        const QString path = SettingsBackup::createUniqueFilename(backupsDir(), epoch);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) { return {}; }
        f.write("<Settings/>");
        f.close();
        if (withSidecar) {
            QJsonObject o;
            o.insert(QStringLiteral("Description"), description);
            o.insert(QStringLiteral("Auto"), automatic);
            QFile j(SettingsBackup::sidecarPath(path));
            if (!j.open(QIODevice::WriteOnly)) { return {}; }
            j.write(QJsonDocument(o).toJson());
        }
        return path;
    }
};

} // namespace

class TestSettingsBackup : public QObject {
    Q_OBJECT

private slots:
    void takeBackupCopiesTheFileAndWritesTheSidecar()
    {
        Rig rig;
        SettingsBackup b(rig.settingsPath);
        int flushed = 0;
        b.setFlushHook([&]() { ++flushed; });

        QString err;
        const QString path = b.takeBackup(QStringLiteral("Before the contest"), false, &err);
        QVERIFY2(!path.isEmpty(), qPrintable(err));
        QCOMPARE(flushed, 1);
        QVERIFY(path.startsWith(rig.backupsDir() + QStringLiteral("/settings_backup_")));
        QVERIFY(path.endsWith(QStringLiteral(".xml")));
        QCOMPARE(Rig::read(path), QStringLiteral("<Settings><A>1</A></Settings>"));

        // The sidecar, with Thetis's two keys.
        const QString json = Rig::read(SettingsBackup::sidecarPath(path));
        const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
        QCOMPARE(o.value(QStringLiteral("Description")).toString(), QStringLiteral("Before the contest"));
        QCOMPARE(o.value(QStringLiteral("Auto")).toBool(true), false);

        // Listed, with what the sidecar says.
        const QList<SettingsBackupInfo> list = b.orderedBackups();
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().fullFilePath, path);
        QCOMPARE(list.first().description, QStringLiteral("Before the contest"));
        QCOMPARE(list.first().automatic, false);
        QVERIFY(list.first().ageSeconds >= 0 && list.first().ageSeconds < 60);
    }

    void emptyDescriptionIsARefusal()
    {
        Rig rig;
        SettingsBackup b(rig.settingsPath);
        QString err;
        QVERIFY(b.takeBackup(QString(), false, &err).isEmpty());
        QVERIFY(!err.isEmpty());
        QVERIFY(!QDir(rig.backupsDir()).exists()
                || QDir(rig.backupsDir()).entryList(QDir::Files).isEmpty());
    }

    // From Thetis clsDBMan.cs:1333-1351 [@852bf0e] createUniqueFilename:
    // the counter suffix when the second lands in the same second.
    void sameSecondGetsACounterSuffix()
    {
        Rig rig;
        QDir().mkpath(rig.backupsDir());
        const QString first = SettingsBackup::createUniqueFilename(rig.backupsDir(), 1700000000);
        QCOMPARE(first, rig.backupsDir() + QStringLiteral("/settings_backup_1700000000.xml"));
        QFile f(first);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        const QString second = SettingsBackup::createUniqueFilename(rig.backupsDir(), 1700000000);
        QCOMPARE(second, rig.backupsDir() + QStringLiteral("/settings_backup_1700000000_1.xml"));
        QFile g(second);
        QVERIFY(g.open(QIODevice::WriteOnly));
        g.close();
        QCOMPARE(SettingsBackup::createUniqueFilename(rig.backupsDir(), 1700000000),
                 rig.backupsDir() + QStringLiteral("/settings_backup_1700000000_2.xml"));

        // Both parse: parts[2] is the epoch for the plain and the _1 name.
        SettingsBackup b(rig.settingsPath);
        const QList<SettingsBackupInfo> list = b.orderedBackups();
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.at(0).secondsSinceEpoch, qint64(1700000000));
        QCOMPARE(list.at(1).secondsSinceEpoch, qint64(1700000000));
    }

    void listIsNewestFirstAndDefaultsWithoutSidecar()
    {
        Rig rig;
        rig.plant(1700000000, QStringLiteral("old"), false);
        rig.plant(1700500000, QStringLiteral("newer"), true);
        rig.plant(1700250000, QString(), false, /*withSidecar=*/false);
        // Something else in the folder is ignored.
        QFile other(rig.backupsDir() + QStringLiteral("/notes.txt"));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.close();

        SettingsBackup b(rig.settingsPath);
        const QList<SettingsBackupInfo> list = b.orderedBackups();
        QCOMPARE(list.size(), 3);
        QCOMPARE(list.at(0).secondsSinceEpoch, qint64(1700500000));
        QCOMPARE(list.at(0).description, QStringLiteral("newer"));
        QCOMPARE(list.at(0).automatic, true);
        QCOMPARE(list.at(1).secondsSinceEpoch, qint64(1700250000));
        QCOMPARE(list.at(1).description, QStringLiteral("Default"));
        QCOMPARE(list.at(1).automatic, false);
        QCOMPARE(list.at(2).secondsSinceEpoch, qint64(1700000000));
        QCOMPARE(list.at(0).dateTimeOfBackup.toUTC().toSecsSinceEpoch(), qint64(1700500000));
    }

    void renameKeepsTheAutoFlag()
    {
        Rig rig;
        const QString path = rig.plant(1700000000, QStringLiteral("Startup"), true);
        SettingsBackup b(rig.settingsPath);

        QVERIFY(!b.renameBackup(path, QString()));                       // empty: no
        QVERIFY(!b.renameBackup(path, QStringLiteral("Startup")));       // unchanged: no
        QVERIFY(b.renameBackup(path, QStringLiteral("Keep this one")));
        const QList<SettingsBackupInfo> list = b.orderedBackups();
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().description, QStringLiteral("Keep this one"));
        QCOMPARE(list.first().automatic, true);
        QVERIFY(!b.renameBackup(rig.backupsDir() + QStringLiteral("/settings_backup_1.xml"),
                                QStringLiteral("nothing there")));
    }

    void removeTakesTheSidecarAlongAndStaysInTheFolder()
    {
        Rig rig;
        const QString a = rig.plant(1700000000, QStringLiteral("a"), false);
        const QString c = rig.plant(1700000002, QStringLiteral("c"), false);
        SettingsBackup b(rig.settingsPath);

        b.removeBackups({a, rig.settingsPath});
        QVERIFY(!QFileInfo::exists(a));
        QVERIFY(!QFileInfo::exists(SettingsBackup::sidecarPath(a)));
        QVERIFY(QFileInfo::exists(c));
        // The live settings file is outside the folder: untouched.
        QVERIFY(QFileInfo::exists(rig.settingsPath));
        QCOMPARE(b.orderedBackups().size(), 1);
    }

    void exportCopiesAndRestoreReplacesTheLiveFile()
    {
        Rig rig;
        SettingsBackup b(rig.settingsPath);
        const QString path = b.takeBackup(QStringLiteral("golden"), false);
        QVERIFY(!path.isEmpty());

        const QString dest = rig.dir.path() + QStringLiteral("/exported.xml");
        QString err;
        QVERIFY2(b.exportBackup(path, dest, &err), qPrintable(err));
        QCOMPARE(Rig::read(dest), QStringLiteral("<Settings><A>1</A></Settings>"));
        // Exporting over an existing file replaces it.
        QVERIFY(b.exportBackup(path, dest, &err));
        QVERIFY(!b.exportBackup(rig.backupsDir() + QStringLiteral("/nope.xml"), dest, &err));

        // The live file moves on, then the golden copy comes back.
        rig.writeSettings(QStringLiteral("<Settings><A>2</A></Settings>"));
        int flushed = 0;
        b.setFlushHook([&]() { ++flushed; });
        QVERIFY2(b.restore(path, &err), qPrintable(err));
        QCOMPARE(flushed, 1);
        QCOMPARE(Rig::read(rig.settingsPath), QStringLiteral("<Settings><A>1</A></Settings>"));

        // ... and the state being replaced was kept as "Before restore",
        // a manual copy (never pruned), newest in the list.
        const QList<SettingsBackupInfo> list = b.orderedBackups();
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.first().description, QStringLiteral("Before restore"));
        QCOMPARE(list.first().automatic, false);
        QCOMPARE(Rig::read(list.first().fullFilePath), QStringLiteral("<Settings><A>2</A></Settings>"));

        QVERIFY(!b.restore(rig.backupsDir() + QStringLiteral("/nope.xml"), &err));
    }

    // From Thetis clsDBMan.cs:1929-2010 [@852bf0e] pruneForGFS, against
    // a fixed clock: everything of the last 7 days stays; older than
    // that, one per (year, week); older than 30 days, one per month on
    // top; only automatic copies go.
    void pruneKeepsSevenDaysThenOnePerWeek()
    {
        Rig rig;
        // Monday 2026-09-14 12:00 local as "now"; ages in days.
        const QDateTime now(QDate(2026, 9, 14), QTime(12, 0), QTimeZone::LocalTime);
        auto at = [&](double daysAgo) {
            return now.toUTC().toSecsSinceEpoch() - qint64(daysAgo * 86400.0);
        };
        const QString d1  = rig.plant(at(1),  QStringLiteral("Startup"),  true);   // keep: < 7 d
        const QString d6  = rig.plant(at(6.9),QStringLiteral("Shutdown"), true);   // keep: < 7 d
        // 8 d and 9 d ago = Sun 6.9. and Sat 5.9.: same ISO week (36) --
        // the newer (8 d) stays, the older (9 d) goes.
        const QString d8  = rig.plant(at(8),  QStringLiteral("Startup"),  true);
        const QString d9  = rig.plant(at(9),  QStringLiteral("Startup"),  true);
        // 12 d ago = Wed 2.9., week 36 as well, older still -> goes.
        const QString d12 = rig.plant(at(12), QStringLiteral("Shutdown"), true);
        // 15 d ago = Sun 30.8., week 35 -> the only one there: stays.
        const QString d15 = rig.plant(at(15), QStringLiteral("Startup"),  true);
        // A manual copy from 40 days back never goes, whatever the rule.
        const QString d40 = rig.plant(at(40), QStringLiteral("Before the contest"), false);
        // An automatic one 41 d back, same week (Wed 5.8. / Tue 4.8.,
        // week 32) as the manual one -- the manual one is newer, so this
        // one is not the newest of its week; but it IS the newest
        // automatic of August? No: monthly keeps the newest of the
        // month, which is the manual one. So it goes.
        const QString d41 = rig.plant(at(41), QStringLiteral("Startup"),  true);
        // One automatic copy alone in July: newest of its week and of
        // its month -> stays.
        const QString d60 = rig.plant(at(60), QStringLiteral("Shutdown"), true);

        SettingsBackup b(rig.settingsPath);
        QCOMPARE(b.pruneEnabled(), false);               // clsDBMan.cs:161
        QCOMPARE(b.pruneForGfs(now), 0);                 // off: nothing goes
        QVERIFY(QFileInfo::exists(d9));

        b.setPruneEnabled(true);
        QCOMPARE(b.pruneForGfs(now), 3);
        QVERIFY(QFileInfo::exists(d1));
        QVERIFY(QFileInfo::exists(d6));
        QVERIFY(QFileInfo::exists(d8));
        QVERIFY(!QFileInfo::exists(d9));
        QVERIFY(!QFileInfo::exists(SettingsBackup::sidecarPath(d9)));
        QVERIFY(!QFileInfo::exists(d12));
        QVERIFY(QFileInfo::exists(d15));
        QVERIFY(QFileInfo::exists(d40));
        QVERIFY(!QFileInfo::exists(d41));
        QVERIFY(QFileInfo::exists(d60));

        // A second run changes nothing more.
        QCOMPARE(b.pruneForGfs(now), 0);
    }

    // From Thetis clsDBMan.cs:1993 [@852bf0e]: the description alone
    // makes a copy automatic for the prune, and a manual copy in a
    // crowded week is never touched.
    void onlyAutomaticCopiesArePruned()
    {
        Rig rig;
        const QDateTime now(QDate(2026, 9, 14), QTime(12, 0), QTimeZone::LocalTime);
        auto at = [&](double daysAgo) {
            return now.toUTC().toSecsSinceEpoch() - qint64(daysAgo * 86400.0);
        };
        const QString newest = rig.plant(at(8),  QStringLiteral("Startup"), true);
        const QString manual = rig.plant(at(9),  QStringLiteral("mine"), false);
        const QString named  = rig.plant(at(10), QStringLiteral("Shutdown"), false); // Auto=false, but the name
        const QString flagged = rig.plant(at(11), QStringLiteral("anything"), true);

        SettingsBackupInfo probe;
        probe.description = QStringLiteral("Shutdown");
        QVERIFY(SettingsBackup::isPrunable(probe));
        probe.description = QStringLiteral("mine");
        QVERIFY(!SettingsBackup::isPrunable(probe));
        probe.automatic = true;
        QVERIFY(SettingsBackup::isPrunable(probe));

        SettingsBackup b(rig.settingsPath);
        b.setPruneEnabled(true);
        QCOMPARE(b.pruneForGfs(now), 2);
        QVERIFY(QFileInfo::exists(newest));
        QVERIFY(QFileInfo::exists(manual));
        QVERIFY(!QFileInfo::exists(named));
        QVERIFY(!QFileInfo::exists(flagged));
    }

    void takeBackupPrunesWhenEnabled()
    {
        Rig rig;
        const qint64 nowSecs = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
        // Two automatic copies 20 and 21 days back. Whether they share an
        // ISO week depends on today's weekday, so plant them 20 and 27
        // days back instead: always different weeks -> both stay; and
        // one 20.5 days back: same week as the 20-day one -> goes.
        const QString a = rig.plant(nowSecs - 20 * 86400, QStringLiteral("Startup"), true);
        const QString a2 = rig.plant(nowSecs - 20 * 86400 - 3600, QStringLiteral("Startup"), true);
        const QString c = rig.plant(nowSecs - 27 * 86400, QStringLiteral("Startup"), true);

        SettingsBackup b(rig.settingsPath);
        b.setPruneEnabled(true);
        const QString fresh = b.takeBackup(QStringLiteral("Shutdown"), true);
        QVERIFY(!fresh.isEmpty());
        QVERIFY(QFileInfo::exists(fresh));
        QVERIFY(QFileInfo::exists(a));
        QVERIFY(!QFileInfo::exists(a2));
        QVERIFY(QFileInfo::exists(c));
    }

    void weekOfYearIsIso()
    {
        QCOMPARE(SettingsBackup::weekOfYear(QDate(2026, 9, 14)), 38);
        QCOMPARE(SettingsBackup::weekOfYear(QDate(2026, 9, 13)), 37);
        QCOMPARE(SettingsBackup::weekOfYear(QDate(2027, 1, 1)), 53);   // Friday, ISO week 53 of 2026
        QCOMPARE(SettingsBackup::sidecarPath(QStringLiteral("/x/backups/settings_backup_1.xml")),
                 QStringLiteral("/x/backups/settings_backup_1.json"));
        QCOMPARE(SettingsBackup::sidecarPath(QStringLiteral("/x.y/backups/noext")),
                 QStringLiteral("/x.y/backups/noext.json"));
    }
};

QTEST_GUILESS_MAIN(TestSettingsBackup)
#include "tst_settings_backup.moc"
