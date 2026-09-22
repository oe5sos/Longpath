// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/sat/SatelliteTracker.cpp  (Longpath)
// =================================================================
// Longpath-original. No Thetis port.
//
// Der Propagator ist Vallados SGP4 (third_party/sgp4, MIT via python-sgp4,
// docs/attribution/SGP4-PROVENANCE.md). Alles andere hier — TLE-Satz,
// Pruefsumme, Beobachter-Geometrie — ist nach den ueblichen Formeln
// (Vallado, Fundamentals of Astrodynamics, Kap. 3 und 4) selbst
// geschrieben:
//   * Beobachter geodaetisch → TEME ueber die Ortssternzeit
//     θ = GMST + λ und den Ellipsoid (a = 6378.135 km, WGS-72, derselbe
//     Radius wie im Propagator; f = 1/298.26).
//   * Sichtvektor ρ = r_sat − r_obs, gedreht ins SEZ-System (Sued, Ost,
//     Zenit); Elevation = asin(Z/|ρ|), Azimut = atan2(E, −S).
//   * Subsatellitenpunkt: Laenge = atan2(y, x) − GMST, geodaetische Breite
//     iterativ aus der geozentrischen.
// TEME und ECEF unterscheiden sich um Polbewegung und Nutation — fuer
// „steht der Satellit ueber dem Horizont" weit unter einem Grad.
// =================================================================
#include "core/sat/SatelliteTracker.h"

#include "SGP4.h"

#include <QStringList>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Longpath {

namespace {

constexpr double kPi        = 3.14159265358979323846;
constexpr double kDeg2Rad   = kPi / 180.0;
constexpr double kRad2Deg   = 180.0 / kPi;
constexpr double kEarthRadiusKm = 6378.135;           // WGS-72, wie SGP4 wgs72
constexpr double kFlattening    = 1.0 / 298.26;        // WGS-72
constexpr double kE2 = kFlattening * (2.0 - kFlattening);

double wrapDeg360(double d)
{
    d = std::fmod(d, 360.0);
    if (d < 0.0) { d += 360.0; }
    return d;
}

double wrapDeg180(double d)
{
    d = wrapDeg360(d);
    if (d > 180.0) { d -= 360.0; }
    return d;
}

struct Entry {
    QString  name;
    QString  catalogNumber;
    QString  line1;
    QString  line2;
    elsetrec rec{};
    bool     ok{false};
};

} // namespace

struct SatelliteTracker::Impl {
    QVector<Entry> sats;

    // Vallado's twoline2rv schreibt in die Zeilenpuffer (Leerzeichen →
    // Punkte/Nullen), deshalb Kopien in 130er-Puffern.
    static bool init(Entry& e)
    {
        char l1[130]; char l2[130];
        std::memset(l1, 0, sizeof l1);
        std::memset(l2, 0, sizeof l2);
        const QByteArray a = e.line1.toLatin1();
        const QByteArray b = e.line2.toLatin1();
        if (a.size() < 69 || b.size() < 69) { return false; }
        std::memcpy(l1, a.constData(), std::min<int>(a.size(), 129));
        std::memcpy(l2, b.constData(), std::min<int>(b.size(), 129));
        double startmfe = 0.0, stopmfe = 0.0, deltamin = 0.0;
        e.rec = elsetrec{};
        SGP4Funcs::twoline2rv(l1, l2, 'c', 'e', 'i', wgs72,
                              startmfe, stopmfe, deltamin, e.rec);
        e.ok = (e.rec.error == 0);
        return e.ok;
    }

    bool propagate(const Entry& e, double jd, double r[3], double v[3]) const
    {
        if (!e.ok) { return false; }
        elsetrec rec = e.rec;   // sgp4() schreibt Fehlerzustand in den Satz
        const double tsinceMin = (jd - (rec.jdsatepoch + rec.jdsatepochF)) * 1440.0;
        const bool ok = SGP4Funcs::sgp4(rec, tsinceMin, r, v);
        if (!ok || rec.error != 0) { return false; }
        if (!std::isfinite(r[0]) || !std::isfinite(r[1]) || !std::isfinite(r[2])) { return false; }
        return true;
    }

