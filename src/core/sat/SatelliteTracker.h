// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/sat/SatelliteTracker.h  (Longpath)
// =================================================================
// Longpath-original. No Thetis port.
//
// Welche Satelliten stehen gerade ueber dem Horizont? Ein Satz
// Zwei-Zeilen-Elemente (TLE, z. B. die Amateurfunk-Gruppe von CelesTrak)
// wird mit Vallados SGP4/SDP4 (third_party/sgp4, siehe
// docs/attribution/SGP4-PROVENANCE.md) auf einen Zeitpunkt gerechnet;
// die Beobachtergeometrie (geodaetisch → TEME, SEZ-Drehung, Azimut/
// Elevation/Entfernung, Subsatellitenpunkt) steht hier.
//
// Der Tracker rechnet, er holt nichts: die TLE-Zeilen kommen von
// TleStore (Datei/Netz) oder direkt aus einem String (Pruefstaende).
// =================================================================
#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

#include <memory>

namespace Longpath {

// Ein Satellit, wie der Beobachter ihn zu einem Zeitpunkt sieht.
struct SatelliteSight {
    QString name;          // TLE-Titelzeile, getrimmt ("ISS (ZARYA)")
    QString catalogNumber; // NORAD-Nummer als Text ("25544")
    double  azimuthDeg{0.0};    // 0..360, von Nord ueber Ost
    double  elevationDeg{0.0};  // ueber dem Horizont positiv
    double  rangeKm{0.0};       // Schraegentfernung
    double  subLatDeg{0.0};     // Subsatellitenpunkt (geodaetisch)
    double  subLonDeg{0.0};     // -180..180
    double  altitudeKm{0.0};    // Hoehe ueber dem Ellipsoid (WGS-72-Radius)
};

// Beobachterstandort, geodaetisch.
struct Observer {
    double latDeg{0.0};
    double lonDeg{0.0};
    double altitudeM{0.0};
};

class SatelliteTracker {
public:
    SatelliteTracker();
    ~SatelliteTracker();
    SatelliteTracker(SatelliteTracker&&) noexcept;
    SatelliteTracker& operator=(SatelliteTracker&&) noexcept;
    SatelliteTracker(const SatelliteTracker&) = delete;
    SatelliteTracker& operator=(const SatelliteTracker&) = delete;

    // Liest einen TLE-Text (Dreizeiler: Name, Zeile 1, Zeile 2; auch
    // Zweizeiler ohne Namen) und ersetzt den Satz. Zeilen mit falscher
    // Pruefsumme oder unbrauchbaren Elementen werden ausgelassen und
    // gezaehlt. Liefert die Zahl der uebernommenen Satelliten.
    int loadTle(const QString& text, int* rejected = nullptr);

    int  count() const;
    bool isEmpty() const { return count() == 0; }
    QStringList names() const;

    // Aeltestes Epochendatum im Satz — sagt, wie frisch die Bahndaten sind.
    QDateTime oldestEpoch() const;

    // Alle Satelliten des Satzes zum Zeitpunkt `utc`, wie `obs` sie sieht.
    // `minElevationDeg` filtert (0 = ueber dem geometrischen Horizont;
    // negativ = alle, auch unter dem Horizont). Sortiert nach Elevation,
    // hoechster zuerst.
    QVector<SatelliteSight> inView(const QDateTime& utc, const Observer& obs,
                                   double minElevationDeg = 0.0) const;

    // Ein einzelner Satellit (Name wie in names()). false, wenn unbekannt
    // oder der Propagator fuer diesen Zeitpunkt scheitert.
    bool sight(const QString& name, const QDateTime& utc, const Observer& obs,
               SatelliteSight* out) const;

    // ── Bausteine, oeffentlich fuer die Pruefstaende ─────────────────
    // Julianisches Datum (UT1 ≈ UTC) einer Zeit.
    static double julianDate(const QDateTime& utc);
    // Mittlere Sternzeit Greenwich in Radiant fuer ein JD.
    static double gmstRad(double jd);
    // TLE-Pruefsumme (Modulo 10 ueber Ziffern, '-' zaehlt 1).
    static bool checksumOk(const QString& line);
    // Beobachter in TEME-Koordinaten (km) fuer ein JD.
    static void observerTeme(const Observer& obs, double jd, double out[3]);
    // TEME-Ortsvektor → Beobachtersicht (Az/El/Entfernung) fuer ein JD.
    static void topocentric(const double rSatKm[3], const Observer& obs, double jd,
                            double* azDeg, double* elDeg, double* rangeKm);
    // TEME-Ortsvektor → Subsatellitenpunkt.
    static void subPoint(const double rSatKm[3], double jd,
                         double* latDeg, double* lonDeg, double* altKm);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace Longpath
