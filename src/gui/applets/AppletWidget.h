// =================================================================
// src/gui/applets/AppletWidget.h  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-18 — Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Shared applet base class. Title-bar gradient, slider-row,
//                 and toggle-button helpers extracted from the AetherSDR
//                 `src/gui/AppletPanel.{h,cpp}` styling pattern (already
//                 registered for AppletPanelWidget in Bucket A); this base
//                 class hoists that styling up so every Longpath applet
//                 (Cat/Cwx/Dvk/Tuner/Eq/Fm/Tx/Rx/PhoneCw/Diversity/Digital/
//                 PureSignal) inherits identical visuals without
//                 duplicating the code.
// =================================================================

#pragma once
#include <QWidget>
#include <QPointer>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

class QSlider;
class QPushButton;
class QFrame;
class QHBoxLayout;
class QLabel;

namespace Longpath {

class RadioModel;
class SliceModel;

class AppletWidget : public QWidget {
    Q_OBJECT
public:
    explicit AppletWidget(RadioModel* model, QWidget* parent = nullptr);
    ~AppletWidget() override = default;

    virtual QString appletId() const = 0;
    virtual QString appletTitle() const = 0;
    virtual QIcon appletIcon() const;
    virtual void syncFromModel() = 0;

    // Optional extended-settings surface, reached from the shared ⚙
    // button in the grid cell's title bar (top-right of every applet —
    // GridCellWidget::buildCellButtons). Most applets expose everything
    // inline and have nothing extra to show; the default reports that,
    // and the title bar hides its ⚙ for them rather than showing a
    // button that opens nothing. An applet with a deeper settings
    // surface (e.g. TxApplet's fine TX-settings popup) overrides both.
    virtual bool hasExtendedSettings() const { return false; }
    virtual void openExtendedSettings() {}

    // --- Empfaengerbindung (Longpath, 2026-10-09) -----------------------
    //
    // Bis hierher hingen die Decoder-Applets am AKTIVEN Empfaenger
    // (`RadioModel::activeSlice()`). Wer den Empfaenger wechselte, nahm
    // den Decoder mit -- RX1 auf Sprache lassen und RX2 nebenher CW
    // dekodieren ging nicht.
    //
    // Mit der Bindung bekommt jedes Applet eine eigene Wahl: dem
    // aktiven folgen (Vorgabe, also genau das bisherige Verhalten) oder
    // an einem festen Empfaenger haengen bleiben. Die Wahl liegt im
    // geteilten Zahnrad der Zelle (`openExtendedSettings`), der Zustand
    // steht in der Titelleiste -- man soll sehen, welcher Empfaenger
    // dekodiert wird, ohne ein Menue aufzuklappen.

    /// Wahl "dem aktiven Empfaenger folgen".
    static constexpr int kFollowsActive = -1;

public:
    /// Die gewaehlte Empfaenger-KENNUNG, oder `kFollowsActive`.
    ///
    /// Eine Kennung, keine Listenposition, und das ist der Kern der
    /// Sache: Positionen rutschen. Wer "RX2" waehlt und dann RX1
    /// loescht, haengt mit einer Position ploetzlich an einem anderen
    /// Geraet -- vom Pruefstand gefangen
    /// (`placeholderWhenTheFIXEDReceiverIsRemoved`). Kennungen bleiben
    /// (`RadioModel::sliceById`: "stable for the life of a slice"), und
    /// Martin meint beim Waehlen den Empfaenger, nicht den Listenplatz.
    /// Die Beschriftung "RX2" rechnet sich aus der aktuellen Position.
    int receiverChoice() const { return m_receiverChoice; }
    /// Setzt die Wahl (eine Empfaenger-Kennung) und merkt sie sich unter
    /// `appletId()`.
    void setReceiverChoice(int sliceId);
    /// Der Empfaenger, der jetzt gilt: der gewaehlte, oder der aktive,
    /// wenn die Wahl `kFollowsActive` ist. Nullptr, wenn es ihn nicht
    /// (mehr) gibt -- ein Empfaenger kann verschwinden, waehrend ein
    /// Applet auf ihn zeigt.
    SliceModel* boundSlice() const;
    /// Kurzform fuer die Titelleiste: leer beim Folgen, sonst "RX2".
    QString receiverChoiceLabel() const;

signals:
    /// Der gebundene Empfaenger hat gewechselt -- entweder weil die Wahl
    /// geaendert wurde, oder weil der aktive wechselte und wir ihm
    /// folgen.
    void boundSliceChanged(SliceModel* slice);

protected:
    /// Liest die gemerkte Wahl und haengt sich an
    /// `RadioModel::activeSliceChanged`. Aus dem Konstruktor der
    /// abgeleiteten Klasse aufrufen, sobald `appletId()` benutzbar ist.
    void initReceiverBinding();
    /// Ein fertiges Menue zur Empfaengerwahl, zum Einhaengen in das
    /// Zahnrad-Popup der abgeleiteten Klasse.
    QWidget* receiverChoiceWidget(QWidget* parent);

    // Call from subclass constructor to add the gradient title bar
    QWidget* appletTitleBar(const QString& text);
    /// Schreibt den Zusatz hinter den Titel ("CW DECODER · RX2").
    void setTitleBarSuffix(const QString& suffix);

    // Create standard slider row: [Label(labelWidth) | Slider(stretch) | ValueLabel(36px)]
    QHBoxLayout* sliderRow(const QString& label, QSlider* slider,
                           QLabel* valueLabel, int labelWidth = 62);

    // Button factories with checked-state colors from StyleConstants
    QPushButton* styledButton(const QString& text, int w = -1, int h = 22);
    QPushButton* greenToggle(const QString& text, int w = -1, int h = 22);
    QPushButton* blueToggle(const QString& text, int w = -1, int h = 22);
    QPushButton* amberToggle(const QString& text, int w = -1, int h = 22);

    // Inset value display label (dark background, subtle border)
    QLabel* insetValue(const QString& text = {}, int w = 40);

    // Horizontal divider line
    QFrame* divider();

    // QPointer statt rohem Zeiger (2026-09-25): das Hauptfenster baut das
    // RadioModel VOR seinen Kind-Widgets ab, die Applets ueberleben es also
    // um einen Augenblick. Jeder Applet-Destruktor, der dann noch
    // m_model->... anfasste, las freigegebenen Speicher -- AddressSanitizer
    // in tst_quit_leaves_no_pending_deletes: heap-use-after-free in
    // RadioModel::audioEngine() aus ~RttyDecoderApplet(); in der CI als
    // SIGSEGV/SIGBUS beim Beenden (#83, #90). Der QPointer wird null, sobald
    // das Modell weg ist; die vorhandenen "if (m_model && ...)"-Pruefungen
    // greifen damit von selbst.
    QPointer<RadioModel> m_model;
    bool m_updatingFromModel = false;

private:
    int      m_receiverChoice{kFollowsActive};
    QString  m_titleBarText;
    QPointer<QLabel> m_titleBarLabel;
    /// Der zuletzt gemeldete Empfaenger -- damit `boundSliceChanged`
    /// nur feuert, wenn sich wirklich etwas aendert.
    QPointer<SliceModel> m_lastBound;
    /// Prueft, ob sich `boundSlice()` geaendert hat, und meldet es.
    void refreshBoundSlice();
};

} // namespace Longpath
