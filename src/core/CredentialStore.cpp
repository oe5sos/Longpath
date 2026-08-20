// =================================================================
// src/core/CredentialStore.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see CredentialStore.h for provenance and for
// the reasoning behind the CLI-based keychain access.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "CredentialStore.h"

#include <QHash>
#include <QProcess>

namespace Longpath {

namespace {

// Session fallback for platforms without a keychain path, and for a
// macOS keychain that refuses (locked, denied, or running headless in
// CI). Never written to disk.
QHash<QString, QString>& sessionVault()
{
    static QHash<QString, QString> vault;
    return vault;
}

QString vaultKey(const QString& key, const QString& account)
{
    return key + QLatin1Char('\x1f') + account;
}

#ifdef Q_OS_MACOS
constexpr int kSecurityTimeoutMs = 5000;

// The service name the entry appears under in Keychain Access, so the
// operator can find and delete it without going through this app.
QString serviceName(const QString& key)
{
    return QStringLiteral("NereusSDR: %1").arg(key);
}

// Runs `security` and reports whether it succeeded. The secret is
// passed as an argument, which is visible in the process list for the
// lifetime of the call — a few milliseconds, and only to this user.
// stdin would avoid even that, but `security` reads the password from
// argv only; the alternative is linking Security.framework, which is
// the right fix if this ever guards something more valuable than a
// callsign-lookup login.
bool runSecurity(const QStringList& args, QString* stdOut = nullptr)
{
    QProcess proc;
    proc.start(QStringLiteral("/usr/bin/security"), args);
    if (!proc.waitForFinished(kSecurityTimeoutMs)) {
        proc.kill();
        return false;
    }
    if (stdOut) {
        *stdOut = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    }
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}
#endif

} // namespace

bool CredentialStore::isPersistent()
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

QString CredentialStore::backendDescription()
{
#ifdef Q_OS_MACOS
    // User-visible text — plain English, no source cites.
    return QStringLiteral("Stored in your macOS login keychain");
#else
    return QStringLiteral("Kept for this session only — re-enter after a restart");
#endif
}

bool CredentialStore::store(const QString& key, const QString& account,
                            const QString& secret)
{
    sessionVault().insert(vaultKey(key, account), secret);

#ifdef Q_OS_MACOS
    // -U updates in place; without it a second save fails with
    // "already exists" and the operator sees a changed password
    // silently not take effect.
    const bool ok = runSecurity({
        QStringLiteral("add-generic-password"),
        QStringLiteral("-U"),
        QStringLiteral("-s"), serviceName(key),
        QStringLiteral("-a"), account,
        QStringLiteral("-w"), secret,
    });
    return ok;
#else
    return false;   // session only — caller may warn
#endif
}

QString CredentialStore::retrieve(const QString& key, const QString& account)
{
#ifdef Q_OS_MACOS
    QString out;
    if (runSecurity({
            QStringLiteral("find-generic-password"),
            QStringLiteral("-s"), serviceName(key),
            QStringLiteral("-a"), account,
            QStringLiteral("-w"),
        }, &out)) {
        return out;
    }
    // Fall through to the session copy: the keychain may be locked, or
    // the operator may have denied access this once.
#endif
    return sessionVault().value(vaultKey(key, account));
}

bool CredentialStore::erase(const QString& key, const QString& account)
{
    sessionVault().remove(vaultKey(key, account));
#ifdef Q_OS_MACOS
    return runSecurity({
        QStringLiteral("delete-generic-password"),
        QStringLiteral("-s"), serviceName(key),
        QStringLiteral("-a"), account,
    });
#else
    return true;
#endif
}

} // namespace Longpath
