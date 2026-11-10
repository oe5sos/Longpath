// no-port-check: Longpath-original, no Thetis logic.
//
// src/core/SotaGipfel.cpp  (Longpath) -- siehe SotaGipfel.h.
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#include "core/SotaGipfel.h"

#include "core/AppSettings.h"
#include "core/BeamHeading.h"
#include "core/LogCategories.h"
#include "core/Maidenhead.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>

namespace Longpath {

namespace {

QString userAgent()
{
    const QString call = AppSettings::stationCallsign();
    return call.isEmpty() ? QStringLiteral("Longpath")
                          : QStringLiteral("Longpath (%1)").arg(call);
}

} // namespace

SotaGipfelVerzeichnis::SotaGipfelVerzeichnis(const QString& cachePfad, QObject* parent)
    : QObject(parent)
    , m_pfad(cachePfad)
{
    m_abruf = standardHttpAbruf(this, userAgent());
    m_takt.setSingleShot(true);
    connect(&m_takt, &QTimer::timeout, this, &SotaGipfelVerzeichnis::naechste);
    laden();
}

SotaGipfelVerzeichnis& SotaGipfelVerzeichnis::instanz()
{
    static SotaGipfelVerzeichnis* s = new SotaGipfelVerzeichnis(
        AppSettings::dataDir() + QStringLiteral("/sota-summits.json"));
    return *s;
}

QString SotaGipfelVerzeichnis::normalisiere(const QString& ref)
{
    QString r = ref.trimmed().toUpper();
    r.remove(QLatin1Char(' '));
    static const QRegularExpression kForm(
        QStringLiteral("^[A-Z0-9]{1,4}/[A-Z0-9]{2,3}-[0-9]{3}$"));
    return kForm.match(r).hasMatch() ? r : QString();
}

QUrl SotaGipfelVerzeichnis::anfrageUrl(const QString& ref)
{
    const QString r = normalisiere(ref);
    if (r.isEmpty()) { return QUrl(); }
    const int schnitt = r.indexOf(QLatin1Char('/'));
    return QUrl(QStringLiteral("https://api2.sota.org.uk/api/summits/%1/%2")
                    .arg(r.left(schnitt), r.mid(schnitt + 1)));
}

bool SotaGipfelVerzeichnis::leseAntwort(const QByteArray& json, SotaGipfel& aus)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) { return false; }
    const QJsonObject o = doc.object();
    const QString ref = normalisiere(o.value(QStringLiteral("summitCode")).toString());
    const QJsonValue la = o.value(QStringLiteral("latitude"));
    const QJsonValue lo = o.value(QStringLiteral("longitude"));
    if (ref.isEmpty() || !la.isDouble() || !lo.isDouble()) { return false; }
    const double lat = la.toDouble(), lon = lo.toDouble();
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) { return false; }
    aus = SotaGipfel{};
    aus.ref = ref;
    aus.name = o.value(QStringLiteral("name")).toString().trimmed();
    aus.lat = lat;
    aus.lon = lon;
    aus.altM = o.value(QStringLiteral("altM")).toInt();
    return true;
}

void SotaGipfelVerzeichnis::setAbruf(HttpAbruf abruf)
{
    m_abruf = std::move(abruf);
}

void SotaGipfelVerzeichnis::setTakt(int ms)
{
    m_taktMs = std::max(0, ms);
}

bool SotaGipfelVerzeichnis::finde(const QString& ref, SotaGipfel& aus) const
{
    const QString r = normalisiere(ref);
    if (r.isEmpty()) { return false; }
    const auto it = m_gipfel.constFind(r);
    if (it == m_gipfel.constEnd()) { return false; }
    aus = it.value();
    return true;
}

bool SotaGipfelVerzeichnis::istUnbekannt(const QString& ref) const
{
    return m_unbekannt.contains(normalisiere(ref));
}

