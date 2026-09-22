// =================================================================
// src/gui/SettingsBackupDialog.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/frmDBMan.cs, original licence from
//   Thetis source is included below
//   Project Files/Source/Console/frmDBMan.Designer.cs (texts, tooltips)
//   Project Files/Source/Console/frmDBMan.resx (the prune tooltip)
//   Project Files/Source/Console/clsDBMan.cs (the questions asked
//   before a backup, a rename, a removal)
//
// See SettingsBackupDialog.h for what is and is not ported.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from Thetis
//                 v2.10.3.15-5-g852bf0e. See the header.
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
#include "gui/SettingsBackupDialog.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "gui/StyleConstants.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace Longpath {

namespace {

const QLatin1String kKeyOnStartup("BackupOnStartup");
const QLatin1String kKeyOnShutdown("BackupOnShutdown");
const QLatin1String kKeyPrune("PruneBackups");
const QLatin1String kKeyGeometry("SettingsBackupDialogGeometryState");

bool settingIsTrue(QLatin1String key, bool fallback)
{
    return AppSettings::instance()
               .value(key, fallback ? QStringLiteral("True") : QStringLiteral("False"))
               .toString() == QLatin1String("True");
}

void setSettingBool(QLatin1String key, bool on)
{
    AppSettings::instance().setValue(key, on ? QStringLiteral("True") : QStringLiteral("False"));
}

// From Thetis common.cs:965-985 [@852bf0e] DateTimeStringForFile: the
// short date and short time of the culture, with / : . turned into _.
QString dateTimeStringForFile()
{
    const QLocale loc = QLocale::system();
    const QDateTime now = QDateTime::currentDateTime();
    QString s = loc.toString(now.date(), QLocale::ShortFormat) + QLatin1Char('_')
                + loc.toString(now.time(), QLocale::ShortFormat);
    s.replace(QLatin1Char('/'), QLatin1Char('_'));
    s.replace(QLatin1Char(':'), QLatin1Char('_'));
    s.replace(QLatin1Char('.'), QLatin1Char('_'));
    s.replace(QLatin1Char(' '), QLatin1Char('_'));
    return s;
}

} // namespace

SettingsBackupDialog::SettingsBackupDialog(const QString& settingsFilePath, QWidget* parent)
    : QDialog(parent)
    , m_backup(settingsFilePath)
{
    // From Thetis frmDBMan.cs:60 [@852bf0e]
    //   this.Text = $"Database Manager  [v{Common.GetVerNum()}]";
    setWindowTitle(QStringLiteral("Settings Backups"));
    setModal(false);
    resize(820, 420);
    buildUi();

    // Longpath: a manual copy holds the state of now, not of the last
    // save (SettingsBackup.h, setFlushHook).
    m_backup.setFlushHook([]() { AppSettings::instance().save(); });
    m_backup.setPruneEnabled(settingIsTrue(kKeyPrune, false));

    // From Thetis frmDBMan.cs:62-67 [@852bf0e]
    //   Common.RestoreForm(this, "DBManForm", true);
    const QByteArray st = AppSettings::instance().value(kKeyGeometry).toByteArray();
    if (!st.isEmpty()) { restoreGeometry(st); }

    refresh();
}

void SettingsBackupDialog::setOperatorHooks(
    std::function<bool(const QString&)> ask,
    std::function<QString(const QString&, const QString&, const QString&)> prompt,
    std::function<QString(const QString&)> saveFile)
{
    m_ask = std::move(ask);
    m_prompt = std::move(prompt);
    m_saveFile = std::move(saveFile);
}

