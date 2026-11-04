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
//   2026-10-14 -- Zweiter Weg ueber github.com, wenn api.github.com
//                 nicht antwortet, by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
//   2026-11-04 -- Dritter Weg ueber CHANGELOG.md, by Martin Fischer
//                 (OE5SOS), AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QFile>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/UpdateChecker.h"
#include "core/UpdateInstaller.h"
#include "gui/TitleBar.h"
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

// CHANGELOG.md wie im Baum: ein Titel, dann Abschnitte je Version, mit
// "### " darin (die duerfen den Abschnitt NICHT beenden).
const QByteArray kChangelog = QByteArrayLiteral(
    "# Changelog\n"
    "\n"
    "## [0.8.2] - 2026-10-22\n"
    "\n"
    "Ein Punkt, und er betrifft Windows.\n"
    "\n"
    "### Tondiagnose auf jeder Plattform (#326)\n"
    "\n"
    "Der Eintrag stand hinter einem Plattform-Tor.\n"
    "\n"
    "## [0.8.1] - 2026-10-22\n"
    "\n"
    "Aelter, darf nicht gewinnen.\n"
);
} // namespace

class TstUpdateChecker : public QObject {
    Q_OBJECT

private slots:
    // ── Ein Fehlschlag darf nicht aussehen wie „noch nichts gehoert" ──
    //
    // Betreiber, 2026-11-03 nachts: „grundsaetzlich sollten alle
    // versionen einen automatischen updater haben!" -- und ein
    // Updater, der stillschweigend scheitert, ist keiner.
    //
    // Bis 2026-11-04 hing am Signal checkFailed nur ein qWarning und
    // ein deleteLater. Im Kopf blieb damit stehen, was vor der
    // Abfrage dort stand, und „nichts gesehen" hiess wieder
    // zweierlei: alles aktuell ODER gar nicht nachgesehen. Genau das
    // hat am 2026-10-14 auf dem Mac mini eine Stunde gekostet, als
    // die Pruefung bei jedem Start in einen haengenden
    // TLS-Handshake lief.
    void aFailedCheckSaysSoInTheTitleBar()
    {
        TitleBar leiste(nullptr);
        auto* anzeige = leiste.findChild<QLabel*>(QStringLiteral("titleBarUpdateState"));
        QVERIFY2(anzeige, "keine Aktualisierungsanzeige im Kopf");
        const QString vorher = anzeige->text();

        leiste.setUpdateState(QStringLiteral("· not checked"),
                              TitleBar::AktualisierungsStand::Unbekannt,
                              QStringLiteral("Host api.github.com not found"));
        QVERIFY2(anzeige->text() != vorher,
                 "die Anzeige steht nach einem Fehlschlag noch auf dem alten Text");
        QCOMPARE(anzeige->text(), QStringLiteral("· not checked"));
        // Der Grund gehoert in den Tooltip -- auf der Leiste waere er
        // zu lang, beim Melden eines Fehlers ist er aber genau das,
        // was zaehlt.
        QVERIFY2(anzeige->toolTip().contains(QStringLiteral("api.github.com")),
                 qPrintable(QStringLiteral("Tooltip ohne Grund: \"%1\"").arg(anzeige->toolTip())));

        // Und ohne Grund steht wieder die allgemeine Erklaerung da,
        // nicht ein leerer Tooltip.
        leiste.setUpdateState(QStringLiteral("· up to date"),
                              TitleBar::AktualisierungsStand::Aktuell);
        QVERIFY2(!anzeige->toolTip().isEmpty(), "Tooltip nach up-to-date ist leer");
        QVERIFY2(!anzeige->toolTip().contains(QStringLiteral("api.github.com")),
                 "der alte Fehlergrund klebt im Tooltip fest");
    }

