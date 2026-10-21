#pragma once
// =================================================================
// src/gui/RecordingScheduleDialog.h  (Longpath)
// =================================================================
//
// Die Bedienung des Aufnahme-Zeitplans.
//
// WARUM ES SIE ERST JETZT GIBT. `RecordingScheduler` ist seit August
// gebaut, gespeichert und vom Modell gestartet -- aber es gab keinen
// Weg, einen Eintrag ANZULEGEN. Das Applet zeigte „none" und konnte
// nur zusehen; dieselbe Lage wie bei der Wiedergabe, nur eine Zeile
// tiefer. Wer eine Bake um 03:00 mitschneiden wollte, konnte es
// nicht sagen.
//
// Bewusst schlicht: eine Liste, darunter die Felder fuer einen neuen
// Eintrag. Kein Kalender, keine Wiederholung -- beides gibt es im
// Unterbau auch nicht, und eine Flaeche, die mehr verspricht als die
// Technik haelt, ist schlimmer als keine.
//
// Modification history (Longpath):
//   2026-10-22 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QDialog>

class QComboBox;
class QDateTimeEdit;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace Longpath {

class RecordingModel;

class RecordingScheduleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RecordingScheduleDialog(RecordingModel* model,
                                     QWidget* parent = nullptr);

    // Fuer den Prueflauf: die Bedienteile, ohne sie zu suchen.
    QListWidget*   listeForTest()  const { return m_liste; }
    QPushButton*   addButtonForTest() const { return m_addBtn; }
    QPushButton*   removeButtonForTest() const { return m_removeBtn; }
    QDateTimeEdit* startForTest()  const { return m_start; }
    QSpinBox*      dauerForTest()  const { return m_dauer; }
    QComboBox*     artForTest()    const { return m_art; }

private:
    void buildUI();
    void refresh();
    void onAdd();
    void onRemove();

    RecordingModel* m_model{nullptr};

    QListWidget*   m_liste{nullptr};
    QDateTimeEdit* m_start{nullptr};
    QSpinBox*      m_dauer{nullptr};
    QComboBox*     m_art{nullptr};
    QPushButton*   m_addBtn{nullptr};
    QPushButton*   m_removeBtn{nullptr};
    QLabel*        m_hinweis{nullptr};
};

}  // namespace Longpath
