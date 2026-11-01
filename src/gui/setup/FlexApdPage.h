// no-port-check: Longpath-eigene Datei, kein Port.
// =================================================================
// src/gui/setup/FlexApdPage.h  (Longpath-eigen)
// =================================================================
//
// Setup -> Transmit -> Predistortion (Flex). Die Bedienung fuer die
// Vorverzerrung, die in der MASCHINE rechnet.
//
// Warum eine eigene Seite und nicht ein Reiter in PsForm: PsForm ist
// die Oberflaeche ueber WDSPs `calcc` — Sammelzaehler, Rueckkopplungs-
// pegel, gelernte Kurve. Davon gibt es hier nichts. Die Flex meldet
// drei Wahrheitswerte und je Sendeantenne einen Messpunkt, mehr nicht.
// Eine Anzeige, die Felder von PureSignal zeigt und sie nicht fuellen
// kann, waere eine Behauptung ueber eine Messung, die niemand gemacht
// hat.
//
// Die Seite zeigt sich nur, wenn die verbundene Maschine eine
// Vorverzerrung MELDET (`FlexApdState::bekannt()`); sonst steht dort
// ein Satz, warum nichts da ist. Eine 6000er-Maschine und jede aeltere
// Firmware schicken auf `sub apd all` nichts — und Abwesenheit soll
// als Abwesenheit dastehen, nicht als leere Regler.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-01 -- Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Claude Code. Felder und Wortlaute am 2026-11-01 an
//                 der FLEX-8400 gemessen, siehe core/FlexApdState.h.
// =================================================================

#pragma once

#include <QHash>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QPushButton;

namespace Longpath {

class RadioModel;

class FlexApdPage : public QWidget {
    Q_OBJECT

public:
    explicit FlexApdPage(RadioModel* model, QWidget* parent = nullptr);

private slots:
    /// Der Zustand der Maschine hat sich geaendert — alles neu zeichnen.
    void refreshFromRadio();

private:
    /// Die Messpunkt-Zeilen neu aufbauen. Sie haengen daran, welche
    /// Sendeantennen die Maschine fuehrt, und das weisz man erst,
    /// wenn sie es gesagt hat.
    void rebuildSamplerRows();

    RadioModel* m_model{nullptr};

    QLabel*      m_banner{nullptr};
    QCheckBox*   m_enable{nullptr};
    QGroupBox*   m_samplerBox{nullptr};
    QFormLayout* m_samplerForm{nullptr};
    QPushButton* m_reset{nullptr};
    QLabel*      m_learned{nullptr};

    /// Sendeantenne -> Auswahlfeld. Wird beim Neuaufbau geleert.
    QHash<QString, QComboBox*> m_samplerCombos;

    /// Waehrend wir die Felder aus dem Zustand fuellen, darf kein
    /// Feld einen Befehl ausloesen — sonst schickt jede Meldung der
    /// Maschine denselben Wert zurueck an die Maschine.
    bool m_updatingFromModel{false};
};

}  // namespace Longpath