void SettingsBackupDialog::buildUi()
{
    setStyleSheet(QStringLiteral("QDialog { background: %1; }")
                      .arg(QLatin1String(Style::kAppBg)));
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(10, 10, 10, 10);
    col->setSpacing(8);

    // From Thetis frmDBMan.Designer.cs:456 [@852bf0e]
    //   this.lblDabaseBackups_active_selected.Text = "Database Backups";
    // and frmDBMan.cs:304 "Database Backups for currently ACTIVE database"
    m_caption = new QLabel(QStringLiteral("Backups of the current settings file"), this);
    m_caption->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                 .arg(QLatin1String(Style::kTextSecondary)));
    col->addWidget(m_caption);

    auto* row = new QHBoxLayout;
    row->setSpacing(8);

    // ── The list (lstBackups) ────────────────────────────────────────
    // From Thetis frmDBMan.Designer.cs:175-188 [@852bf0e]
    //   this.colDescription.Text = "Description";
    //   this.colTimeDate.Text = "TimeDate";
    //   this.colBackupAge.Text = "Age";
    //   this.colBackupFilename.Text = "Filename";
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Description"),
                                        QStringLiteral("Time/Date"),
                                        QStringLiteral("Age"),
                                        QStringLiteral("Filename")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setStyleSheet(QStringLiteral(
        "QTableWidget { background: %1; alternate-background-color: %4;"
        "  color: %2; border: 1px solid %3; gridline-color: %3;"
        "  font-size: 11px; }"
        "QTableWidget::item:selected { background: %6; color: %2; }"
        "QHeaderView::section { background: %4; color: %5; border: none;"
        "  border-bottom: 1px solid %3; padding: 3px 6px; font-size: 11px; }"
    ).arg(QString::fromLatin1(Style::kInsetBg),
          QString::fromLatin1(Style::kTextPrimary),
          QString::fromLatin1(Style::kBorderSubtle),
          QString::fromLatin1(Style::kButtonBg),
          QString::fromLatin1(Style::kTextSecondary),
          QString::fromLatin1(Style::kAccent)));
    row->addWidget(m_table, 1);

    // ── The buttons, tooltips from frmDBMan.Designer.cs [@852bf0e] ───
    auto* btns = new QVBoxLayout;
    btns->setSpacing(6);
    m_takeBtn = new QPushButton(QStringLiteral("Take Backup Now"), this);
    // Designer.cs:368
    m_takeBtn->setToolTip(QStringLiteral("Take backup now of the active database"));
    m_restoreBtn = new QPushButton(QStringLiteral("Restore..."), this);
    // Designer.cs:316 "Make the selected backup available" -- Longpath
    // restores it in place, see the header.
    m_restoreBtn->setToolTip(QStringLiteral(
        "Copy the selected backup over the current settings and close Longpath"));
    m_renameBtn = new QPushButton(QStringLiteral("Rename..."), this);
    // Designer.cs:199
    m_renameBtn->setToolTip(QStringLiteral("Change description for the selected database backup"));
    m_exportBtn = new QPushButton(QStringLiteral("Export..."), this);
    // Designer.cs:238
    m_exportBtn->setToolTip(QStringLiteral("Export the selected backup database"));
    m_removeBtn = new QPushButton(QStringLiteral("Remove"), this);
    // Designer.cs:329
    m_removeBtn->setToolTip(QStringLiteral("Remove the selected backup"));
    m_folderBtn = new QPushButton(QStringLiteral("Open Folder"), this);
    // Designer.cs:225
    m_folderBtn->setToolTip(QStringLiteral("Open the folder for the selected database"));
    for (QPushButton* b : {m_takeBtn, m_restoreBtn, m_renameBtn, m_exportBtn,
                           m_removeBtn, m_folderBtn}) {
        b->setStyleSheet(Style::buttonBaseStyle());
        btns->addWidget(b);
    }
    btns->addStretch(1);
    row->addLayout(btns);
    col->addLayout(row, 1);

    // ── The switches ─────────────────────────────────────────────────
    auto* sw = new QHBoxLayout;
    sw->setSpacing(16);
    const QString checkStyle = QStringLiteral("QCheckBox { color: %1; }")
                                   .arg(QLatin1String(Style::kTextPrimary));
    // Designer.cs:355 / 342: "Toggle Backup On Start-Up" / "Toggle Backup
    // On Shut-Down" (buttons per database in Thetis; one file here, so
    // two check boxes). Both off by default, clsDBMan.cs:124-125.
    m_onStartup = new QCheckBox(QStringLiteral("Backup on start-up"), this);
    m_onStartup->setToolTip(QStringLiteral("Take an automatic backup when Longpath starts"));
    m_onStartup->setChecked(settingIsTrue(kKeyOnStartup, false));
    connect(m_onStartup, &QCheckBox::toggled, this, [](bool on) {
        setSettingBool(kKeyOnStartup, on);
    });
    m_onShutdown = new QCheckBox(QStringLiteral("Backup on shut-down"), this);
    m_onShutdown->setToolTip(QStringLiteral("Take an automatic backup when Longpath closes"));
    m_onShutdown->setChecked(settingIsTrue(kKeyOnShutdown, false));
    connect(m_onShutdown, &QCheckBox::toggled, this, [](bool on) {
        setSettingBool(kKeyOnShutdown, on);
    });
    // Designer.cs:407 and frmDBMan.resx:123-131 [@852bf0e]
    m_prune = new QCheckBox(QStringLiteral("Prune auto backups"), this);
    m_prune->setToolTip(QStringLiteral(
        "Prune happens after a backup is taken\n"
        "Only auto backups are pruned\n"
        "\n"
        "Last 7 days: Keep all backups\n"
        "Weekly backups: Retain 1 backup per week (the last one of the week)\n"
        "Monthly backups: Retain 1 backup per month (the last one of the month)\n"
        "Yearly backups: Retain 1 backup per year (the last one of the year)"));
    m_prune->setChecked(settingIsTrue(kKeyPrune, false));
    // From Thetis frmDBMan.cs:475-479 [@852bf0e] chkPruneBackups_CheckedChanged
    //   DBMan.PruneBackups = chkPruneBackups.Checked;
    connect(m_prune, &QCheckBox::toggled, this, [this](bool on) {
        setSettingBool(kKeyPrune, on);
        m_backup.setPruneEnabled(on);
    });
    for (QCheckBox* c : {m_onStartup, m_onShutdown, m_prune}) {
        c->setStyleSheet(checkStyle);
        sw->addWidget(c);
    }
    sw->addStretch(1);
    col->addLayout(sw);

    connect(m_takeBtn, &QPushButton::clicked, this, &SettingsBackupDialog::takeBackupNow);
    connect(m_restoreBtn, &QPushButton::clicked, this, &SettingsBackupDialog::restoreSelected);
    connect(m_renameBtn, &QPushButton::clicked, this, &SettingsBackupDialog::renameSelected);
    connect(m_exportBtn, &QPushButton::clicked, this, &SettingsBackupDialog::exportSelected);
    connect(m_removeBtn, &QPushButton::clicked, this, &SettingsBackupDialog::removeSelected);
    connect(m_folderBtn, &QPushButton::clicked, this, &SettingsBackupDialog::openFolder);
    connect(m_table, &QTableWidget::itemSelectionChanged, this,
            &SettingsBackupDialog::updateButtons);
}

