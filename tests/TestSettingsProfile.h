// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from AetherSDR tests/TestSettingsProfile.h [@31b29583].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//
//   2026-08-23 — Portiert, damit Aethers eigene KiwiSDR-Pruefungen fuer
//                Kennwoerter und CSV bei uns laufen koennen.
//
// ── Zwei Aenderungen gegenueber der Vorlage ─────────────────────────
//
// 1. AETHER_SETTINGS_DIR entfaellt. Aether braucht diesen Schalter,
//    weil QStandardPaths unter Windows die Umgebungsvariablen ignoriert
//    (bekannte Ordner laufen ueber die Shell-Schnittstelle) und die
//    Testablagen sonst im gemeinsamen %LOCALAPPDATA%\qttest landen.
//    Bei uns gibt es keinen entsprechenden Schalter in AppSettings, und
//    einen nur fuer Tests einzufuehren waere eine Produktionsaenderung
//    fuer einen Zweck, den setTestModeEnabled bereits erfuellt.
//    Sollte Longpath je unter Windows gepruefft werden, ist das die
//    Stelle, an der es nachzuholen ist.
//
// 2. Der Kommentar zur macOS-CFPreferences-Falle bleibt woertlich —
//    er beschreibt genau das Verhalten, das auch uns treffen wuerde.
//
// Alles Uebrige ist zeichengetreu.

#pragma once

#include <QByteArray>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>

// Must be constructed before QApplication/QCoreApplication and before the
// first AppSettings access.  Qt does not honor the same config environment
// variable on every supported platform, so redirect all relevant homes and
// enable QStandardPaths test mode as a second layer.
class TestSettingsProfile
{
public:
    explicit TestSettingsProfile(const QString& testName)
        : m_root(QDir::tempPath() + QStringLiteral("/") + testName
                 + QStringLiteral("-XXXXXX"))
    {
        if (!m_root.isValid()) {
            return;
        }

        const QByteArray root = m_root.path().toUtf8();
        qputenv("HOME", root);
        qputenv("CFFIXED_USER_HOME", root);
        qputenv("XDG_CONFIG_HOME", root);
        qputenv("LOCALAPPDATA", root);
        qputenv("APPDATA", root);
        QStandardPaths::setTestModeEnabled(true);

        // AppSettings first-run migration still probes the legacy QSettings
        // store. On macOS the native CFPreferences backend can ignore HOME,
        // so force that probe into a private INI directory as well.
        const QString legacyRoot = m_root.path() + QStringLiteral("/legacy-settings");
        QDir().mkpath(legacyRoot);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, legacyRoot);
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, legacyRoot);
    }

    bool isValid() const { return m_root.isValid(); }
    QString path() const { return m_root.path(); }

private:
    QTemporaryDir m_root;
};
