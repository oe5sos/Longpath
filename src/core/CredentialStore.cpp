// =================================================================
// src/core/CredentialStore.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see CredentialStore.h for provenance and for
// the reasoning behind the CLI-based keychain access.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-09-17 — Keychain service name "NereusSDR: …" → "Longpath: …"
//                 with a one-time move of existing entries.
// =================================================================

#include "CredentialStore.h"

#include <QStandardPaths>   // Pruefstaende fassen den Schluesselbund nicht an
#include "core/AppSettings.h"

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
    // Die Sandbox auch hier getrennt (siehe serviceName()).
    const QString ns = AppSettings::isSandbox() ? QStringLiteral("sandbox\x1f")
                                                : QString{};
    return ns + key + QLatin1Char('\x1f') + account;
}

#ifdef Q_OS_MACOS
constexpr int kSecurityTimeoutMs = 5000;

// The service name the entry appears under in Keychain Access, so the
// operator can find and delete it without going through this app.
QString serviceName(const QString& key)
{
    // Eine Sandbox (LONGPATH_CONFIG_DIR) hat ihren eigenen Namensraum:
    // sie sieht die echten Zugaenge nicht -- sonst luede ein Test-QSO mit
    // dem echten Schluessel ins QRZ-Logbuch des Betreibers hoch
    // (2026-09-26, vor einem Nachttest des Logbuchs gefunden).
    if (AppSettings::isSandbox()) {
        return QStringLiteral("Longpath Sandbox: %1").arg(key);
    }
    return QStringLiteral("Longpath: %1").arg(key);
}

// ── Der Name bis zum 2026-09-17 ─────────────────────────────────────
//
// Der Dienstname ist der SUCHSCHLUESSEL fuer alles, was im Schluessel-
// bund liegt — QRZ-Zugang, SpotHub, KiwiSDR-Passwoerter. Ein blosses
// Umbenennen haette vorhandene Zugangsdaten unauffindbar gemacht (nicht
// kaputt, nur unerreichbar). Darum der Umzug: unter dem alten Namen
// lesen, unter dem neuen schreiben, den alten Eintrag loeschen — beim
// ersten Zugriff, einmal je Eintrag. Betreiber: "wir haben kein nereus
// … weg damit."
QString legacyServiceName(const QString& key)
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

namespace {
// ── Pruefstaende fassen den echten Schluesselbund nicht an (2026-09-30) ──────
//
// QStandardPaths::setTestModeEnabled(true) laeuft in jedem Testbinaer vor
// main() (tests/TestSandboxInit.cpp) und leitet Dateipfade in einen Sandkasten
// um. Den SCHLUESSELBUND leitet es nicht um: der haengt am Benutzerkonto, nicht
// am Pfad. Ein Test, der hier etwas ablegt, schreibt also in den echten
// Anmeldebund des Betreibers — und laesst es dort stehen, wenn er unterwegs
// abbricht.
//
// Aufgefallen am 2026-09-30 beim Bau des TCI-Fernzugriffs: der neue Pruefstand
// legte ein Token an und raeumte es im cleanup() wieder weg. Das ging gut,
// war aber Glueck und kein Entwurf. Deshalb nimmt der Testbetrieb denselben
// Speicher-Tresor, den Linux und Windows ohnehin benutzen.
bool useSessionVaultOnly()
{
    return QStandardPaths::isTestModeEnabled();
}
}  // namespace

bool CredentialStore::isPersistent()
{
    if (useSessionVaultOnly()) { return false; }
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
    // Im Testbetrieb ist der Speicher-Tresor oben alles, was passiert.
    if (useSessionVaultOnly()) { return true; }

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
    if (ok) {
        // Ein Eintrag unter dem Namen von vor dem 2026-09-17 waere ab
        // jetzt nur noch eine zweite, veraltende Kopie.
        runSecurity({
            QStringLiteral("delete-generic-password"),
            QStringLiteral("-s"), legacyServiceName(key),
            QStringLiteral("-a"), account,
        });
    }
    return ok;
#else
    return false;   // session only — caller may warn
#endif
}

QString CredentialStore::retrieve(const QString& key, const QString& account)
{
    if (useSessionVaultOnly()) {
        return sessionVault().value(vaultKey(key, account));
    }
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
    // Der Umzug (siehe legacyServiceName): unter dem alten Namen lesen,
    // unter dem neuen ablegen, den alten Eintrag danach loeschen. Nie aus
    // einer Sandbox -- sie wuerde die echten alten Eintraege loeschen.
    if (!AppSettings::isSandbox() && runSecurity({
            QStringLiteral("find-generic-password"),
            QStringLiteral("-s"), legacyServiceName(key),
            QStringLiteral("-a"), account,
            QStringLiteral("-w"),
        }, &out)) {
        if (runSecurity({
                QStringLiteral("add-generic-password"),
                QStringLiteral("-U"),
                QStringLiteral("-s"), serviceName(key),
                QStringLiteral("-a"), account,
                QStringLiteral("-w"), out,
            })) {
            runSecurity({
                QStringLiteral("delete-generic-password"),
                QStringLiteral("-s"), legacyServiceName(key),
                QStringLiteral("-a"), account,
            });
        }
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
    if (useSessionVaultOnly()) { return true; }
#ifdef Q_OS_MACOS
    const bool ok = runSecurity({
        QStringLiteral("delete-generic-password"),
        QStringLiteral("-s"), serviceName(key),
        QStringLiteral("-a"), account,
    });
    // Ein noch nicht umgezogener Eintrag geht mit; sein Fehlen ist kein Fehler.
    const bool legacyOk = runSecurity({
        QStringLiteral("delete-generic-password"),
        QStringLiteral("-s"), legacyServiceName(key),
        QStringLiteral("-a"), account,
    });
    return ok || legacyOk;
#else
    return true;
#endif
}

} // namespace Longpath
