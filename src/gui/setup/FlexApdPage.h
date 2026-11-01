// no-port-check: Longpath-eigene Datei, kein Port.
// =================================================================
// src/gui/setup/FlexApdPage.h  (Longpath-eigen)
// =================================================================
//
// Setup -> Transmit -> Predistortion (Flex). Die Bedienung fuer die
// Vorverzerrung, die in der MASCHINE rechnet.
//
// ── Warum der Signalweg oben steht (Entwurf C, 2026-11-01) ──────────
//
// Drei Entwuerfe gingen als Blaetter hinaus; der Betreiber hat C
// gewaehlt. Begruendung, die auch fuer spaetere Aenderungen gilt: die
// schwere Frage bei einer Vorverzerrung ist nicht „ein oder aus",
// sondern WOHER die Maschine ihr eigenes Signal hoert. Ein Schalter
// und ein Auswahlfeld beantworten das nicht — ein Weg schon:
//
//   TX → PA → COUPLER → FEEDBACK, darunter die Rueckfuehrung als
//   Bogen. Ist die Vorverzerrung aus, ist der Bogen offen und grau.
//
// Die Beschriftung wandert mit der Wahl: ohne externen Messpunkt
// heiszt die Endstufe „own PA" und der Koppler „built-in", mit
// Messpunkt „external amp" und „amp sampler". Derselbe Weg, andere
// Beschriftung — keine zweite Seite fuer den zweiten Fall.
//
// Hausstil: Farbe traegt nur der AKTIVE Weg; alles andere ist Grau auf
// Fast-Schwarz. Die Messwerte unten stehen in der Messwert-Farbe.
//
// Warum eine eigene Seite und nicht ein Reiter in PsForm: PsForm ist
// die Oberflaeche ueber WDSPs `calcc` — Sammelzaehler,
// Rueckkopplungspegel, gelernte Kurve. Davon gibt es hier nichts. Die
// Flex meldet drei Wahrheitswerte und je Sendeantenne einen Messpunkt.
// Eine Anzeige, die PureSignal-Felder zeigt und sie nicht fuellen
// kann, waere eine Behauptung ueber eine Messung, die niemand gemacht
// hat.
//
// Die Bedienung zeigt sich nur, wenn die verbundene Maschine eine
// Vorverzerrung MELDET (`FlexApdState::bekannt()`); sonst steht dort
// ein Satz, warum nichts da ist. Eine 6000er-Maschine und jede
// aeltere Firmware schicken auf `sub apd all` nichts — und Abwesenheit
// soll als Abwesenheit dastehen, nicht als leere Regler.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-01 -- Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Claude Code. Felder und Wortlaute am 2026-11-01 an
//                 der FLEX-8400 gemessen, siehe core/FlexApdState.h.
//   2026-11-01 -- Entwurf C umgesetzt (Signalweg oben), nach Wahl des
//                 Betreibers am Entwurfsblatt.
// =================================================================

#pragma once

#include <QHash>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QFrame;
class QLabel;
class QPushButton;

namespace Longpath {

class RadioModel;

/// Der Signalweg als Bild: TX → PA → Koppler → Rueckweg, mit der
/// Rueckfuehrung als Bogen. Kein Q_OBJECT — das Ding zeichnet nur,
/// es meldet nichts.
class FlexApdFlowWidget : public QWidget {
public:
    explicit FlexApdFlowWidget(QWidget* parent = nullptr);

    /// `laeuft` faerbt den Weg, `externerMesspunkt` tauscht die
    /// Beschriftung von Endstufe und Koppler, `messpunkt` steht in der
    /// letzten Station (z. B. „XVTA").
    void setZustand(bool laeuft, bool externerMesspunkt, const QString& messpunkt);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool    m_laeuft{false};
    bool    m_extern{false};
    QString m_messpunkt;
};

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

    QFrame*            m_karte{nullptr};
    QLabel*            m_banner{nullptr};
    QCheckBox*         m_enable{nullptr};
    FlexApdFlowWidget* m_flow{nullptr};
    QFormLayout*       m_samplerForm{nullptr};
    QWidget*           m_samplerHost{nullptr};
    QPushButton*       m_reset{nullptr};
    QLabel*            m_messwerte{nullptr};
    QLabel*            m_helper{nullptr};

    /// Sendeantenne -> Auswahlfeld. Wird beim Neuaufbau geleert.
    QHash<QString, QComboBox*> m_samplerCombos;

    /// Waehrend wir die Felder aus dem Zustand fuellen, darf kein
    /// Feld einen Befehl ausloesen — sonst schickt jede Meldung der
    /// Maschine denselben Wert zurueck an die Maschine.
    bool m_updatingFromModel{false};
};

}  // namespace Longpath
