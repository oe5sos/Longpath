#pragma once

// =================================================================
// src/gui/setup/KeyboardSetupPages.h  (Longpath)
// =================================================================
//
// Setup > Keyboard > Shortcuts: alle Tastenkuerzel sehen, suchen,
// aendern, zuruecksetzen, als Datei sichern und laden -- und der eine
// Schalter, der sie alle ausmacht.
//
// Bis 2026-11-10 ein Platzhalter (Suchfeld, Tabelle und vier Knoepfe,
// alle gesperrt mit „NYI"). Die Belegung selbst ist HotKeyBindings aus
// dem Upstream NereusSDR; die Seite ist Longpath-eigen und kleiner als
// Upstreams Keyboard-Seite (keine Tastaturkarte, keine Bindung an einen
// bestimmten Empfaenger).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Platzhalter durch die echte Seite ersetzt von Martin
//                 Fischer (OE5SOS), KI-gestuetzt ueber Anthropic Claude.
//   2026-11-17 — Gruppe „CW key (MIDI)" fuer die Morsetaste am HaliKey
//                 (src/core/cw/CwTastung), KI-gestuetzt ueber Anthropic
//                 Claude.
// =================================================================

#include "gui/SetupPage.h"

#include <QHash>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;

namespace Longpath {

class CwTastung;
class HotKeyBindings;

// ---------------------------------------------------------------------------
// Keyboard > Shortcuts
// ---------------------------------------------------------------------------
class KeyboardShortcutsPage : public SetupPage {
    Q_OBJECT

public:
    // bindings darf null sein (Pruefstand ohne Funkmodell): dann bleibt
    // die Seite stehen, aber gesperrt, mit einem Satz warum.
    // cwKey darf ebenfalls null sein: dann steht die Gruppe „CW key
    // (MIDI)" da, aber gesperrt.
    explicit KeyboardShortcutsPage(HotKeyBindings* bindings, CwTastung* cwKey = nullptr,
                                   QWidget* parent = nullptr);

    // Fuer den Pruefstand: die Taste einer Handlung so, wie die Tabelle
    // sie zeigt.
    QString shownKeysFor(const QString& id) const;
    QTreeWidget* table() const { return m_table; }
    QGroupBox* cwKeyGroup() const { return m_cwGroup; }
    QCheckBox* cwKeyTransmitCheck() const { return m_cwKeyTx; }
    QLabel* cwKeyStatus() const { return m_cwStatus; }

    // Die Taste einer Handlung so, wie der Betreiber sie liest (auf dem
    // Mac mit ⌘ und ⇧). Leer bleibt leer.
    static QString nativeKeyText(const QString& chord);

private:
    void buildUI();
    void refresh();
    void applyFilter(const QString& text);
    void updateButtons();
    QString selectedId() const;

    void changeSelected();
    void clearSelected();
    void resetSelected();
    void resetAll();
    void importFile();
    void exportFile();
    void say(const QString& text, bool warning);

    void buildCwKeyGroup();
    void refreshCwKey();
    void refreshCwPorts();

    HotKeyBindings* m_bindings{nullptr};
    CwTastung*      m_cwKey{nullptr};

    QGroupBox*   m_cwGroup{nullptr};
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

    QCheckBox*   m_enabledCheck{nullptr};
    QCheckBox*   m_fKeysCheck{nullptr};   // nur macOS sichtbar
    QCheckBox*   m_fKeysHoldCheck{nullptr};  // nur macOS sichtbar
    QLineEdit*   m_searchEdit{nullptr};
    QTreeWidget* m_table{nullptr};
    QLabel*      m_status{nullptr};
    QPushButton* m_editButton{nullptr};
    QPushButton* m_clearButton{nullptr};
    QPushButton* m_resetButton{nullptr};
    QPushButton* m_resetAllButton{nullptr};
    QPushButton* m_importButton{nullptr};
    QPushButton* m_exportButton{nullptr};

    QHash<QString, QTreeWidgetItem*> m_itemById;
};

} // namespace Longpath
