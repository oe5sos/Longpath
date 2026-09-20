// =================================================================
// src/core/SettingsBackup.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/clsDBMan.cs, original licence from
//   Thetis source is included below
//
// See SettingsBackup.h for what is and is not ported.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from Thetis
//                 v2.10.3.15-5-g852bf0e. See the header.
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
#include "core/SettingsBackup.h"
#include "core/LogCategories.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTimeZone>

#include <algorithm>

namespace Longpath {

namespace {

// Thetis names its copies database_backup_<epoch>.xml; Longpath's file
// is the settings file, so: settings_backup_<epoch>.xml. The layout of
// the name (two underscores, then the epoch) is what
// getOrderedBackupFiles parses, so it is kept.
const QLatin1String kBackupPrefix("settings_backup_");
const QLatin1String kBackupGlob("settings_backup_*.xml");
const QLatin1String kKeyDescription("Description");
const QLatin1String kKeyAuto("Auto");

} // namespace

SettingsBackup::SettingsBackup(const QString& settingsFilePath)
    : m_settingsFilePath(settingsFilePath)
{
}

QString SettingsBackup::backupDir() const
{
    // From Thetis clsDBMan.cs:1235 [@852bf0e]
    //   string backup_directory = _db_data_path + guid.ToString() + "\\backups";
    return QFileInfo(m_settingsFilePath).absolutePath() + QStringLiteral("/backups");
}

QString SettingsBackup::sidecarPath(const QString& backupFilePath)
{
    // System.IO.Path.ChangeExtension(backup_filename, ".json")
    const int dot = backupFilePath.lastIndexOf(QLatin1Char('.'));
    const int slash = backupFilePath.lastIndexOf(QLatin1Char('/'));
    if (dot > slash) {
        return backupFilePath.left(dot) + QStringLiteral(".json");
    }
    return backupFilePath + QStringLiteral(".json");
}

// From Thetis clsDBMan.cs:1333-1351 [@852bf0e]
//   private static string createUniqueFilename(string directoryPath)
//   {
//       DateTimeOffset now = DateTimeOffset.UtcNow;
//       long secondsSinceEpoch = now.ToUnixTimeSeconds();
//
//       string baseFilename = $"database_backup_{secondsSinceEpoch}";
//       string fullPath = Path.Combine(directoryPath, $"{baseFilename}.xml");
//
//       int counter = 1;
//       while (File.Exists(fullPath))
//       {
//           fullPath = Path.Combine(directoryPath, $"{baseFilename}_{counter}.xml");
//           counter++;
//       }
//
//       return fullPath;
//   }
QString SettingsBackup::createUniqueFilename(const QString& directoryPath,
                                             qint64 secondsSinceEpoch)
{
    const QString baseFilename = kBackupPrefix + QString::number(secondsSinceEpoch);
    QString fullPath = directoryPath + QLatin1Char('/') + baseFilename + QStringLiteral(".xml");

    int counter = 1;
    while (QFileInfo::exists(fullPath)) {
        fullPath = directoryPath + QLatin1Char('/') + baseFilename + QLatin1Char('_')
                   + QString::number(counter) + QStringLiteral(".xml");
        counter++;
    }

    return fullPath;
}

bool SettingsBackup::writeSidecar(const QString& backupFilePath,
                                  const QString& description, bool automatic) const
{
    // From Thetis clsDBMan.cs:1250-1264 [@852bf0e]
    //   BackupFileInfo bfi = new BackupFileInfo()
    //   {
    //       Auto = auto,
    //       Description = desc
    //   };
    //   string jsonFilePath = System.IO.Path.ChangeExtension(backup_filename, ".json");
    //   string jsonString = JsonConvert.SerializeObject(bfi, Newtonsoft.Json.Formatting.Indented);
    //   try
    //   {
    //       File.WriteAllText(jsonFilePath, jsonString);
    //   }
    //   catch
    //   {
    //       ok = false;
    //   }
    QJsonObject o;
    o.insert(kKeyDescription, description);
    o.insert(kKeyAuto, automatic);
    QFile f(sidecarPath(backupFilePath));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray bytes = QJsonDocument(o).toJson(QJsonDocument::Indented);
    return f.write(bytes) == bytes.size();
}

bool SettingsBackup::readSidecar(const QString& backupFilePath, QString* description,
                                 bool* automatic)
{
    // From Thetis clsDBMan.cs:1308-1316 [@852bf0e]
    //   string jsonFilePath = System.IO.Path.ChangeExtension(filePath, ".json");
    //   string desc = "Default";
    //   bool auto = false;
    //   if (File.Exists(jsonFilePath))
    //   {
    //       string jsonString = File.ReadAllText(jsonFilePath);
    //       BackupFileInfo backup_info_json = JsonConvert.DeserializeObject<BackupFileInfo>(jsonString);
    //       desc = backup_info_json.Description;
    //       auto = backup_info_json.Auto;
    //   }
    *description = QStringLiteral("Default");
    *automatic = false;
    QFile f(sidecarPath(backupFilePath));
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    if (o.contains(kKeyDescription)) {
        *description = o.value(kKeyDescription).toString(*description);
    }
    if (o.contains(kKeyAuto)) {
        *automatic = o.value(kKeyAuto).toBool(false);
    }
    return true;
}

// From Thetis clsDBMan.cs:1218-1280 [@852bf0e]
//   public static bool TakeBackup(Guid highlighted, string description = "", bool auto = false)
//   {
//       ...
//       bool ok = false;
//       Guid guid = _dbman_settings.ActiveDB_GUID;
//
//       string backup_directory = _db_data_path + guid.ToString() + "\\backups";
//       try
//       {
//           // TODO: backup limits GFS
//           string backup_filename = "";
//           string db_filename_xml = _db_data_path + guid.ToString() + "\\database.xml";
//           if (File.Exists(db_filename_xml) && Directory.Exists(backup_directory))
//           {
//               backup_filename = createUniqueFilename(backup_directory);
//
//               File.Copy(db_filename_xml, backup_filename, true);
//               ok = true;
//           }
//           if (ok)
//           {
//               //copied ok, make json to store desc
//               ...
//           }
//       }
//       catch { }
//
//       if (ok && _prune_backups)
//       {
//           pruneForGFS(backup_directory);
//       }
//       ...
//       return ok;
//   }
QString SettingsBackup::takeBackup(const QString& description, bool automatic,
                                   QString* error)
{
    // Thetis asks for a description when none was given and gives up on
    // an empty answer; the dialog does the asking here, so an empty
    // description is a refusal at this level too.
    if (description.isEmpty()) {
        if (error) { *error = QStringLiteral("no description"); }
        return {};
    }

    if (m_flush) {
        m_flush();
    }

    const QString dir = backupDir();
    // Longpath: the folder is made on demand. Thetis creates it together
    // with the database folder and refuses to copy when it is missing.
    if (!QDir().mkpath(dir)) {
        if (error) { *error = QStringLiteral("cannot create %1").arg(dir); }
        return {};
    }
    if (!QFileInfo::exists(m_settingsFilePath)) {
        if (error) { *error = QStringLiteral("no settings file at %1").arg(m_settingsFilePath); }
        return {};
    }

    const QString backupFilename =
        createUniqueFilename(dir, QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    if (!QFile::copy(m_settingsFilePath, backupFilename)) {
        if (error) { *error = QStringLiteral("cannot copy to %1").arg(backupFilename); }
        qCWarning(lcSettingsBackup) << "backup copy failed:" << backupFilename;
        return {};
    }
    if (!writeSidecar(backupFilename, description, automatic)) {
        // Thetis flips ok to false and leaves the copy where it is; the
        // copy is then listed as "Default", not auto. Same here.
        if (error) { *error = QStringLiteral("cannot write %1").arg(sidecarPath(backupFilename)); }
        qCWarning(lcSettingsBackup) << "sidecar write failed:" << sidecarPath(backupFilename);
        return {};
    }
    qCInfo(lcSettingsBackup) << "backup taken:" << backupFilename
                             << (automatic ? "(auto)" : "") << description;

    if (m_prune) {
        pruneForGfs();
    }
    return backupFilename;
}

// From Thetis clsDBMan.cs:1288-1332 [@852bf0e]
//   private static List<BackupFileInfo> getOrderedBackupFiles(string backupFolderPath)
//   {
//       List<BackupFileInfo> backupFiles = new List<BackupFileInfo>();
//       if (!Directory.Exists(backupFolderPath)) return backupFiles;
//       DateTime epoch = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);
//
//       foreach (string filePath in Directory.GetFiles(backupFolderPath, "database_backup_*.xml"))
//       {
//           try
//           {
//               string fileName = Path.GetFileNameWithoutExtension(filePath);
//               string[] parts = fileName.Split('_');
//
//               if (parts.Length >= 3 && long.TryParse(parts[2], out long secondsSinceEpoch)) // 3 as database_backup_342423432.xml  could also be database_backup_342423432_1.xml
//               {
//                   DateTime backupDateTimeUtc = epoch.AddSeconds(secondsSinceEpoch);
//                   DateTime backupDateTimeLocal = backupDateTimeUtc.ToLocalTime();
//                   TimeSpan age = DateTime.UtcNow - backupDateTimeUtc;
//                   ...
//                   backupFiles.Add(new BackupFileInfo { ... });
//               }
//           }
//           catch { }
//       }
//
//       return backupFiles.OrderByDescending(f => f.SecondsSinceEpoch).ToList();
//   }
QList<SettingsBackupInfo> SettingsBackup::orderedBackups() const
{
    QList<SettingsBackupInfo> backupFiles;
    const QDir dir(backupDir());
    if (!dir.exists()) {
        return backupFiles;
    }
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();

    const QFileInfoList files = dir.entryInfoList({QString(kBackupGlob)}, QDir::Files);
    for (const QFileInfo& fi : files) {
        const QString fileName = fi.completeBaseName();
        const QStringList parts = fileName.split(QLatin1Char('_'));
        bool ok = false;
        qint64 secondsSinceEpoch = 0;
        if (parts.size() >= 3) {
            secondsSinceEpoch = parts.at(2).toLongLong(&ok);
        }
        if (!ok) {
            continue;
        }
        const QDateTime backupDateTimeUtc =
            QDateTime::fromSecsSinceEpoch(secondsSinceEpoch, QTimeZone::utc());

        SettingsBackupInfo info;
        info.fullFilePath = fi.absoluteFilePath();
        info.dateTimeOfBackup = backupDateTimeUtc.toLocalTime();
        info.secondsSinceEpoch = secondsSinceEpoch;
        info.ageSeconds = backupDateTimeUtc.secsTo(nowUtc);
        readSidecar(info.fullFilePath, &info.description, &info.automatic);
        info.sizeBytes = fi.size();
        backupFiles.append(info);
    }

    // Newest first; a same-second pair (the _1 suffix) by name, so the
    // order is the same on every file system.
    std::stable_sort(backupFiles.begin(), backupFiles.end(),
                     [](const SettingsBackupInfo& a, const SettingsBackupInfo& b) {
                         if (a.secondsSinceEpoch != b.secondsSinceEpoch) {
                             return a.secondsSinceEpoch > b.secondsSinceEpoch;
                         }
                         return a.fullFilePath > b.fullFilePath;
                     });
    return backupFiles;
}

// From Thetis clsDBMan.cs:1364-1384 [@852bf0e]
//   foreach (string file_path in file_paths)
//   {
//       if (File.Exists(file_path))
//       {
//           try
//           {
//               File.Delete(file_path);
//           }
//           catch { }
//       }
//       string jsonFilePath = System.IO.Path.ChangeExtension(file_path, ".json");
//       if (File.Exists(jsonFilePath))
//       {
//           try
//           {
//               File.Delete(jsonFilePath);
//           }
//           catch { }
//       }
//   }
void SettingsBackup::removeBackups(const QStringList& filePaths)
{
    const QString dir = QDir(backupDir()).absolutePath();
    for (const QString& filePath : filePaths) {
        // Longpath: only files inside the backups folder are removed
        // through this door, whatever the dialog hands over.
        if (!QFileInfo(filePath).absolutePath().startsWith(dir)) {
            qCWarning(lcSettingsBackup) << "refusing to remove outside backups:" << filePath;
            continue;
        }
        if (QFileInfo::exists(filePath)) {
            QFile::remove(filePath);
        }
        const QString jsonFilePath = sidecarPath(filePath);
        if (QFileInfo::exists(jsonFilePath)) {
            QFile::remove(jsonFilePath);
        }
    }
}

// From Thetis clsDBMan.cs:1653-1708 [@852bf0e] RenameBackup: read the
// sidecar (or "Default"/not auto when there is none), ask for the new
// description, write the sidecar back with the auto flag kept.
bool SettingsBackup::renameBackup(const QString& filePath, const QString& description)
{
    //   if (string.IsNullOrEmpty(desc) || desc == tmp_desc) return;
    if (description.isEmpty() || !QFileInfo::exists(filePath)) {
        return false;
    }
    QString oldDescription;
    bool automatic = false;
    readSidecar(filePath, &oldDescription, &automatic);
    if (description == oldDescription) {
        return false;
    }
    //   backup_info_json = new BackupFileInfo();
    //   backup_info_json.Description = desc;
    //   backup_info_json.Auto = auto;
    return writeSidecar(filePath, description, automatic);
}

// From Thetis clsDBMan.cs:1778-1817 [@852bf0e] ExportBackup: the file
// dialog is the dialog's; here only
//   try
//   {
//       if (File.Exists(filename))
//           File.Delete(filename);
//   }
//   catch { }
//   try
//   {
//       File.Copy(path, filename);
//   }
//   catch { }
bool SettingsBackup::exportBackup(const QString& filePath, const QString& destination,
                                  QString* error)
{
    if (!QFileInfo::exists(filePath)) {
        if (error) { *error = QStringLiteral("no such backup: %1").arg(filePath); }
        return false;
    }
    if (QFileInfo::exists(destination) && !QFile::remove(destination)) {
        if (error) { *error = QStringLiteral("cannot replace %1").arg(destination); }
        return false;
    }
    if (!QFile::copy(filePath, destination)) {
        if (error) { *error = QStringLiteral("cannot copy to %1").arg(destination); }
        return false;
    }
    return true;
}

bool SettingsBackup::restore(const QString& filePath, QString* error)
{
    if (!QFileInfo::exists(filePath)) {
        if (error) { *error = QStringLiteral("no such backup: %1").arg(filePath); }
        return false;
    }
    // A copy of what is being replaced, never pruned: the way back if
    // the restored file turns out to be the wrong one. Thetis takes the
    // same kind of copy before a container import (setup.cs:35183
    // [@852bf0e]: DBMan.TakeBackup(Guid.Empty, "Before container import", false);).
    if (QFileInfo::exists(m_settingsFilePath)) {
        const bool pruneWas = m_prune;
        m_prune = false;
        const QString safety = takeBackup(QStringLiteral("Before restore"), false, error);
        m_prune = pruneWas;
        if (safety.isEmpty()) {
            return false;
        }
        if (!QFile::remove(m_settingsFilePath)) {
            if (error) { *error = QStringLiteral("cannot replace %1").arg(m_settingsFilePath); }
            return false;
        }
    } else {
        QDir().mkpath(QFileInfo(m_settingsFilePath).absolutePath());
    }
    if (!QFile::copy(filePath, m_settingsFilePath)) {
        if (error) { *error = QStringLiteral("cannot copy %1 to %2").arg(filePath, m_settingsFilePath); }
        qCWarning(lcSettingsBackup) << "restore copy failed:" << filePath;
        return false;
    }
    qCInfo(lcSettingsBackup) << "restored" << filePath << "over" << m_settingsFilePath;
    return true;
}

bool SettingsBackup::isPrunable(const SettingsBackupInfo& info)
{
    return info.automatic || info.description == QLatin1String("Startup")
           || info.description == QLatin1String("Shutdown");
}

// From Thetis clsDBMan.cs:1919-1923 [@852bf0e]
//   private static int getWeekOfYear(DateTime date)
//   {
//       System.Globalization.CultureInfo cul = System.Globalization.CultureInfo.CurrentCulture;
//       return cul.Calendar.GetWeekOfYear(date, System.Globalization.CalendarWeekRule.FirstFourDayWeek, DayOfWeek.Monday);
//   }
int SettingsBackup::weekOfYear(const QDate& date)
{
    // FirstFourDayWeek + Monday is the ISO 8601 week, which is what
    // QDate::weekNumber() computes.
    return date.weekNumber();
}

// From Thetis clsDBMan.cs:1929-2010 [@852bf0e]
//   private static void pruneForGFS(string backup_folder_path)
//   {
//       if (!_prune_backups || !Directory.Exists(backup_folder_path)) return;
//       ...
//           backups = new DirectoryInfo(backup_folder_path).GetFiles("*.xml")
//               .OrderByDescending(f => f.CreationTime)
//               .ToList();
//
//           DateTime now = DateTime.Now;
//
//           List<FileInfo> last_7_days = backups
//               .Where(f => (now - f.CreationTime).TotalDays <= 7)
//               .ToList();
//
//           List<FileInfo> weekly_backups = backups
//               .Where(f => (now - f.CreationTime).TotalDays > 7)
//               .GroupBy(f => new { Year = f.CreationTime.Year, Week = getWeekOfYear(f.CreationTime) })
//               .Select(g => g.OrderByDescending(f => f.CreationTime).First())
//               .ToList();
//
//           List<FileInfo> monthly_backups = backups
//               .Where(f => (now - f.CreationTime).TotalDays > 30)
//               .GroupBy(f => new { Year = f.CreationTime.Year, Month = f.CreationTime.Month })
//               .Select(g => g.OrderByDescending(f => f.CreationTime).First())
//               .ToList();
//
//           List<FileInfo> yearly_backups = backups
//               .Where(f => (now - f.CreationTime).TotalDays > 365)
//               .GroupBy(f => f.CreationTime.Year)
//               .Select(g => g.OrderByDescending(f => f.CreationTime).First())
//               .ToList();
//
//           keep_backups = last_7_days
//               .Concat(weekly_backups)
//               .Concat(monthly_backups)
//               .Concat(yearly_backups)
//               .Distinct()
//               .ToList();
//       ...
//           foreach (FileInfo backup in backups)
//           {
//               if (!keep_backups.Contains(backup))
//               {
//                   ... is_auto = backup_info_json.Auto || backup_info_json.Description == "Startup" || backup_info_json.Description == "Shutdown";
//                   if (is_auto)
//                   {
//                       backup.Delete();
//                       ... json_file_path delete
//                   }
//               }
//           }
//   }
//
// Longpath: the time of a copy is the epoch in its name, not the file
// system's creation time (which ext4 does not keep and a copy changes);
// `now` is a parameter so the rule can be tested against a fixed clock.
int SettingsBackup::pruneForGfs(const QDateTime& now)
{
    if (!m_prune || !QDir(backupDir()).exists()) {
        return 0;
    }
    const QList<SettingsBackupInfo> backups = orderedBackups();   // newest first
    const qint64 nowSecs = now.toUTC().toSecsSinceEpoch();
    constexpr qint64 kDay = 24 * 60 * 60;

    auto ageDays = [&](const SettingsBackupInfo& b) {
        return double(nowSecs - b.secondsSinceEpoch) / double(kDay);
    };

    QSet<QString> keep;
    // last_7_days
    for (const SettingsBackupInfo& b : backups) {
        if (ageDays(b) <= 7.0) { keep.insert(b.fullFilePath); }
    }
    // weekly / monthly / yearly: `backups` is newest first, so the first
    // member of a group is its newest (OrderByDescending(...).First()).
    QSet<QString> weeklySeen, monthlySeen, yearlySeen;
    for (const SettingsBackupInfo& b : backups) {
        const QDate d = b.dateTimeOfBackup.date();   // local, as DateTime.Now is
        if (ageDays(b) > 7.0) {
            const QString key = QStringLiteral("%1/%2").arg(d.year()).arg(weekOfYear(d));
            if (!weeklySeen.contains(key)) { weeklySeen.insert(key); keep.insert(b.fullFilePath); }
        }
        if (ageDays(b) > 30.0) {
            const QString key = QStringLiteral("%1/%2").arg(d.year()).arg(d.month());
            if (!monthlySeen.contains(key)) { monthlySeen.insert(key); keep.insert(b.fullFilePath); }
        }
        if (ageDays(b) > 365.0) {
            const QString key = QString::number(d.year());
            if (!yearlySeen.contains(key)) { yearlySeen.insert(key); keep.insert(b.fullFilePath); }
        }
    }

    int removed = 0;
    for (const SettingsBackupInfo& b : backups) {
        if (keep.contains(b.fullFilePath)) { continue; }
        if (!isPrunable(b)) { continue; }
        removeBackups({b.fullFilePath});
        ++removed;
    }
    if (removed > 0) {
        qCInfo(lcSettingsBackup) << "pruned" << removed << "automatic backup(s)";
    }
    return removed;
}

} // namespace Longpath