    bool sightOf(const Entry& e, double jd, const Observer& obs, SatelliteSight* out) const
    {
        double r[3], v[3];
        if (!propagate(e, jd, r, v)) { return false; }
        SatelliteSight s;
        s.name = e.name;
        s.catalogNumber = e.catalogNumber;
        topocentric(r, obs, jd, &s.azimuthDeg, &s.elevationDeg, &s.rangeKm);
        subPoint(r, jd, &s.subLatDeg, &s.subLonDeg, &s.altitudeKm);
        if (out) { *out = s; }
        return true;
    }
};

SatelliteTracker::SatelliteTracker() : d(std::make_unique<Impl>()) {}
SatelliteTracker::~SatelliteTracker() = default;
SatelliteTracker::SatelliteTracker(SatelliteTracker&&) noexcept = default;
SatelliteTracker& SatelliteTracker::operator=(SatelliteTracker&&) noexcept = default;

bool SatelliteTracker::checksumOk(const QString& line)
{
    if (line.size() < 69) { return false; }
    int sum = 0;
    for (int i = 0; i < 68; ++i) {
        const QChar c = line.at(i);
        if (c.isDigit())    { sum += c.digitValue(); }
        else if (c == '-')  { sum += 1; }
    }
    return (sum % 10) == line.at(68).digitValue();
}

int SatelliteTracker::loadTle(const QString& text, int* rejected)
{
    QVector<Entry> sats;
    int bad = 0;
    const QStringList raw = text.split(QLatin1Char('\n'));
    QStringList lines;
    for (QString l : raw) {
        l = l.trimmed();
        if (!l.isEmpty()) { lines << l; }
    }
    QString pendingName;
    for (int i = 0; i < lines.size(); ++i) {
        const QString& l = lines.at(i);
        if (l.startsWith(QLatin1String("1 ")) && i + 1 < lines.size()
            && lines.at(i + 1).startsWith(QLatin1String("2 "))) {
            Entry e;
            e.line1 = l;
            e.line2 = lines.at(i + 1);
            e.catalogNumber = e.line1.mid(2, 5).trimmed();
            e.name = pendingName.isEmpty()
                         ? QStringLiteral("NORAD %1").arg(e.catalogNumber)
                         : pendingName;
            pendingName.clear();
            ++i;
            if (!checksumOk(e.line1) || !checksumOk(e.line2) || !Impl::init(e)) {
                ++bad;
                continue;
            }
            sats.push_back(std::move(e));
        } else if (l.startsWith(QLatin1String("2 "))) {
            ++bad;   // Zeile 2 ohne Zeile 1
        } else {
            pendingName = l;
        }
    }
    d->sats = std::move(sats);
    if (rejected) { *rejected = bad; }
    return d->sats.size();
}

int SatelliteTracker::count() const { return d->sats.size(); }

QStringList SatelliteTracker::names() const
{
    QStringList out;
    for (const Entry& e : d->sats) { out << e.name; }
    return out;
}

QDateTime SatelliteTracker::oldestEpoch() const
{
    double oldest = 0.0;
    for (const Entry& e : d->sats) {
        const double jd = e.rec.jdsatepoch + e.rec.jdsatepochF;
        if (oldest == 0.0 || jd < oldest) { oldest = jd; }
    }
    if (oldest == 0.0) { return {}; }
    // JD 2440587.5 = 1970-01-01T00:00Z
    const qint64 ms = qRound64((oldest - 2440587.5) * 86400.0 * 1000.0);
    // QTimeZone::utc(), nicht QTimeZone::UTC: das Enum gibt es erst ab
    // Qt 6.5, das Linux-aarch64-Paket baut mit Ubuntus Qt 6.4.2
    // (release.yml; 0.6.4-Lauf 2026-09-22 brach genau hier ab).
    return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::utc());
}

double SatelliteTracker::julianDate(const QDateTime& utcIn)
{
    const QDateTime utc = utcIn.toUTC();
    const QDate dt = utc.date();
    const QTime tm = utc.time();
    double jd = 0.0, jdFrac = 0.0;
    SGP4Funcs::jday_SGP4(dt.year(), dt.month(), dt.day(),
                         tm.hour(), tm.minute(),
                         tm.second() + tm.msec() / 1000.0, jd, jdFrac);
    return jd + jdFrac;
}

