// no-port-check: Longpath-original, no Thetis logic (see core/RundumLauf.h).
//
// src/gui/RundumDialog.h  (Longpath)
//
// Das Fenster zum Rundum-Lauf (2026-11-08): Start/Stopp, Fortschritt und
// ein Polarbild -- Belegung je Richtung als Flaeche, Rauschboden als Linie.

#pragma once

#include "core/RundumLauf.h"

#include <QDialog>
#include <QPointer>

class QLabel;
class QPushButton;

namespace Longpath {

class RadioModel;
class SpectrumWidget;

/// Das Polarbild allein -- malt eine RundumKarte.
class RundumBild : public QWidget {
    Q_OBJECT
public:
    explicit RundumBild(QWidget* parent = nullptr);
    void setKarte(const RundumKarte* k) { m_karte = k; update(); }
    void setRichtung(double az, bool gueltig) { m_az = az; m_azGueltig = gueltig; update(); }
    QSize sizeHint() const override { return {360, 360}; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    const RundumKarte* m_karte{nullptr};
    double m_az{0.0};
    bool   m_azGueltig{false};
};

class RundumDialog : public QDialog {
    Q_OBJECT
public:
    RundumDialog(RadioModel* radio, SpectrumWidget* spektrum, QWidget* parent = nullptr);
    ~RundumDialog() override;

    RundumLauf& laufForTest() { return m_lauf; }
    QString statusTextForTest() const;
    QString ergebnisTextForTest() const;

private:
    void starte();
    void beende(bool vollstaendig);
    void zeigeErgebnis();

    QPointer<RadioModel>     m_radio;
    QPointer<SpectrumWidget> m_spektrum;
    RundumLauf   m_lauf;
    RundumBild*  m_bild{nullptr};
    QLabel*      m_status{nullptr};
    QLabel*      m_ergebnis{nullptr};
    QPushButton* m_start{nullptr};
    QPushButton* m_stopp{nullptr};
};

} // namespace Longpath
