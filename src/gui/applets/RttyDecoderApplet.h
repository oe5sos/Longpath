// =================================================================
// src/gui/applets/RttyDecoderApplet.h  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR -- GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       -- per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a structural derivative of AetherSDR's RTTY panel
//   (the control set on PanadapterApplet -- text output, mark/space
//   level meters, lock/SNR status, sensitivity slider -- backed by
//   RttyDecoder/RttyDecoderSensitivity). The widget tree itself is new
//   Longpath code against this project's own AppletWidget base class
//   (Longpath has no PanadapterApplet equivalent to port line-by-line),
//   but the set of controls and what each one does is carried over.
//   No Thetis equivalent exists (see RttyDecoder.h for the full
//   sole-source rationale).
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 SS5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-06 -- Created for Longpath by OE5SOS with AI-assisted
//                 transformation via Anthropic Claude Code. Mark/Shift
//                 are deliberately READ-ONLY display here, not editable
//                 controls: SliceModel already owns rttyMarkHz/
//                 rttyShiftHz (Thetis-sourced) and RxApplet's own
//                 RttyMarkShiftContainer already edits them on the VFO
//                 flag -- this applet reads them live instead of
//                 duplicating a second set of +/- buttons.
// =================================================================

#pragma once
#include "AppletWidget.h"

#include <QPointer>

#include <memory>

class QLabel;
class QComboBox;
class QSlider;
class QPlainTextEdit;
class QPushButton;
class QLineEdit;

namespace Longpath {

class RadioModel;
class SliceModel;
class RttyDecoder;
class AudioTapRing;
class HGauge;

// Native RTTY (Baudot/ITA2) decoder display. Visible only when the active
// slice's mode is DSPMode::DIGL (RTTY is a DIGL submode -- see
// RxApplet::applyModeVisibility's own documented rule, "RTTY -> NUR DIGL",
// which this applet's mode gate matches exactly).
//
// Controls:
//   1. Status row  -- lock capsule, SNR (monospace), live Mark/Shift Hz
//                     read from the bound slice (not editable here)
//   2. Level row   -- Mark/Space envelope meters (HGauge, gradient bars
//                     per HAUSSTIL.md, not LED chains)
//   3. Text output -- read-only, monospace, capped scrollback
//   4. Baud rate combo, reverse-polarity toggle, sensitivity slider, Clear
class RttyDecoderApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit RttyDecoderApplet(RadioModel* model, QWidget* parent = nullptr);
    ~RttyDecoderApplet() override;

    QString appletId()    const override { return QStringLiteral("RttyDecoder"); }
    QString appletTitle() const override { return QStringLiteral("RTTY Decoder"); }
    void    syncFromModel() override;

    // Bound from MainWindow on construction and again on every active-slice
    // change (mirrors AudioEngine's other taps -- see RttyDecoder.h).
    void setSlice(SliceModel* slice);

    // ── Senden (2026-10-16) ──────────────────────────────────────────
    //
    // Der Dekoder hoerte seit 2026-09-06, und seit PR #257 gibt es mit
    // RttyTxSource (core/RttyTx.h) auch das Gegenstueck — nur kam man
    // nicht heran. Das holt das hier nach, Zug um Zug wie bei
    // Ft8Applet::sendeNachricht(): Text auf Toene legen, Sendeweg auf
    // die RTTY-Quelle legen, tasten, und nach der Laenge des Textes
    // alles zurueck.
    //
    // Was hier NICHT eingebaut ist, genau wie bei FT8: eine
    // Folgeschaltung. Kein Rundruf in Schleife, keine Antwort von
    // selbst. Ein Druck, eine Aussendung.
    //
    // Anders als FT8 wird nicht auf ein Fenster gewartet — RTTY hat
    // keine Fenstergrenzen. Gesendet wird sofort.
    void sendeText(const QString& text);
    void abbrechen();
    bool sendetGerade() const { return m_sendet; }

private slots:
    void onTextDecoded(const QString& text, float confidence);
    void onStatsUpdated(float markLevel, float spaceLevel, float snrDb, bool locked);

public:
    // ── Aus dem Textstrom ein Rufzeichen (2026-10-17) ───────────────
    //
    // RTTY ist kein Nachrichtenformat, sondern ein Zeichenstrom. Es
    // gibt keine Felder, keine Grenzen, kein „CQ OE5SOS JN78" mit
    // garantierten drei Feldern wie bei FT8. Es gibt Betriebsbrauch:
    //
    //     CQ CQ CQ DE DL1ABC DL1ABC DL1ABC K
    //     OE5SOS DE DL1ABC DL1ABC KN
    //
    // Was in beiden Faellen traegt, ist das Wort DE und das Feld
    // danach. Genau das liest diese Funktion, und ABSICHTLICH nicht
    // mehr: wer im Strom jedes rufzeichenaehnliche Feld einsammelt,
    // meldet Reihenfolgen wie „599 599 OE5SOS" als Station und fuellt
    // die Spotliste mit Unsinn. Ein falscher Spot ist schlimmer als
    // ein fehlender — er steht auf dem Panadapter und wird geglaubt.
    //
    // Das Feld muss ABGESCHLOSSEN sein, also ein Trennzeichen hinter
    // sich haben. Ohne diese Bedingung meldet der Lauf „DL1AB",
    // sobald das B angekommen ist, und „DL1ABC" eine Zehntelsekunde
    // spaeter — zwei Spots fuer eine Station, einer davon falsch.
    //
    // Gibt das Rufzeichen hinter dem LETZTEN abgeschlossenen DE
    // zurueck, oder leer. Statisch und ohne Fenster — der Prueflauf
    // braucht weder Scheibe noch Dekoder.
    static QString rufzeichenNachDe(const QString& strom);

#ifdef LONGPATH_BUILD_TESTS
    /// Einen Textschnipsel so hineingeben, wie ihn der Dekoder
    /// liefern wuerde — mit voller Zuversicht, damit die
    /// Empfindlichkeitsschwelle nicht mitspielt. Hier INLINE, nicht
    /// in der .cpp: eine Definition dort braeuchte denselben
    /// #ifdef-Waechter, und ohne ihn ist der Bau ohne Tests rot
    /// (Windows und Linux), waehrend er oertlich gruen aussieht.
    void speiseTextFuerTest(const QString& text) { onTextDecoded(text, 1.0f); }
#endif

private:
    void buildUI();
    void loadSettings();
    void saveSettings() const;
    void applyMarkShiftFromSlice();
    /// Steht ein abgeschlossenes Rufzeichen hinter DE im Puffer und
    /// ist es ein anderes als das letzte, geht ein Fund an
    /// RadioModel::onNativeRttyDecode.
    void meldeFundWennRufzeichen();
    void updateAudioTap();

