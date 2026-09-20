// =================================================================
// tests/tst_settings_backup_dialog.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis source (tests for the port of):
//   Project Files/Source/Console/frmDBMan.cs (the backups half)
//   original licence from Thetis source is included in the ported file
//   (src/gui/SettingsBackupDialog.h)
//
// The Settings Backups window against a temporary settings file:
//   * the list shows one row per copy, newest first, description /
//     time / age / file name; automatic copies are marked
//   * Restore / Rename / Export need exactly one selected row, Remove
//     one or more (lstBackups_SelectedIndexChanged)
//   * Take Backup Now asks for a description and copies; a cancelled
//     prompt copies nothing
//   * Rename and Remove go through the operator hooks
//   * Restore replaces the live file, inhibits AppSettings::save() and
//     asks the main window to close
//   * the three switches persist as True/False in AppSettings
//   * formatAge: 00:00:05, 1d 02:00:00, 1y 2d 00:00:00, 2y 00:00:00
//   * the window renders to a PNG when LONGPATH_GRAB_DIR is set
//
// no-port-check: tests a Thetis port (see gui/SettingsBackupDialog.h); registered in THETIS-PROVENANCE.md
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================
/*  frmDBMan.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

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
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/SettingsBackup.h"
#include "gui/SettingsBackupDialog.h"

using namespace Longpath;

namespace {

QPushButton* button(QWidget& w, const QString& text)
{
    for (QPushButton* b : w.findChildren<QPushButton*>()) {
        if (b && b->text() == text) { return b; }
    }
    return nullptr;
}

struct Rig {
    QTemporaryDir dir;
    QString settingsPath;

    Rig()
    {
        settingsPath = dir.path() + QStringLiteral("/Longpath.settings");
        write(settingsPath, QStringLiteral("<Settings><A>live</A></Settings>"));
        AppSettings::instance().setSaveInhibited(false);
        AppSettings::instance().remove(QStringLiteral("BackupOnStartup"));
        AppSettings::instance().remove(QStringLiteral("BackupOnShutdown"));
        AppSettings::instance().remove(QStringLiteral("PruneBackups"));
    }
    ~Rig() { AppSettings::instance().setSaveInhibited(false); }

    static void write(const QString& path, const QString& text)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(text.toUtf8());
    }

    static QString read(const QString& path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) { return {}; }
        return QString::fromUtf8(f.readAll());
    }

    QString plant(qint64 epoch, const QString& description, bool automatic) const
    {
        SettingsBackup b(settingsPath);
        QDir().mkpath(b.backupDir());
        const QString path = SettingsBackup::createUniqueFilename(b.backupDir(), epoch);
        write(path, QStringLiteral("<Settings><A>%1</A></Settings>").arg(description));
        // The sidecar the way the program writes it.
        write(SettingsBackup::sidecarPath(path),
              QStringLiteral("{\n    \"Auto\": %1,\n    \"Description\": \"%2\"\n}\n")
                  .arg(automatic ? QStringLiteral("true") : QStringLiteral("false"), description));
        return path;
    }
};

} // namespace

class TestSettingsBackupDialog : public QObject {
    Q_OBJECT

private slots:
    void listShowsTheCopiesNewestFirst()
    {
        Rig rig;
        rig.plant(1700000000, QStringLiteral("old one"), false);
        rig.plant(1700500000, QStringLiteral("Startup"), true);

        SettingsBackupDialog dlg(rig.settingsPath);
        QTableWidget* t = dlg.tableForTest();
        QCOMPARE(t->columnCount(), 4);
        QCOMPARE(t->horizontalHeaderItem(0)->text(), QStringLiteral("Description"));
        QCOMPARE(t->horizontalHeaderItem(3)->text(), QStringLiteral("Filename"));
        QCOMPARE(t->rowCount(), 2);
        QCOMPARE(t->item(0, 0)->text(), QStringLiteral("Startup"));
        QCOMPARE(t->item(0, 0)->toolTip(), QStringLiteral("Automatic backup"));
        QCOMPARE(t->item(0, 3)->text(), QStringLiteral("settings_backup_1700500000.xml"));
        QCOMPARE(t->item(1, 0)->text(), QStringLiteral("old one"));
        QVERIFY(t->item(1, 0)->toolTip().isEmpty());
        QCOMPARE(t->item(1, 1)->text(),
                 SettingsBackupDialog::formatDateTime(
                     QDateTime::fromSecsSinceEpoch(1700000000, QTimeZone::utc()).toLocalTime()));
        QVERIFY(t->isEnabled());

        // Nothing selected: only Take Backup Now and Open Folder live.
        QVERIFY(button(dlg, QStringLiteral("Take Backup Now"))->isEnabled());
        QVERIFY(button(dlg, QStringLiteral("Open Folder"))->isEnabled());
        QVERIFY(!dlg.restoreButtonForTest()->isEnabled());
        QVERIFY(!dlg.removeButtonForTest()->isEnabled());
        QVERIFY(!button(dlg, QStringLiteral("Rename..."))->isEnabled());
        QVERIFY(!button(dlg, QStringLiteral("Export..."))->isEnabled());

        // One row: everything; two rows: only Remove.
        t->selectRow(0);
        QVERIFY(dlg.restoreButtonForTest()->isEnabled());
        QVERIFY(dlg.removeButtonForTest()->isEnabled());
        QVERIFY(button(dlg, QStringLiteral("Rename..."))->isEnabled());
        QVERIFY(button(dlg, QStringLiteral("Export..."))->isEnabled());
        t->setRangeSelected(QTableWidgetSelectionRange(0, 0, 1, 3), true);
        QVERIFY(!dlg.restoreButtonForTest()->isEnabled());
        QVERIFY(dlg.removeButtonForTest()->isEnabled());
        QVERIFY(!button(dlg, QStringLiteral("Rename..."))->isEnabled());
    }

    void emptyFolderDisablesTheList()
    {
        Rig rig;
        SettingsBackupDialog dlg(rig.settingsPath);
        QCOMPARE(dlg.tableForTest()->rowCount(), 0);
        QVERIFY(!dlg.tableForTest()->isEnabled());
    }

    void takeBackupNowAsksForADescription()
    {
        Rig rig;
        SettingsBackupDialog dlg(rig.settingsPath);
        QStringList prompts;
        QString answer;
        dlg.setOperatorHooks(
            [](const QString&) { return true; },
            [&](const QString& title, const QString& label, const QString& text) {
                prompts << title + QStringLiteral("|") + label + QStringLiteral("|") + text;
                return answer;
            },
            [](const QString&) { return QString(); });

        // Cancelled: nothing.
        dlg.takeBackupNow();
        QCOMPARE(prompts.size(), 1);
        QCOMPARE(prompts.first(),
                 QStringLiteral("Settings Backup|Please enter a description for the backup.|"));
        QCOMPARE(dlg.tableForTest()->rowCount(), 0);

        answer = QStringLiteral("Before the contest");
        dlg.takeBackupNow();
        QCOMPARE(dlg.tableForTest()->rowCount(), 1);
        QCOMPARE(dlg.tableForTest()->item(0, 0)->text(), QStringLiteral("Before the contest"));
        QVERIFY(dlg.tableForTest()->item(0, 0)->toolTip().isEmpty());   // manual
        const QString path = dlg.tableForTest()->item(0, 0)->data(Qt::UserRole).toString();
        QCOMPARE(Rig::read(path), QStringLiteral("<Settings><A>live</A></Settings>"));
    }

    void renameAndRemoveGoThroughTheHooks()
    {
        Rig rig;
        const QString a = rig.plant(1700000000, QStringLiteral("a"), false);
        const QString b = rig.plant(1700000010, QStringLiteral("b"), true);
        SettingsBackupDialog dlg(rig.settingsPath);
        QStringList questions;
        bool yes = false;
        QString newName;
        dlg.setOperatorHooks(
            [&](const QString& q) { questions << q; return yes; },
            [&](const QString&, const QString&, const QString& text) {
                return newName.isEmpty() ? text : newName;   // default = current
            },
            [](const QString&) { return QString(); });

        // Rename row 1 ("a"): unchanged text is a no-op, a new one sticks
        // and the auto flag stays what it was.
        dlg.tableForTest()->selectRow(1);
        dlg.renameSelected();
        QCOMPARE(dlg.tableForTest()->item(1, 0)->text(), QStringLiteral("a"));
        newName = QStringLiteral("a, renamed");
        dlg.renameSelected();
        QCOMPARE(dlg.tableForTest()->item(1, 0)->text(), QStringLiteral("a, renamed"));
        QVERIFY(dlg.tableForTest()->item(1, 0)->toolTip().isEmpty());

        // Remove, answered No, then Yes.
        dlg.tableForTest()->selectRow(0);
        dlg.removeSelected();
        QCOMPARE(questions.size(), 1);
        QCOMPARE(questions.first(), QStringLiteral("Do you want to remove this backup?"));
        QVERIFY(QFileInfo::exists(b));
        QCOMPARE(dlg.tableForTest()->rowCount(), 2);
        yes = true;
        dlg.tableForTest()->selectRow(0);
        dlg.removeSelected();
        QVERIFY(!QFileInfo::exists(b));
        QVERIFY(!QFileInfo::exists(SettingsBackup::sidecarPath(b)));
        QCOMPARE(dlg.tableForTest()->rowCount(), 1);
        QVERIFY(QFileInfo::exists(a));

        // Both selected: the plural question.
        rig.plant(1700000020, QStringLiteral("c"), false);
        dlg.refresh();
        dlg.tableForTest()->setRangeSelected(QTableWidgetSelectionRange(0, 0, 1, 3), true);
        yes = false;
        dlg.removeSelected();
        QCOMPARE(questions.last(), QStringLiteral("Do you want to remove these 2 backups?"));
        QCOMPARE(dlg.tableForTest()->rowCount(), 2);
    }

    void exportUsesTheSavePathHook()
    {
        Rig rig;
        rig.plant(1700000000, QStringLiteral("golden"), false);
        SettingsBackupDialog dlg(rig.settingsPath);
        QString suggested;
        const QString dest = rig.dir.path() + QStringLiteral("/out.xml");
        dlg.setOperatorHooks(
            [](const QString&) { return true; },
            [](const QString&, const QString&, const QString& text) { return text; },
            [&](const QString& name) { suggested = name; return dest; });
        dlg.tableForTest()->selectRow(0);
        dlg.exportSelected();
        QVERIFY(suggested.startsWith(QStringLiteral("Longpath_settings_export_backup_golden_")));
        QVERIFY(suggested.endsWith(QStringLiteral(".xml")));
        QCOMPARE(Rig::read(dest), QStringLiteral("<Settings><A>golden</A></Settings>"));
    }

    void restoreReplacesTheLiveFileAndAsksToClose()
    {
        Rig rig;
        const QString golden = rig.plant(1700000000, QStringLiteral("golden"), false);
        SettingsBackupDialog dlg(rig.settingsPath);
        QSignalSpy spy(&dlg, &SettingsBackupDialog::restoreCompleted);
        bool yes = false;
        dlg.setOperatorHooks(
            [&](const QString&) { return yes; },
            [](const QString&, const QString&, const QString& text) { return text; },
            [](const QString&) { return QString(); });

        // Nothing selected, or answered No: nothing happens.
        dlg.restoreSelected();
        dlg.tableForTest()->selectRow(0);
        dlg.restoreSelected();
        QCOMPARE(spy.count(), 0);
        QCOMPARE(Rig::read(rig.settingsPath), QStringLiteral("<Settings><A>live</A></Settings>"));
        QVERIFY(!AppSettings::instance().saveInhibited());

        yes = true;
        dlg.restoreSelected();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(Rig::read(rig.settingsPath), QStringLiteral("<Settings><A>golden</A></Settings>"));
        QVERIFY(AppSettings::instance().saveInhibited());

        // The state that was replaced is kept as "Before restore".
        SettingsBackup b(rig.settingsPath);
        const QList<SettingsBackupInfo> list = b.orderedBackups();
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.first().description, QStringLiteral("Before restore"));
        QVERIFY(QFileInfo::exists(golden));
        AppSettings::instance().setSaveInhibited(false);
    }

    void switchesPersistAsTrueFalse()
    {
        Rig rig;
        SettingsBackupDialog dlg(rig.settingsPath);
        QCheckBox* startup = nullptr;
        QCheckBox* shutdown = nullptr;
        for (QCheckBox* c : dlg.findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("Backup on start-up")) { startup = c; }
            if (c->text() == QStringLiteral("Backup on shut-down")) { shutdown = c; }
        }
        QVERIFY(startup && shutdown && dlg.pruneCheckForTest());
        // Off by default, clsDBMan.cs:124-125 and :161.
        QVERIFY(!startup->isChecked());
        QVERIFY(!shutdown->isChecked());
        QVERIFY(!dlg.pruneCheckForTest()->isChecked());
        QVERIFY(!dlg.backup().pruneEnabled());

        startup->setChecked(true);
        dlg.pruneCheckForTest()->setChecked(true);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("BackupOnStartup")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("PruneBackups")).toString(),
                 QStringLiteral("True"));
        QVERIFY(dlg.backup().pruneEnabled());
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("BackupOnShutdown")));

        // A second window reads them back.
        SettingsBackupDialog again(rig.settingsPath);
        QVERIFY(again.pruneCheckForTest()->isChecked());
        QVERIFY(again.backup().pruneEnabled());
        dlg.pruneCheckForTest()->setChecked(false);
        startup->setChecked(false);
    }

    // From Thetis frmDBMan.cs:88-117 [@852bf0e] formatTimeSpanWithYears
    void formatAgeReadsLikeThetis()
    {
        QCOMPARE(SettingsBackupDialog::formatAge(5), QStringLiteral("00:00:05"));
        QCOMPARE(SettingsBackupDialog::formatAge(3 * 3600 + 7 * 60 + 9), QStringLiteral("03:07:09"));
        QCOMPARE(SettingsBackupDialog::formatAge(86400 + 2 * 3600), QStringLiteral("1d 02:00:00"));
        QCOMPARE(SettingsBackupDialog::formatAge(367 * 86400), QStringLiteral("1y 2d 00:00:00"));
        QCOMPARE(SettingsBackupDialog::formatAge(730 * 86400 + 61), QStringLiteral("2y 00:01:01"));
        QCOMPARE(SettingsBackupDialog::formatAge(-3), QStringLiteral("00:00:00"));
    }

    // The window with four copies, rendered to a PNG when
    // LONGPATH_GRAB_DIR is set -- the picture for the design doc.
    void dialogRendersTheList()
    {
        Rig rig;
        const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
        rig.plant(now - 40, QStringLiteral("Startup"), true);
        rig.plant(now - 3 * 86400 - 1234, QStringLiteral("Before the contest"), false);
        rig.plant(now - 9 * 86400, QStringLiteral("Shutdown"), true);
        rig.plant(now - 400 * 86400, QStringLiteral("Before restore"), false);

        SettingsBackupDialog dlg(rig.settingsPath);
        dlg.resize(820, 360);
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        dlg.tableForTest()->selectRow(1);
        const QPixmap pm = dlg.grab();
        QVERIFY(!pm.isNull());
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            QVERIFY(pm.save(grabDir + QStringLiteral("/longpath-grab-SettingsBackupDialog.png")));
        }
        dlg.close();
        QVERIFY(!dlg.isVisible());
    }
};

QTEST_MAIN(TestSettingsBackupDialog)
#include "tst_settings_backup_dialog.moc"
