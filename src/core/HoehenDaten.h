// no-port-check: Longpath-original, no Thetis logic.
//
// =================================================================
// src/core/HoehenDaten.h  (Longpath)
// =================================================================
//
// ── Gelaendehoehen fuer das UKW-Hoehenprofil (2026-11-11) ───────────
//
// Quelle: OpenTopoData, oeffentliche API, vom Betreiber am 2026-11-09
// freigegeben:
//
//   GET https://api.opentopodata.org/v1/eudem25m,srtm90m
//           ?locations=lat,lon|lat,lon|...
//   -> {"status":"OK","results":[{"elevation":874.06,...}, ...]}
//
// EU-DEM mit 25 m, wo es es gibt, sonst SRTM mit 90 m. Die Grenzen der
// oeffentlichen API: 100 Punkte je Abruf, ein Abruf je Sekunde, 1000 am
// Tag. Ein Weg mit 300 Punkten sind darum drei Abrufe, eine Sekunde
// auseinander -- auch ueber mehrere Wege und Fenster hinweg, der Takt
// gilt fuer das ganze Programm.
//
// Jeder Weg wird einmal geholt und liegt dann in
// <dataDir>/hoehenprofile/<schluessel>.json; der Schluessel sind die
// beiden Enden (auf 1e-5 Grad, ~1 m) und die Punktzahl. Das Rufzeichen
// geht nicht mit hinaus -- nur Koordinaten, und ein neutraler
// User-Agent.
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#pragma once

#include "core/Hoehenprofil.h"
#include "core/HttpAbruf.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

namespace Longpath {

class HoehenAbruf : public QObject {
    Q_OBJECT
public:
    static constexpr int kPunkteJeAbruf = 100;
    static constexpr int kAbstandMs = 1100;   // > 1 s, mit Rand

    struct Weg {
        double lat1{0.0}, lon1{0.0}, lat2{0.0}, lon2{0.0};
        int    punkte{100};
    };

    explicit HoehenAbruf(QObject* parent = nullptr);

    /// Vorgabe: <dataDir>/hoehenprofile
    void setCacheOrdner(const QString& ordner);
    QString cacheOrdner() const { return m_ordner; }
    void setAbruf(HttpAbruf abruf);

    static QString schluessel(const Weg& w);
    static QUrl anfrageUrl(const QVector<Hoehenprofil::Punkt>& punkte);
    /// Eine Antwort lesen. Fehlende Werte (Meer, Luecke) als NaN.
    static bool leseAntwort(const QByteArray& json, QVector<double>& aus,
                            QString* fehler = nullptr);

    /// Aus dem Cache, ohne Netz.
    bool ausCache(const Weg& w, QVector<double>& aus) const;
    /// Einen Cache-Eintrag von aussen anlegen (Pruefstaende, Werkzeuge).
    bool inCacheLegen(const Weg& w, const QVector<double>& hoehen) const;

    /// Holen -- aus dem Cache sofort (im naechsten Ereignisdurchlauf),
    /// sonst stueckweise aus dem Netz. Meldet fertig() oder fehler().
    /// Ein neuer Aufruf verwirft einen laufenden.
    void hole(const Weg& w);
    bool laeuft() const { return m_laeuft; }

    /// Abrufe, die wirklich hinausgingen (fuer Pruefstaende).
    int abrufeGestellt() const { return m_gestellt; }

signals:
    void fertig(const QString& schluessel, const QVector<double>& hoehen);
    void fehler(const QString& schluessel, const QString& text);
    void fortschritt(int erledigt, int gesamt);

private:
    void naechstesStueck();

    QString   m_ordner;
    HttpAbruf m_abruf;
    Weg       m_weg;
    QString   m_schluessel;
    QVector<Hoehenprofil::Punkt> m_punkte;
    QVector<double> m_hoehen;
    int  m_naechster{0};
    bool m_laeuft{false};
    int  m_generation{0};
    int  m_gestellt{0};
};

} // namespace Longpath
