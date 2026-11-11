// no-port-check: Longpath-original — see GfzIndizes.h.
//
// =================================================================
// src/core/GfzIndizes.cpp  (Longpath)
// =================================================================
//
// Longpath-original — siehe GfzIndizes.h.

#include "core/GfzIndizes.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Longpath {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Weniger Tage als das ist keine vollstaendige GFZ-Datei (seit 1932 sind
// es ueber 34 000). Ein abgeschnittener Download darf den Cache nicht
// ersetzen. Fuer lies() selbst gilt die Schwelle nicht — der Prüfstand
// liest einen Ausschnitt.
constexpr int kMindestTageImDownload = 30000;

QString dateiname() { return QStringLiteral("Kp_ap_Ap_SN_F107_since_1932.txt"); }

} // namespace

// ── Lesen ─────────────────────────────────────────────────────────────

int GfzIndizes::lies(const QByteArray& text)
{
    const QList<QByteArray> zeilen = text.split('\n');

    // Die Kopfzeile mit den Spaltennamen: "#YYY MM DD days days_m Bsr dB
    // Kp1 … Kp8 ap1 … ap8 Ap SN F10.7obs F10.7adj D". Das erste Feld ist
    // das Jahr; das '#' frisst dort ein Y. Nach GANZEN Namen gesucht: die
    // Beschreibungszeile darueber („# Kp1 to Kp8 (…), … F10.7obs,
    // F10.7adj") enthaelt dieselben Woerter als Text.
    QHash<QString, int> spalte;
    for (const QByteArray& z : zeilen) {
        if (!z.startsWith('#')) { continue; }
        const QStringList namen = QString::fromLatin1(z.mid(1))
                                      .split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (!namen.contains(QLatin1String("MM")) || !namen.contains(QLatin1String("DD"))
            || !namen.contains(QLatin1String("Kp1"))
            || !namen.contains(QLatin1String("F10.7obs"))) {
            continue;
        }
        for (int i = 0; i < namen.size(); ++i) { spalte.insert(namen.at(i), i); }
        break;
    }
    const int sJahr = spalte.value(QStringLiteral("YYY"),
                                   spalte.value(QStringLiteral("YYYY"), -1));
    const int sMonat = spalte.value(QStringLiteral("MM"), -1);
    const int sTag   = spalte.value(QStringLiteral("DD"), -1);
    std::array<int, 8> sKp{};
    for (int i = 0; i < 8; ++i) {
        sKp[size_t(i)] = spalte.value(QStringLiteral("Kp%1").arg(i + 1), -1);
    }
    const int sAp   = spalte.value(QStringLiteral("Ap"), -1);
    const int sSn   = spalte.value(QStringLiteral("SN"), -1);
    const int sObs  = spalte.value(QStringLiteral("F10.7obs"), -1);
    const int sAdj  = spalte.value(QStringLiteral("F10.7adj"), -1);
    const int sD    = spalte.value(QStringLiteral("D"), -1);

    int noetig = std::max({sJahr, sMonat, sTag, sAp, sSn, sObs, sAdj, sD});
    for (int k : sKp) {
        if (k < 0) { return 0; }
        noetig = std::max(noetig, k);
    }
    if (sJahr < 0 || sMonat < 0 || sTag < 0 || sObs < 0 || sAdj < 0
        || sAp < 0 || sSn < 0 || sD < 0) {
        return 0;
    }

    QHash<qint64, GfzTag> tage;
    QDate erster, letzter;
    for (const QByteArray& z : zeilen) {
        if (z.isEmpty() || z.startsWith('#')) { continue; }
        const QList<QByteArray> f = z.simplified().split(' ');
        if (f.size() <= noetig) { continue; }
        const QDate datum(f.at(sJahr).toInt(), f.at(sMonat).toInt(), f.at(sTag).toInt());
        if (!datum.isValid()) { continue; }
        GfzTag t;
        t.datum = datum;
        bool ok = true;
        for (int i = 0; i < 8; ++i) {
            bool k = false;
            t.kp[size_t(i)] = f.at(sKp[size_t(i)]).toDouble(&k);
            ok = ok && k;
        }
        bool a = false, b = false, c = false, d = false, e = false;
        t.ap      = f.at(sAp).toInt(&a);
        t.sn      = f.at(sSn).toInt(&b);
        t.f107obs = f.at(sObs).toDouble(&c);
        t.f107adj = f.at(sAdj).toDouble(&d);
        t.d       = f.at(sD).toInt(&e);
        if (!ok || !a || !b || !c || !d || !e) { continue; }
        tage.insert(datum.toJulianDay(), t);
        if (!erster.isValid() || datum < erster) { erster = datum; }
        if (!letzter.isValid() || datum > letzter) { letzter = datum; }
    }
    if (tage.isEmpty()) { return 0; }
    m_tage = std::move(tage);
    m_erster = erster;
    m_letzter = letzter;
    return int(m_tage.size());
}

const GfzTag* GfzIndizes::tag(const QDate& d) const
{
    if (!d.isValid()) { return nullptr; }
    const auto it = m_tage.constFind(d.toJulianDay());
    return it == m_tage.constEnd() ? nullptr : &it.value();
}

double GfzIndizes::sfi(const QDate& d) const
{
    const GfzTag* t = tag(d);
    // −1.0 ist „fehlt" (Dateikopf); ein echter Fluss liegt nie unter ~60.
    if (!t || t->f107obs <= 0.0) { return kNaN; }
    return t->f107obs;
}

