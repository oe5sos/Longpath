// no-port-check: Longpath-original, see SolarFluxArchiv.h.
//
// src/core/SolarFluxArchiv.cpp  (Longpath)

#include "core/SolarFluxArchiv.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

Q_LOGGING_CATEGORY(lcSolarFlux, "longpath.logbuch.flux")

namespace Longpath {

SolarFluxArchiv::SolarFluxArchiv(const QString& cachePfad, QObject* parent)
    : QObject(parent)
    , m_pfad(cachePfad.isEmpty() ? standardPfad() : cachePfad)
{
}

QString SolarFluxArchiv::standardPfad()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
         + QStringLiteral("/swpc-solar-cycle.json");
}

bool SolarFluxArchiv::ladeCache()
{
    if (m_cacheGelesen) { return hatWerte(); }
    m_cacheGelesen = true;
    QFile f(m_pfad);
    if (!f.open(QIODevice::ReadOnly)) { return false; }
    m_werte = LogbuchBilder::fluxAusSwpc(f.readAll());
    m_stand = m_werte.isEmpty() ? QDateTime() : QFileInfo(m_pfad).lastModified().toUTC();
    return hatWerte();
}

bool SolarFluxArchiv::brauchtAbruf(const QDateTime& cacheStand, const QDateTime& letzterVersuch,
                                   const QDateTime& jetzt)
{
    if (letzterVersuch.isValid() && letzterVersuch.secsTo(jetzt) < 3600) { return false; }
    if (!cacheStand.isValid()) { return true; }
    return cacheStand.secsTo(jetzt) >= 24 * 3600;
}

bool SolarFluxArchiv::uebernimm(const QByteArray& json, const QDateTime& jetzt)
{
    const LogbuchBilder::FluxJeMonat neu = LogbuchBilder::fluxAusSwpc(json);
    if (neu.isEmpty()) { return false; }      // kaputte Antwort: den alten Cache behalten
    QDir().mkpath(QFileInfo(m_pfad).absolutePath());
    QSaveFile f(m_pfad);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(json);
        if (!f.commit()) { qCWarning(lcSolarFlux) << "Cache nicht geschrieben:" << m_pfad; }
    }
    m_werte = neu;
    m_stand = jetzt.toUTC();
    m_cacheGelesen = true;
    return true;
}

void SolarFluxArchiv::holeWennNoetig(const QDateTime& jetzt)
{
    ladeCache();
    if (m_laeuft || !brauchtAbruf(m_stand, m_letzterVersuch, jetzt)) { return; }
    m_letzterVersuch = jetzt;
    m_laeuft = true;
    if (!m_netz) { m_netz = new QNetworkAccessManager(this); }
    QNetworkRequest req{QUrl(QString::fromLatin1(kUrl))};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Longpath"));
    req.setTransferTimeout(30000);
    QNetworkReply* reply = m_netz->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_laeuft = false;
        if (reply->error() != QNetworkReply::NoError) {
            qCInfo(lcSolarFlux) << "SWPC nicht erreichbar:" << reply->errorString()
                                << (hatWerte() ? "-- Cache bleibt" : "-- kein Cache");
            return;
        }
        if (uebernimm(reply->readAll(), QDateTime::currentDateTimeUtc())) {
            qCInfo(lcSolarFlux) << "Solar-Flux geholt:" << m_werte.size() << "Monate bis"
                                << m_werte.lastKey();
            emit aktualisiert();
        } else {
            qCWarning(lcSolarFlux) << "SWPC-Antwort ohne F10.7-Werte -- Cache bleibt";
        }
    });
}

} // namespace Longpath
