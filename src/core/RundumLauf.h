// no-port-check: Longpath-original, no Thetis logic (Thetis has no rotor
// sweep). Drives RotorController and bins band statistics by bearing.
//
// =================================================================
// src/core/RundumLauf.h  (Longpath)
// =================================================================
//
// ── Rundum-Lauf: wo ist Betrieb, woher kommt Stoerung ───────────────
//
// 2026-11-08, Betreiber: „starte wie du meinst" (aus der Ideenliste:
// Richtungs-Scan mit dem Rotor). Fuer KW-Beam und 2-m-Stapel gleich.
//
// Der Rotor faehrt den Bereich ab, den er von seiner Richtung aus
// erreichen darf (core/RotorBereich.h; ohne Grenze der ganze Kreis, ab
// dem Anschlag): erst an dessen Anfang, dann in 30-Grad-Schritten bis
// zum Ende, dann zurueck auf die Ausgangsrichtung. Waehrend er dreht, liefert der
// Panadapter je Bild drei Zahlen ueber den sichtbaren Ausschnitt:
// Rauschboden (Median), Belegung (Summe der dB ueber Boden + 6 dB je
// Bildpunkt) und die Zahl der erkannten Signale. Sie werden nach der
// gemeldeten Richtung in 5-Grad-Sektoren gesammelt.
//
// Warum Schritte und nicht "0 -> 359": ein Rotor mit Anschlag im Sueden
// wuerde von 1 nach 359 Grad den kurzen Weg ueber Nord nehmen und nichts
// ueberstreichen. Bei 30-Grad-Schritten ist der Weg eindeutig; nimmt ein
// Rotor an seinem Anschlag den langen Weg, sammelt er unterwegs nur mehr.
//
// Gemessen wird nur, solange die Position frisch ist -- eine Richtung,
// die der Rotor nicht gemeldet hat, bekommt keine Werte untergeschoben.
// Gezaehlt wird bei der zuletzt GEMELDETEN Richtung. Zwischen zwei
// Meldungen dreht die Antenne weiter; der Nachlauf ist Drehgeschwindigkeit
// mal Meldeabstand (GS-232 etwa 6 Grad/s bei ~1 s, also ein Sektor).

#pragma once

#include "core/RotorBereich.h"

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

namespace Longpath {

class RotorController;

/// Die Sammlung je Sektor -- rein, ohne Rotor pruefbar.
class RundumKarte {
public:
    static constexpr int kSektorGrad = 5;
    static constexpr int kSektoren   = 360 / kSektorGrad;

    struct Sektor {
        int   proben{0};
        double bodenSumme{0.0};      ///< fuer den Mittelwert
        float belegungMax{0.0f};
        int   signaleMax{0};
        float boden() const { return proben > 0 ? static_cast<float>(bodenSumme / proben) : 0.0f; }
    };

    void leeren();
    void nimm(double azimutGrad, float bodenDbm, float belegung, int signale);
    const QVector<Sektor>& sektoren() const { return m_s; }
    int abgedeckt() const;               ///< Sektoren mit mindestens einer Probe

    /// Richtung (Sektormitte, Grad) mit der groessten Belegung; -1 ohne Daten.
    int meisteBelegung() const;
    /// Richtung mit dem hoechsten Rauschboden, und um wieviel er ueber dem
    /// ruhigsten Sektor liegt (dB). -1 ohne Daten.
    int lautesterBoden(float* ueberRuhigstemDb = nullptr) const;

private:
    QVector<Sektor> m_s = QVector<Sektor>(kSektoren);
};

/// Der Ablauf am Rotor.
class RundumLauf : public QObject {
    Q_OBJECT
public:
    enum class Phase { Ruhe, Lauf, Zurueck };
    Q_ENUM(Phase)

    static constexpr int kSchrittGrad   = 30;
    static constexpr int kStillstandMs  = 20000;   ///< ohne neue Position: Abbruch
    static constexpr double kAngekommenGrad = 4.0;

    explicit RundumLauf(QObject* parent = nullptr);

    void setRotor(RotorController* r);
    /// Fuer Pruefstaende: die Stillstandswache kuerzer stellen.
    void setStillstandMs(int ms) { m_wache.setInterval(ms); }
    Phase phase() const { return m_phase; }
    const RundumKarte& karte() const { return m_karte; }
    const QVector<double>& plan() const { return m_plan; }

    /// Die Ziele fuer einen Lauf ab `startAz` im Bereich `b` -- rein.
    /// Leer, wenn `startAz` selbst nicht erlaubt ist.
    static QVector<double> plane(double startAz, const RotorBereich& b);

    /// Startet; false (und fehler()), wenn kein Rotor verbunden ist oder
    /// seine Position nicht frisch ist.
    bool start();
    /// Haelt an und dreht auf die Ausgangsrichtung zurueck.
    void abbrechen();

public slots:
    /// Eine Bandlage vom Panadapter (je Bild).
    void nimmBandLage(float bodenDbm, float belegung, int signale);

signals:
    void fortschritt(int schritt, int schritte, double azimutGrad);
    void fertig(bool vollstaendig);
    void fehler(const QString& text);

private:
    void naechsterSchritt();
    void beiPosition(double az);
    void stillstand();

    QPointer<RotorController> m_rotor;
    RundumKarte m_karte;
    Phase  m_phase{Phase::Ruhe};
    double m_startAz{0.0};
    double m_ziel{0.0};
    int    m_schritt{0};
    QVector<double> m_plan;              ///< die Ziele der Reihe nach
    double m_az{0.0};
    bool   m_azFrisch{false};
    QTimer m_wache;
};

} // namespace Longpath
