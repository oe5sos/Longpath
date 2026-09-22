// =================================================================
// src/gui/UpdateDialog.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Betreiber 2026-09-21: "unter Help moechte ich den automatischen
// Downloader/Installer der neuesten Version. Ein Klick, Installation
// automatisch." Help > Check for Updates...: fragt die neueste
// Veroeffentlichung ab (UpdateChecker), zeigt Version und Notizen, und
// EIN Knopf macht den Rest -- herunterladen, Pruefsumme, einspielen
// (UpdateInstaller), Longpath neu starten.
//
// Dazu die stille Pruefung beim Start (Haken im Dialog, Vorgabe an,
// hoechstens einmal je 20 Stunden): gibt es eine neuere Version, geht
// dieser Dialog von selbst auf; sonst passiert nichts, auch offline.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#pragma once

#include "core/UpdateChecker.h"

#include <QDialog>

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTextBrowser;

namespace Longpath {

class UpdateInstaller;

class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateDialog(const QString& runningVersion, QWidget* parent = nullptr);
    ~UpdateDialog() override;

    // Einstellungsschluessel (AppSettings, "True"/"False" bzw. ISO-Datum).
    static QString startupCheckKey() { return QStringLiteral("UpdateCheckOnStartup"); }
    static QString lastCheckKey()    { return QStringLiteral("UpdateLastCheckUtc"); }
    // Ob die stille Startpruefung jetzt faellig ist (Haken an, letzte
    // Pruefung aelter als 20 h oder nie).
    static bool startupCheckDue(bool enabled, const QDateTime& lastCheckUtc,
                                const QDateTime& nowUtc);

    void checkNow();
    // Fuer die Startpruefung: ein schon bekanntes Ergebnis anzeigen.
    void showRelease(const ReleaseInfo& release, bool newer);

    // Fuer Tests.
    QLabel*       statusForTest() const { return m_status; }
    QPushButton*  actionForTest() const { return m_action; }
    QProgressBar* progressForTest() const { return m_progress; }
    UpdateChecker* checkerForTest() const { return m_checker; }

signals:
    // Eingespielt und Neustart-Helfer gestartet: der Aufrufer beendet
    // das Programm.
    void restartRequested();

private:
    void buildUi();
    void setBusy(bool busy);
    void onLatestKnown(const ReleaseInfo& release, bool newer);
    void onCheckFailed(const QString& why);
    void onDownloadVerified(const QString& path);
    void onInstalled(const QString& target);
    void onActionClicked();
    void startDownload();

    enum class Action { None, Download, Retry, OpenPage };
    Action m_mode{Action::None};

    QString m_runningVersion;
    UpdateChecker* m_checker{nullptr};
    UpdateInstaller* m_installer{nullptr};
    std::optional<ReleaseInfo> m_release;
    std::optional<ReleaseAsset> m_asset;

    QLabel*        m_versions{nullptr};
    QLabel*        m_status{nullptr};
    QTextBrowser*  m_notes{nullptr};
    QProgressBar*  m_progress{nullptr};
    QPushButton*   m_action{nullptr};
    QPushButton*   m_close{nullptr};
    QCheckBox*     m_startupCheck{nullptr};
};

} // namespace Longpath
