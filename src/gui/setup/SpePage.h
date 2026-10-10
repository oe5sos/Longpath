// =================================================================
// src/gui/setup/SpePage.h  (Longpath-native)
// =================================================================
//
// Setup -> Network -> SPE Expert. Kein Port: AetherSDR haengt den SPE
// als eine ZEILE in seinen Peripherals-Tab, Longpath hat fuer jedes
// Zubehoer eine eigene Seite (RfKitPage, FourO3APage). Aufbau und
// Wortwahl folgen RfKitPage, damit die Seite sich nicht fremd anfuehlt.
//
// Ein Unterschied zum RF2K-S, und er traegt die halbe Seite: der SPE
// haengt am seriellen Anschluss ODER ueber einen ser2net-Vermittler am
// Netz. Dieselben Bytes, zwei Wege -- also ein Umschalter und zwei
// Eingabefelder, von denen immer nur eines etwas tut.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 -- Created by Martin Fischer, AI-assisted via Anthropic
//                 Claude (Claude Code). Zeus-Punkt 7.
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

class SpePage : public QWidget {
    Q_OBJECT

public:
    explicit SpePage(RadioModel* model, QWidget* parent = nullptr);

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
