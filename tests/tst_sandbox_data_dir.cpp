// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test, no Thetis logic.
//
// Eine Sandbox-Instanz (LONGPATH_CONFIG_DIR) darf nie in die echten Daten
// schreiben (2026-09-26): das Logbuch, der QRZ-Zwischenspeicher und die
// Stationsfotos lagen trotz der Variable weiter unter AppConfigLocation --
// ein Test des Logbuchs in der Sandbox haette das echte Log veraendert.

#include <QtTest>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/CallsignCache.h"
#include "gui/widgets/RotorLogbookPanel.h"
#include "gui/widgets/StationPhoto.h"

using namespace Longpath;

class TstSandboxDataDir : public QObject { Q_OBJECT
private slots:
    void withTheOverrideEverythingStaysInTheSandbox()
    {
        QTemporaryDir dir;
        qputenv("LONGPATH_CONFIG_DIR", dir.path().toLocal8Bit());
        QCOMPARE(AppSettings::dataDir(), dir.path());
        QVERIFY(RotorLogbookPanel::logbookPath().startsWith(dir.path()));
        QVERIFY(CallsignCache::defaultPath().startsWith(dir.path()));
        QVERIFY(StationPhoto::cacheDir().startsWith(dir.path()));
        qunsetenv("LONGPATH_CONFIG_DIR");
    }

    void withoutItNothingChanges()
    {
        qunsetenv("LONGPATH_CONFIG_DIR");
        QCOMPARE(AppSettings::dataDir(),
                 QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    }
};

QTEST_MAIN(TstSandboxDataDir)
#include "tst_sandbox_data_dir.moc"
