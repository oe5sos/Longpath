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
// Reine Funktionen (parse*, compareVersions, assetNameFor) sind statisch
// und ohne Netz pruefbar; der Netzteil laeuft ueber QNetworkAccessManager
// mit Weiterleitungen (GitHub-Pakete liegen hinter einem Redirect).
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
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

    // -- Netz ------------------------------------------------------------
    void checkLatest();
    // Laedt das Paket nach <targetDir>/<asset.name>; danach wird
    // SHA256SUMS.txt derselben Veroeffentlichung geholt und verglichen.
    void downloadAndVerify(const ReleaseInfo& release, const ReleaseAsset& asset,
                           const QString& targetDir);
    void cancel();

    static QUrl latestReleaseApiUrl();
    static QByteArray userAgent();

signals:
    void latestKnown(const Longpath::ReleaseInfo& release, bool newerThanRunning);
    void checkFailed(const QString& why);
    void downloadProgress(qint64 received, qint64 total);
    void verifying();
    void downloadVerified(const QString& filePath);
    void downloadFailed(const QString& why);

private:
    void fetchSumsAndVerify(const ReleaseInfo& release, const QString& filePath);

    QString m_runningVersion;
    QNetworkAccessManager* m_net{nullptr};
    QNetworkReply* m_active{nullptr};
    QFile* m_out{nullptr};
    bool m_cancelled{false};
};

} // namespace Longpath

Q_DECLARE_METATYPE(Longpath::ReleaseInfo)
