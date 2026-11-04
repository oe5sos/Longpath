// =================================================================
// src/core/UpdateChecker.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Betreiber 2026-09-21: "unter Help moechte ich den automatischen
// Downloader/Installer der neuesten Version. Ein Klick, Installation
// automatisch." Dieser Baustein ist die Haelfte ohne Bedienflaeche:
// die neueste Veroeffentlichung bei GitHub abfragen, die Version mit der
// laufenden vergleichen, das zur Plattform passende Paket bestimmen, es
// herunterladen und gegen SHA256SUMS.txt aus derselben Veroeffentlichung
// pruefen. Das Einspielen macht UpdateInstaller, die Bedienflaeche
// UpdateDialog.
//
// Was hier bewusst NICHT passiert: keine GPG-Pruefung der Signaturen
// (*.asc). Ein stock-macOS hat kein gpg, und der oeffentliche Schluessel
// liegt nicht im Programm. Die Pruefsumme ueber HTTPS von GitHub sichert
// gegen einen verstuemmelten Download; die Echtheit haengt an GitHubs
// TLS. Wer mehr will, prueft SHA256SUMS.txt.asc von Hand (README).
//
// Zweiter Weg ohne API (2026-10-14). api.github.com liegt inzwischen in
// Azure (20.250.x). Auf dem Mac mini des Betreibers baut sich die
// TCP-Verbindung dorthin auf, das Client Hello geht hinaus -- und der
// Handshake bleibt stehen, bis die Zeit ablaeuft, waehrend github.com
// selbst (140.82.x) einwandfrei antwortet. Dort stand die Fassung 0.6.5
// und konnte sich nie selbst holen. Betreiber dazu: "macht der enduser
// auch nicht alles" -- also darf die Loesung nicht in /etc/hosts stehen,
// sondern muss im Programm sein: scheitert die API, wird die
// Veroeffentlichung von der Webseite abgeleitet. /releases/latest leitet
// auf /releases/tag/<tag> weiter, und jede Datei liegt unter
// /releases/download/<tag>/<name>. Das reicht fuer alles, was der
// Updater braucht: Version, Paket, SHA256SUMS.txt.
//
// Dritter Weg ohne jeden Azure-Host (2026-11-04). Der zweite Weg oben
// hat am Mac mini nie geholfen, weil github.com in DENSELBEN
// Azure-Bereich aufloest wie api.github.com -- zwei Wege, eine
// Sackgasse. Gemessen am Geraet, bei MTU 1500, dieselbe Anfrage ueber
// zwei Server-IPs:
//
//   api.github.com via 140.82.121.5 (alt)     HTTP 200, 51 429 B, < 1 s
//   api.github.com via 20.250.119.72 (Azure)  TCP offen, 0 B, 21 s Zeit aus
//   github.com     -> 20.250.119.67 (Azure)   0 B, 20 s Zeit aus
//   codeload       -> 20.250.119.71 (Azure)   0 B, 20 s Zeit aus
//   raw.githubusercontent.com -> 185.199.110.133 (Fastly)
//                                             HTTP 200, 104 045 B, < 1 s
//
// Grosse Antworten sind also nicht das Problem; das Paketgroessen-Loch
// liegt allein auf der Route zu GitHubs Azure-Frontends, wo auch das
// ICMP "Fragmentation needed" verschluckt wird. raw.githubusercontent.com
// liegt bei Fastly und ist davon nicht betroffen -- und dort liegt
// CHANGELOG.md, aus dem release.yml:103 die Notizen einer
// Veroeffentlichung zieht. Die oberste Ueberschrift "## [0.8.2] - ..."
// ist damit dieselbe Auskunft wie das "Latest" der API (nachgesehen fuer
// die vier jungsten Abschnitte: zu jedem gibt es den Tag). Dieser Weg
// liefert sogar MEHR als der zweite, naemlich die Notizen.
//
// Was er nicht kann: herunterladen. Die Pakete liegen unter
// github.com/.../releases/download/, also in der Sackgasse. Wo ein
// Auto-Install laeuft, ist das kein Verlust -- der haelt die App
// ohnehin auf dem jungsten Commit, VOR jeder Veroeffentlichung.
//
// Reine Funktionen (parse*, compareVersions, assetNameFor) sind statisch
// und ohne Netz pruefbar; der Netzteil laeuft ueber QNetworkAccessManager
// mit Weiterleitungen (GitHub-Pakete liegen hinter einem Redirect).
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
//   2026-10-14 -- Zweiter Weg ueber github.com, wenn api.github.com
//                 nicht antwortet, by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
//   2026-11-04 -- Fortschritt melden und den letzten erfolgreichen Weg
//                 merken, by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
//   2026-11-04 -- Dritter Weg ueber raw.githubusercontent.com +
//                 CHANGELOG.md, weil github.com dieselbe Azure-Route
//                 nimmt wie api.github.com; ReleaseInfo::Source statt
//                 des einen bool, by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