    // Die Verdrahtung selbst: im Fehlzweig der Startpruefung MUSS
    // setUpdateState stehen. Geprueft wird genau der eine Lambda-Rumpf
    // -- nicht ein Fenster von n Zeichen um die Fundstelle, das beim
    // letzten Mal den Nachbarn mitgenommen hat (Lehre 2026-10-30).
    void theFailureBranchIsWiredToTheTitleBar()
    {
        const QString wurzel = qEnvironmentVariable("LONGPATH_SOURCE_DIR");
        QVERIFY2(!wurzel.isEmpty(), "LONGPATH_SOURCE_DIR fehlt -- ueber ctest laufen lassen");
        QFile datei(wurzel + QStringLiteral("/src/gui/MainWindow.cpp"));
        QVERIFY2(datei.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(datei.fileName()));
        const QString quelle = QString::fromUtf8(datei.readAll());

        const int anfang = quelle.indexOf(QStringLiteral("UpdateChecker::checkFailed"));
        QVERIFY2(anfang > 0, "der Fehlzweig der Startpruefung ist verschwunden");
        const int ende = quelle.indexOf(QStringLiteral("});"), anfang);
        QVERIFY2(ende > anfang, "kein Ende des Lambda gefunden");
        const QString rumpf = quelle.mid(anfang, ende - anfang);
        // Kurz genug, dass hier wirklich nur dieser eine Zweig steht.
        QVERIFY2(rumpf.length() < 1100,
                 qPrintable(QStringLiteral("Der abgegriffene Rumpf ist %1 Zeichen lang -- "
                                           "da steht mehr als der Fehlzweig drin").arg(rumpf.length())));
        QVERIFY2(rumpf.contains(QStringLiteral("setUpdateState")),
                 "der Fehlzweig sagt dem Kopf nichts -- der Bediener sieht einen leeren Platz");
    }

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
        // Ein Entwicklerbau (2026-11-04, Worktree-Bau vom Updater
        // ueberschrieben, danach SIGABRT beim Start) ist kein Ziel:
        // dann /Applications, nicht das laufende Buendel.
        QVERIFY(UpdateInstaller::isDeveloperBundle(
            QStringLiteral("/Users/x/Longpath/worktrees/foo/build/Longpath.app")));
        QVERIFY(UpdateInstaller::isDeveloperBundle(QStringLiteral("/Users/x/L/build-debug/Longpath.app")));
        QVERIFY(UpdateInstaller::isDeveloperBundle(QStringLiteral("/Users/x/L/cmake-build-release/Longpath.app")));
        QVERIFY(!UpdateInstaller::isDeveloperBundle(QStringLiteral("/Applications/Longpath.app")));
        QVERIFY(!UpdateInstaller::isDeveloperBundle(QStringLiteral("/Users/x/Apps/Longpath.app")));
        QCOMPARE(UpdateInstaller::installTargetPath(
                     QStringLiteral("/Users/x/Longpath/worktrees/foo/build/Longpath.app/Contents/MacOS"), QString()),
                 QStringLiteral("/Applications/Longpath.app"));
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

    // ── Zweiter Weg ohne api.github.com (2026-10-14) ──────────────────
    //
    // Der Mac mini des Betreibers stand auf 0.6.5 und kam nie weiter:
    // TCP zu api.github.com (Azure) steht, der TLS-Handshake bleibt
    // haengen. github.com selbst antwortet. Diese reinen Funktionen sind
    // der Unterbau dafuer.

    void readsTheTagFromAReleaseUrl()
    {
        QCOMPARE(UpdateChecker::tagFromReleaseUrl(
                     QUrl(QStringLiteral("https://github.com/oe5sos/Longpath/releases/tag/v0.6.8"))),
                 QStringLiteral("v0.6.8"));
        // Vorab-Anhang bleibt am Tag.
        QCOMPARE(UpdateChecker::tagFromReleaseUrl(
                     QUrl(QStringLiteral("https://github.com/oe5sos/Longpath/releases/tag/v0.7.0-rc2"))),
                 QStringLiteral("v0.7.0-rc2"));
        // Alles, was nicht auf ein Tag zeigt, gilt nicht: die
        // Anmeldeseite (privates Repo), die Uebersicht, ein leerer Pfad.
        QVERIFY(UpdateChecker::tagFromReleaseUrl(
                    QUrl(QStringLiteral("https://github.com/login?return_to=%2Foe5sos"))).isEmpty());
        QVERIFY(UpdateChecker::tagFromReleaseUrl(
                    QUrl(QStringLiteral("https://github.com/oe5sos/Longpath/releases"))).isEmpty());
        QVERIFY(UpdateChecker::tagFromReleaseUrl(QUrl()).isEmpty());
        // Ein Tag ohne Ziffern ist keines von uns.
        QVERIFY(UpdateChecker::tagFromReleaseUrl(
                    QUrl(QStringLiteral("https://github.com/oe5sos/Longpath/releases/tag/latest"))).isEmpty());
    }

