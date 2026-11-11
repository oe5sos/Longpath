// no-port-check: Longpath-original, no Thetis logic (Thetis has no rotor).
//
// =================================================================
// src/gui/widgets/RotorBandWidget.h  (Longpath)
// =================================================================
//
// ── Ein schmales Band fuer die Rotorzeile im Logbuch ──────────────────
//
// 2026-11-11, Betreiber: „im logbuch sollte auch ersichtlich sein, dass
// sobald ich ROTATE klicke, der rotator auch wirklich dreht" und „weiters
// sollte alles sehr übersichtlich gestaltet werden" -- gewaehlt: Entwurf C
// (Blatt ~/Longpath/werkzeug/rotorzeile-2026-11-11/blaetter/C-band.png).
//
// Ein Ausschnitt von ±90° um die Antenne, eine Zeile hoch:
//   Nadel     wo der Rotor steht (nur echte Ablesungen; ohne Rotor keine)
//   Keil      die Strahlbreite um die Nadel
//   Dreieck   die Station: hohl, solange nicht gedreht wird; gefuellt,
//             sobald sie das Ziel eines Drehbefehls ist
//
// Warum nicht RotorDialWidget in Bandform: das ist fuer Panelgroesse
// gebaut; in Zeilenhoehe deckt seine eigene Ablesung die Skala zu.
// Die Ablesung steht hier als Text daneben (LogbookWindow).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11  Martin Fischer  Neu. AI tooling: Anthropic Claude.

#pragma once

#include <QWidget>

namespace Longpath {

class RotorBandWidget : public QWidget {
    Q_OBJECT
public:
    explicit RotorBandWidget(QWidget* parent = nullptr);

    /// Wo der Rotor steht; < 0 oder NaN: unbekannt (kein Rotor).
    void setStand(double deg);
    /// Wo die Station liegt (Peilung); < 0: keine.
    void setStation(double deg);
    /// Ob die Station gerade das Ziel eines Drehbefehls ist.
    void setZiel(bool istZiel);
    /// Farbe von Nadel und Keil -- wie der Zustand daneben.
    void setFarbe(const char* hex);
    void setStrahlbreite(double deg);

    double stand() const noexcept { return m_stand; }
    double station() const noexcept { return m_station; }
    bool   istZiel() const noexcept { return m_ziel; }
    const char* farbe() const noexcept { return m_farbe; }
    /// Die Mitte des Ausschnitts: der Rotor, sonst die Station.
    double mitte() const;

    QSize sizeHint() const override { return {kBreite, kHoehe}; }

    static constexpr int kBreite = 300;
    static constexpr int kHoehe  = 34;
    static constexpr double kSpanneGrad = 180.0;   ///< ±90°

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    void aktualisiereText();

    double      m_stand{-1.0};
    double      m_station{-1.0};
    bool        m_ziel{false};
    const char* m_farbe;
    double      m_strahl{40.0};
};

} // namespace Longpath
