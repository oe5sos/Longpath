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
//   2026-10-14 -- Zweiter Weg ueber github.com, wenn api.github.com
//                 nicht antwortet, by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
//   2026-11-04 -- Dritter Weg ueber raw.githubusercontent.com +
//                 CHANGELOG.md, by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "UpdateChecker.h"
#include "AppSettings.h"

#include <QCryptographicHash>
#include <QDateTime>
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
#include <QLoggingCategory>
#include <QSysInfo>
#include <QTimeZone>

namespace Longpath {

// Eigene Kategorie, Vorgabe QtInfoMsg: der zweite Weg muss im Protokoll
// stehen, ohne dass jemand vorher einen Haken setzt -- genau dieser Fall
// (stumme API) war am 2026-10-14 nicht nachweisbar, weil die
// Startpruefung ihr Scheitern nirgends vermerkt hat.
Q_LOGGING_CATEGORY(lcUpdate, "longpath.update", QtInfoMsg)

namespace {
constexpr int kApiTimeoutMs = 15000;
// Kurze Frist fuer die zwei Schritte ueber GitHubs Azure-Frontends,
// wenn die letzte Pruefung dort ohnehin durchfiel. Vier Sekunden
// reichen fuer eine Antwort, die kommt (gemessen: die alte GitHub-IP
// antwortet in 0,22 s), und ersparen 22 s Warten, wenn keine kommt.
constexpr int kAzureKurzMs = 4000;
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
    const QString zuletzt = AppSettings::instance()
                                .value(lastSourceKey(), QStringLiteral("api"))
                                .toString();
    m_azureTimeoutMs = (zuletzt == QStringLiteral("api")) ? kApiTimeoutMs : kAzureKurzMs;
}

QString UpdateChecker::sourceName(ReleaseInfo::Source s)
{
    switch (s) {
    case ReleaseInfo::Source::Api:       return QStringLiteral("api");
    case ReleaseInfo::Source::Website:   return QStringLiteral("website");
    case ReleaseInfo::Source::Changelog: return QStringLiteral("changelog");
    }
    return QStringLiteral("api");
}

// ── reine Funktionen ──────────────────────────────────────────────────

QUrl UpdateChecker::latestReleaseApiUrl()
{
    // LONGPATH_UPDATE_API_URL: nur fuer Pruefstaende -- laesst die API
    // gezielt scheitern (etwa auf einen geschlossenen Port zeigen), damit
    // der zweite Weg ueber github.com belegt werden kann und nicht bloss
    // behauptet. Im Betrieb nie gesetzt.
    const QString override = qEnvironmentVariable("LONGPATH_UPDATE_API_URL");
    if (!override.isEmpty()) { return QUrl(override); }
    return QUrl(QStringLiteral("https://api.github.com/repos/oe5sos/Longpath/releases/latest"));
}

QUrl UpdateChecker::latestReleaseWebUrl()
{
    // LONGPATH_UPDATE_WEB_URL: nur fuer Pruefstaende, wie
    // LONGPATH_UPDATE_API_URL. Ohne das waere der dritte Weg auf einem
    // Rechner MIT funktionierendem GitHub nicht pruefbar -- die Kette
    // stiege schon hier aus, und der Prueflauf waere falsch gruen.
    const QString override = qEnvironmentVariable("LONGPATH_UPDATE_WEB_URL");
    if (!override.isEmpty()) { return QUrl(override); }
    return QUrl(QStringLiteral("https://github.com/oe5sos/Longpath/releases/latest"));
}

QString UpdateChecker::tagFromReleaseUrl(const QUrl& url)
{
    // ".../releases/tag/v0.6.8" -- nur diese Form. Ein beliebiger Pfad
    // (etwa die Anmeldeseite, auf die GitHub bei einem privaten Repo
    // umleitet) darf nicht als Tag durchgehen.
    const QStringList parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() < 2) { return {}; }
    if (parts.at(parts.size() - 2) != QStringLiteral("tag")) { return {}; }
    const QString tag = parts.last();
    // Ohne Ziffer ist es kein Tag von uns.
    return numericVersion(tag).isEmpty() ? QString() : tag;
}

