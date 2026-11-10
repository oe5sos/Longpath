// no-port-check: Longpath-original, no Thetis logic.
//
// src/core/Hoehenprofil.cpp  (Longpath) -- siehe Hoehenprofil.h.
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#include "core/Hoehenprofil.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace Longpath::Hoehenprofil {

namespace {

constexpr double kGradZuRad = M_PI / 180.0;

double winkelAbstand(double p1, double l1, double p2, double l2)
{
    const double a = std::sin((p2 - p1) / 2.0);
    const double b = std::sin((l2 - l1) / 2.0);
    const double h = a * a + std::cos(p1) * std::cos(p2) * b * b;
    return 2.0 * std::asin(std::sqrt(std::clamp(h, 0.0, 1.0)));
}

double sauber(double v)
{
    return std::isfinite(v) ? v : 0.0;
}

} // namespace

QVector<Punkt> grosskreisPunkte(double lat1, double lon1,
                                double lat2, double lon2, int n)
{
    QVector<Punkt> aus;
    if (n < 2) { return aus; }
    aus.reserve(n);
    const double p1 = lat1 * kGradZuRad, l1 = lon1 * kGradZuRad;
    const double p2 = lat2 * kGradZuRad, l2 = lon2 * kGradZuRad;
    const double d = winkelAbstand(p1, l1, p2, l2);
    for (int i = 0; i < n; ++i) {
        const double f = static_cast<double>(i) / (n - 1);
        if (d < 1e-12) {
            aus.push_back({lat1, lon1});
            continue;
        }
        // Kugel-Interpolation (slerp) auf dem Einheitsvektor -- dieselbe
        // wie zwischen() im Entwurf profil.py.
        const double a = std::sin((1.0 - f) * d) / std::sin(d);
        const double b = std::sin(f * d) / std::sin(d);
        const double x = a * std::cos(p1) * std::cos(l1) + b * std::cos(p2) * std::cos(l2);
        const double y = a * std::cos(p1) * std::sin(l1) + b * std::cos(p2) * std::sin(l2);
        const double z = a * std::sin(p1) + b * std::sin(p2);
        aus.push_back({std::atan2(z, std::hypot(x, y)) / kGradZuRad,
                       std::atan2(y, x) / kGradZuRad});
    }
    return aus;
}

double grosskreisKm(double lat1, double lon1, double lat2, double lon2)
{
    return winkelAbstand(lat1 * kGradZuRad, lon1 * kGradZuRad,
                         lat2 * kGradZuRad, lon2 * kGradZuRad) * kErdradiusKm;
}

int punktzahlFuer(double km)
{
    // Ein Punkt je Kilometer zeigt einen Grat, den drei Kilometer
    // ueberspringen (der Entwurf hatte 100 Punkte auf 344 km). Mehr als
    // drei Abrufe je Weg sind es nicht wert: OpenTopoData erlaubt einen
    // je Sekunde und 1000 am Tag.
    const int n = static_cast<int>(std::lround(km)) + 1;
    return std::clamp(n, 100, 300);
}

double ausbuchtungM(double dKm, double gesamtKm, double k)
{
    return dKm * (gesamtKm - dKm) / (2.0 * k * kErdradiusKm) * 1000.0;
}

double wellenlaengeM(double freqMHz)
{
    return freqMHz > 0.0 ? kC / freqMHz : 0.0;
}

double fresnelRadiusM(double d1Km, double d2Km, double freqMHz)
{
    const double ges = d1Km + d2Km;
    if (ges <= 0.0 || freqMHz <= 0.0) { return 0.0; }
    const double v = wellenlaengeM(freqMHz) * (d1Km * 1000.0) * (d2Km * 1000.0)
                     / (ges * 1000.0);
    return v > 0.0 ? std::sqrt(v) : 0.0;
}

