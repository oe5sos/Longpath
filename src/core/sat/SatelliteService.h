// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/sat/SatelliteService.h  (Longpath)
// =================================================================
// Longpath-original. No Thetis port.
//
// Ein Objekt fuer das ganze Programm: haelt den TLE-Speicher (TleStore)
// und den Rechner (SatelliteTracker) zusammen, kennt den Standort des
// Betreibers (Locator aus den Einstellungen, oder einer, den der
// Aufrufer mitgibt) und liefert zwei Dinge:
//
//   * inView(): welche Satelliten JETZT (oder zu einem Zeitpunkt) ueber
//     dem Horizont stehen — fuer die Karte und fuer den Stempel.
//   * stamp(): der Text, der beim Loggen in den Eintrag kommt, ADIF-Feld
//     APP_LONGPATH_SATS. Zum Zeitpunkt des QSO gerechnet, mit den
//     Bahndaten von damals — deshalb wird beim Loggen gestempelt, nicht
//     spaeter nachgerechnet (ein TLE von heute taugt nicht fuer ein QSO
//     von vor drei Jahren).
// =================================================================
// Modification history (Longpath):
//   2026-09-21 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================
#pragma once

#include "core/sat/SatelliteTracker.h"
#include "core/sat/TleStore.h"

#include <QObject>
#include <QTimer>

namespace Longpath {

class SatelliteService : public QObject {
    Q_OBJECT
public:
    explicit SatelliteService(QObject* parent = nullptr);

    // Datei lesen, bei Bedarf vom Netz holen, danach einmal am Tag.
    void start();
    // Ohne Netz (Pruefstaende): TLE-Text direkt setzen.
    void setTleText(const QString& text);

    const SatelliteTracker& tracker() const { return m_tracker; }
    TleStore&               store()         { return m_store; }
    bool hasCatalog() const { return !m_tracker.isEmpty(); }

    // Standort aus einem Locator; leer → Einstellung StationGridSquare.
    // false, wenn kein brauchbarer Locator da ist.
    bool observerFor(const QString& grid, Observer* out) const;

    QVector<SatelliteSight> inView(const QDateTime& utc, const QString& grid = QString(),
                                   double minElevationDeg = 0.0) const;

    // ADIF-Feldname des Stempels und sein Text („ISS (ZARYA) el 34° az 210°
    // · AO-91 el 12° az 88°"); leer, wenn nichts in Sicht oder kein
    // Standort.
    static QString stampField() { return QStringLiteral("APP_LONGPATH_SATS"); }
    static QString describe(const QVector<SatelliteSight>& sights, int maxCount = 8);
    QString stamp(const QDateTime& utc, const QString& grid = QString()) const;

signals:
    void catalogChanged();

private:
    void adopt(const QString& text);

    TleStore         m_store;
    SatelliteTracker m_tracker;
    QTimer           m_daily;
};

} // namespace Longpath
