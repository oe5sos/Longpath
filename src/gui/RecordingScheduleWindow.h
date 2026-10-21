#pragma once
// =================================================================
// src/gui/RecordingScheduleWindow.h  (Longpath)
// =================================================================
//
// Das Fenster, in dem Zeitplan-Eintraege fuer die Aufnahme angelegt
// werden. Erreichbar ueber das Zahnrad im Kopf des Aufnahme-Applets --
// dort, wo im Hausstil die Optionen sitzen.
//
// WARUM ES DAS BRAUCHT. `RecordingScheduler` kann seit August adden,
// entfernen, an- und abschalten und sichert sich selbst. Bis zum
// 2026-10-21 wurde `add()` ausserhalb der Prueflaeufe aber NIRGENDS
// gerufen: das Applet zeigte „n entries", und es gab keinen Weg, einen
// anzulegen.
//
// WARUM EIN FENSTER UND KEIN POPUP. Ein `Qt::Popup` schliesst beim
// Klick daneben (so macht es CwDecoderApplet). Hier wird getippt, eine
// Zeit eingestellt und eine Liste gelesen -- das ueberlebt keinen
// Popup. Nicht modal, damit daneben weiter gefunkt werden kann.
//
// Der Betreiber hat die Form am 2026-10-21 offengelassen („ganz egal",
// „entscheide du"); gewaehlt ist Variante B aus den drei
// Entwurfsblaettern, weil das Zahnrad die Hausregel fuer Optionen ist
// und das Applet damit schmal bleibt.
//
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer,
//                geschrieben mit Claude Opus 5 (Anthropic).
// =================================================================

#include <QWidget>

class QComboBox;
class QDateTimeEdit;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace Longpath {

class RecordingModel;

class RecordingScheduleWindow : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingScheduleWindow(RecordingModel* model,
                                     QWidget* parent = nullptr);

    /// Fuer den Prueflauf: die Bedienteile beim Namen.
    QTableWidget*  tabelle() const { return m_table; }
    QPushButton*   anlegenKnopf() const { return m_add; }
    QDateTimeEdit* startFeld() const { return m_start; }
    QSpinBox*      minutenFeld() const { return m_minutes; }
    QComboBox*     artFeld() const { return m_kind; }

private:
    void buildUI();
    void refresh();
    void onAdd();
    void onRemove(int id);
    void onToggle(int id, bool on);

    RecordingModel* m_model{nullptr};

    QTableWidget*  m_table{nullptr};
    QDateTimeEdit* m_start{nullptr};
    QSpinBox*      m_minutes{nullptr};
    QComboBox*     m_kind{nullptr};
    QPushButton*   m_add{nullptr};
    QLabel*        m_note{nullptr};
};

}  // namespace Longpath