// From Thetis frmDBMan.cs:88-117 [@852bf0e]
//   private string formatTimeSpanWithYears(TimeSpan difference)
//   {
//       int totalDays = difference.Days;
//       int years = totalDays / 365;
//       int days = totalDays % 365;
//
//       string age;
//
//       if (years > 0)
//       {
//           if (days > 0)
//           {
//               age = $"{years}y {days}d {difference.Hours.ToString("00")}:{difference.Minutes.ToString("00")}:{difference.Seconds.ToString("00")}";
//           }
//           else
//           {
//               age = $"{years}y {difference.Hours.ToString("00")}:{difference.Minutes.ToString("00")}:{difference.Seconds.ToString("00")}";
//           }
//       }
//       else if (days > 0)
//       {
//           age = $"{days}d {difference.Hours.ToString("00")}:{difference.Minutes.ToString("00")}:{difference.Seconds.ToString("00")}";
//       }
//       else
//       {
//           age = $"{difference.Hours.ToString("00")}:{difference.Minutes.ToString("00")}:{difference.Seconds.ToString("00")}";
//       }
//
//       return age;
//   }
QString SettingsBackupDialog::formatAge(qint64 seconds)
{
    if (seconds < 0) { seconds = 0; }
    const qint64 totalDays = seconds / 86400;
    const qint64 years = totalDays / 365;
    const qint64 days = totalDays % 365;
    const qint64 rem = seconds % 86400;
    const QString hms = QStringLiteral("%1:%2:%3")
                            .arg(rem / 3600, 2, 10, QLatin1Char('0'))
                            .arg((rem % 3600) / 60, 2, 10, QLatin1Char('0'))
                            .arg(rem % 60, 2, 10, QLatin1Char('0'));
    if (years > 0) {
        if (days > 0) {
            return QStringLiteral("%1y %2d %3").arg(years).arg(days).arg(hms);
        }
        return QStringLiteral("%1y %2").arg(years).arg(hms);
    }
    if (days > 0) {
        return QStringLiteral("%1d %2").arg(days).arg(hms);
    }
    return hms;
}