QUrl UpdateChecker::assetDownloadUrl(const QString& tag, const QString& assetName)
{
    return QUrl(QStringLiteral("https://github.com/oe5sos/Longpath/releases/download/%1/%2")
                    .arg(tag, assetName));
}

ReleaseInfo UpdateChecker::releaseFromTag(const QString& tag)
{
    ReleaseInfo r;
    r.tag = tag;
    r.source = ReleaseInfo::Source::Website;
    r.version = numericVersion(tag);
    r.name = QStringLiteral("Longpath %1").arg(r.version);
    // Die Notizen stehen nur in der API. Statt eines leeren Feldes die
    // Wahrheit samt Verweis -- der Bediener soll nicht raten, warum der
    // Text fehlt.
    r.notes = QStringLiteral(
        "_Release notes are not available: api.github.com could not be reached, "
        "so this release was read from the release page instead. "
        "Open the release page to read them._");
    r.htmlUrl = QUrl(QStringLiteral("https://github.com/oe5sos/Longpath/releases/tag/%1").arg(tag));
    // SHA256SUMS.txt liegt bei jeder Veroeffentlichung an derselben
    // Stelle; downloadAndVerify() braucht genau diesen Eintrag.
    ReleaseAsset sums;
    sums.name = QString::fromLatin1(kSumsAssetName);
    sums.url = assetDownloadUrl(tag, sums.name);
    r.assets.append(sums);
    return r;
}

QByteArray UpdateChecker::userAgent()
{
#ifdef LONGPATH_VERSION
    return QByteArrayLiteral("Longpath/" LONGPATH_VERSION " (+https://github.com/oe5sos/Longpath)");
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

QUrl UpdateChecker::changelogRawUrl()
{
    // Wie latestReleaseApiUrl(): nur fuer Pruefstaende umlenkbar, damit
    // der Weg ohne Netz gefahren werden kann.
    const QString override = qEnvironmentVariable("LONGPATH_UPDATE_CHANGELOG_URL");
    if (!override.isEmpty()) { return QUrl(override); }
    return QUrl(QStringLiteral(
        "https://raw.githubusercontent.com/oe5sos/Longpath/main/CHANGELOG.md"));
}

std::optional<ReleaseInfo> UpdateChecker::parseChangelogLatest(const QByteArray& markdown,
                                                               QString* error)
{
    const auto fehler = [error](const QString& why) -> std::optional<ReleaseInfo> {
        if (error) { *error = why; }
        return std::nullopt;
    };
    if (markdown.isEmpty()) { return fehler(QStringLiteral("CHANGELOG.md is empty")); }

    // "## [0.8.2] - 2026-10-22" -- die Version darf einen Anhang tragen
    // ("0.6.3-rc1"), das Datum ist freiwillig. Die oberste Ueberschrift
    // dieser Form gewinnt; "# Changelog" darueber wird uebergangen.
    static const QRegularExpression ueberschrift(
        QStringLiteral(R"(^##\s+\[(\d[^\]]*)\](?:\s*-\s*(\d{4}-\d{2}-\d{2}))?\s*$)"));
    // Jede weitere "## "-Zeile beendet den Abschnitt. "### " nicht --
    // die gehoeren zum Text.
    static const QRegularExpression naechste(QStringLiteral(R"(^##\s+\S)"));

    const QStringList zeilen = QString::fromUtf8(markdown).split(QLatin1Char('\n'));
    int kopf = -1;
    QRegularExpressionMatch treffer;
    for (int i = 0; i < zeilen.size(); ++i) {
        const auto m = ueberschrift.match(zeilen.at(i));
        if (m.hasMatch()) { kopf = i; treffer = m; break; }
    }
    if (kopf < 0) {
        return fehler(QStringLiteral("no \"## [version]\" heading in CHANGELOG.md"));
    }

    ReleaseInfo r;
    r.source  = ReleaseInfo::Source::Changelog;
    r.version = treffer.captured(1).trimmed();
    r.tag     = QStringLiteral("v%1").arg(r.version);
    r.name    = QStringLiteral("Longpath %1").arg(r.version);
    r.htmlUrl = QUrl(QStringLiteral("https://github.com/oe5sos/Longpath/releases/tag/%1")
                         .arg(r.tag));
    if (!treffer.captured(2).isEmpty()) {
        // Mittag UTC: der Abschnitt nennt nur den Tag, und 00:00 liesse
        // die Ortszeit westlich von Greenwich einen Tag zurueckfallen.
        r.publishedAt = QDateTime(QDate::fromString(treffer.captured(2), Qt::ISODate),
                                  QTime(12, 0), QTimeZone::utc());
    }

    QStringList text;
    for (int i = kopf + 1; i < zeilen.size(); ++i) {
        if (naechste.match(zeilen.at(i)).hasMatch()) { break; }
        text.append(zeilen.at(i));
    }
    while (!text.isEmpty() && text.constLast().trimmed().isEmpty()) { text.removeLast(); }
    while (!text.isEmpty() && text.constFirst().trimmed().isEmpty()) { text.removeFirst(); }
    r.notes = text.join(QLatin1Char('\n'));

    // SHA256SUMS.txt liegt bei jeder Veroeffentlichung an derselben
    // Stelle -- wie in releaseFromTag(). Erreichbar ist sie auf diesem
    // Weg allerdings nicht (github.com ist ja gerade tot); der Eintrag
    // steht da, damit downloadAndVerify() nichts fehlt, falls die Route
    // zwischendurch wieder aufgeht.
    ReleaseAsset sums;
    sums.name = QString::fromLatin1(kSumsAssetName);
    sums.url  = assetDownloadUrl(r.tag, sums.name);
    r.assets.append(sums);
    return r;
}

void UpdateChecker::checkLatest()
{
    cancel();
    m_cancelled = false;
    emit trying(QStringLiteral("Asking api.github.com for the latest release…"));
    QNetworkReply* reply = m_net->get(request(latestReleaseApiUrl(), m_azureTimeoutMs,
                                              QByteArrayLiteral("application/vnd.github+json")));
    m_active = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_active == reply) { m_active = nullptr; }
        if (m_cancelled) { return; }
        // Stumme API (Azure-Handshake bleibt stehen, DNS weg, Ratelimit)
        // oder eine Antwort, die kein Release ist: zweiter Weg ueber
        // github.com. Erst wenn auch der scheitert, gibt es einen Fehler.
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcUpdate) << "api.github.com failed:" << reply->errorString()
                                << "-- trying github.com instead";
            checkLatestViaWebsite(reply->errorString());
            return;
        }
        QString err;
        const auto release = parseLatestRelease(reply->readAll(), &err);
        if (!release) {
            qCWarning(lcUpdate) << "api.github.com answered but not with a release:" << err
                                << "-- trying github.com instead";
            checkLatestViaWebsite(err);
            return;
        }
        AppSettings::instance().setValue(lastSourceKey(), sourceName(release->source));
        emit latestKnown(*release, compareVersions(release->version, m_runningVersion) > 0);
    });
}

