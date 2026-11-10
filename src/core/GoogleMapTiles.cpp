// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/GoogleMapTiles.cpp  (Longpath)
// =================================================================
// Longpath-original. No Thetis port. Siehe GoogleMapTiles.h.
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/GoogleMapTiles.h"

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
KeyCache& cache()
{
    static KeyCache c;
    return c;
}
} // namespace

GoogleMapsKey& GoogleMapsKey::notifier()
{
    static GoogleMapsKey n;
    return n;
}

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
    emit notifier().changed();
    return ok;
}

void GoogleMapsKey::erase()
{
    CredentialStore::erase(credentialKey(), credentialAccount());
    cache().value.clear();
    cache().loaded = true;
    emit notifier().changed();
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

QUrl GoogleTilesProbe::rootTilesetUrl(const QString& key, const QUrl& base)
{
    QUrl u = base.resolved(QUrl(QStringLiteral("v1/3dtiles/root.json")));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("key"), key.trimmed());
    u.setQuery(q);
    return u;
}

void GoogleTilesProbe::check(const QString& key)
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply.clear();
    }
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
        emit finished(Verdict::Network, sentence(Verdict::Network, why));
        return;
    }
    QString detail;
    const Verdict v = interpret(status, body, &detail);
    emit finished(v, sentence(v, detail));
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