// From Thetis frmDBMan.cs:70-86 [@852bf0e] localDateTimeFormat:
//   string formattedDateTime = dateTime.ToString("G", localCulture);
// "G" is the short date followed by the long time.
QString SettingsBackupDialog::formatDateTime(const QDateTime& dt)
{
    const QLocale loc = QLocale::system();
    return loc.toString(dt.date(), QLocale::ShortFormat) + QLatin1Char(' ')
           + dt.time().toString(QStringLiteral("HH:mm:ss"));
}

// From Thetis frmDBMan.cs:118-142 [@852bf0e]
//   internal void InitBackups(List<DBMan.BackupFileInfo> backups)
//   {
//       lstBackups.Items.Clear();
//       foreach(DBMan.BackupFileInfo backup in backups)
//       {
//           ListViewItem lvi = new ListViewItem(backup.Description);
//
//           lvi.SubItems.Add(localDateTimeFormat(backup.DateTimeOfBackup));
//           TimeSpan difference = DateTime.Now - backup.DateTimeOfBackup;
//           string age = formatTimeSpanWithYears(difference);
//           lvi.SubItems.Add(age);
//           lvi.SubItems.Add(backup.FullFilePath);
//
//           lvi.Tag = backup.FullFilePath;
//
//           lstBackups.Items.Add(lvi);
//       }
//       ...
//       lstBackups.Enabled = lstBackups.Items.Count > 0;
//
//       lstBackups_SelectedIndexChanged(this, EventArgs.Empty);
//   }
void SettingsBackupDialog::refresh()
{
    const QList<SettingsBackupInfo> backups = m_backup.orderedBackups();
    m_table->setRowCount(0);
    for (const SettingsBackupInfo& backup : backups) {
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        auto* desc = new QTableWidgetItem(backup.description);
        desc->setData(Qt::UserRole, backup.fullFilePath);
        // Longpath: the automatic copies say so, since only they are
        // pruned.
        if (backup.automatic) {
            desc->setToolTip(QStringLiteral("Automatic backup"));
            desc->setForeground(QColor(QString::fromLatin1(Style::kTextSecondary)));
        }
        m_table->setItem(r, 0, desc);
        m_table->setItem(r, 1, new QTableWidgetItem(formatDateTime(backup.dateTimeOfBackup)));
        m_table->setItem(r, 2, new QTableWidgetItem(formatAge(backup.ageSeconds)));
        m_table->setItem(r, 3, new QTableWidgetItem(QFileInfo(backup.fullFilePath).fileName()));
    }
    m_table->resizeColumnsToContents();
    m_table->setEnabled(m_table->rowCount() > 0);
    m_caption->setText(QStringLiteral("Backups of %1  (%2 in %3)")
                           .arg(QFileInfo(m_backup.settingsFilePath()).fileName())
                           .arg(m_table->rowCount())
                           .arg(QDir::toNativeSeparators(m_backup.backupDir())));
    updateButtons();
}

