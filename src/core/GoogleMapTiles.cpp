// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/GoogleMapTiles.cpp  (Longpath)
// =================================================================
// Longpath-original. No Thetis port. Siehe GoogleMapTiles.h.
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
//   2026-11-10 — Cesium ion als kostenlose Quelle, Map3dAccess.
// =================================================================

#include "core/GoogleMapTiles.h"

#include "core/AppSettings.h"
#include "core/CredentialStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>

namespace Longpath {

// ── Ablage ───────────────────────────────────────────────────────────

namespace {
struct KeyCache {
    bool    loaded{false};
    QString value;
};
KeyCache& cache()          // Google-Schluessel
{
    static KeyCache c;
    return c;
}
KeyCache& ionCache()       // Cesium-ion-Zugang
{
    static KeyCache c;
    return c;
}
const QString kSourceKey = QStringLiteral("Map3dSource");
} // namespace

// ── Welcher Zugang gilt ──────────────────────────────────────────────

Map3dAccess& Map3dAccess::notifier()
{
    static Map3dAccess n;
    return n;
}

Map3dAccess::Source Map3dAccess::chosenSource()
{
    // Vorgabe Cesium ion: kostenlos, ohne Karte (Betreiber 2026-11-10).
    const QString v = AppSettings::instance().value(kSourceKey, QStringLiteral("ion")).toString();
    return v == QLatin1String("google") ? Source::Google : Source::CesiumIon;
}

void Map3dAccess::setChosenSource(Source s)
{
    if (s == chosenSource()
        && AppSettings::instance().value(kSourceKey).isValid()) { return; }
    AppSettings::instance().setValue(kSourceKey, s == Source::Google ? QStringLiteral("google")
                                                                     : QStringLiteral("ion"));
    emit notifier().changed();
}

QString Map3dAccess::secretFor(Source s)
{
    return s == Source::Google ? GoogleMapsKey::load() : CesiumIonToken::load();
}

QString Map3dAccess::secret() { return secretFor(chosenSource()); }

QString Map3dAccess::sourceName(Source s)
{
    return s == Source::Google ? QStringLiteral("Google") : QStringLiteral("Cesium ion");
}

// ── Cesium-ion-Zugang ────────────────────────────────────────────────

QString CesiumIonToken::credentialKey()     { return QStringLiteral("cesium.iontoken"); }
QString CesiumIonToken::credentialAccount() { return QStringLiteral("ion"); }

QString CesiumIonToken::load()
{
    KeyCache& c = ionCache();
    if (!c.loaded) {
        c.value  = CredentialStore::retrieve(credentialKey(), credentialAccount()).trimmed();
        c.loaded = true;
    }
    return c.value;
}

bool CesiumIonToken::save(const QString& token)
{
    const QString t = token.trimmed();
    if (t.isEmpty()) {
        erase();
        return true;
    }
    const bool ok = CredentialStore::store(credentialKey(), credentialAccount(), t);
    ionCache().value  = t;
    ionCache().loaded = true;
    emit Map3dAccess::notifier().changed();
    return ok;
}

void CesiumIonToken::erase()
{
    CredentialStore::erase(credentialKey(), credentialAccount());
    ionCache().value.clear();
    ionCache().loaded = true;
    emit Map3dAccess::notifier().changed();
}

void CesiumIonToken::forgetCachedForTest()
{
    ionCache() = KeyCache{};
}

bool CesiumIonToken::looksLikeToken(const QString& token)
{
    static const QRegularExpression re(
        QStringLiteral("^eyJ[0-9A-Za-z_\\-]+\\.[0-9A-Za-z_\\-]+\\.[0-9A-Za-z_\\-]+$"));
    return re.match(token.trimmed()).hasMatch();
}

// ── Google-Schluessel ────────────────────────────────────────────────

QString GoogleMapsKey::credentialKey()     { return QStringLiteral("google.maptiles"); }
QString GoogleMapsKey::credentialAccount() { return QStringLiteral("maptiles"); }

QString GoogleMapsKey::load()
{
    KeyCache& c = cache();
    if (!c.loaded) {
        c.value  = CredentialStore::retrieve(credentialKey(), credentialAccount()).trimmed();
        c.loaded = true;
    }
    return c.value;
}

bool GoogleMapsKey::save(const QString& key)
{
    const QString k = key.trimmed();
    if (k.isEmpty()) {
        erase();
        return true;
    }
    const bool ok = CredentialStore::store(credentialKey(), credentialAccount(), k);
    // Auch wenn der Schluesselbund ablehnt: fuer diesen Lauf gilt der
    // Schluessel (CredentialStore haelt ihn im Speicher-Tresor).
    cache().value  = k;
    cache().loaded = true;
    emit Map3dAccess::notifier().changed();
    return ok;
}

void GoogleMapsKey::erase()
{
    CredentialStore::erase(credentialKey(), credentialAccount());
    cache().value.clear();
    cache().loaded = true;
    emit Map3dAccess::notifier().changed();
}

void GoogleMapsKey::forgetCachedForTest()
{
    cache() = KeyCache{};
}

bool GoogleMapsKey::looksLikeApiKey(const QString& key)
{
    static const QRegularExpression re(QStringLiteral("^AIza[0-9A-Za-z_\\-]{35}$"));
    return re.match(key.trimmed()).hasMatch();
}

// ── Probe ────────────────────────────────────────────────────────────

GoogleTilesProbe::GoogleTilesProbe(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
    , m_base(defaultEndpoint())
    , m_ionBase(ionDefaultEndpoint())
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        if (m_reply) { m_reply->abort(); }   // landet in onFinished()
    });
}

