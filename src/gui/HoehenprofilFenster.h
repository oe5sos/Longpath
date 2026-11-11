// no-port-check: Longpath-original, no Thetis logic.
//
// =================================================================
// src/gui/HoehenprofilFenster.h  (Longpath)
// =================================================================
//
// ── "Profile" aus der Stationskarte (2026-11-11) ────────────────────
//
// Ein Fenster je Logbuch: der Knopf "Profile" in der Stationskarte
// (QsoDetailPane) zeigt hier das Hoehenprofil des gewaehlten 2-m-,
// 70-cm- oder 23-cm-QSOs; ein zweiter Druck auf einem anderen QSO
// wechselt den Inhalt, statt ein zweites Fenster zu oeffnen.
//
// Warum dort und nicht im Kartenfenster: die Karte bauen gerade zwei
// andere Zweige um (Azimutalkarte, 3D-Flug -- beide mit einem vierten
// Stapelplatz in QsoMapWindow); die Stationskarte beruehrt nur der
// Rotor-Zweig, und der nicht in der Knopfreihe. Und das Profil gehoert
// zu EINEM QSO -- wie die Peilung, die schon in der Karte steht.
//
// Enden: der eigene Gipfel (MY_SOTA_REF) mit Lage und Hoehe aus der
// SOTA-Datenbank, sonst die Mitte von MY_GRIDSQUARE; die Gegenstation
// genauso ueber SOTA_REF bzw. GRIDSQUARE. Antennenhoehen ueber Grund:
// eigene 10 m, am Gipfel 5 m (portabel), Gegenstation 10 m -- im
// Fenster aenderbar und gemerkt (AppSettings ProfileAntenna*).
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#pragma once

#include "core/Hoehenprofil.h"
#include "core/HoehenDaten.h"
#include "core/SotaGipfel.h"
#include "models/LogEntry.h"

#include <QDialog>

class QDoubleSpinBox;
class QLabel;

namespace Longpath {

class HoehenprofilWidget;

class HoehenprofilFenster : public QDialog {
    Q_OBJECT
public:
    explicit HoehenprofilFenster(QWidget* parent = nullptr);

    /// Das QSO zeigen. `ersatzLocator`: der eigene Locator, wenn das QSO
    /// keinen MY_GRIDSQUARE hat (StationGridSquare).
    void setEntry(const LogEntry& e, const QString& ersatzLocator = QString());

    /// Fuer Pruefstaende und Werkzeuge.
    HoehenAbruf& abruf() { return m_abruf; }
    HoehenprofilWidget* profil() const { return m_profil; }
    QString statusText() const;
    QDoubleSpinBox* eigeneAntenne() const { return m_antA; }
    QDoubleSpinBox* gegenAntenne() const { return m_antB; }
    /// Der Weg, den das Fenster gerade zeigt oder holt.
    HoehenAbruf::Weg weg() const { return m_weg; }
    bool hatWeg() const { return m_hatWeg; }

    /// Die Vorgaben fuer die Antennenhoehen (m ueber Grund).
    static double vorgabeEigene(bool gipfel);
    static double vorgabeGegen();

signals:
    void profilFertig();

protected:
    void showEvent(QShowEvent* ev) override;

private:
    void neuAufbauen();       // Enden bestimmen, Hoehen holen
    void rechnen();           // aus m_hoehen das Bild
    void gipfelBekannt(const QString& ref);
    void zeigeFehler(const QString& text);

    HoehenprofilWidget* m_profil{nullptr};
    QLabel* m_quelle{nullptr};
    QLabel* m_status{nullptr};
    QDoubleSpinBox* m_antA{nullptr};
    QDoubleSpinBox* m_antB{nullptr};
    class QPushButton* m_zu{nullptr};

    HoehenAbruf m_abruf;
    LogEntry m_entry;
    QString  m_ersatz;
    SotaGipfelVerzeichnis::Ende m_a;
    SotaGipfelVerzeichnis::Ende m_b;
    HoehenAbruf::Weg m_weg;
    bool m_hatWeg{false};
    QString m_schluessel;
    QVector<double> m_hoehen;
    bool m_setzt{false};
};

} // namespace Longpath
