// no-port-check: Longpath-original, no Thetis logic (Thetis has no QSO map).
//
// =================================================================
// src/gui/widgets/AzimutalKarteWidget.h  (Longpath)
// =================================================================
//
// ── Azimutalkarte um den eigenen Standort (2026-11-10) ──────────────
//
// Das Entwurfsblatt vom 09.11. (werkzeug/logbuch-2026-11-09/blaetter/
// 1-azimutalkarte.png), Betreiber: „absoluter wahnsinn". Jetzt im
// Logbuch, als vierte Ansicht der QSO-Karte.
//
// Mittabstandstreue Azimutalprojektion um das eigene QTH: jede gerade
// Linie aus der Mitte ist eine Rotorrichtung, jeder Ring eine
// Entfernung, der Rand der Gegenpunkt. Radius = Winkelabstand / pi,
// also LINEAR -- anders als das Radar (DxRadarWidget, Wurzel), das die
// Naehe aufloest; hier soll die Welt stimmen.
//
// Was darauf liegt:
//   - das Weltbild, abgedunkelt wie auf dem Blatt (45 % Farbe, auf
//     42 % Helligkeit, leicht warm) -- damit die Strahlen tragen,
//   - die Nacht zur jetzigen Uhrzeit, mit Daemmerungssaum (minuetlich),
//   - jedes gezeigte QSO als Strahl in seiner Bandfarbe, NICHT je Ort
//     zusammengefasst: hell wird es, wo viele zusammenlaufen, und genau
//     das soll man sehen,
//   - der Rotor live: Keil in Strahlbreite und Nadel,
//   - die nachgeschlagene Station: kurzer Weg hell, langer Weg
//     gestrichelt (ueber den Rand = den Gegenpunkt).
//
// Gefiltert wird dort, wo die Karte schon filtert (Band-/Betriebsart-
// Pillen, Zeitraum, die Tabelle des Logbuchs) -- diese Ansicht zeigt,
// was die anderen drei auch zeigen.
//
// Gerechnet wird erst, wenn die Ansicht zu sehen ist: die drei Ebenen
// (Weltbild, Nacht, Strahlen) tragen ein „veraltet" und werden im
// paintEvent nachgezogen. Die Karte liegt meist verdeckt im Stapel, und
// jede Filteraenderung im Logbuch ruft setPunkte().
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#pragma once

#include <QDateTime>
#include <QImage>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

class QPainter;
class QTimer;

namespace Longpath {

class AzimutalKarteWidget : public QWidget {
    Q_OBJECT
public:
    struct Punkt {
        double  lat{0.0};
        double  lon{0.0};        // positiv Ost
        QString band;            // "20m"
        QString call;
        bool    genau{true};     // false = Landesmitte
        bool    markiert{false}; // in der Tabelle ausgewaehlt
    };

    explicit AzimutalKarteWidget(QWidget* parent = nullptr);

    void setHome(double lat, double lon, const QString& locator = QString());
    void clearHome();
    bool hasHome() const { return m_hasHome; }

    void setPunkte(const QVector<Punkt>& punkte);
    int  punktAnzahl() const { return m_punkte.size(); }

    // Rotor wie beim Radar: Stellung und Strahlbreite in Grad; eine
    // negative Stellung = unbekannt, dann kein Keil.
    void   setRotorHeading(double deg);
    void   setRotorBeamWidth(double deg);
    double rotorHeading() const { return m_rotorDeg; }

    // Die nachgeschlagene Station: kurzer und langer Weg.
    void    setZiel(const QString& call, double lat, double lon);
    void    clearZiel();
    bool    hatZiel() const { return m_hatZiel; }
    QString zielCall() const { return m_zielCall; }

    // Bandfarben -- eine Sprache fuer Strahl, Punkt und Legende.
    // Gedaempft, kein kraeftiges Rot, kein Auswahl-Blau (Hausregeln).
    static QColor bandFarbe(const QString& band);

    // Geometrie, fuer Tests offen. Einheitsscheibe, y nach Norden,
    // Radius 1 = Gegenpunkt (20 015 km).
    static QPointF projektion(double homeLat, double homeLon, double lat, double lon);
    static bool    umkehrung(double homeLat, double homeLon, double x, double y,
                             double& lat, double& lon);

    // Fuer Pruefstaende: die Ebenen (gebaut, falls veraltet) und eine
    // feste Uhrzeit fuer die Nacht.
    QImage hintergrundForTest() { sorgeFuerEbenen(); return m_hintergrund; }
    QImage nachtForTest()       { sorgeFuerEbenen(); return m_nacht; }
    QImage strahlenForTest()    { sorgeFuerEbenen(); return m_strahlen; }
    void   setZeitForTest(const QDateTime& utc);
    // Bildpunkt (Widget-Koordinaten) eines Ortes.
    QPointF bildpunktForTest(double lat, double lon) const { return bildpunkt(lat, lon); }
    QPointF mitteForTest() const { return mitte(); }
    double  radiusForTest() const { return radiusPx(); }

    QSize sizeHint() const override { return {520, 520}; }
    QSize minimumSizeHint() const override { return {220, 220}; }

signals:
    // Ein Klick auf einen Punkt -- wie beim Radar, damit das Fenster
    // dieselbe Karteikarte zeigt.
    void pointClicked(const QString& label, double lat, double lon);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    double  radiusPx() const;
    QPointF mitte() const;
    QPointF bildpunkt(double lat, double lon) const;
    QPointF bildpunktNormiert(const QPointF& q) const;
    int     sollRechenPx() const;

    void    sorgeFuerEbenen();
    void    baueHintergrund(int n);   // Weltbild, bei Standort/Bild/Groesse
    void    baueNacht();              // Nachtlasur, minuetlich
    void    baueStrahlen();           // QSO-Strahlen, bei Punkten/Groesse
    void    projiziereNeu();          // m_proj aus m_punkte

    void    zeichneSkala(QPainter& p) const;
    void    zeichneRotor(QPainter& p) const;
    void    zeichneZiel(QPainter& p) const;
    void    zeichneLegende(QPainter& p) const;
    void    zeichneHover(QPainter& p) const;

    bool    m_hasHome{false};
    double  m_homeLat{0.0}, m_homeLon{0.0};
    QString m_homeLocator;

    QVector<Punkt>   m_punkte;
    QVector<QPointF> m_proj;          // je Punkt, Einheitsscheibe
    QVector<int>     m_reihenfolge;   // haeufige Baender zuerst gemalt

    double  m_rotorDeg{-1.0};
    double  m_strahlDeg{30.0};

    bool    m_hatZiel{false};
    QString m_zielCall;
    double  m_zielLat{0.0}, m_zielLon{0.0};

    // Weltbild und Nacht in einer Rechengroesse (Einheitsscheibe auf
    // n x n), beim Zeichnen auf die Scheibe skaliert. Die Strahlen in
    // Bildschirmpunkten des Widgets.
    QImage  m_hintergrund;
    QImage  m_nacht;
    QImage  m_strahlen;
    bool    m_hintergrundAlt{true};
    bool    m_nachtAlt{true};
    bool    m_strahlenAlt{true};
    int     m_rechenPx{0};

    // Das Weltbild, einmal auf eine sinnvolle Breite verkleinert:
    // 5400 Punkte Breite auf eine Scheibe von 900 abzutasten, flimmert.
    QImage  m_weltKlein;
    qint64  m_weltSchluessel{0};

    QTimer*   m_nachtTakt{nullptr};
    QDateTime m_zeitTest;

    int     m_hover{-1};
};

} // namespace Longpath
