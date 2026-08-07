#pragma once

// =================================================================
// src/core/CredentialStore.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. AetherSDR routes secrets to the OS keychain via
// QtKeychain (`src/core/AppSettings.h` [@3a1f59e], "they divert to
// QtKeychain / the session vault"); NereusSDR has no such dependency
// and no credential storage of any kind, so this is the smallest thing
// that is not "write the password into the settings XML".
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//                 Added for the QRZ XML login.
// =================================================================

#include <QString>

namespace NereusSDR {

// Somewhere to put a password that is not the settings file.
//
// macOS: the login keychain, through the `security` command-line tool.
// A CLI call rather than linking Security.framework — it keeps this
// file platform-neutral, needs no new build dependency, and the cost
// (one process spawn) is paid once at startup, not per lookup.
//
// Everywhere else: memory only, for the lifetime of the process. The
// operator re-enters the password each start. That is worse for
// convenience and better than the alternative, and `isPersistent()`
// tells the UI which of the two it got so it can say so plainly
// instead of silently forgetting.
//
// Nothing here is encryption. A keychain entry is readable by anyone
// who can already run code as this user; it protects against the
// password sitting in a plain file that gets copied into a backup, a
// screenshot, or a support bundle — which is the realistic risk.
class CredentialStore {
public:
    // `key` names the secret, e.g. "qrz.password". `account` is the
    // username it belongs to — the keychain keys on both, so changing
    // the username does not silently reuse the old password.
    static bool  store(const QString& key, const QString& account,
                       const QString& secret);
    static QString retrieve(const QString& key, const QString& account);
    static bool  erase(const QString& key, const QString& account);

    // True when secrets survive a restart on this platform.
    static bool isPersistent();

    // Human-readable one-liner for the credentials dialog, so the
    // operator knows where the password went.
    static QString backendDescription();
};

} // namespace NereusSDR