    void buildsTheReleaseFromTheTagAlone()
    {
        const ReleaseInfo r = UpdateChecker::releaseFromTag(QStringLiteral("v0.6.8"));
        QCOMPARE(r.tag, QStringLiteral("v0.6.8"));
        QCOMPARE(r.version, QStringLiteral("0.6.8"));
        QCOMPARE(r.htmlUrl.toString(),
                 QStringLiteral("https://github.com/oe5sos/Longpath/releases/tag/v0.6.8"));
        // SHA256SUMS.txt muss dabei sein, sonst bricht downloadAndVerify
        // mit "release carries no SHA256SUMS.txt" ab.
        const auto sums = r.asset(QStringLiteral("SHA256SUMS.txt"));
        QVERIFY(sums.has_value());
        QCOMPARE(sums->url.toString(),
                 QStringLiteral("https://github.com/oe5sos/Longpath/releases/download/v0.6.8/SHA256SUMS.txt"));
        // Die fehlenden Notizen werden benannt, nicht verschwiegen.
        QVERIFY(r.notes.contains(QStringLiteral("api.github.com")));
        QCOMPARE(r.source, ReleaseInfo::Source::Website);
    }

    void buildsDownloadUrlsLikeReleaseYml()
    {
        QCOMPARE(UpdateChecker::assetDownloadUrl(QStringLiteral("v0.6.8"),
                     UpdateChecker::assetNameFor(QStringLiteral("0.6.8"),
                                                 QStringLiteral("macos"),
                                                 QStringLiteral("x86_64"))).toString(),
                 QStringLiteral("https://github.com/oe5sos/Longpath/releases/download/"
                                "v0.6.8/Longpath-0.6.8-macOS-intel.dmg"));
        QCOMPARE(UpdateChecker::assetDownloadUrl(QStringLiteral("v0.6.8"),
                     UpdateChecker::assetNameFor(QStringLiteral("0.6.8"),
                                                 QStringLiteral("linux"),
                                                 // QSysInfo meldet "arm64"; "aarch64" ist nur
                                                 // der Dateiname in release.yml.
                                                 QStringLiteral("arm64"))).toString(),
                 QStringLiteral("https://github.com/oe5sos/Longpath/releases/download/"
                                "v0.6.8/Longpath-0.6.8-aarch64.AppImage"));
    }

    void apiUrlIsOverridableForTestsOnly()
    {
        QCOMPARE(UpdateChecker::latestReleaseApiUrl().host(), QStringLiteral("api.github.com"));
        qputenv("LONGPATH_UPDATE_API_URL", "https://127.0.0.1:9/nope");
        QCOMPARE(UpdateChecker::latestReleaseApiUrl().host(), QStringLiteral("127.0.0.1"));
        qunsetenv("LONGPATH_UPDATE_API_URL");
        QCOMPARE(UpdateChecker::latestReleaseApiUrl().host(), QStringLiteral("api.github.com"));
    }