// From Thetis frmDBMan.cs:323-330 [@852bf0e]
//   private void lstBackups_SelectedIndexChanged(object sender, EventArgs e)
//   {
//       btnMakeBackupAvailable.Enabled = lstBackups.SelectedItems.Count == 1;
//       btnExportBackup.Enabled = lstBackups.SelectedItems.Count == 1;
//       btnRenameBackup.Enabled = lstBackups.SelectedItems.Count == 1;
//
//       btnRemoveBackup.Enabled = lstBackups.SelectedItems.Count > 0;
//   }
void SettingsBackupDialog::updateButtons()
{
    const int n = selectedPaths().size();
    m_restoreBtn->setEnabled(n == 1);
    m_exportBtn->setEnabled(n == 1);
    m_renameBtn->setEnabled(n == 1);
    m_removeBtn->setEnabled(n > 0);
}

QStringList SettingsBackupDialog::selectedPaths() const
{
    QStringList paths;
    const QList<QTableWidgetSelectionRange> ranges = m_table->selectedRanges();
    for (const QTableWidgetSelectionRange& range : ranges) {
        for (int r = range.topRow(); r <= range.bottomRow(); ++r) {
            if (QTableWidgetItem* it = m_table->item(r, 0)) {
                const QString p = it->data(Qt::UserRole).toString();
                if (!p.isEmpty() && !paths.contains(p)) { paths.append(p); }
            }
        }
    }
    return paths;
}

QString SettingsBackupDialog::descriptionOf(const QString& path) const
{
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem* it = m_table->item(r, 0);
        if (it && it->data(Qt::UserRole).toString() == path) { return it->text(); }
    }
    return {};
}

bool SettingsBackupDialog::askOperator(const QString& question)
{
    if (m_ask) { return m_ask(question); }
    // MessageBoxButtons.YesNo, MessageBoxDefaultButton.Button2 (No)
    return QMessageBox::question(this, windowTitle(), question,
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) == QMessageBox::Yes;
}

QString SettingsBackupDialog::promptOperator(const QString& title, const QString& label,
                                             const QString& text)
{
    if (m_prompt) { return m_prompt(title, label, text); }
    bool ok = false;
    const QString s = QInputDialog::getText(this, title, label, QLineEdit::Normal, text, &ok);
    return ok ? s : QString();
}

QString SettingsBackupDialog::askSavePath(const QString& suggestedName)
{
    if (m_saveFile) { return m_saveFile(suggestedName); }
    // From Thetis clsDBMan.cs:1791-1799 [@852bf0e]
    //   SaveFileDialog saveFileDialog = new SaveFileDialog
    //   {
    //       Filter = "XML files (*.xml)|*.xml|All files (*.*)|*.*",
    //       DefaultExt = "xml",
    //       FileName = save_file,
    //       Title = "Export Database",
    //       InitialDirectory = myDocumentsPath
    //   };
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QFileDialog::getSaveFileName(this, QStringLiteral("Export Backup"),
                                        docs + QLatin1Char('/') + suggestedName,
                                        QStringLiteral("XML files (*.xml);;All files (*.*)"));
}

void SettingsBackupDialog::takeBackupNow()
{
    // From Thetis clsDBMan.cs:1222-1228 [@852bf0e]
    //   if (string.IsNullOrEmpty(description))
    //   {
    //       desc = InputBox.Show("Database Backup", "Please enter a description for the backup.", "", true);
    //       if (string.IsNullOrEmpty(desc)) return false;
    //   }
    const QString desc = promptOperator(QStringLiteral("Settings Backup"),
                                        QStringLiteral("Please enter a description for the backup."),
                                        QString());
    if (desc.isEmpty()) { return; }
    QString err;
    if (m_backup.takeBackup(desc, false, &err).isEmpty()) {
        qCWarning(lcSettingsBackup) << "take backup failed:" << err;
    }
    refresh();
}