GoogleTilesProbe::~GoogleTilesProbe()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
}

QUrl GoogleTilesProbe::defaultEndpoint()
{
    return QUrl(QStringLiteral("https://tile.googleapis.com/"));
}

QUrl GoogleTilesProbe::ionDefaultEndpoint()
{
    return QUrl(QStringLiteral("https://api.cesium.com/"));
}

QUrl GoogleTilesProbe::ionEndpointUrl(const QUrl& base)
{
    return base.resolved(QUrl(QStringLiteral("v1/assets/%1/endpoint")
                                  .arg(CesiumIonToken::kGoogle3dAssetId)));
}

QUrl GoogleTilesProbe::rootTilesetUrl(const QString& key, const QUrl& base)
{
    QUrl u = base.resolved(QUrl(QStringLiteral("v1/3dtiles/root.json")));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("key"), key.trimmed());
    u.setQuery(q);
    return u;
}

void GoogleTilesProbe::check(Map3dAccess::Source s, const QString& secret)
{
    if (s == Map3dAccess::Source::Google) { check(secret); }
    else                                  { checkIon(secret); }
}

void GoogleTilesProbe::checkIon(const QString& token)
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply.clear();
    }
    m_asking = Map3dAccess::Source::CesiumIon;
    const QString t = token.trimmed();
    if (t.isEmpty()) {
        emit finished(Verdict::Empty, sentenceIon(Verdict::Empty));
        return;
    }
    QNetworkRequest req(ionEndpointUrl(m_ionBase));
    // Im Kopf, nicht in der URL: so steht der Zugang in keinem Protokoll
    // eines Zwischenstuecks.
    req.setRawHeader("Authorization", "Bearer " + t.toUtf8());
    req.setTransferTimeout(m_timeoutMs);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_nam->get(req);
    connect(m_reply.data(), &QNetworkReply::finished,
            this, &GoogleTilesProbe::onFinished);
    m_timer->start(m_timeoutMs + 1000);
}

void GoogleTilesProbe::check(const QString& key)
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply.clear();
    }
    m_asking = Map3dAccess::Source::Google;
    const QString k = key.trimmed();
    if (k.isEmpty()) {
        emit finished(Verdict::Empty, sentence(Verdict::Empty));
        return;
    }
    QNetworkRequest req(rootTilesetUrl(k, m_base));
    req.setTransferTimeout(m_timeoutMs);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_nam->get(req);
    connect(m_reply.data(), &QNetworkReply::finished,
            this, &GoogleTilesProbe::onFinished);
    m_timer->start(m_timeoutMs + 1000);
}

void GoogleTilesProbe::onFinished()
{
    m_timer->stop();
    QNetworkReply* r = m_reply.data();
    m_reply.clear();
    if (!r) { return; }
    r->deleteLater();

    const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = r->readAll();
    if (status == 0) {
        // Keine HTTP-Antwort: kein Netz, Namensaufloesung, Zeitgrenze.
        const QString why = r->error() == QNetworkReply::OperationCanceledError
            ? QStringLiteral("no answer within %1 s").arg(m_timeoutMs / 1000)
            : r->errorString();
        emit finished(Verdict::Network, sentenceFor(m_asking, Verdict::Network, why));
        return;
    }
    QString detail;
    const Verdict v = m_asking == Map3dAccess::Source::Google
        ? interpret(status, body, &detail)
        : interpretIon(status, body, &detail);
    emit finished(v, sentenceFor(m_asking, v, detail));
}

