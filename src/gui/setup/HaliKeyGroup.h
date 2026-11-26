#pragma once

// =================================================================
// src/gui/setup/HaliKeyGroup.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Fusstaster und Morsetaste am HaliKey MIDI ────────────────────────
//
// Die Einstellgruppe zu src/core/cw/CwTastung. Sie steht auf
// Setup > CAT & Network > MIDI Control -- dort hat der Betreiber sie am
// 2026-11-17 gesucht (Bildschirmfoto: die Seite zeigte noch den
// Platzhalter „MIDI mapping table will appear here"), waehrend sie
// einen Commit lang unter Keyboard > Shortcuts lag. Eine Einstellung
// gehoert dorthin, wo man nach ihr sieht.
//
// Zwei Schalter stehen oben und getrennt: lesen und senden sind nicht
// dasselbe, und der zweite („Key the transmitter") ist ab Werk aus.
//
// cwKey darf null sein (Pruefstand ohne Funkmodell): dann steht die
// Gruppe da, aber gesperrt, mit einem Satz warum.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QGroupBox>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;

namespace Longpath {

class CwTastung;

class HaliKeyGroup : public QGroupBox {
    Q_OBJECT

public:
    explicit HaliKeyGroup(CwTastung* cwKey, QWidget* parent = nullptr);

    // Fuer den Pruefstand.
    QCheckBox* readCheck() const { return m_cwEnabled; }
    QCheckBox* keyTransmitCheck() const { return m_cwKeyTx; }
    QComboBox* typeCombo() const { return m_cwType; }
    QLabel* statusLabel() const { return m_cwStatus; }

private:
    void buildUI();
    void refresh();
    void refreshPorts();

    CwTastung*   m_cwKey{nullptr};

    QCheckBox*   m_cwEnabled{nullptr};
    QCheckBox*   m_cwKeyTx{nullptr};
    QComboBox*   m_cwPort{nullptr};
    QPushButton* m_cwRescan{nullptr};
    QComboBox*   m_cwType{nullptr};
    QSpinBox*    m_cwWpm{nullptr};
    QCheckBox*   m_cwSwap{nullptr};
    QCheckBox*   m_cwSidetone{nullptr};
    QSpinBox*    m_cwPitch{nullptr};
    QSlider*     m_cwVolume{nullptr};
    QSpinBox*    m_cwHang{nullptr};
    QSpinBox*    m_cwNoteDit{nullptr};
    QSpinBox*    m_cwNoteDah{nullptr};
    QSpinBox*    m_cwNoteKey{nullptr};
    QSpinBox*    m_cwNotePtt{nullptr};
    QLabel*      m_cwStatus{nullptr};
};

} // namespace Longpath