void SettingsBackupDialog::restoreSelected()
{
    const QStringList paths = selectedPaths();
    if (paths.size() != 1) { return; }
    // After Thetis clsDBMan.cs:964-966 [@852bf0e] MakeActiveDB
    //   MessageBox.Show("Do you want to activate the selected database? This will cause Thetis to restart.",
    if (!askOperator(QStringLiteral(
            "Do you want to restore the selected backup?\n\n"
            "The current settings are backed up first (\"Before restore\"), "
            "then Longpath closes. Start it again to work with the "
            "restored settings."))) {
        return;
    }
    QString err;
    if (!m_backup.restore(paths.first(), &err)) {
        qCWarning(lcSettingsBackup) << "restore failed:" << err;
        if (!m_ask) {
            QMessageBox::warning(this, windowTitle(),
                                 QStringLiteral("The backup could not be restored:\n%1").arg(err));
        }
        refresh();
        return;
    }
    AppSettings::instance().setSaveInhibited(true);
    emit restoreCompleted();
}

void SettingsBackupDialog::renameSelected()
{
    const QStringList paths = selectedPaths();
    if (paths.size() != 1) { return; }
    const QString current = descriptionOf(paths.first());
    // From Thetis clsDBMan.cs:1676-1677 [@852bf0e]
    //   string desc = InputBox.Show("Database Change Description", "Please edit the description.", tmp_desc, true);
    //   if (string.IsNullOrEmpty(desc) || desc == tmp_desc) return;
    const QString desc = promptOperator(QStringLiteral("Backup Description"),
                                        QStringLiteral("Please edit the description."), current);
    if (desc.isEmpty() || desc == current) { return; }
    m_backup.renameBackup(paths.first(), desc);
    refresh();
}

void SettingsBackupDialog::exportSelected()
{
    const QStringList paths = selectedPaths();
    if (paths.size() != 1) { return; }
    const QString desc = descriptionOf(paths.first());
    // From Thetis clsDBMan.cs:1782-1788 [@852bf0e]
    //   string datetime = Common.DateTimeStringForFile();
    //   string save_file;
    //   if(string.IsNullOrEmpty(desc))
    //       save_file = $"Thetis_database_export_backup_{datetime}.xml";
    //   else
    //       save_file = $"Thetis_database_export_backup_{desc}_{datetime}.xml";
    const QString datetime = dateTimeStringForFile();
    QString saveFile;
    if (desc.isEmpty()) {
        saveFile = QStringLiteral("Longpath_settings_export_backup_%1.xml").arg(datetime);
    } else {
        QString safe = desc;
        // replace any non valid filename chars with _
        static const QString kBad = QStringLiteral("/\\:*?\"<>|");
        for (QChar& c : safe) { if (kBad.contains(c)) { c = QLatin1Char('_'); } }
        saveFile = QStringLiteral("Longpath_settings_export_backup_%1_%2.xml").arg(safe, datetime);
    }
    const QString dest = askSavePath(saveFile);
    if (dest.isEmpty()) { return; }
    QString err;
    if (!m_backup.exportBackup(paths.first(), dest, &err)) {
        qCWarning(lcSettingsBackup) << "export failed:" << err;
    }
}

void SettingsBackupDialog::removeSelected()
{
    const QStringList paths = selectedPaths();
    if (paths.isEmpty()) { return; }
    // From Thetis clsDBMan.cs:1356 [@852bf0e]
    //   string msg = file_paths.Count == 1 ? "Do you want to remove this backup?" : $"Do you want to remove these {file_paths.Count} backups?";
    const QString msg = paths.size() == 1
                            ? QStringLiteral("Do you want to remove this backup?")
                            : QStringLiteral("Do you want to remove these %1 backups?").arg(paths.size());
    if (!askOperator(msg)) { return; }
    m_backup.removeBackups(paths);
    refresh();
}

// From Thetis clsDBMan.cs:1709-1725 [@852bf0e] OpenFolder:
//   Process.Start("explorer.exe", folder_path);
void SettingsBackupDialog::openFolder()
{
    const QString dir = m_backup.backupDir();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void SettingsBackupDialog::closeEvent(QCloseEvent* event)
{
    AppSettings::instance().setValue(kKeyGeometry, saveGeometry());
    event->accept();
    hide();
}

} // namespace Longpath
