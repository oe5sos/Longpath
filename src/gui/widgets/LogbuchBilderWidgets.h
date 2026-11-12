// no-port-check: Longpath-original widgets, no Thetis logic.
//
// src/gui/widgets/LogbuchBilderWidgets.h  (Longpath)
//
// Die drei Logbuch-Bilder (2026-11-08, Betreiber: „Alle drei") als
// Widgets -- gemalt wie die gewaehlten Entwurfsblaetter, gerechnet in
// core/LogbuchBilder. Keine eigene Datenhaltung: wer sie fuellt, sagt,
// was gezeigt wird.

#pragma once

#include "core/LogbuchBilder.h"
#include "core/RotorBereich.h"

#include <QWidget>

namespace Longpath {

/// A  Bedarfs-Matrix unter der Eingabezeile.
class BedarfsMatrixWidget : public QWidget {
    Q_OBJECT
public:
    explicit BedarfsMatrixWidget(QWidget* parent = nullptr);
    void setBedarf(const LogbuchBilder::Bedarf& b);
    const LogbuchBilder::Bedarf& bedarf() const { return m_b; }
    QSize sizeHint() const override { return {780, 150}; }
    QSize minimumSizeHint() const override { return {520, 150}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    LogbuchBilder::Bedarf m_b;
};

/// B  Band x UTC-Stunde.
class BandStundeWidget : public QWidget {
    Q_OBJECT
public:
    explicit BandStundeWidget(QWidget* parent = nullptr);
    void setDaten(const LogbuchBilder::BandStunde& d, const QString& titel,
                  const QString& meinFeld);
    const LogbuchBilder::BandStunde& daten() const { return m_d; }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return {520, 200}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    LogbuchBilder::BandStunde m_d;
    QString m_titel;
    QString m_feld;
};

/// C  Richtungsrose.
class RichtungsRoseWidget : public QWidget {
    Q_OBJECT
public:
    explicit RichtungsRoseWidget(QWidget* parent = nullptr);
    void setRose(const LogbuchBilder::Rose& r, const QString& titel);
    /// Rotor: Richtung (< 0 = unbekannt), Strahlbreite, erlaubter Bereich.
    void setRotor(double beamDeg, double breiteDeg, const RotorBereich& bereich);
    const LogbuchBilder::Rose& rose() const { return m_r; }
    QSize sizeHint() const override { return {600, 668}; }
    QSize minimumSizeHint() const override { return {360, 400}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    LogbuchBilder::Rose m_r;
    QString m_titel;
    double m_beam{-1.0};
    double m_breite{60.0};
    RotorBereich m_bereich;
};

} // namespace Longpath