GoogleTilesProbe::Verdict GoogleTilesProbe::interpretIon(int httpStatus,
                                                         const QByteArray& body,
                                                         QString* message)
{
    const QJsonObject obj = QJsonDocument::fromJson(body).object();
    if (httpStatus == 200) {
        // Eigenes Asset: {"type","url","accessToken"}; Googles Kacheln
        // laufen als externes Asset: {"externalType":"3DTILES",
        // "options":{"url":…}} (so liest es CesiumJS 1.146).
        const bool external = obj.value(QStringLiteral("externalType")).toString()
                                  == QLatin1String("3DTILES")
            && !obj.value(QStringLiteral("options")).toObject()
                    .value(QStringLiteral("url")).toString().isEmpty();
        const bool own = !obj.value(QStringLiteral("url")).toString().isEmpty();
        if (external || own) { return Verdict::Ok; }
        if (message) { *message = QStringLiteral("unexpected answer (no tileset address)"); }
        return Verdict::Other;
    }
    // {"code":"INVALID_TOKEN","message":"Invalid access token"} -- so am
    // 2026-11-10 mit einem erfundenen Zugang gemessen (HTTP 401).
    const QString code = obj.value(QStringLiteral("code")).toString();
    const QString text = obj.value(QStringLiteral("message")).toString();
    if (message) { *message = text.isEmpty() ? QStringLiteral("HTTP %1").arg(httpStatus) : text; }
    if (httpStatus == 429 || code.contains(QLatin1String("QUOTA"), Qt::CaseInsensitive)
        || code.contains(QLatin1String("LIMIT"), Qt::CaseInsensitive)
        || text.contains(QLatin1String("quota"), Qt::CaseInsensitive)) {
        return Verdict::Quota;
    }
    if (httpStatus == 401) { return Verdict::InvalidKey; }
    if (httpStatus == 403 || httpStatus == 404) { return Verdict::AssetMissing; }
    return Verdict::Other;
}

QString GoogleTilesProbe::sentenceIon(Verdict v, const QString& detail)
{
    QString s;
    switch (v) {
    case Verdict::Ok:
        s = QStringLiteral("Token accepted — Google's 3D tiles are available "
                           "through Cesium ion. Press Save.");
        break;
    case Verdict::Empty:
        s = QStringLiteral("Paste your Cesium ion token first.");
        break;
    case Verdict::InvalidKey:
        s = QStringLiteral("Cesium ion does not know this token — copy it again "
                           "from Access Tokens (it starts with \"eyJ\").");
        break;
    case Verdict::AssetMissing:
        s = QStringLiteral("This token cannot open Google's 3D tiles — check that "
                           "\"Google Photorealistic 3D Tiles\" is in My Assets "
                           "(step 3) and that the token may read assets.");
        break;
    case Verdict::Quota:
        s = QStringLiteral("The free monthly allowance of your Cesium ion account "
                           "is used up — it starts again next month.");
        break;
    case Verdict::Network:
        s = QStringLiteral("Could not reach Cesium ion");
        break;
    default:
        s = QStringLiteral("Cesium ion refused the token");
        break;
    }
    if ((v == Verdict::Network || v == Verdict::Other) && !detail.isEmpty()) {
        s += QStringLiteral(" — ") + detail;
    }
    return s;
}

QString GoogleTilesProbe::sentenceFor(Map3dAccess::Source s, Verdict v,
                                      const QString& detail)
{
    return s == Map3dAccess::Source::Google ? sentence(v, detail)
                                            : sentenceIon(v, detail);
}

bool GoogleTilesProbe::isAccessProblem(Verdict v)
{
    switch (v) {
    case Verdict::InvalidKey:
    case Verdict::ApiNotEnabled:
    case Verdict::KeyRestricted:
    case Verdict::AppRestricted:
    case Verdict::NoBilling:
    case Verdict::AssetMissing:
    case Verdict::Quota:
        return true;
    default:
        return false;
    }
}

