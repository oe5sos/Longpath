// =================================================================
// src/gui/UpdateDialog.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Zweck: UpdateDialog.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "UpdateDialog.h"

#include "core/AppSettings.h"
#include "core/UpdateInstaller.h"
#include "gui/StyleConstants.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>

namespace Longpath {

namespace {
constexpr qint64 kStartupCheckIntervalSecs = 20 * 60 * 60;

QString megabytes(qint64 bytes)
{
    return QString::number(static_cast<double>(bytes) / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
}
} // namespace

bool UpdateDialog::startupCheckDue(bool enabled, const QDateTime& lastCheckUtc, const QDateTime& nowUtc)
{
    if (!enabled) { return false; }
    if (!lastCheckUtc.isValid()) { return true; }
    return lastCheckUtc.secsTo(nowUtc) >= kStartupCheckIntervalSecs;
}

UpdateDialog::UpdateDialog(const QString& runningVersion, QWidget* parent)
    : QDialog(parent)
    , m_runningVersion(UpdateChecker::numericVersion(runningVersion))
    , m_checker(new UpdateChecker(runningVersion, this))
    , m_installer(new UpdateInstaller(this))
{
    setWindowTitle(QStringLiteral("Longpath Update"));
    setModal(false);
    buildUi();

    connect(m_checker, &UpdateChecker::latestKnown, this, &UpdateDialog::onLatestKnown);
    connect(m_checker, &UpdateChecker::checkFailed, this, &UpdateDialog::onCheckFailed);
    connect(m_checker, &UpdateChecker::downloadProgress, this, [this](qint64 got, qint64 total) {
        if (total > 0) {
            m_progress->setRange(0, 1000);
            m_progress->setValue(static_cast<int>(got * 1000 / total));
            m_status->setText(QStringLiteral("Downloading %1 of %2").arg(megabytes(got), megabytes(total)));
        } else {
            m_progress->setRange(0, 0);
            m_status->setText(QStringLiteral("Downloading %1").arg(megabytes(got)));
        }
    });
    connect(m_checker, &UpdateChecker::verifying, this, [this]() {
        m_progress->setRange(0, 0);
        m_status->setText(QStringLiteral("Verifying the checksum"));
    });
    connect(m_checker, &UpdateChecker::downloadVerified, this, &UpdateDialog::onDownloadVerified);
    connect(m_checker, &UpdateChecker::downloadFailed, this, [this](const QString& why) {
        setBusy(false);
        m_status->setText(QStringLiteral("Download failed: %1").arg(why));
        m_action->setText(QStringLiteral("Try again"));
        m_mode = Action::Download;
        m_action->setEnabled(true);
    });
    connect(m_installer, &UpdateInstaller::progress, this, [this](const QString& step) {
        m_status->setText(step);
    });
    connect(m_installer, &UpdateInstaller::failed, this, [this](const QString& why) {
        setBusy(false);
        m_status->setText(QStringLiteral("Install failed: %1").arg(why));
        m_action->setText(QStringLiteral("Try again"));
        m_mode = Action::Download;
        m_action->setEnabled(true);
    });
    connect(m_installer, &UpdateInstaller::installed, this, &UpdateDialog::onInstalled);
}

UpdateDialog::~UpdateDialog()
{
    if (m_checker) { m_checker->cancel(); }
}

void UpdateDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 12);
    root->setSpacing(8);

    m_versions = new QLabel(QStringLiteral("Installed: %1").arg(m_runningVersion), this);
    m_versions->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;").arg(Style::kTextPrimary));
    root->addWidget(m_versions);

