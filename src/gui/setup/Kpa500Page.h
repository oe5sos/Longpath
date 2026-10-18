// no-port-check: Longpath-eigene Datei, kein Port.
// =================================================================
// src/gui/setup/Kpa500Page.h  (Longpath-eigen)
// =================================================================
//
// Setup -> Network -> Elecraft KPA500. Aufbau wie SpePage daneben
// (und wie RfKitPage/FourO3APage davor): Umschalter, Anschlussangaben,
// Lebenszeile.
//
// EIN FELD MEHR ALS BEIM SPE, und es ist kein Schmuck: die DATENRATE
// des seriellen Anschlusses. Der SPE passt sich an (115200 und
// abwaerts), der KPA500 nicht -- seine Rate steht in ^BRP und kennt
// vier Werte (4800/9600/19200/38400). Welcher eingestellt ist, sagt
// Elecrafts Beschreibung nicht, und an der falschen kommt NICHTS durch.
// Also fragt die Seite danach, statt eine Zahl anzunehmen.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 -- Neu, Zeus-Punkt 7. Martin Fischer, AI-assisted via
//                 Anthropic Claude (Claude Code).
// =================================================================

#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace Longpath {

class RadioModel;

class Kpa500Page : public QWidget {
    Q_OBJECT

public:
    explicit Kpa500Page(RadioModel* model, QWidget* parent = nullptr);

private slots:
    void onMasterToggled(bool on);
    void onModeChanged();
    void saveAndApply();
    void refreshConnectionBanner();
    void refreshLiveStatus();

private:
    void applyMasterGate(bool on);
    void reloadFromPeripherals();
    bool serialMode() const;

    RadioModel* m_model{nullptr};

    QLabel*      m_connectionBanner{nullptr};
    QCheckBox*   m_master{nullptr};
    QComboBox*   m_mode{nullptr};
    QLineEdit*   m_serialPort{nullptr};
    QLabel*      m_serialPortLabel{nullptr};
    QComboBox*   m_baud{nullptr};
    QLabel*      m_baudLabel{nullptr};
    QLineEdit*   m_host{nullptr};
    QLabel*      m_hostLabel{nullptr};
    QSpinBox*    m_port{nullptr};
    QLabel*      m_portLabel{nullptr};
    QCheckBox*   m_autoReconnect{nullptr};
    QPushButton* m_applyBtn{nullptr};
    QLabel*      m_liveStatus{nullptr};
    QWidget*     m_connBox{nullptr};
};

}  // namespace Longpath
