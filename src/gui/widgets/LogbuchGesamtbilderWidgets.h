// no-port-check: Longpath-original widgets, no Thetis logic.
//
// src/gui/widgets/LogbuchGesamtbilderWidgets.h  (Longpath)
//
// Drei Gesamtbilder des Logbuchs (2026-11-10, Betreiber am 09.11.:
// Sonnenzyklus, Kontinent-Uhr, DXCC-Wand) als Widgets -- gemalt wie
// die gewaehlten Entwurfsblaetter (werkzeug/logbuch-2026-11-09/
// blaetter: Bernstein auf #0c0c0e, die Bandfarben der Azimutalkarte),
// gerechnet in core/LogbuchGesamtbilder. Keine eigene Datenhaltung:
// wer sie fuellt, sagt, was gezeigt wird.

#pragma once

#include "core/LogbuchGesamtbilder.h"

#include <QDateTime>
#include <QWidget>

class QTimer;

namespace Longpath {

/// D  QSOs je Monat gegen den Solar-Flux.
class SonnenzyklusWidget : public QWidget {
    Q_OBJECT
public:
    explicit SonnenzyklusWidget(QWidget* parent = nullptr);
    /// `fluxStand`: Zeitpunkt der Flux-Datei (ungueltig = keine);
    /// `holtGerade`: ein Abruf laeuft.
    void setDaten(const LogbuchBilder::Sonnenzyklus& d, const QDateTime& fluxStand,
                  bool holtGerade);
    const LogbuchBilder::Sonnenzyklus& daten() const { return m_d; }
    /// Der Satz, der ohne Flux-Werte im Bild steht (sonst leer).
    QString hinweis() const;
    /// Die Zeile unter dem Bild.
    QString fusszeile() const;
    QSize sizeHint() const override { return {1100, 600}; }
    QSize minimumSizeHint() const override { return {560, 360}; }
protected:
    void paintEvent(QPaintEvent*) override;
    bool event(QEvent* e) override;
private:
    QRectF plotFlaeche() const;
    int monatBei(const QPointF& p) const;
    LogbuchBilder::Sonnenzyklus m_d;
    QDateTime m_stand;
    bool m_holt{false};
};

/// E  Kontinent-Uhr: 24 Stunden UTC, ein Ring je Kontinent.
class KontinentUhrWidget : public QWidget {
    Q_OBJECT
public:
    explicit KontinentUhrWidget(QWidget* parent = nullptr);
    void setDaten(const LogbuchBilder::KontinentUhr& d);
    const LogbuchBilder::KontinentUhr& daten() const { return m_d; }
    /// Fuer Pruefstaende: eine feste Uhrzeit statt der echten.
    void setJetztFuerTest(const QDateTime& t) { m_jetztTest = t; update(); }
    QDateTime jetzt() const;
    /// Der Takt, der den Zeiger jede Minute nachzieht (laeuft nur,
    /// solange das Bild zu sehen ist).
    QTimer* takt() const { return m_takt; }
    /// Ring (Index in KontinentUhr::kontinente()) und Stunde unter
    /// einem Punkt; false = keine Zelle.
    bool zelleBei(const QPointF& p, int* ring, int* stunde) const;
    QString zellenText(int ring, int stunde) const;
    QSize sizeHint() const override { return {760, 820}; }
    QSize minimumSizeHint() const override { return {420, 460}; }
protected:
    void paintEvent(QPaintEvent*) override;
    bool event(QEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
private:
    void stelleTakt();
    void geometrie(QPointF* mitte, double* radius) const;
    LogbuchBilder::KontinentUhr m_d;
    QDateTime m_jetztTest;
    QTimer* m_takt{nullptr};
};

/// F  DXCC-Wand: jedes Gebiet eine Kachel, nach Kontinent in Bloecken.
class DxccWandWidget : public QWidget {
    Q_OBJECT
public:
    explicit DxccWandWidget(QWidget* parent = nullptr);
    void setDaten(const LogbuchBilder::DxccWand& w);
    const LogbuchBilder::DxccWand& daten() const { return m_w; }
    QString titel() const;
    QString fusszeile() const;
    /// Kachel unter einem Punkt (Index in daten().kacheln), sonst -1.
    int kachelBei(const QPointF& p) const;
    /// Der Tooltip einer Kachel: Gebiet und Baender.
    QString kachelText(int index) const;
    /// Die Rechtecke der Kacheln bei der jetzigen Groesse (fuer
    /// Pruefstaende: keine darf eine andere ueberdecken).
    QVector<QRectF> kachelRechtecke() const;
    QSize sizeHint() const override { return {1280, 640}; }
    QSize minimumSizeHint() const override { return {640, 400}; }
protected:
    void paintEvent(QPaintEvent*) override;
    bool event(QEvent* e) override;
private:
    struct Anordnung {
        double teilung{0.0};                 ///< Kachelschritt waagerecht
        QVector<QRectF> kacheln;             ///< je Kachel, wie m_w.kacheln
        QVector<QPair<QString, QPointF>> koepfe;   ///< Kontinent, Ort des Kopfs
    };
    Anordnung anordnen() const;
    LogbuchBilder::DxccWand m_w;
};

} // namespace Longpath