#include <optional>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

namespace Longpath {

struct ReleaseAsset {
    QString name;
    QUrl    url;
    qint64  size{0};
};

struct ReleaseInfo {
    QString   tag;        // "v0.6.3"
    QString   version;    // "0.6.3"
    QString   name;       // Ueberschrift der Veroeffentlichung
    QString   notes;      // Markdown
    QDateTime publishedAt;
    QUrl      htmlUrl;
    QVector<ReleaseAsset> assets;

    // Woher die Auskunft stammt. Der Dialog sagt es dem Bediener, denn
    // die drei Wege wissen nicht gleich viel: ohne Api fehlt die
    // Dateiliste, und bei Changelog war github.com ueberhaupt nicht
    // erreichbar -- dann ist auch kein Herunterladen moeglich. Gehoert
    // an die Veroeffentlichung, nicht an den Abfrager: die
    // Startpruefung benutzt einen eigenen UpdateChecker und reicht nur
    // dieses Objekt an den Dialog weiter.
    //
    // Eine Aufzaehlung statt zweier bool, damit es den Zustand
    // "beides zugleich" nicht gibt.
    enum class Source {
        Api,        // api.github.com, mit Dateiliste und Notizen
        Website,    // github.com/releases/latest -- nur das Tag
        Changelog,  // raw.githubusercontent.com + CHANGELOG.md
    };
    Source source{Source::Api};

    std::optional<ReleaseAsset> asset(const QString& name) const;
};

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(const QString& runningVersion, QObject* parent = nullptr);

    QString runningVersion() const { return m_runningVersion; }

    // -- reine Funktionen ------------------------------------------------
    static std::optional<ReleaseInfo> parseLatestRelease(const QByteArray& json,
                                                         QString* error = nullptr);
    // "0.6.3" vs "0.6.10": zahlenweise; ein Anhang ("-rc1") zaehlt weniger
    // als die nackte Version. <0: a aelter, 0: gleich, >0: a neuer.
    static int compareVersions(const QString& a, const QString& b);
    // "v0.6.3 · fix/x@abc-dirty" -> "0.6.3": nur die Ziffern vorne zaehlen.
    static QString numericVersion(const QString& text);
    // Paketname je Plattform, wie release.yml ihn schreibt:
    //   macOS  arm64  -> Longpath-<v>-macOS-apple-silicon.dmg
    //   macOS  x86_64 -> Longpath-<v>-macOS-intel.dmg
    //   Windows       -> Longpath-<v>-Windows-x64-setup.exe
    //   Linux  x86_64 -> Longpath-<v>-x86_64.AppImage (aarch64 analog)
    static QString assetNameFor(const QString& version, const QString& os,
                                const QString& cpuArch);
    static QString assetNameForThisMachine(const QString& version);
    // "<hex>  <name>" je Zeile -> name -> hex (klein).
    static QHash<QString, QString> parseSha256Sums(const QByteArray& text);
    static QString sha256Of(const QString& filePath);

    // -- Zweiter Weg ohne api.github.com (siehe Kopf) --------------------
    // https://github.com/oe5sos/Longpath/releases/latest -- antwortet mit
    // einer Weiterleitung auf das Tag, ohne JSON und ohne API-Host.
    // Mit LONGPATH_UPDATE_WEB_URL gesetzt die dort genannte Adresse
    // (nur fuer Pruefstaende).
    static QUrl latestReleaseWebUrl();
    // ".../releases/tag/v0.6.8" -> "v0.6.8"; alles andere -> leer.
    static QString tagFromReleaseUrl(const QUrl& url);
    // ".../releases/download/<tag>/<name>" -- die Namensregel aus
    // release.yml, dieselbe, die assetNameFor() schon kennt.
    static QUrl assetDownloadUrl(const QString& tag, const QString& assetName);
    // Was sich allein aus dem Tag ableiten laesst: Version, Seite,
    // SHA256SUMS.txt. Ohne Notizen -- die stehen nur in der API.
    static ReleaseInfo releaseFromTag(const QString& tag);

