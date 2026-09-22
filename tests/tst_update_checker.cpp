// =================================================================
// tests/tst_update_checker.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. Der Ein-Klick-Updater (Help > Check for
// Updates...) ohne Netz: Release-JSON wie von GitHub, Versionsvergleich,
// Paketwahl je Plattform, SHA256SUMS, die Startpruefungs-Regel, der
// Zielpfad und der Neustart-Helfer -- und auf macOS ein echtes
// Einspielen aus einem selbst gebauten DMG in ein Wegwerf-Ziel.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>

#include "core/UpdateChecker.h"
#include "core/UpdateInstaller.h"
#include "gui/UpdateDialog.h"

using namespace Longpath;

namespace {
// moc vertraegt kein "//" in einem Raw-String (Lehre 2026-09-20), darum
// gewoehnliche Literale.
const QByteArray kReleaseJson = QByteArrayLiteral(
    "{\n"
    "  \"tag_name\": \"v0.6.4\",\n"
    "  \"name\": \"Longpath 0.6.4\",\n"
    "  \"body\": \"## Neu\\n- CW-Decoder\\n\",\n"
    "  \"published_at\": \"2026-09-22T06:00:00Z\",\n"
    "  \"html_url\": \"https://github.com/oe5sos/Longpath/releases/tag/v0.6.4\",\n"
    "  \"assets\": [\n"
    "    {\"name\": \"Longpath-0.6.4-macOS-apple-silicon.dmg\", \"browser_download_url\": \"https://github.com/oe5sos/Longpath/releases/download/v0.6.4/Longpath-0.6.4-macOS-apple-silicon.dmg\", \"size\": 76824617},\n"
    "    {\"name\": \"Longpath-0.6.4-macOS-intel.dmg\", \"browser_download_url\": \"https://github.com/oe5sos/Longpath/releases/download/v0.6.4/Longpath-0.6.4-macOS-intel.dmg\", \"size\": 76582209},\n"
    "    {\"name\": \"Longpath-0.6.4-Windows-x64-setup.exe\", \"browser_download_url\": \"https://github.com/oe5sos/Longpath/releases/download/v0.6.4/Longpath-0.6.4-Windows-x64-setup.exe\", \"size\": 94847639},\n"
    "    {\"name\": \"SHA256SUMS.txt\", \"browser_download_url\": \"https://github.com/oe5sos/Longpath/releases/download/v0.6.4/SHA256SUMS.txt\", \"size\": 1200}\n"
    "  ]\n"
    "}\n"
);
} // namespace

class TstUpdateChecker : public QObject {
    Q_OBJECT

private slots:
    void parsesAGitHubRelease()
    {
        QString err;
        const auto r = UpdateChecker::parseLatestRelease(kReleaseJson, &err);
        QVERIFY2(r.has_value(), qPrintable(err));
        QCOMPARE(r->tag, QStringLiteral("v0.6.4"));
        QCOMPARE(r->version, QStringLiteral("0.6.4"));
        QCOMPARE(r->assets.size(), 4);
        QVERIFY(r->asset(QStringLiteral("SHA256SUMS.txt")).has_value());
        QCOMPARE(r->asset(QStringLiteral("Longpath-0.6.4-macOS-intel.dmg"))->size, qint64(76582209));
        QVERIFY(!r->asset(QStringLiteral("nope")).has_value());
        QVERIFY(r->publishedAt.isValid());
        QVERIFY(r->notes.contains(QStringLiteral("CW-Decoder")));

        QVERIFY(!UpdateChecker::parseLatestRelease(QByteArrayLiteral("{\"message\":\"Not Found\"}"), &err).has_value());
        QVERIFY(!UpdateChecker::parseLatestRelease(QByteArrayLiteral("not json"), &err).has_value());
    }

    void versionsCompareNumerically()
    {
        QCOMPARE(UpdateChecker::numericVersion(QStringLiteral("v0.6.3")), QStringLiteral("0.6.3"));
        QCOMPARE(UpdateChecker::numericVersion(QStringLiteral("0.6.3 · fix/x@abc-dirty")), QStringLiteral("0.6.3"));
        QCOMPARE(UpdateChecker::numericVersion(QStringLiteral("v0.6.3-rc3")), QStringLiteral("0.6.3-rc3"));
        QCOMPARE(UpdateChecker::numericVersion(QStringLiteral("garbage")), QString());

        QVERIFY(UpdateChecker::compareVersions(QStringLiteral("0.6.4"), QStringLiteral("0.6.3")) > 0);
        QVERIFY(UpdateChecker::compareVersions(QStringLiteral("0.6.10"), QStringLiteral("0.6.9")) > 0);   // nicht lexikalisch
        QVERIFY(UpdateChecker::compareVersions(QStringLiteral("0.7"), QStringLiteral("0.6.99")) > 0);
        QCOMPARE(UpdateChecker::compareVersions(QStringLiteral("v0.6.3"), QStringLiteral("0.6.3")), 0);
        QVERIFY(UpdateChecker::compareVersions(QStringLiteral("0.6.3-rc3"), QStringLiteral("0.6.3")) < 0);  // Vorab < fertig
        QVERIFY(UpdateChecker::compareVersions(QStringLiteral("0.6.3-rc2"), QStringLiteral("0.6.3-rc1")) > 0);
        QVERIFY(UpdateChecker::compareVersions(QStringLiteral("0.6.3"), QStringLiteral("0.6.3 · x@y-dirty")) == 0);
    }