double profilFrequenzMHz(const QString& band, double freqMHz)
{
    struct Bereich { const char* name; double von, bis, typisch; };
    // Typisch: die SSB-Anrufe der Baender (IARU R1), wo der Betrieb ist.
    static const Bereich kBereiche[] = {
        {"2m",   144.0,  148.0,  144.300},
        {"70cm", 420.0,  450.0,  432.200},
        {"23cm", 1240.0, 1300.0, 1296.200},
    };
    const QString b = band.trimmed().toLower();
    for (const Bereich& r : kBereiche) {
        if (freqMHz >= r.von && freqMHz <= r.bis) { return freqMHz; }
    }
    for (const Bereich& r : kBereiche) {
        if (b == QLatin1String(r.name)) { return r.typisch; }
    }
    return 0.0;
}

bool istUkwBand(const QString& band, double freqMHz)
{
    return profilFrequenzMHz(band, freqMHz) > 0.0;
}

QString locatorGenauigkeit(const QString& locator)
{
    // Halbe Kantenlaenge, gerundet -- dieselben Stufen wie die
    // Lageangabe in der Stationskarte.
    const int n = locator.trimmed().size();
    if (n >= 8) { return QStringLiteral("±300 m"); }
    if (n >= 6) { return QStringLiteral("±3 km"); }
    return QStringLiteral("±60 km");
}

double locatorUnsicherheitKm(const QString& locator)
{
    const int n = locator.trimmed().size();
    if (n >= 8) { return 0.3; }
    if (n >= 6) { return 3.0; }
    return 60.0;
}

Ergebnis rechne(const Eingabe& ein)
{
    Ergebnis e;
    const int n = ein.distKm.size();
    if (n < 3 || ein.gelaendeM.size() != n) { return e; }
    const double D = ein.distKm.last();
    if (!(D > 0.0)) { return e; }

    e.gesamtKm = D;
    e.ausbuchtung.resize(n);
    e.gelaende.resize(n);
    e.sicht.resize(n);
    e.fresnel.resize(n);
    e.inZone.resize(n);
    e.verdeckt.resize(n);
    e.unsicher.resize(n);
    e.unsicherAKm = std::clamp(ein.unsicherAKm, 0.0, 0.25 * D);
    e.unsicherBKm = std::clamp(ein.unsicherBKm, 0.0, 0.25 * D);

    const double h0 = sauber(ein.gelaendeM.first());
    const double h1 = sauber(ein.gelaendeM.last());
    e.hoeheA = h0 + ein.antenneA;
    e.hoeheB = h1 + ein.antenneB;

    int inZone = 0;
    e.maxUeberLinieM = -1e9;
    for (int i = 0; i < n; ++i) {
        const double d = ein.distKm.at(i);
        e.ausbuchtung[i] = ausbuchtungM(d, D, ein.k);
        e.gelaende[i] = sauber(ein.gelaendeM.at(i)) + e.ausbuchtung[i];
        e.sicht[i] = e.hoeheA + (e.hoeheB - e.hoeheA) * d / D;
        e.fresnel[i] = fresnelRadiusM(d, D - d, ein.freqMHz);
        e.unsicher[i] = (i > 0 && d < e.unsicherAKm)
                        || (i < n - 1 && D - d < e.unsicherBKm);
        if (e.unsicher[i]) {
            e.inZone[i] = false;
            e.verdeckt[i] = false;
            continue;
        }
        e.inZone[i] = e.gelaende[i] > e.sicht[i] - kZonenAnteil * e.fresnel[i];
        e.verdeckt[i] = e.gelaende[i] > e.sicht[i];
        if (e.inZone[i]) { ++inZone; }
        const double ueber = e.gelaende[i] - e.sicht[i];
        if (ueber > e.maxUeberLinieM) {
            e.maxUeberLinieM = ueber;
            e.maxUeberLinieIndex = i;
        }
    }
    e.anteilInZone = static_cast<double>(inZone) / n;
    const bool verdeckt = std::any_of(e.verdeckt.cbegin(), e.verdeckt.cend(),
                                      [](bool b) { return b; });

    if (verdeckt) {
        // ── Horizontstrahlen ────────────────────────────────────────
        // Von jeder Antennenspitze die steilste Gerade, die das Gelaende
        // gerade noch beruehrt. Wo beide sich schneiden, sehen beide
        // Stationen denselben Luftraum -- das gemeinsame Volumen.
        double sA = -1e18, sB = -1e18;
        for (int j = 1; j < n; ++j) {
            if (e.unsicher.at(j)) { continue; }
            const double s = (e.gelaende.at(j) - e.hoeheA) / ein.distKm.at(j);
            if (s > sA) { sA = s; e.tangenteA = j; }
        }
        for (int j = 0; j < n - 1; ++j) {
            if (e.unsicher.at(j)) { continue; }
            const double s = (e.gelaende.at(j) - e.hoeheB) / (D - ein.distKm.at(j));
            if (s > sB) { sB = s; e.tangenteB = j; }
        }
        if (sA + sB > 0.0) {
            e.hatHorizont = true;
            e.steigungA = sA;
            e.steigungB = sB;
            e.schnittKm = (e.hoeheB + sB * D - e.hoeheA) / (sA + sB);
            e.schnittHoeheM = e.hoeheA + sA * e.schnittKm;
            // Gelaende unter dem Schnittpunkt, linear zwischen den
            // Nachbarpunkten (wie np.interp im Entwurf).
            const auto it = std::lower_bound(ein.distKm.cbegin(), ein.distKm.cend(),
                                             e.schnittKm);
            int j = static_cast<int>(it - ein.distKm.cbegin());
            j = std::clamp(j, 1, n - 1);
            const double x0 = ein.distKm.at(j - 1), x1 = ein.distKm.at(j);
            const double t = x1 > x0 ? std::clamp((e.schnittKm - x0) / (x1 - x0), 0.0, 1.0)
                                     : 0.0;
            const double grund = e.gelaende.at(j - 1)
                                 + t * (e.gelaende.at(j) - e.gelaende.at(j - 1));
            e.schnittUeberGrundM = e.schnittHoeheM - grund;
            // Steigung in m je km -> Winkel: atan(s / 1000).
            e.streuwinkelGrad = (std::atan(sA / 1000.0) + std::atan(sB / 1000.0))
                                / kGradZuRad;
        }
    }

    if (verdeckt && e.maxUeberLinieM < kFastSichtM) {
        e.urteil = Urteil::FastSicht;
    } else if (verdeckt) {
        e.urteil = Urteil::KeineSicht;
    } else if (inZone > 0) {
        e.urteil = Urteil::FresnelAngeschnitten;
    } else {
        e.urteil = Urteil::FreieSicht;
    }
    e.gueltig = true;
    return e;
}