bool SotaGipfelVerzeichnis::wartet(const QString& ref) const
{
    const QString r = normalisiere(ref);
    return !r.isEmpty() && (m_unterwegs == r || m_warteschlange.contains(r));
}

void SotaGipfelVerzeichnis::anfragen(const QString& ref)
{
    const QString r = normalisiere(ref);
    if (r.isEmpty() || m_gipfel.contains(r) || m_unbekannt.contains(r)
        || m_gefragt.contains(r)) {
        return;
    }
    m_gefragt.insert(r);
    m_warteschlange.append(r);
    if (!m_laeuft && !m_takt.isActive()) { naechste(); }
}

void SotaGipfelVerzeichnis::anfragenFuer(const QVector<LogEntry>& log)
{
    for (const LogEntry& e : log) {
        if (!e.mySotaRef.trimmed().isEmpty()) { anfragen(e.mySotaRef); }
        if (!e.sotaRef.trimmed().isEmpty())   { anfragen(e.sotaRef); }
    }
}

void SotaGipfelVerzeichnis::naechste()
{
    if (m_laeuft || m_warteschlange.isEmpty()) { return; }
    const QString ref = m_warteschlange.takeFirst();
    m_laeuft = true;
    m_unterwegs = ref;
    ++m_gestellt;
    qCInfo(lcSpots) << "SOTA summit lookup" << ref;
    QPointer<SotaGipfelVerzeichnis> selbst(this);
    m_abruf(anfrageUrl(ref), [selbst, ref](const HttpAntwort& a) {
        if (selbst) { selbst->beantwortet(ref, a); }
    });
}

void SotaGipfelVerzeichnis::beantwortet(const QString& ref, const HttpAntwort& a)
{
    m_laeuft = false;
    m_unterwegs.clear();
    SotaGipfel g;
    if (a.netzFehler.isEmpty() && a.status == 200 && leseAntwort(a.body, g)) {
        // Die API kann eine Referenz umschreiben; gespeichert wird unter
        // der gefragten, damit finde() mit dem Wert aus dem Log trifft.
        g.ref = ref;
        m_gipfel.insert(ref, g);
        speichern();
        emit gipfelBekannt(ref);
    } else if (a.netzFehler.isEmpty() && (a.status == 400 || a.status == 404)) {
        // "Not found" ist eine Antwort -- der Gipfel wird nicht wieder
        // gefragt, auch nicht nach einem Neustart.
        m_unbekannt.insert(ref);
        speichern();
        emit gipfelFehler(ref, QStringLiteral("%1 is not in the SOTA summit list").arg(ref));
    } else {
        const QString text = !a.netzFehler.isEmpty()
            ? a.netzFehler
            : QStringLiteral("HTTP %1").arg(a.status);
        qCWarning(lcSpots) << "SOTA summit lookup failed" << ref << text;
        emit gipfelFehler(ref, QStringLiteral("Couldn't look up %1: %2").arg(ref, text));
    }
    if (!m_warteschlange.isEmpty()) { m_takt.start(m_taktMs); }
}

void SotaGipfelVerzeichnis::laden()
{
    QFile f(m_pfad);
    if (!f.open(QIODevice::ReadOnly)) { return; }
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    const QJsonObject gipfel = o.value(QStringLiteral("summits")).toObject();
    for (auto it = gipfel.constBegin(); it != gipfel.constEnd(); ++it) {
        const QString ref = normalisiere(it.key());
        const QJsonObject g = it.value().toObject();
        if (ref.isEmpty() || !g.value(QStringLiteral("lat")).isDouble()) { continue; }
        SotaGipfel s;
        s.ref = ref;
        s.name = g.value(QStringLiteral("name")).toString();
        s.lat = g.value(QStringLiteral("lat")).toDouble();
        s.lon = g.value(QStringLiteral("lon")).toDouble();
        s.altM = g.value(QStringLiteral("altM")).toInt();
        m_gipfel.insert(ref, s);
    }
    for (const QJsonValue& v : o.value(QStringLiteral("unknown")).toArray()) {
        const QString ref = normalisiere(v.toString());
        if (!ref.isEmpty()) { m_unbekannt.insert(ref); }
    }
}

