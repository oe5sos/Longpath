// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Weltkarte der Kiwis (Entwurf A) und
// "Wo werde ich gehoert" (Entwurf C), 2026-11-25.
//
// Gezeichnet mit QPainter auf einer gleichabstaendigen Zylinderprojektion
// (Laenge/Breite linear, wie FlatMapWidget::project()); die Kuestenlinie
// ist Natural Earth 110m, Public Domain (KiwiWorldCoast.cpp). Keine neue
// Abhaengigkeit, kein Netz.
//
// Farbe im Hausstil: Gruen = frei / gehoert, Amber = voll / schwach, Grau
// = nicht erreichbar / nichts, das eigene Signal in der TX-Farbe, ein
// geoeffneter Kiwi mit blauem Ring (anfassbar). Alles andere Grau auf
// Fast-Schwarz.

#pragma once

#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

class QToolButton;

namespace Longpath {

struct KiwiMapPoint {
    enum class State { Free, Partial, Full, Offline };
    // Nur im "Gehoert"-Modus: die Farbe folgt dem Urteil statt dem Zustand.
    enum class Heard { None, Hears, Weak, Nothing, Unknown };

    QString id;                 // Schluessel (Endpunkt oder Profilkennung)
    QString name;
    double lat{0.0};
    double lon{0.0};
    State state{State::Free};
    Heard heard{Heard::None};
    QString label;              // kurz neben dem Punkt, z. B. "S7"
    QString detail;             // Zeilen fuer den Hinweis beim Ueberfahren
    bool open{false};           // verbunden: Ring und Linie vom Standort
};

class KiwiWorldMap : public QWidget {
    Q_OBJECT

public:
    explicit KiwiWorldMap(QWidget* parent = nullptr);

    void setPoints(const QVector<KiwiMapPoint>& points);
    int pointCount() const { return m_points.size(); }
    // Standort (Locator-Mitte). valid=false: kein Standortpunkt.
    void setHome(double lat, double lon, bool valid, const QString& label);
    void setSelectedId(const QString& id);
    QString selectedId() const { return m_selected; }
    // Gehoert-Modus: Farben nach Urteil, Beschriftung immer an.
    void setHeardMode(bool on);
    // Ausschnitt: an = um Standort und geoeffnete Kiwis herum (mindestens
    // 40 x 20 Grad), aus = ganze Welt.
    void setAutoFit(bool on);
    bool autoFit() const { return m_autoFit; }

    // Fuer Hinweis, Klick und Pruefung.
    QPointF toWidget(double lat, double lon) const;
    // Naechster Punkt innerhalb von 9 px, sonst -1.
    int pointAt(const QPointF& pos) const;
    QString idAt(const QPointF& pos) const;
    // Vergroessern (+1) und verkleinern (-1); 1.0 = der Grundausschnitt
    // (Welt oder setAutoFit). Mit der Maus ziehen verschiebt.
    void zoomStep(int direction);
    double zoom() const { return m_zoom; }

    // Sichtbarer Ausschnitt in Grad.
    void viewBounds(double& lat0, double& lat1, double& lon0, double& lon1) const;

    QSize sizeHint() const override { return QSize(640, 300); }
    QSize minimumSizeHint() const override { return QSize(240, 120); }

signals:
    void pointClicked(const QString& id);
    void pointHovered(const QString& id);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void computeView(double& lat0, double& lat1, double& lon0, double& lon1) const;
    void baseView(double& lat0, double& lat1, double& lon0, double& lon1) const;
    // Neuer Faktor; der Ort unter anchor bleibt stehen.
    void zoomAt(double factor, const QPointF& anchor);
    void updateZoomButtons();

    static constexpr double kZoomStep = 1.7;
    static constexpr double kMaxZoom = 120.0;
    double m_zoom{1.0};
    double m_centerLat{0.0};       // gilt nur bei m_zoom > 1
    double m_centerLon{0.0};
    QToolButton* m_zoomInButton{nullptr};
    QToolButton* m_zoomOutButton{nullptr};
    QPointF m_pressPos;
    QPointF m_dragLast;
    bool m_pressed{false};
    bool m_dragged{false};

    QVector<KiwiMapPoint> m_points;
    bool m_homeValid{false};
    double m_homeLat{0.0};
    double m_homeLon{0.0};
    QString m_homeLabel;
    QString m_selected;
    QString m_hovered;
    bool m_heardMode{false};
    bool m_autoFit{false};
};

} // namespace Longpath
