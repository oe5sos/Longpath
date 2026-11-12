// no-port-check: Longpath-original, no Thetis logic.
//
// src/core/HoehenDaten.cpp  (Longpath) -- siehe HoehenDaten.h.
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#include "core/HoehenDaten.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>
#include <QUrlQuery>

#include <cmath>
#include <limits>

namespace Longpath {

namespace {

// Ein Takt fuer das ganze Programm: zwei offene Profilfenster duerfen
// zusammen nicht oefter als einmal je Sekunde fragen.
QElapsedTimer& uhr()
{
    static QElapsedTimer u;
    if (!u.isValid()) { u.start(); }
    return u;
}
qint64 g_letzterAbrufMs = -1000000;

QString zahl(double v)
{
    return QString::number(v, 'f', 5);
}

} // namespace

HoehenAbruf::HoehenAbruf(QObject* parent)
    : QObject(parent)
    , m_ordner(AppSettings::dataDir() + QStringLiteral("/hoehenprofile"))
{
    m_abruf = standardHttpAbruf(this, QStringLiteral("Longpath"));
}

void HoehenAbruf::setCacheOrdner(const QString& ordner)
{
    m_ordner = ordner;
}

void HoehenAbruf::setAbruf(HttpAbruf abruf)
{
    m_abruf = std::move(abruf);
}

QString HoehenAbruf::schluessel(const Weg& w)
{
    const QString roh = QStringLiteral("eudem25m,srtm90m|%1,%2|%3,%4|%5")
                            .arg(zahl(w.lat1), zahl(w.lon1), zahl(w.lat2), zahl(w.lon2))
                            .arg(w.punkte);
    return QString::fromLatin1(
        QCryptographicHash::hash(roh.toUtf8(), QCryptographicHash::Sha1).toHex().left(20));
}

QUrl HoehenAbruf::anfrageUrl(const QVector<Hoehenprofil::Punkt>& punkte)
{
    QStringList orte;
    orte.reserve(punkte.size());
    for (const Hoehenprofil::Punkt& p : punkte) {
        orte << zahl(p.lat) + QLatin1Char(',') + zahl(p.lon);
    }
    QUrl url(QStringLiteral("https://api.opentopodata.org/v1/eudem25m,srtm90m"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("locations"), orte.join(QLatin1Char('|')));
    url.setQuery(q);
    return url;
}

bool HoehenAbruf::leseAntwort(const QByteArray& json, QVector<double>& aus, QString* fehler)
{
    aus.clear();
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        if (fehler) { *fehler = QStringLiteral("not a JSON answer"); }
        return false;
    }
    const QJsonObject o = doc.object();
    const QString status = o.value(QStringLiteral("status")).toString();
    if (status != QLatin1String("OK")) {
        if (fehler) {
            const QString text = o.value(QStringLiteral("error")).toString();
            *fehler = text.isEmpty() ? QStringLiteral("status %1").arg(status) : text;
        }
        return false;
    }
    const QJsonArray res = o.value(QStringLiteral("results")).toArray();
    aus.reserve(res.size());
    for (const QJsonValue& v : res) {
        const QJsonValue h = v.toObject().value(QStringLiteral("elevation"));
        aus.push_back(h.isDouble() ? h.toDouble()
                                   : std::numeric_limits<double>::quiet_NaN());
    }
    return true;
}

bool HoehenAbruf::ausCache(const Weg& w, QVector<double>& aus) const
{
    QFile f(m_ordner + QLatin1Char('/') + schluessel(w) + QStringLiteral(".json"));
    if (!f.open(QIODevice::ReadOnly)) { return false; }
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    const QJsonArray h = o.value(QStringLiteral("elevation")).toArray();
    if (h.size() != w.punkte) { return false; }
    aus.clear();
    aus.reserve(h.size());
    for (const QJsonValue& v : h) {
        aus.push_back(v.isDouble() ? v.toDouble() : std::numeric_limits<double>::quiet_NaN());
    }
    return true;
}

bool HoehenAbruf::inCacheLegen(const Weg& w, const QVector<double>& hoehen) const
{
    if (hoehen.size() != w.punkte) { return false; }
    QJsonArray h;
    for (double v : hoehen) {
        h.append(std::isfinite(v) ? QJsonValue(v) : QJsonValue());
    }
    QJsonObject o;
    o.insert(QStringLiteral("source"), QStringLiteral("opentopodata eudem25m,srtm90m"));
    o.insert(QStringLiteral("from"), QJsonArray{w.lat1, w.lon1});
    o.insert(QStringLiteral("to"), QJsonArray{w.lat2, w.lon2});
    o.insert(QStringLiteral("points"), w.punkte);
    o.insert(QStringLiteral("elevation"), h);
    QDir().mkpath(m_ordner);
    QSaveFile f(m_ordner + QLatin1Char('/') + schluessel(w) + QStringLiteral(".json"));
    if (!f.open(QIODevice::WriteOnly)) { return false; }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    return f.commit();
}

void HoehenAbruf::hole(const Weg& w)
{
    ++m_generation;
    m_weg = w;
    m_schluessel = schluessel(w);
    m_laeuft = false;

    QVector<double> h;
    if (ausCache(w, h)) {
        const int gen = m_generation;
        const QString key = m_schluessel;
        QTimer::singleShot(0, this, [this, gen, key, h]() {
            if (gen == m_generation) { emit fertig(key, h); }
        });
        return;
    }

    m_punkte = Hoehenprofil::grosskreisPunkte(w.lat1, w.lon1, w.lat2, w.lon2, w.punkte);
    m_hoehen.clear();
    m_naechster = 0;
    m_laeuft = true;
    naechstesStueck();
}

void HoehenAbruf::naechstesStueck()
{
    if (!m_laeuft) { return; }
    const qint64 jetzt = uhr().elapsed();
    const qint64 warten = g_letzterAbrufMs + kAbstandMs - jetzt;
    const int gen = m_generation;
    if (warten > 0) {
        QTimer::singleShot(static_cast<int>(warten), this, [this, gen]() {
            if (gen == m_generation) { naechstesStueck(); }
        });
        return;
    }
    g_letzterAbrufMs = jetzt;

    const int von = m_naechster;
    const int bis = std::min<int>(von + kPunkteJeAbruf, m_punkte.size());
    const QUrl url = anfrageUrl(m_punkte.mid(von, bis - von));
    ++m_gestellt;
    emit fortschritt(von, m_punkte.size());
    const QString key = m_schluessel;
    QPointer<HoehenAbruf> selbst(this);
    m_abruf(url, [this, selbst, gen, key, von, bis](const HttpAntwort& a) {
        if (!selbst || gen != m_generation) { return; }   // weg oder ueberholt
        QString text;
        QVector<double> stueck;
        if (!a.netzFehler.isEmpty()) {
            text = a.netzFehler;
        } else if (a.status == 429) {
            text = QStringLiteral("OpenTopoData: too many requests — try again later");
        } else if (!leseAntwort(a.body, stueck, &text)) {
            text = QStringLiteral("OpenTopoData: %1").arg(text);
        } else if (stueck.size() != bis - von) {
            text = QStringLiteral("OpenTopoData returned %1 heights for %2 points")
                       .arg(stueck.size()).arg(bis - von);
        }
        if (!text.isEmpty()) {
            m_laeuft = false;
            qCWarning(lcSpots) << "Hoehenprofil:" << text;
            emit fehler(key, text);
            return;
        }
        m_hoehen += stueck;
        m_naechster = bis;
        if (m_naechster < m_punkte.size()) {
            naechstesStueck();
            return;
        }
        m_laeuft = false;
        inCacheLegen(m_weg, m_hoehen);
        emit fortschritt(m_punkte.size(), m_punkte.size());
        emit fertig(key, m_hoehen);
    });
}

} // namespace Longpath