double GfzIndizes::kp(const QDateTime& utc) const
{
    if (!utc.isValid()) { return kNaN; }
    const QDateTime u = utc.toUTC();
    const GfzTag* t = tag(u.date());
    if (!t) { return kNaN; }
    // Kp1 = 00–03 UT … Kp8 = 21–24 UT.
    const int fenster = std::clamp(u.time().hour() / 3, 0, 7);
    const double k = t->kp[size_t(fenster)];
    return k < 0.0 ? kNaN : k;
}

QString GfzIndizes::kpText(double kp)
{
    if (std::isnan(kp) || kp < 0.0) { return {}; }
    const int drittel = int(std::lround(kp * 3.0));
    const int ganz = drittel / 3;
    switch (drittel % 3) {
    case 1:  return QStringLiteral("%1+").arg(ganz);
    case 2:  return QStringLiteral("%1-").arg(ganz + 1);
    default: return QString::number(ganz);
    }
}

QString GfzIndizes::kurztext(const QDateTime& utc) const
{
    if (!utc.isValid()) { return {}; }
    QStringList teile;
    const double s = sfi(utc.toUTC().date());
    if (!std::isnan(s)) { teile << QStringLiteral("SFI %1").arg(std::lround(s)); }
    const double k = kp(utc);
    if (!std::isnan(k)) { teile << QStringLiteral("Kp %1").arg(kpText(k)); }
    return teile.join(QStringLiteral(" · "));
}

QString GfzIndizes::hinweis(const QDateTime& utc) const
{
    if (!utc.isValid()) { return {}; }
    const QDateTime u = utc.toUTC();
    const GfzTag* t = tag(u.date());
    if (!t) { return {}; }
    const int f = std::clamp(u.time().hour() / 3, 0, 7);
    QStringList z;
    if (t->f107obs > 0.0) {
        z << QStringLiteral("Solar flux F10.7 %1 (observed at local noon, Penticton)")
                 .arg(t->f107obs, 0, 'f', 1);
    }
    if (t->kp[size_t(f)] >= 0.0) {
        z << QStringLiteral("Kp %1 for %2–%3 UT, Ap %4 for the day")
                 .arg(kpText(t->kp[size_t(f)]))
                 .arg(f * 3, 2, 10, QLatin1Char('0'))
                 .arg(f * 3 + 3, 2, 10, QLatin1Char('0'))
                 .arg(t->ap);
    }
    if (z.isEmpty()) { return {}; }
    z << (t->d == 0 ? QStringLiteral("Preliminary values (GFZ Potsdam)")
                    : QStringLiteral("Definitive Kp (GFZ Potsdam)"));
    return z.join(QLatin1Char('\n'));
}

// ── Die Datei im Cache ──────────────────────────────────────────────

GfzQuelle& GfzQuelle::instanz()
{
    // Absichtlich nie zerstoert: ein statisches QObject mit Netzzugang,
    // das nach QApplication abgebaut wird, ist genau die Sorte Absturz
    // beim Beenden, die dieses Programm schon zu oft hatte.
    static GfzQuelle* q = new GfzQuelle;
    return *q;
}

GfzQuelle::GfzQuelle(QObject* parent)
    : QObject(parent)
    , m_cachePfad(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                  + QStringLiteral("/gfz/") + dateiname())
{
}

QUrl GfzQuelle::standardQuelle()
{
    return QUrl(QStringLiteral("https://kp.gfz.de/app/files/") + dateiname());
}

void GfzQuelle::setCachePfad(const QString& pfad)
{
    m_cachePfad = pfad;
    m_daten = GfzIndizes{};
    m_cacheGelesen = false;
}

void GfzQuelle::ladeCache()
{
    m_cacheGelesen = true;
    QFile f(m_cachePfad);
    if (!f.open(QIODevice::ReadOnly)) { return; }
    GfzIndizes neu;
    if (neu.lies(f.readAll()) > 0) { m_daten = std::move(neu); }
}

const GfzIndizes& GfzQuelle::daten()
{
    if (!m_cacheGelesen) {
        ladeCache();
        aktualisiereWennAlt();
    }
    return m_daten;
}

void GfzQuelle::aktualisiereWennAlt()
{
    if (m_reply) { return; }
    // Das Tor sitzt hier, an der ersten Stelle, die ins Netz will: aus
    // einem Pruefstand nie zum GFZ.
    if (QStandardPaths::isTestModeEnabled()) { return; }

    const QDateTime jetzt = QDateTime::currentDateTimeUtc();
    const QFileInfo cache(m_cachePfad);
    if (cache.exists()
        && cache.lastModified().toUTC().secsTo(jetzt) < qint64(kFrischStunden) * 3600) {
        return;
    }
    const QFileInfo versuch(m_cachePfad + QStringLiteral(".versuch"));
    if (versuch.exists()
        && versuch.lastModified().toUTC().secsTo(jetzt) < qint64(kWiederholStunden) * 3600) {
        return;
    }

    QDir().mkpath(cache.absolutePath());
    {
        QFile marke(versuch.filePath());
        if (marke.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            marke.write(jetzt.toString(Qt::ISODate).toUtf8());
        }
    }

    QNetworkRequest req(standardQuelle());
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Longpath"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(90000);
    if (!m_nam) { m_nam = new QNetworkAccessManager(this); }
    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &GfzQuelle::fertig);
}

void GfzQuelle::fertig()
{
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    if (!reply) { return; }
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) { return; }

    const QByteArray text = reply->readAll();
    GfzIndizes neu;
    // Eine Fehlerseite oder ein abgebrochener Download ersetzt nie den
    // Cache.
    if (neu.lies(text) < kMindestTageImDownload) { return; }

    QDir().mkpath(QFileInfo(m_cachePfad).absolutePath());
    QSaveFile f(m_cachePfad);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(text);
        f.commit();
    }
    m_daten = std::move(neu);
    m_cacheGelesen = true;
    emit aktualisiert();
}

} // namespace Longpath
