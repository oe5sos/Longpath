// =================================================================
// src/gui/applets/eq/StripEqPanel.h  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/gui/StripEqPanel.h at 31b29583
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 §5.
//
// Ported at the bench's request to reproduce AetherSDR's equaliser
// exactly — its display and its behaviour — rather than to approximate
// it. The DSP it drives, core/strip/ClientEq, was already a verbatim
// port of the same upstream, so the pair are back together.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Ported to NereusSDR by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 NereusSDR; include paths rebased onto
//                 core/strip/ and gui/applets/eq/. Behaviour unchanged.
//   2026-08-11 — Undo/Redo members + afterHistoryStep()/
//                 refreshHistoryButtons() added (NereusSDR-original,
//                 backed by EqHistory/EqHost). By Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#pragma once

#include "gui/applets/eq/ClientEqApplet.h"  // for Path enum
#include <QWidget>
#include <memory>

class QComboBox;
class QLabel;
class QPushButton;
class QTimer;

namespace Longpath {

class EqHost;
class ClientEqEditorCanvas;
class ClientEqFftAnalyzer;
class ClientEqIconRow;
class ClientEqOutputFader;
class ClientEqParamRow;

// Floating editor window for the client-side parametric EQ. One single
// instance lives on MainWindow; calling showForPath() swaps its bound
// ClientEq between RX and TX and raises the window. Geometry persists
// across shows via AppSettings (`StripEqPanelGeometry` key).
//
// Phase B.2 layout: title bar, path-indicator strip, large interactive
// curve canvas. Phases B.3+ add the filter-type icon row at the top,
// bottom parameter-text row with per-band columns, master gain knob on
// the right, and the FFT analyzer overlay inside the canvas.
class StripEqPanel : public QWidget {
    Q_OBJECT

public:
    explicit StripEqPanel(EqHost* engine, QWidget* parent = nullptr);
    ~StripEqPanel() override;

    // Switch the editor to the given path and show / raise the window.
    // Safe to call whether or not the window is currently visible.
    void showForPath(ClientEqApplet::Path path);

    // Push the radio's TX low/high filter cutoffs to the canvas as
    // dashed yellow guide lines.  No-op when the editor's current path
    // is RX.  Pass 0 for either edge to suppress that guide.
    void setTxFilterCutoffs(int lowHz, int highHz);

    // Push the active RX slice's filter passband (audio-frequency
    // domain) to the canvas.  No-op when the editor's current path is TX.
    // Cached so RX → TX → RX path swaps restore the correct guides.
    void setRxFilterCutoffs(int audioLowHz, int audioHighHz);

    // Reload every visual control (icon row, canvas, param row, family
    // combo, bypass button) from the engine's current band state.
    // Used after a preset import / load writes new values into the
    // engine without going through the canvas's own gestures, so the
    // text labels would otherwise stay stale.
    void refreshFromEngine();

signals:
    // Fired when the bypass button is toggled in the editor. The docked
    // applet subscribes so its Enable toggle stays in sync — both widgets
    // read/write the same ClientEq::enabled flag underneath.
    void bypassToggled(ClientEqApplet::Path path, bool bypassed);

    // Fired live during a cutoff-line drag.  Audio-domain Hz; MainWindow
    // converts back to TX-filter writes (path = Tx) or active-slice
    // filter writes with mode-aware offset reflection (path = Rx).
    void cutoffsDragRequested(ClientEqApplet::Path path,
                              int audioLowHz, int audioHighHz);

protected:
    void closeEvent(QCloseEvent* ev) override;
    void moveEvent(QMoveEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;
    void showEvent(QShowEvent* ev) override;
    void hideEvent(QHideEvent* ev) override;

private:
    /// Das Rechteck in die Einstellungen schreiben. Wird NICHT direkt
    /// aus moveEvent/resizeEvent gerufen — dazu siehe
    /// m_geometrySaveTimer.
    void saveGeometryToSettings();
    void restoreGeometryFromSettings();

    // Fans a band-selection change out to icon row, canvas, and param row
    // so all three views share the same highlighted column / handle.
    void syncSelection(int idx);

    // Refresh bypass-button state from the bound ClientEq's enabled flag.
    void syncBypassFromEq();

    // Pull latest post-EQ samples from AudioEngine, run the FFT, push
    // the smoothed bins into the curve widget. Called by m_fftTimer.
    void tickFftAnalyzer();

    // ── Undo ─────────────────────────────────────────────────────────
    //
    // The stack itself lives in EqHost, because that is the one object
    // every mutation already passes through. These two are the panel's
    // side of it.
    //
    // afterHistoryStep() reloads every view from the engine rather than
    // trying to update the ones it thinks changed. An undo can move any
    // band, the count, the master gain and the filter family at once,
    // and a partial refresh is how a panel ends up showing a curve the
    // DSP is not playing.
    void afterHistoryStep();
    void refreshHistoryButtons();

    EqHost*               m_audio{nullptr};
    ClientEqApplet::Path       m_path{ClientEqApplet::Path::Rx};
    int                        m_txFilterLowCutHz{0};
    int                        m_txFilterHighCutHz{0};
    int                        m_rxFilterLowCutHz{0};
    int                        m_rxFilterHighCutHz{0};
    int                        m_savedSmoothingFraction{96};
    QString                    m_savedReferenceCurvePreset;
    // The frameless title bar carries the active path label (e.g.
    // "Longpath Parametric EQ — TX").  Held as a void* + cast at use
    // site to keep the inline EditorFramelessTitleBar class out of the
    // public header.
    QWidget*                   m_titleBar{nullptr};
    QComboBox*                 m_familyCombo{nullptr};
    QPushButton*               m_bypass{nullptr};
    QPushButton*               m_undoBtn{nullptr};
    QPushButton*               m_redoBtn{nullptr};
    ClientEqIconRow*           m_iconRow{nullptr};
    ClientEqEditorCanvas*      m_canvas{nullptr};
    ClientEqParamRow*          m_paramRow{nullptr};
    ClientEqOutputFader*       m_outFader{nullptr};
    QTimer*                    m_fftTimer{nullptr};

    // Zusammenfassung für das Schreiben der Fenstergeometrie
    // (2026-08-16). moveEvent und resizeEvent riefen bis dahin direkt
    // saveGeometryToSettings(), und dessen letzte Zeile ist
    // AppSettings::save() — also: Sicherungskopie der Datei anlegen,
    // umbenennen, die VOLLSTÄNDIGE XML-Datei neu schreiben, fsync. Bei
    // rund 6200 Einstellungen. Je Bildpunkt eines Fensterzuges.
    //
    // Jetzt: Ende der Geste in den Speicher, Beenden auf die Platte —
    // dasselbe Muster wie ContainerWidget::endDrag() und
    // AppletPanelWidget::appletsReordered. closeEvent/hideEvent
    // schreiben sofort durch, damit ein Zug, dem gleich das Schliessen
    // folgt, nicht in der Wartezeit verlorengeht.
    QTimer*                    m_geometrySaveTimer{nullptr};
    static constexpr int       kGeometrySaveDelayMs = 400;
    std::unique_ptr<ClientEqFftAnalyzer> m_fftAnalyzer;
    bool                       m_restoring{false};
};

} // namespace Longpath