void SotaGipfelVerzeichnis::speichern() const
{
    QJsonObject gipfel;
    for (auto it = m_gipfel.constBegin(); it != m_gipfel.constEnd(); ++it) {
        QJsonObject g;
        g.insert(QStringLiteral("name"), it.value().name);
        g.insert(QStringLiteral("lat"), it.value().lat);
        g.insert(QStringLiteral("lon"), it.value().lon);
        g.insert(QStringLiteral("altM"), it.value().altM);
        gipfel.insert(it.key(), g);
    }
    QStringList unbekannt(m_unbekannt.cbegin(), m_unbekannt.cend());
    unbekannt.sort();
    QJsonObject o;
    o.insert(QStringLiteral("source"), QStringLiteral("api2.sota.org.uk/api/summits"));
    o.insert(QStringLiteral("summits"), gipfel);
    o.insert(QStringLiteral("unknown"), QJsonArray::fromStringList(unbekannt));

    QDir().mkpath(QFileInfo(m_pfad).absolutePath());
    QSaveFile f(m_pfad);
    if (!f.open(QIODevice::WriteOnly)) {
        qCWarning(lcSpots) << "SOTA summit cache not writable" << m_pfad;
        return;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    f.commit();
}

bool SotaGipfelVerzeichnis::eigenerGipfel(const LogEntry& e, SotaGipfel& aus) const
{
    return finde(e.mySotaRef, aus);
}

bool SotaGipfelVerzeichnis::gegenGipfel(const LogEntry& e, SotaGipfel& aus) const
{
    return finde(e.sotaRef, aus);
}

SotaGipfelVerzeichnis::Ende
SotaGipfelVerzeichnis::eigenesEnde(const LogEntry& e, const QString& ersatzLocator) const
{
    Ende aus;
    if (eigenerGipfel(e, aus.g)) {
        aus.gueltig = true;
        aus.gipfel = true;
        aus.lat = aus.g.lat;
        aus.lon = aus.g.lon;
        return aus;
    }
    QString loc = e.myGridSquare.trimmed();
    if (!isValidGridSquare(loc)) { loc = ersatzLocator.trimmed(); }
    if (isValidGridSquare(loc)) {
        aus.gueltig = true;
        aus.locator = loc.toUpper();
        calculateLatLonFromGridSquare(loc, aus.lat, aus.lon);
    }
    return aus;
}

SotaGipfelVerzeichnis::Ende SotaGipfelVerzeichnis::gegenEnde(const LogEntry& e) const
{
    Ende aus;
    if (gegenGipfel(e, aus.g)) {
        aus.gueltig = true;
        aus.gipfel = true;
        aus.lat = aus.g.lat;
        aus.lon = aus.g.lon;
        return aus;
    }
    const QString loc = e.gridSquare.trimmed();
    if (isValidGridSquare(loc)) {
        aus.gueltig = true;
        aus.locator = loc.toUpper();
        calculateLatLonFromGridSquare(loc, aus.lat, aus.lon);
    }
    return aus;
}

int SotaGipfelVerzeichnis::korrigiereStrecken(QVector<LogEntry>& log) const
{
    if (m_gipfel.isEmpty()) { return 0; }
    int n = 0;
    for (LogEntry& e : log) {
        if (e.mySotaRef.isEmpty() && e.sotaRef.isEmpty()) { continue; }
        const Ende a = eigenesEnde(e);
        const Ende b = gegenEnde(e);
        if (!a.gueltig || !b.gueltig || (!a.gipfel && !b.gipfel)) { continue; }
        const BeamHeading::GreatCircle g =
            BeamHeading::greatCircle(a.lat, a.lon, b.lat, b.lon);
        e.distanceKm = g.km;
        e.bearingDeg = g.bearingDeg;
        ++n;
    }
    return n;
}

} // namespace Longpath
