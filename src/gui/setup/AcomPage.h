// no-port-check: Longpath-eigene Datei, kein Port.
// =================================================================
// src/gui/setup/AcomPage.h  (Longpath-eigen)
// =================================================================
//
// Setup -> Network -> ACOM. Aufbau wie SpePage und Kpa500Page daneben.
//
// ZWEI FELDER WENIGER ALS BEIM KPA500, und beide fehlen aus einem
// Grund, nicht aus Sparsamkeit:
//
//   * KEINE Datenrate. Die Beschreibung des Herstellers schreibt
//     9600 8N1 vor -- da gibt es nichts einzustellen. Der KPA500
//     kennt vier Werte und braucht das Feld darum.
//   * KEIN Modellwaehler. Die Beschreibung dokumentiert genau EINEN
//     Typcode („1 - A600S"); fuer die anderen fuenf Modelle gibt es
//     keine Quelle. Statt den Betreiber raten zu lassen, erkennt die
//     Verbindung die Stufe aus der beobachteten Leistung (ein 500S
//     kann keine 900 W melden). Die Seite zeigt darum an, WAS erkannt
//     wurde, und bietet nichts zum Auswaehlen.
//
// Ein Hinweis mehr dafuer: ueber Netz muss der Vermittler ROH sein.
// Der Datenstrom enthaelt legitim 0xFF, und Telnets IAC-Maskierung
// verfaelscht genau das -- beim SPE ging Telnet noch, hier nicht.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 -- Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Claude Code.
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

class AcomPage : public QWidget {
    Q_OBJECT

public:
    explicit AcomPage(RadioModel* model, QWidget* parent = nullptr);

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