void UpdateChecker::checkLatestViaWebsite(const QString& apiError)
{
    // Die Weiterleitung ist die ganze Auskunft -- ihr folgen muss man
    // nicht, die HTML-Seite dahinter interessiert nicht.
    emit trying(QStringLiteral("api.github.com did not answer — trying github.com…"));
    QNetworkRequest req(latestReleaseWebUrl());
    req.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::ManualRedirectPolicy);
    req.setTransferTimeout(m_azureTimeoutMs);
    QNetworkReply* reply = m_net->get(req);
    m_active = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, apiError]() {
        reply->deleteLater();
        if (m_active == reply) { m_active = nullptr; }
        if (m_cancelled) { return; }
        if (reply->error() != QNetworkReply::NoError) {
            // github.com loest in DENSELBEN Azure-Bereich auf wie
            // api.github.com (gemessen 2026-11-04, siehe Kopf) -- dieser
            // Weg scheitert dort also immer mit. Darum der dritte.
            qCWarning(lcUpdate) << "github.com failed too:" << reply->errorString()
                                << "-- trying raw.githubusercontent.com instead";
            checkLatestViaChangelog(QStringLiteral("%1 (github.com: %2)")
                                        .arg(apiError, reply->errorString()));
            return;
        }
        // Relativ zulaessig, darum ueber die Anfrage-URL aufloesen.
        QUrl target = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        target = target.isEmpty() ? reply->url() : reply->url().resolved(target);
        const QString tag = tagFromReleaseUrl(target);
        if (tag.isEmpty()) {
            qCWarning(lcUpdate) << "github.com answered but pointed nowhere:" << target
                                << "-- trying raw.githubusercontent.com instead";
            checkLatestViaChangelog(
                QStringLiteral("%1 (github.com did not point at a release: %2)")
                    .arg(apiError, target.toString()));
            return;
        }
        qCInfo(lcUpdate) << "github.com points at release" << tag;
        probeAssetAndFinish(releaseFromTag(tag), apiError);
    });
}

