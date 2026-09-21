// =================================================================
// src/core/UpdateChecker.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Zweck und Grenzen: UpdateChecker.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "UpdateChecker.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSysInfo>

namespace Longpath {

namespace {
constexpr int kApiTimeoutMs = 15000;
// Ein 100-MB-Paket ueber eine langsame Leitung: grosszuegig, aber nicht
// unendlich -- ein haengender Download soll irgendwann "fehlgeschlagen"
// heissen statt ewig "laeuft".
constexpr int kDownloadTimeoutMs = 30 * 60 * 1000;
constexpr const char* kSumsAssetName = "SHA256SUMS.txt";

QNetworkRequest request(const QUrl& url, int timeoutMs, const QByteArray& accept = {})
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, UpdateChecker::userAgent());
    if (!accept.isEmpty()) { req.setRawHeader("Accept", accept); }
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(timeoutMs);
    return req;
}
} // namespace

std::optional<ReleaseAsset> ReleaseInfo::asset(const QString& wanted) const
{
    for (const ReleaseAsset& a : assets) {
        if (a.name == wanted) { return a; }
    }
    return std::nullopt;
}

UpdateChecker::UpdateChecker(const QString& runningVersion, QObject* parent)
    : QObject(parent)
    , m_runningVersion(numericVersion(runningVersion))
    , m_net(new QNetworkAccessManager(this))
{
    qRegisterMetaType<ReleaseInfo>("Longpath::ReleaseInfo");
}

// ── reine Funktionen ──────────────────────────────────────────────────

QUrl UpdateChecker::latestReleaseApiUrl()
{
    return QUrl(QStringLiteral("https://api.github.com/repos/oe5sos/Longpath/releases/latest"));
}

QByteArray UpdateChecker::userAgent()
{
#ifdef NEREUSSDR_VERSION
    return QByteArrayLiteral("Longpath/" NEREUSSDR_VERSION " (+https://github.com/oe5sos/Longpath)");
#else
    return QByteArrayLiteral("Longpath (+https://github.com/oe5sos/Longpath)");
#endif
}

std::optional<ReleaseInfo> UpdateChecker::parseLatestRelease(const QByteArray& json, QString* error)
{
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) { *error = QStringLiteral("not a release document: %1").arg(perr.errorString()); }
        return std::nullopt;
    }
    const QJsonObject o = doc.object();
    ReleaseInfo r;
    r.tag = o.value(QStringLiteral("tag_name")).toString();
    if (r.tag.isEmpty()) {
        if (error) { *error = QStringLiteral("release without tag_name"); }
        return std::nullopt;
    }
    r.version = numericVersion(r.tag);
    r.name = o.value(QStringLiteral("name")).toString();
    r.notes = o.value(QStringLiteral("body")).toString();
    r.publishedAt = QDateTime::fromString(o.value(QStringLiteral("published_at")).toString(), Qt::ISODate);
    r.htmlUrl = QUrl(o.value(QStringLiteral("html_url")).toString());
    const QJsonArray assets = o.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue& v : assets) {
        const QJsonObject a = v.toObject();
        ReleaseAsset asset;
        asset.name = a.value(QStringLiteral("name")).toString();
        asset.url = QUrl(a.value(QStringLiteral("browser_download_url")).toString());
        asset.size = static_cast<qint64>(a.value(QStringLiteral("size")).toDouble());
        if (!asset.name.isEmpty() && asset.url.isValid()) { r.assets.append(asset); }
    }
    return r;
}

QString UpdateChecker::numericVersion(const QString& text)
{
    static const QRegularExpression re(QStringLiteral("(\\d+(?:\\.\\d+)*)(?:-([0-9A-Za-z.]+))?"));
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch()) { return {}; }
    QString v = m.captured(1);
    // Ein Vorab-Anhang direkt an der Zahl ("0.6.3-rc3") gehoert zur
    // Version; "0.6.3 · branch@sha-dirty" dagegen nicht (Leerzeichen).
    if (!m.captured(2).isEmpty() && m.capturedStart(2) == m.capturedEnd(1) + 1) {
        v += QLatin1Char('-') + m.captured(2);
    }
    return v;
}

int UpdateChecker::compareVersions(const QString& a, const QString& b)
{
    const auto split = [](const QString& v, QString* suffix) {
        QString core = v;
        const int dash = v.indexOf(QLatin1Char('-'));
        if (dash >= 0) { *suffix = v.mid(dash + 1); core = v.left(dash); }
        QVector<int> parts;
        for (const QString& p : core.split(QLatin1Char('.'))) { parts.append(p.toInt()); }
        while (parts.size() < 4) { parts.append(0); }
        return parts;
    };
    QString sa, sb;
    const QVector<int> pa = split(numericVersion(a), &sa);
    const QVector<int> pb = split(numericVersion(b), &sb);
    for (int i = 0; i < 4; ++i) {
        if (pa[i] != pb[i]) { return pa[i] < pb[i] ? -1 : 1; }
    }
    // Gleiche Zahlen: die nackte Version schlaegt jeden Vorab-Anhang;
    // zwei Anhaenge vergleichen sich als Text ("rc1" < "rc2").
    if (sa.isEmpty() && sb.isEmpty()) { return 0; }
    if (sa.isEmpty()) { return 1; }
    if (sb.isEmpty()) { return -1; }
    return QString::compare(sa, sb) < 0 ? -1 : (sa == sb ? 0 : 1);
}

