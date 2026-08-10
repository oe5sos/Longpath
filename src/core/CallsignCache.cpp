// =================================================================
// src/core/CallsignCache.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See CallsignCache.h for why a stale entry is
// handed back rather than deleted.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/CallsignCache.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace NereusSDR {

QString CallsignCache::defaultPath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/callsign-cache.json");
}

CallsignCache::CallsignCache(const QString& path)
    : m_path(path.isEmpty() ? defaultPath() : path)
{
}

QJsonObject CallsignCache::toJson(const CallsignInfo& i)
{
    QJsonObject o;
    // Only what is set. A file of mostly empty strings is a file that is
    // mostly punctuation, and this one is read by a human when
    // something goes wrong.
    auto str = [&o](const char* k, const QString& v) {
        if (!v.isEmpty()) { o.insert(QLatin1String(k), v); }
    };
    str("call",      i.call);
    str("fname",     i.firstName);
    str("name",      i.lastName);
    str("name_fmt",  i.nameFmt);
    str("city",      i.city);
    str("state",     i.state);
    str("country",   i.country);
    str("county",    i.county);
    str("grid",      i.grid);
    str("class",     i.licenseClass);
    str("email",     i.email);
    str("url",       i.url);
    str("image",     i.imageUrl);
    if (i.hasLatLon) {
        o.insert(QStringLiteral("lat"), i.latitude);
        o.insert(QStringLiteral("lon"), i.longitude);
    }
    if (i.lotw)    { o.insert(QStringLiteral("lotw"), true); }
    if (i.eqsl)    { o.insert(QStringLiteral("eqsl"), true); }
    if (i.mailQsl) { o.insert(QStringLiteral("mqsl"), true); }
    o.insert(QStringLiteral("fetched"), double(i.fetchedUtc));
    return o;
}

CallsignInfo CallsignCache::fromJson(const QJsonObject& o)
{
    CallsignInfo i;
    i.call         = o.value(QStringLiteral("call")).toString();
    i.firstName    = o.value(QStringLiteral("fname")).toString();
    i.lastName     = o.value(QStringLiteral("name")).toString();
    i.nameFmt      = o.value(QStringLiteral("name_fmt")).toString();
    i.city         = o.value(QStringLiteral("city")).toString();
    i.state        = o.value(QStringLiteral("state")).toString();
    i.country      = o.value(QStringLiteral("country")).toString();
    i.county       = o.value(QStringLiteral("county")).toString();
    i.grid         = o.value(QStringLiteral("grid")).toString();
    i.licenseClass = o.value(QStringLiteral("class")).toString();
    i.email        = o.value(QStringLiteral("email")).toString();
    i.url          = o.value(QStringLiteral("url")).toString();
    i.imageUrl     = o.value(QStringLiteral("image")).toString();
    if (o.contains(QStringLiteral("lat"))
        && o.contains(QStringLiteral("lon"))) {
        i.latitude  = o.value(QStringLiteral("lat")).toDouble();
        i.longitude = o.value(QStringLiteral("lon")).toDouble();
        i.hasLatLon = true;
    }
    i.lotw    = o.value(QStringLiteral("lotw")).toBool(false);
    i.eqsl    = o.value(QStringLiteral("eqsl")).toBool(false);
    i.mailQsl = o.value(QStringLiteral("mqsl")).toBool(false);
    // toDouble rather than toInteger: QJsonValue holds numbers as
    // doubles, and an epoch second past 2^31 read through toInt() comes
    // back as zero — which would make every entry permanently stale in
    // 2038 and, more usefully, right now on any value written as a
    // double literal.
    i.fetchedUtc = qint64(o.value(QStringLiteral("fetched")).toDouble(0.0));
    return i;
}

void CallsignCache::load()
{
    m_entries.clear();

    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) { return; }
    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    // A corrupt cache is an empty cache. Refusing to start over a file
    // whose entire purpose is to save a network request would be a poor
    // trade.
    if (err.error != QJsonParseError::NoError || !doc.isObject()) { return; }

    const QJsonObject root = doc.object();
    const QJsonObject calls =
        root.value(QStringLiteral("calls")).toObject();
    for (auto it = calls.constBegin(); it != calls.constEnd(); ++it) {
        if (!it.value().isObject()) { continue; }
        m_entries.insert(it.key(), fromJson(it.value().toObject()));
    }
}

bool CallsignCache::save() const
{
    QJsonObject calls;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        calls.insert(it.key(), toJson(it.value()));
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("calls"), calls);

    QDir().mkpath(QFileInfo(m_path).absolutePath());

    // QSaveFile writes beside the target and renames on commit, so a
    // crash halfway through leaves the previous cache intact rather
    // than a truncated file that will not parse.
    QSaveFile out(m_path);
    if (!out.open(QIODevice::WriteOnly)) { return false; }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return out.commit();
}

bool CallsignCache::contains(const QString& call) const
{
    return m_entries.contains(Callsigns::normalized(call));
}

CallsignInfo CallsignCache::get(const QString& call) const
{
    return m_entries.value(Callsigns::normalized(call));
}

bool CallsignCache::isStale(const CallsignInfo& info) const
{
    if (!info.isValid())     { return true; }
    if (info.fetchedUtc <= 0) { return true; }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    // A timestamp in the future is a clock that was wrong when the entry
    // was written. Treat it as stale rather than as valid until the
    // year it claims.
    if (info.fetchedUtc > now) { return true; }
    return (now - info.fetchedUtc) > m_maxAge;
}

void CallsignCache::put(const QString& call, const CallsignInfo& info)
{
    const QString key = Callsigns::normalized(call);
    if (key.isEmpty()) { return; }

    CallsignInfo copy = info;
    if (copy.fetchedUtc <= 0) {
        copy.fetchedUtc = QDateTime::currentSecsSinceEpoch();
    }
    m_entries.insert(key, copy);
}

void CallsignCache::clear()
{
    m_entries.clear();
}

} // namespace NereusSDR