    void picksThePackageForTheMachine()
    {
        QCOMPARE(UpdateChecker::assetNameFor(QStringLiteral("v0.6.4"), QStringLiteral("macos"), QStringLiteral("arm64")),
                 QStringLiteral("Longpath-0.6.4-macOS-apple-silicon.dmg"));
        QCOMPARE(UpdateChecker::assetNameFor(QStringLiteral("0.6.4"), QStringLiteral("macos"), QStringLiteral("x86_64")),
                 QStringLiteral("Longpath-0.6.4-macOS-intel.dmg"));
        QCOMPARE(UpdateChecker::assetNameFor(QStringLiteral("0.6.4"), QStringLiteral("windows"), QStringLiteral("x86_64")),
                 QStringLiteral("Longpath-0.6.4-Windows-x64-setup.exe"));
        QCOMPARE(UpdateChecker::assetNameFor(QStringLiteral("0.6.4"), QStringLiteral("linux"), QStringLiteral("x86_64")),
                 QStringLiteral("Longpath-0.6.4-x86_64.AppImage"));
        QCOMPARE(UpdateChecker::assetNameFor(QStringLiteral("0.6.4"), QStringLiteral("linux"), QStringLiteral("arm64")),
                 QStringLiteral("Longpath-0.6.4-aarch64.AppImage"));
        // Und die Wahl fuer DIESE Maschine ist einer der Namen aus dem Release.
        const auto r = UpdateChecker::parseLatestRelease(kReleaseJson);
        const QString mine = UpdateChecker::assetNameForThisMachine(r->version);
        QVERIFY(mine.startsWith(QStringLiteral("Longpath-0.6.4-")));
    }

