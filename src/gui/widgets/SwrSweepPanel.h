// =================================================================
// src/gui/widgets/SwrSweepPanel.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. The "Sweep (Radio)" tab of the Antenna window:
// band picker, Start/Stop, progress, the SWR chart, and the trace
// list. Everything radio-shaped arrives through setBackend() — with
// no backend the tab explains itself and stays inert, so the file
// half of the window keeps working without a connection.
//
// Design doc: docs/architecture/2026-08-13-swr-sweep-analyzer-design.md
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-13 — Created by Ralph Martin Fischer (OE5SOS),
//                 AI-assisted implementation via Anthropic Claude
//                 (Cowork).
// =================================================================

#pragma once

#include <QWidget>
#include <functional>

class QTimer;

#include "core/SwrSweepController.h"

class QComboBox;
class QSpinBox;
class QPushButton;
class QProgressBar;
class QLabel;
class QListWidget;

namespace NereusSDR {

class SwrChartWidget;

class SwrSweepPanel : public QWidget
{
    Q_OBJECT

public:
    struct Backend {
        SwrSweepController*            controller{nullptr};
        const safety::BandPlanGuard*   guard{nullptr};
        std::function<DSPMode()>       txMode;             // current TX mode
        std::function<int(Band)>       tunePowerForBand;   // watts
    };

    explicit SwrSweepPanel(QWidget* parent = nullptr);

    /// Wire the radio backend (MainWindow calls this once the
    /// RadioModel exists). Without it the Start button stays disabled
    /// with an explanatory status line.
    void setBackend(const Backend& backend);

protected:
    // ── The label was lying ──────────────────────────────────────────
    //
    // 2026-08-14: the operator raised Tune Pwr from 1 W to 5 W and the
    // panel went on reading "Tune-Leistung: 1 W ⚠ zu wenig zum Messen".
    // It was drawn once, at construction and on a band change, and the
    // tune power lives in TransmitModel where it changes whenever the
    // operator touches the slider.
    //
    // That is the same fault as the character name three commits ago,
    // and worse here: this is the number I had just told the operator
    // to trust. The pre-flight check reads it live and refused
    // correctly; only the label was stale, which is the combination
    // that makes a panel look broken.
    //
    // A one-second poll while visible, rather than a signal: the value
    // arrives through a std::function seam that has no signal to
    // connect to, and inventing one to avoid a timer that costs an
    // integer read per second is the wrong trade.
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private:
    void buildUi();
    void startClicked();
    void refreshTunePowerLabel();
    void refreshTraceList();
    void exportCsv();

    Backend m_backend;

    QComboBox*    m_bandBox{nullptr};
    QSpinBox*     m_pointsBox{nullptr};
    QLabel*       m_powerLabel{nullptr};
    QPushButton*  m_startBtn{nullptr};
    QPushButton*  m_stopBtn{nullptr};
    QProgressBar* m_progress{nullptr};
    QLabel*       m_status{nullptr};
    SwrChartWidget* m_chart{nullptr};
    QListWidget*  m_traceList{nullptr};
    QPushButton*  m_removeTraceBtn{nullptr};
    QPushButton*  m_exportBtn{nullptr};
    QTimer*       m_powerPoll{nullptr};
};

} // namespace NereusSDR