double SatelliteTracker::gmstRad(double jd)
{
    return SGP4Funcs::gstime_SGP4(jd);
}

void SatelliteTracker::observerTeme(const Observer& obs, double jd, double out[3])
{
    const double lat = obs.latDeg * kDeg2Rad;
    const double theta = gmstRad(jd) + obs.lonDeg * kDeg2Rad;   // Ortssternzeit
    const double sinLat = std::sin(lat);
    const double n = kEarthRadiusKm / std::sqrt(1.0 - kE2 * sinLat * sinLat);
    const double h = obs.altitudeM / 1000.0;
    out[0] = (n + h) * std::cos(lat) * std::cos(theta);
    out[1] = (n + h) * std::cos(lat) * std::sin(theta);
    out[2] = (n * (1.0 - kE2) + h) * sinLat;
}

void SatelliteTracker::topocentric(const double rSatKm[3], const Observer& obs, double jd,
                                   double* azDeg, double* elDeg, double* rangeKm)
{
    double rObs[3];
    observerTeme(obs, jd, rObs);
    const double rho[3] = { rSatKm[0] - rObs[0], rSatKm[1] - rObs[1], rSatKm[2] - rObs[2] };
    const double lat = obs.latDeg * kDeg2Rad;
    const double theta = gmstRad(jd) + obs.lonDeg * kDeg2Rad;
    const double sl = std::sin(lat), cl = std::cos(lat);
    const double st = std::sin(theta), ct = std::cos(theta);
    const double s =  sl * ct * rho[0] + sl * st * rho[1] - cl * rho[2];
    const double e = -st * rho[0] + ct * rho[1];
    const double z =  cl * ct * rho[0] + cl * st * rho[1] + sl * rho[2];
    const double range = std::sqrt(rho[0] * rho[0] + rho[1] * rho[1] + rho[2] * rho[2]);
    if (rangeKm) { *rangeKm = range; }
    if (elDeg)   { *elDeg = (range > 0.0) ? std::asin(std::clamp(z / range, -1.0, 1.0)) * kRad2Deg : 0.0; }
    if (azDeg)   { *azDeg = wrapDeg360(std::atan2(e, -s) * kRad2Deg); }
}

void SatelliteTracker::subPoint(const double r[3], double jd,
                                double* latDeg, double* lonDeg, double* altKm)
{
    const double rxy = std::sqrt(r[0] * r[0] + r[1] * r[1]);
    double lon = std::atan2(r[1], r[0]) - gmstRad(jd);
    // Geodaetische Breite iterativ (Vallado, Algorithmus 12).
    double lat = std::atan2(r[2], rxy);
    double c = 1.0;
    for (int i = 0; i < 8; ++i) {
        const double sl = std::sin(lat);
        c = 1.0 / std::sqrt(1.0 - kE2 * sl * sl);
        lat = std::atan2(r[2] + kEarthRadiusKm * c * kE2 * sl, rxy);
    }
    const double alt = rxy / std::cos(lat) - kEarthRadiusKm * c;
    if (latDeg) { *latDeg = lat * kRad2Deg; }
    if (lonDeg) { *lonDeg = wrapDeg180(lon * kRad2Deg); }
    if (altKm)  { *altKm = alt; }
}

QVector<SatelliteSight> SatelliteTracker::inView(const QDateTime& utc, const Observer& obs,
                                                 double minElevationDeg) const
{
    QVector<SatelliteSight> out;
    const double jd = julianDate(utc);
    for (const Entry& e : d->sats) {
        SatelliteSight s;
        if (!d->sightOf(e, jd, obs, &s)) { continue; }
        if (s.elevationDeg >= minElevationDeg) { out.push_back(s); }
    }
    std::sort(out.begin(), out.end(), [](const SatelliteSight& a, const SatelliteSight& b) {
        return a.elevationDeg > b.elevationDeg;
    });
    return out;
}

bool SatelliteTracker::sight(const QString& name, const QDateTime& utc, const Observer& obs,
                             SatelliteSight* out) const
{
    for (const Entry& e : d->sats) {
        if (e.name == name) { return d->sightOf(e, julianDate(utc), obs, out); }
    }
    return false;
}

} // namespace Longpath