    QPointer<SliceModel> m_slice;
    RttyDecoder* m_decoder{nullptr};
    std::unique_ptr<AudioTapRing> m_tapRing;
    class QTimer* m_pumpTimer{nullptr};

    QMetaObject::Connection m_markHzConn;
    QMetaObject::Connection m_shiftHzConn;

    // Row 1 -- status
    QLabel* m_lockCapsule{nullptr};
    QLabel* m_snrValue{nullptr};
    QLabel* m_markShiftInfo{nullptr};

    // Row 2 -- levels
    HGauge* m_markLevel{nullptr};
    HGauge* m_spaceLevel{nullptr};

    // Row 3 -- text
    QPlainTextEdit* m_textOutput{nullptr};
    int m_charCount{0};
    // Die letzten Zeichen, nur fuer die Rufzeichensuche — der Strom
    // selbst steht im Textfeld, aber dort auch gekuerzt und mit der
    // Gestaltung vermischt. Kurz gehalten: ein Rufzeichen hinter DE
    // steht innerhalb weniger Zeichen, und ein langer Puffer wuerde
    // alte Anrufe nach jedem neuen Zeichen erneut melden.
    QString m_rufPuffer;
    // Wen wir zuletzt gemeldet haben. SpotModel entdoppelt selbst,
    // aber ohne das hier geht bei jedem Zeichen ein Fund hinaus,
    // solange DL1ABC noch im Puffer steht.
    QString m_letzterGemeldeterRuf;
    /// Der Rapport aus onStatsUpdated, fuer den Fund.
    int m_letzterSnrDb{0};
    // Above this, drop characters whose confidence is too low to trust --
    // see RttyDecoderSensitivity.h for the slider-to-threshold mapping.
    int m_sensitivity{0};

    // Row 4 -- controls
    QComboBox*   m_baudCombo{nullptr};
    QPushButton* m_reverseBtn{nullptr};
    QSlider*     m_sensitivitySlider{nullptr};
    QLabel*      m_sensitivityValue{nullptr};
    QPushButton* m_clearBtn{nullptr};

    // Row 5 -- senden (2026-10-16)
    QLineEdit*   m_textFeld{nullptr};
    QPushButton* m_sendBtn{nullptr};
    QPushButton* m_stopBtn{nullptr};
    QLabel*      m_sendeStatus{nullptr};
    class QTimer* m_sendeEndeTimer{nullptr};
    bool m_sendet{false};
    // Welcher Mikrofonweg vor dem Senden eingestellt war; -1 heisst
    // „nicht umgelegt". Ohne das kaeme nach dem Senden nicht das
    // Mikrofon zurueck, sondern irgendetwas.
    int  m_quelleVorher{-1};

public:
    // Fuer den Pruefstand.
    QLineEdit*   textFeldForTest()  const { return m_textFeld; }
    QPushButton* sendButtonForTest() const { return m_sendBtn; }
    QPushButton* stopButtonForTest() const { return m_stopBtn; }
    QLabel*      sendeStatusForTest() const { return m_sendeStatus; }
    /// Die Aussendung ist um. Im Betrieb kommt das von einem Zeitgeber
    /// nach der Laenge des Textes — im Prueflauf waeren das Sekunden
    /// Warten je Zug.
    void sendeEndeForTest() { sendeEnde(); }

private:
    void sendeEnde();
};

} // namespace Longpath
