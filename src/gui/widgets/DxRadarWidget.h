// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/widgets/DxRadarWidget.h  (Longpath)
// =================================================================
// Longpath-original. No Thetis port.
//
// Der DX-Radar: die Kontakte um den eigenen Standort, nach Peilung und
// Entfernung — die Frage des Rotorbedieners („wohin muss die Antenne,
// wo war ich heute noch nicht") in einem Bild. Norden oben, Ringe fuer
// die Entfernung mit gestauchter Skala (Wurzel), damit Europa nicht in
// der Mitte klumpt und der Pazifik trotzdem draufpasst. Markierte
// Kontakte heben sich ab, Klick auf einen Punkt meldet ihn wie auf der
// flachen Karte. Dritte Ansicht des Kartenfensters neben Kugel und
// flacher Karte (Betreiber 2026-09-22, nach dem Bild der Vorlage:
// „GLOBE | DX RADAR").
// =================================================================
// Modification history (Longpath):
//   2026-09-22 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================
#pragma once

#include "gui/widgets/MapPoint.h"

#include <QPointF>
#include <QVector>
#include <QWidget>

class QPainter;

namespace Longpath {

class DxRadarWidget : public QWidget {
    Q_OBJECT
public:
    explicit DxRadarWidget(QWidget* parent = nullptr);

    void setHome(double lat, double lon);
    void clearHome();
    bool hasHome() const { return m_hasHome; }

    // Dieselben Punkte wie Kugel und flache Karte; Peilung und Entfernung
    // werden hier gerechnet.
    void setPoints(const QVector<MapPoint>& points);

    // Aeusserster Ring in km (Voreinstellung 20 000 — der halbe Umfang).
    void setMaxRangeKm(double km);
    double maxRangeKm() const { return m_maxKm; }

    // Ein Richtstrahl aus der Rotorsteuerung, in Grad; negativ = keiner.
    void setBeamHeading(double deg);

    // ── Der Rotor (2026-09-26) ───────────────────────────────────────
    //
    // Betreiber: "ich glaube, dass ich auch hier einen Rotor sehen will"
    // und "wir haben ja eine schoene Grafik bereits" -- das Radar. Wie
    // der Rotor-Kegel im Contestprogramm (dort vom Betreiber gewaehlt):
    // ein Kegel in Strahlbreite mit Verlauf, eine verjuengte Nadel mit
    // Gegengewicht, eine blaue Marke am Rand fuer das Ziel. Bernstein =
    // gemessen (wohin die Antenne zeigt), Blau = gewollt (die Station).
    // Negative Werte: nichts zeichnen (kein Rotor, kein Ziel).
    void setRotorHeading(double deg);
    void setRotorTarget(double deg);
    void setRotorBeamWidth(double deg);
    // Ist-Grad und Rest oben links. Aus, wenn die Zahlen schon woanders
    // stehen (die Karteikarte im Logbuch).
    void setRotorReadout(bool on);
    // Klein neben der Karte: Zahlen unten links statt gross oben, und
    // "nicht verbunden", solange keine Ablesung da ist -- der Stand des
    // Rotors muss immer zu sehen sein (Betreiber 2026-09-26: "man muss
    // auch den aktuellen Stand des Rotors sehen!!!").
    void setRotorCompact(bool on);
    void setRotorStatusShown(bool on);
    double rotorHeading() const { return m_rotorDeg; }
    double rotorTarget() const { return m_rotorTargetDeg; }

    // ── Geometrie, fuer Pruefstaende ─────────────────────────────────
    // Entfernung → Radius (0..1) mit Wurzelskala.
    static double radiusFor(double km, double maxKm);
    // Bildpunkt eines Kontakts fuer die aktuelle Groesse.
    QPointF pointAt(double bearingDeg, double km) const;
    int pointsPainted() const { return m_painted; }

    QSize sizeHint() const override { return {520, 520}; }

signals:
    void pointClicked(const QString& label, double lat, double lon);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    struct Placed {
        QPointF px;
        double  bearingDeg{0.0};
        double  km{0.0};
        int     index{-1};
    };
    void relayout();
    double radiusPx() const;
    QPointF centre() const;
    int hitTest(const QPointF& pos) const;

    bool   m_hasHome{false};
    double m_homeLat{0.0}, m_homeLon{0.0};
    double m_maxKm{20000.0};
    double m_beamDeg{-1.0};
    double m_rotorDeg{-1.0};
    double m_rotorTargetDeg{-1.0};
    double m_rotorBeamWidth{40.0};
    bool   m_rotorReadout{true};
    bool   m_rotorCompact{false};
    bool   m_rotorStatusShown{false};
    void paintRotor(QPainter& p, const QPointF& c, double R) const;
    QVector<MapPoint> m_points;
    QVector<Placed>   m_placed;
    int    m_painted{0};
    int    m_hover{-1};
};

} // namespace Longpath