void UpdateChecker::checkLatestViaChangelog(const QString& earlierErrors)
{
    // Letzter Weg: eine gewoehnliche Datei bei Fastly, kein Azure-Host,
    // keine API. Scheitert auch der, hat der Bediener alle drei Gruende
    // in einer Zeile.
    emit trying(QStringLiteral("github.com did not answer either — reading CHANGELOG.md "
                               "from raw.githubusercontent.com…"));
    QNetworkReply* reply = m_net->get(request(changelogRawUrl(), kApiTimeoutMs));
    m_active = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, earlierErrors]() {
        reply->deleteLater();
        if (m_active == reply) { m_active = nullptr; }
        if (m_cancelled) { return; }
        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(QStringLiteral("%1 (raw.githubusercontent.com: %2)")
                                 .arg(earlierErrors, reply->errorString()));
            return;
        }
        QString err;
        const auto release = parseChangelogLatest(reply->readAll(), &err);
        if (!release) {
            emit checkFailed(QStringLiteral("%1 (CHANGELOG.md: %2)")
                                 .arg(earlierErrors, err));
            return;
        }
        qCInfo(lcUpdate) << "CHANGELOG.md names release" << release->tag;
        probeAssetAndFinish(*release, earlierErrors);
    });
}

void UpdateChecker::probeAssetAndFinish(const ReleaseInfo& release, const QString& apiError)
{
    // Ohne API gibt es keine Dateiliste. Statt die Datei zu BEHAUPTEN wird
    // sie mit einem HEAD nachgesehen: das beantwortet beides auf einmal --
    // ob es sie gibt und wie gross sie ist (der Dialog zeigt die Groesse).
    const QString wanted = assetNameForThisMachine(release.version);
    if (wanted.isEmpty()) {
        AppSettings::instance().setValue(lastSourceKey(), sourceName(release.source));
        emit latestKnown(release, compareVersions(release.version, m_runningVersion) > 0);
        return;
    }
    emit trying(QStringLiteral("Found %1 — looking for %2…").arg(release.version, wanted));
    QNetworkRequest req = request(assetDownloadUrl(release.tag, wanted), m_azureTimeoutMs);
    QNetworkReply* reply = m_net->head(req);
    m_active = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, release, wanted, apiError]() {
        reply->deleteLater();
        if (m_active == reply) { m_active = nullptr; }
        if (m_cancelled) { return; }
        ReleaseInfo r = release;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && status == 200) {
            ReleaseAsset a;
            a.name = wanted;
            a.url = assetDownloadUrl(release.tag, wanted);
            a.size = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
            r.assets.append(a);
        } else if (reply->error() == QNetworkReply::NoError) {
            // Kein Paket fuer diese Maschine (etwa Windows seit 0.6.7).
            // Der Dialog sagt das von sich aus und bietet die Seite an.
            qCWarning(lcUpdate) << "no package" << wanted << "in" << release.tag
                                << "-- HTTP" << status;
        } else {
            // Die Frage kam nicht an. Das ist NICHT dasselbe wie "es gibt
            // das Paket nicht" -- ueber den dritten Weg ist github.com
            // ohnehin tot, und ein Zeitablauf darf nicht als Befund
            // gelesen werden (Abwesenheit ist keine Aussage).
            qCWarning(lcUpdate) << "could not ask about" << wanted << "in" << release.tag
                                << "--" << reply->errorString();
        }
        Q_UNUSED(apiError);
        AppSettings::instance().setValue(lastSourceKey(), sourceName(r.source));
        emit latestKnown(r, compareVersions(r.version, m_runningVersion) > 0);
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