    m_status = new QLabel(QStringLiteral("Checking for the latest release…"), this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    root->addWidget(m_status);

    m_notes = new QTextBrowser(this);
    m_notes->setOpenExternalLinks(true);
    m_notes->setMinimumHeight(160);
    m_notes->setStyleSheet(QStringLiteral(
        "QTextBrowser { background: %1; color: %2; border: 1px solid %3; }")
        .arg(Style::kInsetBg, Style::kTextPrimary, Style::kInsetBorder));
    root->addWidget(m_notes, 1);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(8);
    root->addWidget(m_progress);

    m_startupCheck = new QCheckBox(QStringLiteral("Check for updates when Longpath starts"), this);
    m_startupCheck->setChecked(AppSettings::instance()
                                   .value(startupCheckKey(), QStringLiteral("True")).toString()
                               == QStringLiteral("True"));
    connect(m_startupCheck, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(startupCheckKey(),
                                         on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    root->addWidget(m_startupCheck);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    m_close = new QPushButton(QStringLiteral("Close"), this);
    connect(m_close, &QPushButton::clicked, this, &QDialog::close);
    buttons->addWidget(m_close);
    m_action = new QPushButton(QStringLiteral("Download and install"), this);
    m_action->setDefault(true);
    m_action->setEnabled(false);
    connect(m_action, &QPushButton::clicked, this, &UpdateDialog::onActionClicked);
    buttons->addWidget(m_action);
    root->addLayout(buttons);

    resize(560, 420);
}

void UpdateDialog::setBusy(bool busy)
{
    m_progress->setVisible(busy);
    m_action->setEnabled(!busy);
    m_close->setEnabled(!busy);
}

void UpdateDialog::checkNow()
{
    m_release.reset();
    m_asset.reset();
    m_mode = Action::None;
    m_action->setEnabled(false);
    m_action->setText(QStringLiteral("Download and install"));
    m_progress->setRange(0, 0);
    m_progress->setVisible(true);
    m_status->setText(QStringLiteral("Checking for the latest release…"));
    m_checker->checkLatest();
}

void UpdateDialog::showRelease(const ReleaseInfo& release, bool newer)
{
    onLatestKnown(release, newer);
}

void UpdateDialog::onLatestKnown(const ReleaseInfo& release, bool newer)
{
    AppSettings::instance().setValue(lastCheckKey(),
                                     QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    m_release = release;
    m_progress->setVisible(false);
    m_versions->setText(QStringLiteral("Installed: %1    Latest: %2%3")
                            .arg(m_runningVersion, release.version,
                                 release.publishedAt.isValid()
                                     ? QStringLiteral(" (%1)").arg(release.publishedAt.toLocalTime().toString(QStringLiteral("d MMM yyyy")))
                                     : QString()));
    m_notes->setMarkdown(release.notes.isEmpty() ? QStringLiteral("_No release notes._") : release.notes);

    const QString wanted = UpdateChecker::assetNameForThisMachine(release.version);
    m_asset = release.asset(wanted);
    if (!newer) {
        m_status->setText(QStringLiteral("Longpath %1 is up to date.").arg(m_runningVersion));
        m_mode = Action::None;
        m_action->setEnabled(false);
        return;
    }
    if (!m_asset) {
        m_status->setText(QStringLiteral("Version %1 is available, but it carries no package for this machine (%2). "
                                         "Open the release page to pick one by hand.")
                              .arg(release.version, wanted));
        m_action->setText(QStringLiteral("Open release page"));
        m_mode = Action::OpenPage;
        m_action->setEnabled(true);
        return;
    }
    m_status->setText(QStringLiteral("Version %1 is available (%2, %3). One click downloads, verifies and installs it, then restarts Longpath.")
                          .arg(release.version, m_asset->name, megabytes(m_asset->size)));
    m_action->setText(QStringLiteral("Download and install"));
    m_mode = Action::Download;
    m_action->setEnabled(true);
}

void UpdateDialog::onCheckFailed(const QString& why)
{
    m_progress->setVisible(false);
    m_status->setText(QStringLiteral("Could not check for updates: %1").arg(why));
    m_action->setText(QStringLiteral("Try again"));
    m_mode = Action::Retry;
    m_action->setEnabled(true);
}

void UpdateDialog::onActionClicked()
{
    // Sichtbar im Log, welcher Weg den Knopf ausgeloest hat -- ein
    // Download darf nur auf einen Klick hin starten.
    qInfo() << "UpdateDialog: action button triggered, mode" << static_cast<int>(m_mode)
            << "sender" << (sender() ? sender()->metaObject()->className() : "none")
            << "focus" << (QApplication::focusWidget() ? QApplication::focusWidget()->metaObject()->className() : "none");
    switch (m_mode) {
    case Action::Download: startDownload(); break;
    case Action::Retry:    checkNow(); break;
    case Action::OpenPage: if (m_release) { QDesktopServices::openUrl(m_release->htmlUrl); } break;
    case Action::None:     break;
    }
}

void UpdateDialog::startDownload()
{
    if (!m_release || !m_asset) { return; }
    qInfo() << "UpdateDialog: download starts for" << m_asset->name;
    setBusy(true);
    m_progress->setRange(0, 0);
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + QStringLiteral("/longpath-update");
    m_status->setText(QStringLiteral("Downloading %1").arg(m_asset->name));
    m_checker->downloadAndVerify(*m_release, *m_asset, dir);
}

void UpdateDialog::onDownloadVerified(const QString& path)
{
    m_status->setText(QStringLiteral("Installing"));
    m_installer->installAndRelaunch(path);
}

void UpdateDialog::onInstalled(const QString& target)
{
    m_status->setText(QStringLiteral("Installed to %1 -- restarting Longpath").arg(target));
    m_progress->setVisible(false);
#if defined(Q_OS_WIN)
    // Der Installer laeuft schon; er ersetzt die Dateien, sobald wir weg sind.
    Q_UNUSED(target);
#else
    QStringList args = QCoreApplication::arguments();
    if (!args.isEmpty()) { args.removeFirst(); }
    QProcess::startDetached(QStringLiteral("/bin/sh"),
                            UpdateInstaller::relaunchScript(QCoreApplication::applicationPid(), target,
#if defined(Q_OS_MAC)
                                                            true,
#else
                                                            false,
#endif
                                                            args));
#endif
    emit restartRequested();
}

} // namespace Longpath