    void checksumsParseAndMatch()
    {
        const QByteArray sums =
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef  Longpath-0.6.4-macOS-intel.dmg\n"
            "ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789 *Longpath-0.6.4-Windows-x64-setup.exe\n"
            "not a checksum line\n";
        const auto parsed = UpdateChecker::parseSha256Sums(sums);
        QCOMPARE(parsed.size(), 2);
        QCOMPARE(parsed.value(QStringLiteral("Longpath-0.6.4-macOS-intel.dmg")),
                 QStringLiteral("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
        QCOMPARE(parsed.value(QStringLiteral("Longpath-0.6.4-Windows-x64-setup.exe")).left(6), QStringLiteral("abcdef"));

        QTemporaryDir dir;
        const QString f = dir.filePath(QStringLiteral("blob.bin"));
        QFile out(f); QVERIFY(out.open(QIODevice::WriteOnly)); out.write("Longpath"); out.close();
        // sha256("Longpath") -- unabhaengig berechnet
        QCOMPARE(UpdateChecker::sha256Of(f),
                 QString::fromLatin1(QCryptographicHash::hash(QByteArrayLiteral("Longpath"), QCryptographicHash::Sha256).toHex()));
        QCOMPARE(UpdateChecker::sha256Of(dir.filePath(QStringLiteral("missing"))), QString());
    }

    void startupCheckRule()
    {
        const QDateTime now = QDateTime::fromString(QStringLiteral("2026-09-22T08:00:00Z"), Qt::ISODate);
        QVERIFY(UpdateDialog::startupCheckDue(true, QDateTime(), now));                         // nie geprueft
        QVERIFY(UpdateDialog::startupCheckDue(true, now.addSecs(-21 * 3600), now));            // 21 h her
        QVERIFY(!UpdateDialog::startupCheckDue(true, now.addSecs(-3 * 3600), now));            // 3 h her
        QVERIFY(!UpdateDialog::startupCheckDue(false, QDateTime(), now));                      // Haken aus
    }

    void installTargetAndRelaunch()
    {
        QCOMPARE(UpdateInstaller::runningBundlePath(QStringLiteral("/Applications/Longpath.app/Contents/MacOS")),
                 QStringLiteral("/Applications/Longpath.app"));
        QCOMPARE(UpdateInstaller::runningBundlePath(QStringLiteral("/Users/x/build/bin")), QString());
        QCOMPARE(UpdateInstaller::installTargetPath(QStringLiteral("/Users/x/build/bin"), QString()),
                 QStringLiteral("/Applications/Longpath.app"));
        QCOMPARE(UpdateInstaller::installTargetPath(QStringLiteral("/Users/x/Apps/Longpath.app/Contents/MacOS"), QString()),
                 QStringLiteral("/Users/x/Apps/Longpath.app"));
        QCOMPARE(UpdateInstaller::installTargetPath(QStringLiteral("/Applications/Longpath.app/Contents/MacOS"),
                                                    QStringLiteral("/tmp/t/Longpath.app")),
                 QStringLiteral("/tmp/t/Longpath.app"));
        const QStringList script = UpdateInstaller::relaunchScript(4711, QStringLiteral("/Applications/Longpath.app"), true);
        QCOMPARE(script.size(), 2);
        QCOMPARE(script.at(0), QStringLiteral("-c"));
        QVERIFY(script.at(1).contains(QStringLiteral("kill -0 4711")));
        QVERIFY(script.at(1).endsWith(QStringLiteral("open \"/Applications/Longpath.app\"")));
        QVERIFY(UpdateInstaller::relaunchScript(1, QStringLiteral("/x/L.AppImage"), false).at(1)
                    .endsWith(QStringLiteral("exec \"/x/L.AppImage\"")));
        // Die Argumente dieses Laufs (--profile) reisen mit.
        const QString withArgs = UpdateInstaller::relaunchScript(
            1, QStringLiteral("/Applications/Longpath.app"), true,
            {QStringLiteral("--profile"), QStringLiteral("bench")}).at(1);
        QVERIFY(withArgs.endsWith(QStringLiteral("open \"/Applications/Longpath.app\" --args \"--profile\" \"bench\"")));
    }

    void dialogShowsTheReleaseAndArmsTheButton()
    {
        UpdateDialog dlg(QStringLiteral("0.6.3"));
        const auto r = UpdateChecker::parseLatestRelease(kReleaseJson);
        QVERIFY(r.has_value());
        dlg.showRelease(*r, true);
        const QString wanted = UpdateChecker::assetNameForThisMachine(r->version);
        if (r->asset(wanted)) {
            QVERIFY(dlg.actionForTest()->isEnabled());
            QCOMPARE(dlg.actionForTest()->text(), QStringLiteral("Download and install"));
            QVERIFY(dlg.statusForTest()->text().contains(QStringLiteral("0.6.4")));
        } else {
            // Linux-Pruefstand: das Beispiel-Release traegt kein AppImage.
            QCOMPARE(dlg.actionForTest()->text(), QStringLiteral("Open release page"));
        }
        dlg.showRelease(*r, false);
        QVERIFY(!dlg.actionForTest()->isEnabled());
        QVERIFY(dlg.statusForTest()->text().contains(QStringLiteral("up to date")));
    }

    void installsFromADiskImageOnMac()
    {
#if !defined(Q_OS_MAC)
        QSKIP("hdiutil/ditto: macOS only");
#else
        // Ein winziges "Programmpaket" in ein DMG packen ...
        QTemporaryDir work;
        const QString stage = work.filePath(QStringLiteral("stage"));
        QDir().mkpath(stage + QStringLiteral("/Longpath.app/Contents/MacOS"));
        {
            QFile f(stage + QStringLiteral("/Longpath.app/Contents/MacOS/Longpath"));
            QVERIFY(f.open(QIODevice::WriteOnly)); f.write("#!/bin/sh\necho new\n"); f.close();
            QFile p(stage + QStringLiteral("/Longpath.app/Contents/Info.plist"));
            QVERIFY(p.open(QIODevice::WriteOnly)); p.write("<plist/>"); p.close();
        }
        const QString dmg = work.filePath(QStringLiteral("Longpath-9.9.9-macOS-apple-silicon.dmg"));
        QProcess mk;
        mk.start(QStringLiteral("/usr/bin/hdiutil"),
                 {QStringLiteral("create"), QStringLiteral("-quiet"), QStringLiteral("-srcfolder"), stage,
                  QStringLiteral("-volname"), QStringLiteral("Longpath"), QStringLiteral("-format"), QStringLiteral("UDZO"), dmg});
        QVERIFY(mk.waitForFinished(60000));
        QCOMPARE(mk.exitCode(), 0);

        // ... und in ein Wegwerf-Ziel einspielen, wo schon ein "altes" liegt.
        const QString target = work.filePath(QStringLiteral("dest/Longpath.app"));
        QDir().mkpath(target + QStringLiteral("/Contents/MacOS"));
        { QFile f(target + QStringLiteral("/Contents/MacOS/Longpath")); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("old"); f.close(); }
        qputenv("LONGPATH_UPDATE_TARGET", target.toUtf8());
        UpdateInstaller inst;
        QSignalSpy installed(&inst, &UpdateInstaller::installed);
        QSignalSpy failed(&inst, &UpdateInstaller::failed);
        inst.installAndRelaunch(dmg);
        qunsetenv("LONGPATH_UPDATE_TARGET");
        QVERIFY2(failed.isEmpty(), failed.isEmpty() ? "" : qPrintable(failed.first().first().toString()));
        QCOMPARE(installed.count(), 1);
        QCOMPARE(installed.first().first().toString(), target);
        QFile got(target + QStringLiteral("/Contents/MacOS/Longpath"));
        QVERIFY(got.open(QIODevice::ReadOnly));
        QVERIFY(got.readAll().contains("echo new"));
        // Das Alte ist weg, nichts liegt daneben.
        const QStringList left = QDir(work.filePath(QStringLiteral("dest"))).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        QCOMPARE(left, QStringList{QStringLiteral("Longpath.app")});
#endif
    }
};

QTEST_MAIN(TstUpdateChecker)
#include "tst_update_checker.moc"
