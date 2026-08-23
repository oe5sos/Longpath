#pragma once

// =================================================================
// src/gui/widgets/BandwidthFilterPane.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original, nach der Vorlage des Betreibers (Zeus Link
// „BANDWIDTH FILTER", Bildschirmfoto vom 2026-08-20). Die Regeln
// hinter den Zahlen kommen aus Thetis, siehe SliceModel::widthToEdges
// und ::constrainFilter.
//
// Der Durchlass EINES Empfaengers auf einer echten Frequenzachse.
//
// ── Warum nicht das vorhandene Widget ───────────────────────────────
//
// FilterPassbandWidget (RxApplet) zeichnet ein FESTES Trapez: die Form
// aendert sich nicht mit der Breite, die Kanten sitzen immer an
// derselben Stelle. Das ist ein Schema — es sagt „hier ist ein
// Filter", nicht „hier liegt er".
//
// Die Vorlage macht es anders und das ist der ganze Gewinn: die Skala
// liest 13.137 MHz, nicht „−300 Hz". Die Frage, die man wirklich hat,
// ist „liegt meine Flanke auf 13.139?" — und die beantwortet nur eine
// Achse, auf der die Frequenz steht.
//
// Beide bleiben vorerst nebeneinander stehen. Das vorhandene ist eine
// kleine Anzeige im RxApplet, dies hier eine Arbeitsflaeche. Sobald
// klar ist, dass die neue traegt, gehoert die alte zurueckgebaut —
// zwei Stellen, an denen man dasselbe zieht, sind eine zu viel.
//
// ── Drei Ziehbereiche ───────────────────────────────────────────────
//
//   Kante links / rechts   nur diese Kante, die Breite aendert sich
//   Mitte                  der ganze Durchlass, die Breite BLEIBT
//
// Der mittlere ist der Grund fuer die Flaeche: bei CW schiebt man den
// Durchlass ueber eine Stoerung, ohne die Frequenz zu verlieren.
//
// ── Was hier NICHT passiert ─────────────────────────────────────────
//
// Begrenzt wird nicht hier. Die Flaeche meldet Wunschwerte; die Regeln
// (Seitenband, Deckel, Breite-beim-Verschieben) sitzen in
// SliceModel::constrainFilter, damit sie auch fuer CAT und Tastatur
// gelten. Eine Flaeche, die selbst begrenzt, ist die zweite Rechnung,
// die beim naechsten Umbau auseinanderlaeuft.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QColor>
#include <QString>
#include <QWidget>

namespace Longpath {

class BandwidthFilterPane : public QWidget
{
    Q_OBJECT

public:
    explicit BandwidthFilterPane(QWidget* parent = nullptr);

    // Beschriftung links oben, „RX1" / „RX2".
    void setLabel(const QString& text);
    QString label() const { return m_label; }

    // Die Farbe dieses Empfaengers. In der Vorlage ist RX1 blau und
    // RX2 gruen — man vergleicht sie, ohne die Beschriftung zu lesen.
    void setAccent(const QColor& c);

    // Wo der Empfaenger steht. Die Achse wird darum herum gezeichnet.
    void setVfoFrequency(double hz);
    double vfoFrequency() const { return m_vfoHz; }
    int    spanHz() const { return m_spanHz; }
    /// Die geglaettete Kurve. Nur fuer Pruefungen: die Glaettung ist
    /// das, was der Bediener als "ruhig" oder "verzoegert" erlebt, und
    /// eine Behauptung darueber gehoert gemessen.
    const QVector<float>& traceForTest() const { return m_trace; }

    // Wie viel Band die Flaeche zeigt. Die Vorlage zeigt rund 10 kHz.
    void setSpan(int hz);
    int  span() const { return m_spanHz; }

    void setFilter(int low, int high);
    int  filterLow()  const { return m_low; }
    int  filterHigh() const { return m_high; }

    // Ohne Verbindung steht keine Frequenz auf der Achse. Eine
    // erfundene waere eine Behauptung — dieselbe Regel wie beim
    // Panadapter-Kopf und beim Rotorzeiger.
    void setHasFrequency(bool on);

    /// Der Spektrumausschnitt hinter dem Durchlass, in dBm, von der
    /// linken bis zur rechten Kante der angezeigten Spanne.
    ///
    /// Vorbild ist Zeus Link (vorgefuehrt am 2026-08-22): dort zeigt
    /// der Bandfilter das ECHTE Signal, und erst dadurch sieht man,
    /// ob die Kante an der richtigen Stelle sitzt. AetherSDR hat das
    /// nicht — sein FilterPassbandWidget ist ein reiner Kanteneditor.
    /// Nachgesehen, bevor gebaut wurde.
    void setTrace(const QVector<float>& dbm);

signals:
    // Beim Ziehen einer Kante. Die Breite darf sich aendern.
    void filterChanged(int low, int high);

    // Beim Ziehen der Mitte. Die Breite soll erhalten bleiben —
    // deshalb ein eigenes Signal: der Empfaenger ruft dann
    // SliceModel::setFilterCenter, das mit filterShift begrenzt.
    void filterCentreChanged(int centreHz);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void leaveEvent(QEvent*) override;

private:
    enum class Zone { None, LowEdge, Body, HighEdge };

    // Welcher Bereich liegt unter x? Auch fuer den Zeigercursor: er
    // soll VORHER sagen, was ein Ziehen tun wird.
    Zone zoneAt(int x) const;

    int    hzToX(int hz) const;
    int    xToHz(int x) const;
    QRect  plotRect() const;

    QString m_label{QStringLiteral("RX1")};
    QColor  m_accent;
    double  m_vfoHz{0.0};
    QVector<float> m_trace;
    /// Traeger Mittelwert — die blasse Bezugslinie, siehe paintEvent.
    QVector<float> m_avgLine;
    int     m_spanHz{10000};
    int     m_low{-2850};
    int     m_high{-150};
    bool    m_hasFrequency{false};

    Zone   m_drag{Zone::None};
    int    m_dragStartX{0};
    int    m_dragStartLow{0};
    int    m_dragStartHigh{0};
    Zone   m_hover{Zone::None};

    // Fangbereich an den Kanten. Groesser als die gezeichnete Pille,
    // damit man nicht zielen muss.
    static constexpr int kGrabPx = 8;

    // Kleinster Schritt beim Ziehen. Hertz-genau zu ziehen ist auf
    // einer 10-kHz-Achse ohnehin Zufall, und krumme Werte lesen sich
    // schlecht.
    static constexpr int kStepHz = 10;
};

} // namespace Longpath