QString UpdateChecker::assetNameFor(const QString& version, const QString& os, const QString& cpuArch)
{
    const QString v = numericVersion(version);
    if (os == QStringLiteral("macos")) {
        const bool arm = cpuArch.startsWith(QStringLiteral("arm"));
        return QStringLiteral("Longpath-%1-macOS-%2.dmg").arg(v, arm ? QStringLiteral("apple-silicon")
                                                                     : QStringLiteral("intel"));
    }
    if (os == QStringLiteral("windows")) {
        return QStringLiteral("Longpath-%1-Windows-x64-setup.exe").arg(v);
    }
    if (os == QStringLiteral("linux")) {
        const QString arch = cpuArch.startsWith(QStringLiteral("arm")) ? QStringLiteral("aarch64")
                                                                        : QStringLiteral("x86_64");
        return QStringLiteral("Longpath-%1-%2.AppImage").arg(v, arch);
    }
    return {};
}

QString UpdateChecker::assetNameForThisMachine(const QString& version)
{
    return assetNameFor(version, QSysInfo::productType() == QStringLiteral("macos")
                                     ? QStringLiteral("macos")
                                     : QSysInfo::kernelType() == QStringLiteral("winnt")
                                           ? QStringLiteral("windows")
                                           : QStringLiteral("linux"),
                        QSysInfo::currentCpuArchitecture());
}

QHash<QString, QString> UpdateChecker::parseSha256Sums(const QByteArray& text)
{
    QHash<QString, QString> out;
    for (const QByteArray& rawLine : text.split('\n')) {
        const QString line = QString::fromUtf8(rawLine).trimmed();
        if (line.isEmpty()) { continue; }
        // "<hex>  <name>" oder "<hex> *<name>"
        const int sp = line.indexOf(QLatin1Char(' '));
        if (sp != 64) { continue; }
        QString name = line.mid(sp + 1).trimmed();
        if (name.startsWith(QLatin1Char('*'))) { name.remove(0, 1); }
        out.insert(name, line.left(64).toLower());
    }
    return out;
}

QString UpdateChecker::sha256Of(const QString& filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) { return {}; }
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f)) { return {}; }
    return QString::fromLatin1(h.result().toHex());
}

// ── Netz ──────────────────────────────────────────────────────────────

void UpdateChecker::checkLatest()
{
    cancel();
    m_cancelled = false;
    QNetworkReply* reply = m_net->get(request(latestReleaseApiUrl(), kApiTimeoutMs,
                                              QByteArrayLiteral("application/vnd.github+json")));
    m_active = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_active == reply) { m_active = nullptr; }
        if (m_cancelled) { return; }
        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(reply->errorString());
            return;
        }
        QString err;
        const auto release = parseLatestRelease(reply->readAll(), &err);
        if (!release) { emit checkFailed(err); return; }
        emit latestKnown(*release, compareVersions(release->version, m_runningVersion) > 0);
    });
}

void UpdateChecker::downloadAndVerify(const ReleaseInfo& release, const ReleaseAsset& asset,
                                      const QString& targetDir)
{
    cancel();
    m_cancelled = false;
    QDir().mkpath(targetDir);
    const QString path = QDir(targetDir).filePath(asset.name);
    auto* out = new QFile(path, this);
    if (!out->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit downloadFailed(QStringLiteral("cannot write %1").arg(path));
        out->deleteLater();
        return;
    }
    m_out = out;
    QNetworkReply* reply = m_net->get(request(asset.url, kDownloadTimeoutMs));
    m_active = reply;
    connect(reply, &QNetworkReply::readyRead, this, [reply, out]() {
        out->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this, &UpdateChecker::downloadProgress);
    connect(reply, &QNetworkReply::finished, this, [this, reply, out, path, release]() {
        reply->deleteLater();
        if (m_active == reply) { m_active = nullptr; }
        out->write(reply->readAll());
        out->close();
        out->deleteLater();
        if (m_out == out) { m_out = nullptr; }
        if (m_cancelled) { QFile::remove(path); return; }
        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(path);
            emit downloadFailed(reply->errorString());
            return;
        }
        emit verifying();
        fetchSumsAndVerify(release, path);
    });
}

void UpdateChecker::fetchSumsAndVerify(const ReleaseInfo& release, const QString& filePath)
{
    const auto sums = release.asset(QString::fromLatin1(kSumsAssetName));
    if (!sums) {
        QFile::remove(filePath);
        emit downloadFailed(QStringLiteral("release carries no %1").arg(QString::fromLatin1(kSumsAssetName)));
        return;
    }
    QNetworkReply* reply = m_net->get(request(sums->url, kApiTimeoutMs));
    m_active = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, filePath]() {
        reply->deleteLater();
        if (m_active == reply) { m_active = nullptr; }
        if (m_cancelled) { QFile::remove(filePath); return; }
        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(filePath);
            emit downloadFailed(QStringLiteral("checksums: %1").arg(reply->errorString()));
            return;
        }
        const QHash<QString, QString> sums = parseSha256Sums(reply->readAll());
        const QString name = QFileInfo(filePath).fileName();
        const QString expected = sums.value(name);
        if (expected.isEmpty()) {
            QFile::remove(filePath);
            emit downloadFailed(QStringLiteral("no checksum listed for %1").arg(name));
            return;
        }
        const QString actual = sha256Of(filePath);
        if (actual != expected) {
            QFile::remove(filePath);
            emit downloadFailed(QStringLiteral("checksum mismatch for %1").arg(name));
            return;
        }
        emit downloadVerified(filePath);
    });
}

void UpdateChecker::cancel()
{
    m_cancelled = true;
    if (m_active) {
        m_active->abort();
        m_active = nullptr;
    }
    if (m_out) {
        const QString path = m_out->fileName();
        m_out->close();
        m_out->deleteLater();
        m_out = nullptr;
        QFile::remove(path);
    }
}

} // namespace Longpath