GoogleTilesProbe::Verdict GoogleTilesProbe::interpret(int httpStatus,
                                                      const QByteArray& body,
                                                      QString* message)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    const QJsonObject obj = doc.object();

    if (httpStatus == 200) {
        // Ein Tileset hat "asset" und "root". Eine 200 ohne die beiden
        // ist ein Vorschalt-Portal (Hotel-WLAN), keine Zusage von Google.
        if (obj.contains(QStringLiteral("root")) && obj.contains(QStringLiteral("asset"))) {
            return Verdict::Ok;
        }
        if (message) { *message = QStringLiteral("unexpected answer (not a tileset)"); }
        return Verdict::Other;
    }

    // Googles Fehlerform: {"error":{"code","message","status",
    // "details":[{"@type":"…ErrorInfo","reason":"…"}]}}
    const QJsonObject err = obj.value(QStringLiteral("error")).toObject();
    const QString text   = err.value(QStringLiteral("message")).toString();
    const QString status = err.value(QStringLiteral("status")).toString();
    QString reason;
    for (const QJsonValue d : err.value(QStringLiteral("details")).toArray()) {
        const QString r = d.toObject().value(QStringLiteral("reason")).toString();
        if (!r.isEmpty()) { reason = r; break; }
    }
    if (message) { *message = text; }

    // Zuerst der maschinenlesbare Grund ...
    if (reason == QLatin1String("API_KEY_INVALID")) { return Verdict::InvalidKey; }
    if (reason == QLatin1String("BILLING_DISABLED")) { return Verdict::NoBilling; }
    if (reason == QLatin1String("SERVICE_DISABLED")
        || reason == QLatin1String("ACCESS_NOT_CONFIGURED")) {
        return Verdict::ApiNotEnabled;
    }
    if (reason == QLatin1String("API_KEY_SERVICE_BLOCKED")) { return Verdict::KeyRestricted; }
    if (reason.startsWith(QLatin1String("API_KEY_"))
        && reason.endsWith(QLatin1String("_BLOCKED"))) {
        // HTTP_REFERRER / IP_ADDRESS / ANDROID_APP / IOS_APP
        return Verdict::AppRestricted;
    }
    if (httpStatus == 429 || status == QLatin1String("RESOURCE_EXHAUSTED")
        || reason == QLatin1String("RATE_LIMIT_EXCEEDED")) {
        return Verdict::Quota;
    }

    // ... dann der Text als Rueckfall: aeltere Antworten der Kachel-API
    // kamen ohne "details". "are blocked" steht in ZWEI Absagen (Dienst
    // gesperrt, Referer/IP gesperrt) -- darum Referer/IP zuerst.
    auto has = [&text](const char* s) {
        return text.contains(QLatin1String(s), Qt::CaseInsensitive);
    };
    if (has("API key not valid") || has("API key expired")) { return Verdict::InvalidKey; }
    if (has("billing")) { return Verdict::NoBilling; }
    if (has("has not been used in project") || has("is disabled")) {
        return Verdict::ApiNotEnabled;
    }
    if (has("referer") || has("referrer") || has("IP address")) {
        return Verdict::AppRestricted;
    }
    if (has("are blocked")) { return Verdict::KeyRestricted; }
    if (httpStatus == 400 && text.isEmpty()) {
        return Verdict::InvalidKey;
    }
    if (message && message->isEmpty()) {
        *message = QStringLiteral("HTTP %1").arg(httpStatus);
    }
    return Verdict::Other;
}

QString GoogleTilesProbe::sentence(Verdict v, const QString& detail)
{
    QString s;
    switch (v) {
    case Verdict::Ok:
        s = QStringLiteral("Key accepted — Google's 3D tiles are available. Press Save.");
        break;
    case Verdict::Empty:
        s = QStringLiteral("Paste your API key first.");
        break;
    case Verdict::InvalidKey:
        s = QStringLiteral("Google does not know this key — check that it was "
                           "copied completely (it starts with \"AIza\").");
        break;
    case Verdict::ApiNotEnabled:
        s = QStringLiteral("The Map Tiles API is not enabled in this key's "
                           "project — see step 3.");
        break;
    case Verdict::KeyRestricted:
        s = QStringLiteral("The key's API restriction leaves out the Map Tiles "
                           "API — tick it under \"API restrictions\" (step 5).");
        break;
    case Verdict::AppRestricted:
        s = QStringLiteral("The key is limited to websites or IP addresses, which "
                           "blocks Longpath — set \"Application restrictions\" to "
                           "None (step 5).");
        break;
    case Verdict::NoBilling:
        s = QStringLiteral("The project has no billing account. Google requires "
                           "one even for the free monthly quota — see step 2.");
        break;
    case Verdict::Quota:
        s = QStringLiteral("Google says the quota for this key is used up — try "
                           "again later or raise the limit in the console.");
        break;
    case Verdict::Network:
        s = QStringLiteral("Could not reach Google");
        break;
    case Verdict::AssetMissing:   // kommt nur bei Cesium ion vor
    case Verdict::Other:
        s = QStringLiteral("Google refused the key");
        break;
    }
    if ((v == Verdict::Network || v == Verdict::Other) && !detail.isEmpty()) {
        s += QStringLiteral(" — ") + detail;
    }
    return s;
}

} // namespace Longpath