    // -- Dritter Weg ohne Azure-Host (siehe Kopf) ------------------------
    // CHANGELOG.md des Hauptzweigs bei Fastly. Mit
    // LONGPATH_UPDATE_CHANGELOG_URL gesetzt die dort genannte Adresse
    // (nur fuer Pruefstaende, wie LONGPATH_UPDATE_API_URL).
    static QUrl changelogRawUrl();
    // Die oberste Ueberschrift "## [<version>] - <JJJJ-MM-TT>" und den
    // Abschnitt darunter als Notizen. Kein Abschnitt -> leer + Grund.
    static std::optional<ReleaseInfo> parseChangelogLatest(const QByteArray& markdown,
                                                           QString* error = nullptr);

    // -- Netz ------------------------------------------------------------
    void checkLatest();
    // Laedt das Paket nach <targetDir>/<asset.name>; danach wird
    // SHA256SUMS.txt derselben Veroeffentlichung geholt und verglichen.
    void downloadAndVerify(const ReleaseInfo& release, const ReleaseAsset& asset,
                           const QString& targetDir);
    void cancel();

    // Im Betrieb api.github.com; mit LONGPATH_UPDATE_API_URL gesetzt die
    // dort genannte Adresse (nur fuer Pruefstaende, siehe .cpp).
    static QUrl latestReleaseApiUrl();

    // -- Nicht wieder 30 Sekunden stumm warten (2026-11-04) --------------
    //
    // Betreiber, waehrend er dem Dialog zusah: "da sieht es aus, als er
    // nichts taete". Gemessen an seinem Geraet: 15 s api.github.com,
    // 15 s github.com, dann erst eine Antwort -- und der Text stand die
    // ganze Zeit auf "Checking for the latest release...".
    //
    // Darum zweierlei. Erstens sagt trying() jeden Schritt an. Zweitens
    // wird der Weg gemerkt, der zuletzt getragen hat: fiel die letzte
    // Pruefung durch bis zum CHANGELOG, bekommen die beiden
    // Azure-Schritte nur noch eine kurze Frist statt der vollen. Die
    // Reihenfolge bleibt, damit die API weiter gewinnt, sobald sie
    // wieder antwortet -- nur das Warten darauf wird kurz.
    static QString lastSourceKey() { return QStringLiteral("UpdateLastSource"); }
    static QString sourceName(ReleaseInfo::Source s);
    static QByteArray userAgent();

signals:
    // Was gerade versucht wird, in einem Satz fuer die Oberflaeche.
    void trying(const QString& what);
    void latestKnown(const Longpath::ReleaseInfo& release, bool newerThanRunning);
    void checkFailed(const QString& why);
    void downloadProgress(qint64 received, qint64 total);
    void verifying();
    void downloadVerified(const QString& filePath);
    void downloadFailed(const QString& why);

private:
    void fetchSumsAndVerify(const ReleaseInfo& release, const QString& filePath);
    void checkLatestViaWebsite(const QString& apiError);
    void checkLatestViaChangelog(const QString& earlierErrors);
    void probeAssetAndFinish(const ReleaseInfo& release, const QString& apiError);

    // Frist fuer die beiden Schritte ueber GitHubs Azure-Frontends.
    // Voll, solange die API zuletzt getragen hat; kurz, wenn die letzte
    // Pruefung bis zum CHANGELOG durchfiel.
    int m_azureTimeoutMs{0};

    QString m_runningVersion;
    QNetworkAccessManager* m_net{nullptr};
    QNetworkReply* m_active{nullptr};
    QFile* m_out{nullptr};
    bool m_cancelled{false};
};

} // namespace Longpath

Q_DECLARE_METATYPE(Longpath::ReleaseInfo)
