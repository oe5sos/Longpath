#pragma once
// =================================================================
// src/core/SettingsBackup.h  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/clsDBMan.cs, original licence from
//   Thetis source is included below
//
// The backup half of Thetis's Database Manager (DBMan): take a copy of
// the settings file into a `backups` folder next to it, with a small
// JSON sidecar carrying the description and the auto flag; list the
// copies newest first; rename, export, remove; optional copies at
// start-up and shut-down; and the grandfather-father-son prune that
// thins the automatic copies out (all of the last 7 days, then one per
// week, one per month, one per year).
//
// What Longpath does NOT port from DBMan: the several-databases-with-
// GUID-folders model (Longpath has `--profile` for that), the "make
// backup available as a new database" path, import/merge, and the
// restart. Restoring here means: copy the chosen backup over the live
// settings file and close Longpath without saving, so the next start
// reads the restored file (see restore() and AppSettings::
// setSaveInhibited).
//
// The file behind this is Longpath's `<App>.settings` (one XML file,
// like Thetis's database.xml); nothing else is copied.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from Thetis
//                 v2.10.3.15-5-g852bf0e. Static DBMan class with a
//                 GUID-keyed database folder became an instance bound
//                 to one settings file; Newtonsoft JSON sidecar became
//                 QJsonDocument with the same two keys; CreationTime
//                 ordering became the epoch in the file name.
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
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace Longpath {

// From Thetis clsDBMan.cs:129-143 [@852bf0e]
//   public class BackupFileInfo
//   {
//       [JsonIgnore] public string FullFilePath { get; set; }
//       [JsonIgnore] public DateTime DateTimeOfBackup { get; set; }
//       [JsonIgnore] public long SecondsSinceEpoch { get; set; }
//       [JsonIgnore] public TimeSpan AgeSinceBackedUp { get; set; }
//       public string Description { get; set; }
//       public bool Auto { get; set; }
//       public BackupFileInfo() { Auto = false; Description = "Default"; }
//   }
struct SettingsBackupInfo {
    QString   fullFilePath;
    QDateTime dateTimeOfBackup;          // local time
    qint64    secondsSinceEpoch = 0;
    qint64    ageSeconds = 0;            // AgeSinceBackedUp
    QString   description = QStringLiteral("Default");
    bool      automatic = false;         // Auto
    qint64    sizeBytes = 0;             // Longpath: shown in the dialog
};

class SettingsBackup {
public:
    // `settingsFilePath` is the live settings file; the backups live in
    // "<its folder>/backups".
    explicit SettingsBackup(const QString& settingsFilePath);

    QString settingsFilePath() const { return m_settingsFilePath; }
    QString backupDir() const;

    // Longpath: called before every copy of the live file, so a manual
    // backup holds the operator's current state, not the state at the
    // last save (Thetis copies whatever is on disk). Unset for the
    // start-up copy, where the file on disk IS the state.
    void setFlushHook(std::function<void()> flush) { m_flush = std::move(flush); }

    // From Thetis clsDBMan.cs:1924-1928 [@852bf0e] PruneBackups
    bool pruneEnabled() const { return m_prune; }
    void setPruneEnabled(bool on) { m_prune = on; }

    // From Thetis clsDBMan.cs:1218-1280 [@852bf0e] TakeBackup(highlighted,
    // description, auto). Returns the path of the copy, or an empty
    // string (and `error`) when nothing was copied. Prunes afterwards
    // when pruneEnabled(), as TakeBackup does.
    QString takeBackup(const QString& description, bool automatic,
                       QString* error = nullptr);

    // From Thetis clsDBMan.cs:1288-1332 [@852bf0e] getOrderedBackupFiles:
    // newest first.
    QList<SettingsBackupInfo> orderedBackups() const;

    // From Thetis clsDBMan.cs:1352-1385 [@852bf0e] RemoveBackupDB, the
    // file part (the question is the dialog's).
    void removeBackups(const QStringList& filePaths);

    // From Thetis clsDBMan.cs:1653-1708 [@852bf0e] RenameBackup, the
    // file part: rewrite the sidecar with the new description.
    bool renameBackup(const QString& filePath, const QString& description);

    // From Thetis clsDBMan.cs:1778-1817 [@852bf0e] ExportBackup, the
    // file part: copy a backup to a place of the operator's choosing.
    bool exportBackup(const QString& filePath, const QString& destination,
                      QString* error = nullptr);

    // Longpath: copy a backup over the live settings file. Takes a
    // "Before restore" copy of the live file first (as Thetis's setup.cs
    // does before a container import, "Before container import"). The
    // caller must then stop AppSettings from saving and close the
    // program; nothing here touches the in-memory settings.
    bool restore(const QString& filePath, QString* error = nullptr);

    // From Thetis clsDBMan.cs:1929-2010 [@852bf0e] pruneForGFS. Returns
    // how many automatic copies were removed. `now` is a test seam.
    int pruneForGfs(const QDateTime& now = QDateTime::currentDateTime());

    // From Thetis clsDBMan.cs:1333-1351 [@852bf0e] createUniqueFilename
    static QString createUniqueFilename(const QString& directoryPath,
                                        qint64 secondsSinceEpoch);

    // Sidecar path of a backup: same name, ".json" instead of ".xml"
    // (System.IO.Path.ChangeExtension(backup_filename, ".json")).
    static QString sidecarPath(const QString& backupFilePath);

    // From Thetis clsDBMan.cs:1993 [@852bf0e]: what the prune may delete.
    //   is_auto = backup_info_json.Auto || backup_info_json.Description == "Startup"
    //             || backup_info_json.Description == "Shutdown";
    static bool isPrunable(const SettingsBackupInfo& info);

    // From Thetis clsDBMan.cs:1919-1923 [@852bf0e] getWeekOfYear:
    // FirstFourDayWeek, Monday -- the ISO week.
    static int weekOfYear(const QDate& date);

private:
    bool writeSidecar(const QString& backupFilePath, const QString& description,
                      bool automatic) const;
    static bool readSidecar(const QString& backupFilePath, QString* description,
                            bool* automatic);

    QString m_settingsFilePath;
    bool    m_prune = false;             // clsDBMan.cs:161 _prune_backups = false
    std::function<void()> m_flush;
};

} // namespace Longpath
