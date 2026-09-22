#pragma once
// =================================================================
// src/gui/SettingsBackupDialog.h  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/frmDBMan.cs, original licence from
//   Thetis source is included below
//
// The backups half of Thetis's Database Manager window: the list of
// copies (description, time, age, file), Take Backup Now / Restore /
// Rename / Export / Remove / Open Folder, and the three switches
// (backup on start-up, on shut-down, prune auto backups). The upper
// half of that window -- several databases, make active, duplicate,
// import -- is not ported; Longpath has one settings file per profile.
//
// Thetis's "Make the selected backup available" (a new database from a
// backup, then "Make active" + restart) became "Restore": the backup is
// copied over the live settings file, saving is stopped, and Longpath
// closes; the operator starts it again.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Ported for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from Thetis
//                 v2.10.3.15-5-g852bf0e. WinForms ListView became
//                 QTableWidget; InputBox/MessageBox/SaveFileDialog are
//                 operator hooks so tests can answer them.
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
#include "core/SettingsBackup.h"

#include <QDialog>

#include <functional>

class QCheckBox;
class QCloseEvent;
class QLabel;
class QPushButton;
class QTableWidget;

namespace Longpath {

class SettingsBackupDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsBackupDialog(const QString& settingsFilePath,
                                  QWidget* parent = nullptr);

    // Tests: answer the questions without modal boxes.
    //   ask(question)                 -> yes/no (MessageBox YesNo)
    //   prompt(title, label, text)    -> the text, empty = cancelled (InputBox.Show)
    //   saveFile(suggestedName)       -> a path, empty = cancelled (SaveFileDialog)
    void setOperatorHooks(std::function<bool(const QString&)> ask,
                          std::function<QString(const QString&, const QString&, const QString&)> prompt,
                          std::function<QString(const QString&)> saveFile);

    SettingsBackup& backup() { return m_backup; }
    QTableWidget* tableForTest() const { return m_table; }
    QPushButton* restoreButtonForTest() const { return m_restoreBtn; }
    QPushButton* removeButtonForTest() const { return m_removeBtn; }
    QCheckBox* pruneCheckForTest() const { return m_prune; }

    // Re-read the folder into the list (InitBackups).
    void refresh();

    // From Thetis frmDBMan.cs:88-117 [@852bf0e] formatTimeSpanWithYears
    static QString formatAge(qint64 seconds);
    // From Thetis frmDBMan.cs:70-86 [@852bf0e] localDateTimeFormat ("G")
    static QString formatDateTime(const QDateTime& dt);

signals:
    // A backup has been copied over the live settings file and saving is
    // inhibited: the main window should close the program now.
    void restoreCompleted();

public slots:
    // From Thetis frmDBMan.cs:311-321 [@852bf0e] btnTakeBackupNow_Click
    void takeBackupNow();
    // From Thetis frmDBMan.cs:332-341 [@852bf0e] btnMakeBackupAvailable_Click
    // (Longpath: restore, see the header)
    void restoreSelected();
    // From Thetis frmDBMan.cs:459-473 [@852bf0e] btnRenameBackup_Click
    void renameSelected();
    // From Thetis frmDBMan.cs:427-434 [@852bf0e] btnExportBackup_Click
    void exportSelected();
    // From Thetis frmDBMan.cs:343-362 [@852bf0e] btnRemoveBackup_Click
    void removeSelected();
    // From Thetis frmDBMan.cs:436-445 [@852bf0e] btnOpenFolder_Click
    void openFolder();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void updateButtons();
    QStringList selectedPaths() const;
    QString descriptionOf(const QString& path) const;
    bool askOperator(const QString& question);
    QString promptOperator(const QString& title, const QString& label, const QString& text);
    QString askSavePath(const QString& suggestedName);

    SettingsBackup m_backup;
    QTableWidget*  m_table{nullptr};
    QLabel*        m_caption{nullptr};
    QPushButton*   m_takeBtn{nullptr};
    QPushButton*   m_restoreBtn{nullptr};
    QPushButton*   m_renameBtn{nullptr};
    QPushButton*   m_exportBtn{nullptr};
    QPushButton*   m_removeBtn{nullptr};
    QPushButton*   m_folderBtn{nullptr};
    QCheckBox*     m_onStartup{nullptr};
    QCheckBox*     m_onShutdown{nullptr};
    QCheckBox*     m_prune{nullptr};
    std::function<bool(const QString&)> m_ask;
    std::function<QString(const QString&, const QString&, const QString&)> m_prompt;
    std::function<QString(const QString&)> m_saveFile;
};

} // namespace Longpath