    // Der Weg selbst, am echten GitHub -- nur mit LONGPATH_NET_TESTS=1,
    // weil CI-Laeufer und ein Rechner ohne Netz sonst rot waeren. Die API
    // zeigt dabei auf einen geschlossenen Port: genau der Zustand des
    // Mac mini, nur ohne Warten.
    void fallsBackToGithubComWhenTheApiIsDead()
    {
        if (qEnvironmentVariable("LONGPATH_NET_TESTS") != QStringLiteral("1")) {
            QSKIP("set LONGPATH_NET_TESTS=1 to run the live github.com fallback");
        }
        qputenv("LONGPATH_UPDATE_API_URL", "https://127.0.0.1:9/dead");
        UpdateChecker checker(QStringLiteral("0.0.1"));
        QSignalSpy known(&checker, &UpdateChecker::latestKnown);
        QSignalSpy failed(&checker, &UpdateChecker::checkFailed);
        checker.checkLatest();
        QVERIFY2(known.wait(60000), "github.com fallback produced no release");
        QCOMPARE(failed.count(), 0);

        const auto release = known.at(0).at(0).value<ReleaseInfo>();
        QCOMPARE(release.source, ReleaseInfo::Source::Website);
        QVERIFY(!release.version.isEmpty());
        QCOMPARE(known.at(0).at(1).toBool(), true);   // 0.0.1 ist aelter
        // Das Paket fuer diese Maschine muss per HEAD gefunden und mit
        // seiner echten Groesse eingetragen worden sein.
        const auto mine = release.asset(UpdateChecker::assetNameForThisMachine(release.version));
        QVERIFY2(mine.has_value(), "no package for this machine in the fallback release");
        QVERIFY(mine->size > 1000000);
        qunsetenv("LONGPATH_UPDATE_API_URL");
    }

    // ── Dritter Weg: CHANGELOG.md ────────────────────────────────────
    //
    // Der zweite Weg (github.com) hat am Mac mini nie geholfen, weil
    // github.com in denselben Azure-Bereich aufloest wie
    // api.github.com. raw.githubusercontent.com liegt bei Fastly und
    // antwortet dort (gemessen 2026-11-04: 104 045 B in unter 1 s).

    void parsesTheTopmostChangelogSection()
    {
        QString err;
        const auto r = UpdateChecker::parseChangelogLatest(kChangelog, &err);
        QVERIFY2(r.has_value(), qPrintable(err));
        QCOMPARE(r->version, QStringLiteral("0.8.2"));
        QCOMPARE(r->tag, QStringLiteral("v0.8.2"));
        QCOMPARE(r->name, QStringLiteral("Longpath 0.8.2"));
        QCOMPARE(r->source, ReleaseInfo::Source::Changelog);
        QCOMPARE(r->publishedAt.date(), QDate(2026, 10, 22));
        QCOMPARE(r->htmlUrl.toString(),
                 QStringLiteral("https://github.com/oe5sos/Longpath/releases/tag/v0.8.2"));
        // Die Notizen sind der Abschnitt -- mehr, als der zweite Weg
        // liefern kann (dort stehen sie nur in der API).
        QVERIFY(r->notes.contains(QStringLiteral("betrifft Windows")));
        QVERIFY(r->notes.contains(QStringLiteral("### Tondiagnose")));
        // Und er endet an der naechsten Version, nicht erst am Dateiende.
        QVERIFY(!r->notes.contains(QStringLiteral("darf nicht gewinnen")));
        QVERIFY(!r->notes.startsWith(QLatin1Char('\n')));
        QVERIFY(!r->notes.endsWith(QLatin1Char('\n')));
        // SHA256SUMS.txt liegt bei, damit downloadAndVerify() nichts fehlt.
        QVERIFY(r->asset(QStringLiteral("SHA256SUMS.txt")).has_value());
    }

    void changelogWithASuffixVersionAndNoDate()
    {
        const auto r = UpdateChecker::parseChangelogLatest(
            QByteArrayLiteral("# Changelog\n\n## [0.6.3-rc1]\n\nKandidat.\n"));
        QVERIFY(r.has_value());
        QCOMPARE(r->version, QStringLiteral("0.6.3-rc1"));
        QCOMPARE(r->tag, QStringLiteral("v0.6.3-rc1"));
        QVERIFY(!r->publishedAt.isValid());   // kein Datum -> keine Erfindung
        QCOMPARE(r->notes, QStringLiteral("Kandidat."));
    }