QString urteilKurz(Urteil u)
{
    switch (u) {
    case Urteil::FreieSicht:           return QStringLiteral("Clear line of sight");
    case Urteil::FresnelAngeschnitten: return QStringLiteral("Line of sight, Fresnel zone clipped");
    case Urteil::FastSicht:            return QStringLiteral("Near line of sight");
    case Urteil::KeineSicht:           return QStringLiteral("No line of sight");
    }
    return QString();
}

QString urteilText(const Ergebnis& e, const QString& genauigkeit)
{
    if (!e.gueltig) { return QString(); }
    switch (e.urteil) {
    case Urteil::FreieSicht:
        return QStringLiteral("Clear line of sight — 60 % of the Fresnel zone stays free");
    case Urteil::FresnelAngeschnitten:
        return QStringLiteral("Line of sight, but terrain clips the Fresnel zone");
    case Urteil::FastSicht: {
        QString s = QStringLiteral("Near line of sight — terrain only %1 m above the line")
                        .arg(e.maxUeberLinieM, 0, 'f', 0);
        if (!genauigkeit.isEmpty()) {
            s += QStringLiteral(" (other station %1)").arg(genauigkeit);
        }
        return s;
    }
    case Urteil::KeineSicht:
        return QStringLiteral("No line of sight — terrain rises up to %1 m above the line: "
                              "diffraction or tropo")
            .arg(e.maxUeberLinieM, 0, 'f', 0);
    }
    return QString();
}

} // namespace Longpath::Hoehenprofil
