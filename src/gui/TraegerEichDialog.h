// no-port-check: Longpath-original dialog; the calibration arithmetic it
// calls is ported in core/FrequenzEichung.h and cited there.
//
// src/gui/TraegerEichDialog.h  (Longpath)
//
// Das Fenster nach "Calibrate frequency on this carrier" (2026-11-08):
// zeigt die gemessene Frequenz, nimmt die wahre entgegen, sagt, wohin
// die Korrektur wirkt (Transverter-LO-Fehler, Korrekturfaktor des
// Geraets oder nur Anzeige), und uebernimmt sie.

#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace Longpath {

class RadioModel;

class TraegerEichDialog : public QDialog {
    Q_OBJECT
public:
    TraegerEichDialog(RadioModel* radio, bool gefunden, double geraetHz,
                      double anzeigeHz, float snrDb, QWidget* parent = nullptr);

    /// Fuer Pruefstaende: die wahre Frequenz setzen und uebernehmen.
    void setWahrHzForTest(double hz);
    bool uebernehmenForTest();
    QString zielTextForTest() const;
    QString abweichungTextForTest() const;
    bool uebernehmenMoeglichForTest() const;

private:
    void aktualisiere();
    bool uebernimm();

    RadioModel* m_radio{nullptr};
    double m_geraetHz{0.0};
    double m_anzeigeHz{0.0};
    QDoubleSpinBox* m_wahr{nullptr};
    QLabel* m_abweichung{nullptr};
    QLabel* m_ziel{nullptr};
    QPushButton* m_uebernehmen{nullptr};
};

} // namespace Longpath