    void changelogWithoutASectionIsAnError()
    {
        QString err;
        QVERIFY(!UpdateChecker::parseChangelogLatest(QByteArrayLiteral("# Changelog\n"), &err));
        QVERIFY(!err.isEmpty());
        err.clear();
        QVERIFY(!UpdateChecker::parseChangelogLatest(QByteArray(), &err));
        QVERIFY(!err.isEmpty());
        // Eine HTML-Fehlerseite statt der Datei darf nicht als
        // Veroeffentlichung durchgehen.
        QVERIFY(!UpdateChecker::parseChangelogLatest(
            QByteArrayLiteral("<html><body>404: Not Found</body></html>")));
    }

    void changelogUrlIsOverridableForTestsOnly()
    {
        QCOMPARE(UpdateChecker::changelogRawUrl().host(),
                 QStringLiteral("raw.githubusercontent.com"));
        qputenv("LONGPATH_UPDATE_CHANGELOG_URL", "https://127.0.0.1:9/nope");
        QCOMPARE(UpdateChecker::changelogRawUrl().host(), QStringLiteral("127.0.0.1"));
        qunsetenv("LONGPATH_UPDATE_CHANGELOG_URL");
        QCOMPARE(UpdateChecker::changelogRawUrl().host(),
                 QStringLiteral("raw.githubusercontent.com"));
    }

    // Der ganze Weg, OHNE Netz: beide GitHub-Hosts zeigen auf einen
    // geschlossenen Port -- genau die Lage am Mac mini --, und
    // CHANGELOG.md kommt aus einer Datei. Damit laeuft die Pruefung auch
    // auf einem CI-Laeufer ohne Internet.
    void fallsBackToTheChangelogWhenBothGithubHostsAreDead()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString datei = dir.filePath(QStringLiteral("CHANGELOG.md"));
        QFile f(datei);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write(kChangelog) == kChangelog.size());
        f.close();

        qputenv("LONGPATH_UPDATE_API_URL", "https://127.0.0.1:9/dead");
        qputenv("LONGPATH_UPDATE_WEB_URL", "https://127.0.0.1:9/dead");
        qputenv("LONGPATH_UPDATE_CHANGELOG_URL",
                QUrl::fromLocalFile(datei).toString().toUtf8());

        UpdateChecker checker(QStringLiteral("0.8.1"));
        QSignalSpy known(&checker, &UpdateChecker::latestKnown);
        QSignalSpy failed(&checker, &UpdateChecker::checkFailed);
        checker.checkLatest();
        QVERIFY2(known.wait(90000), "the changelog fallback produced no release");
        QCOMPARE(failed.count(), 0);

        const auto release = known.at(0).at(0).value<ReleaseInfo>();
        QCOMPARE(release.source, ReleaseInfo::Source::Changelog);
        QCOMPARE(release.version, QStringLiteral("0.8.2"));
        QCOMPARE(known.at(0).at(1).toBool(), true);   // 0.8.1 ist aelter
        QVERIFY(release.notes.contains(QStringLiteral("betrifft Windows")));

        qunsetenv("LONGPATH_UPDATE_API_URL");
        qunsetenv("LONGPATH_UPDATE_WEB_URL");
        qunsetenv("LONGPATH_UPDATE_CHANGELOG_URL");
    }

    // Und die Gegenprobe: ist auch CHANGELOG.md nicht zu holen, gibt es
    // einen Fehler, der ALLE DREI Gruende nennt -- nicht einen davon.
    void allThreeWaysDeadNamesAllThreeReasons()
    {
        qputenv("LONGPATH_UPDATE_API_URL", "https://127.0.0.1:9/dead");
        qputenv("LONGPATH_UPDATE_WEB_URL", "https://127.0.0.1:9/dead");
        qputenv("LONGPATH_UPDATE_CHANGELOG_URL", "file:///nonexistent/CHANGELOG.md");
        UpdateChecker checker(QStringLiteral("0.0.1"));
        QSignalSpy known(&checker, &UpdateChecker::latestKnown);
        QSignalSpy failed(&checker, &UpdateChecker::checkFailed);
        checker.checkLatest();
        QVERIFY2(failed.wait(90000), "no failure reported although every way was dead");
        QCOMPARE(known.count(), 0);
        const QString why = failed.at(0).at(0).toString();
        QVERIFY2(why.contains(QStringLiteral("github.com")), qPrintable(why));
        QVERIFY2(why.contains(QStringLiteral("raw.githubusercontent.com"))
                     || why.contains(QStringLiteral("CHANGELOG.md")), qPrintable(why));
        qunsetenv("LONGPATH_UPDATE_API_URL");
        qunsetenv("LONGPATH_UPDATE_WEB_URL");
        qunsetenv("LONGPATH_UPDATE_CHANGELOG_URL");
    }

    // ── „da sieht es aus, als er nichts täte" ─────────────────────────
    //
    // Betreiber am 2026-11-04, waehrend er dem Dialog zusah. Gemessen
    // an seinem Geraet: 15 s api.github.com, 15 s github.com, dann erst
    // eine Antwort -- und der Text stand die ganze Zeit still. Jeder
    // Schritt muss sich melden.
    void saysWhichWayItIsTrying()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString datei = dir.filePath(QStringLiteral("CHANGELOG.md"));
        QFile f(datei);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(kChangelog);
        f.close();

        qputenv("LONGPATH_UPDATE_API_URL", "https://127.0.0.1:9/dead");
        qputenv("LONGPATH_UPDATE_WEB_URL", "https://127.0.0.1:9/dead");
        qputenv("LONGPATH_UPDATE_CHANGELOG_URL",
                QUrl::fromLocalFile(datei).toString().toUtf8());

        UpdateChecker checker(QStringLiteral("0.8.1"));
        QSignalSpy sagt(&checker, &UpdateChecker::trying);
        QSignalSpy known(&checker, &UpdateChecker::latestKnown);
        checker.checkLatest();
        QVERIFY(known.wait(90000));

        // Drei Schritte mindestens: API, github.com, CHANGELOG. Jeder
        // nennt den Host, damit der Bediener sieht, worauf gewartet wird.
        QVERIFY2(sagt.count() >= 3, qPrintable(QStringLiteral("nur %1 Ansagen").arg(sagt.count())));
        QStringList alle;
        for (int i = 0; i < sagt.count(); ++i) { alle << sagt.at(i).at(0).toString(); }
        const QString text = alle.join(QLatin1Char('|'));
        QVERIFY2(text.contains(QStringLiteral("api.github.com")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("github.com")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("raw.githubusercontent.com")), qPrintable(text));

        qunsetenv("LONGPATH_UPDATE_API_URL");
        qunsetenv("LONGPATH_UPDATE_WEB_URL");
        qunsetenv("LONGPATH_UPDATE_CHANGELOG_URL");
    }

    // Und der Weg, der getragen hat, wird gemerkt -- damit die beiden
    // Azure-Schritte beim naechsten Mal nur eine kurze Frist bekommen
    // statt wieder 30 s zu verschenken.
    void remembersTheWayThatWorked()
    {
        auto& s = AppSettings::instance();
        s.setValue(UpdateChecker::lastSourceKey(), QStringLiteral("api"));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString datei = dir.filePath(QStringLiteral("CHANGELOG.md"));
        QFile f(datei);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(kChangelog);
        f.close();

        qputenv("LONGPATH_UPDATE_API_URL", "https://127.0.0.1:9/dead");
        qputenv("LONGPATH_UPDATE_WEB_URL", "https://127.0.0.1:9/dead");
        qputenv("LONGPATH_UPDATE_CHANGELOG_URL",
                QUrl::fromLocalFile(datei).toString().toUtf8());

        UpdateChecker checker(QStringLiteral("0.8.1"));
        QSignalSpy known(&checker, &UpdateChecker::latestKnown);
        checker.checkLatest();
        QVERIFY(known.wait(90000));
        QCOMPARE(s.value(UpdateChecker::lastSourceKey()).toString(),
                 QStringLiteral("changelog"));

        // Zurueck auf Anfang, damit kein anderer Fall darauf stolpert.
        s.setValue(UpdateChecker::lastSourceKey(), QStringLiteral("api"));
        qunsetenv("LONGPATH_UPDATE_API_URL");
        qunsetenv("LONGPATH_UPDATE_WEB_URL");
        qunsetenv("LONGPATH_UPDATE_CHANGELOG_URL");
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
