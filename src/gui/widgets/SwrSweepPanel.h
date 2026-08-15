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

#include <QPair>
#include <QWidget>
#include <functional>

class QTimer;

#include "core/SwrSweepController.h"
#include "core/antenna/Touchstone.h"

class QComboBox;
class QDoubleSpinBox;
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
    /// The power TUNE will actually use, and which control it came
    /// from. Both halves matter — see the note on tuneDrive().
    struct TuneDrive {
        int     watts{0};
        QString sourceLabel;     // "RF-Power-Regler", "Tune-Pwr-Regler", …
    };

    struct Backend {
        SwrSweepController*            controller{nullptr};
        const safety::BandPlanGuard*   guard{nullptr};
        std::function<DSPMode()>       txMode;      // current TX mode
        /// ── Not simply tunePowerForBand() ────────────────────────────
        ///
        /// TransmitModel has THREE possible sources for the tune drive
        /// and picks between them with tuneDrivePowerSource(); the
        /// default is DriveSlider, which means TUNE reads the RF Power
        /// slider and the Tune Pwr slider does nothing at all.
        ///
        /// The panel asked tunePowerForBand() unconditionally and so
        /// displayed — and gated on — a number the radio was not using.
        /// 2026-08-14 that sent the operator raising Tune Pwr from 1 W
        /// to 5 W twice while the actual drive stayed at the RF Power
        /// slider's 1 W, and produced the 0.01 W forward reading we
        /// spent three rounds chasing. He worked it out before I did.
        ///
        /// So the seam hands over the resolved value AND the name of
        /// the control it came from, because "5 W" on its own is what
        /// made this invisible.
        std::function<TuneDrive(Band)> tuneDrive;

        /// The coupler's raw ADC counts, forward and reverse, as last
        /// reported by the radio. Shown live beside the power so the
        /// operator can press TUNE and watch whether they move — which
        /// is the whole diagnosis in one glance, and took a day to
        /// arrive at without it.
        std::function<QPair<quint16, quint16>()> rawAdc;

    // ── Which coupler constants are actually in force ────────────────
    //
    // 2026-08-14. The status bar names the board the hardware reports
    // — an Anvelina Pro 3 announces itself as an OrionMkII, so it says
    // "OrionMKII" no matter what. The scaling, meanwhile, follows the
    // model override, which lives on a different page. Two different
    // answers to "what radio is this", neither of them the one that
    // decides the SWR.
    //
    // It matters more than it sounds. The two profiles differ in the
    // bridge ratio, 0.894 against 1.000, and worked through: at a true
    // SWR of 2.5 they disagree by 0.38, at 3.0 by 0.68. That is the
    // same size as the gap Martin sees against his VNA on 80 m. Below
    // about 1.5 they agree to within a few hundredths, which is why
    // this never showed up on a good antenna.
    //
    // So the panel says which profile the arithmetic is using, next to
    // the counts it is using it on.
    std::function<QString()> couplerProfile;

        /// ── Band, both ways ──────────────────────────────────────────
        ///
        /// "wenn ich bei Suite einen Bandwechsel mache, sollte dieser
        ///  auch automatisch im SDR sein oder umgekehrt."
        ///
        /// Right, and the alternative is worse than untidy: the panel
        /// could offer to sweep 80 m while the radio sat on 20, and the
        /// operator would read a curve for a band the antenna was never
        /// switched to.
        ///
        /// radioBand() is polled on the second that already refreshes
        /// the power label; setRadioBand() is called only when the
        /// operator picks from the combo. Neither fires while a sweep
        /// runs — changing band mid-sweep would abandon the measurement
        /// halfway and leave the transmitter on a frequency the plan
        /// did not choose.
        std::function<Band()>     radioBand;
        std::function<void(Band)> setRadioBand;
    };

    explicit SwrSweepPanel(QWidget* parent = nullptr);

    /// Wire the radio backend (MainWindow calls this once the
    /// RadioModel exists). Without it the Start button stays disabled
    /// with an explanatory status line.
    void setBackend(const Backend& backend);

    /// Strip the panel down to its control strip: band, points, power,
    /// coupler counts, Start/Stop and the status line. The chart and
    /// the trace list are hidden.
    ///
    /// The antenna window used to be two tabs — measure on one, read
    /// the result on the other — with a second, poorer chart on the
    /// measuring side. Asked for directly: "hätte ich gerne alles auf
    /// einem fenster". One window, one chart, and this panel becomes
    /// the row of controls above it.
    void setCompact(bool compact);

signals:
    /// A sweep finished with something worth analysing. Carries the
    /// measurement in the same shape a .s1p arrives in, so the file
    /// half of the window can analyse it identically — asked for
    /// directly: "der sweep sollte nach beendigung genauso wie das
    /// beispiel aussehen."
    ///
    /// Not emitted for a sweep that measured nothing, and not for one
    /// whose reverse channel never moved: neither is a measurement, and
    /// handing either to an analysis that will dutifully draw it is how
    /// a fabricated curve gets treated as data.
    void analysisReady(const NereusSDR::Sweep& sweep);

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
    /// Move the combo to whatever band the radio is on. Blocked while a
    /// sweep runs and while the operator has the list open.
    void followRadioBand();
    bool sweepRunning() const;
    void refreshTraceList();
    void exportCsv();

    Backend m_backend;

    QComboBox*    m_bandBox{nullptr};

    // ── Wo der Bereich war ───────────────────────────────────────────
    //
    // Hier standen ein „Band / Bereich"-Wähler und zwei Frequenzfelder.
    // Die Idee kam aus einer echten Frage:
    //
    //   „Wenn ich eine Endfed von 10 bis 160 m habe, möchte ich auch
    //    eingeben, von welcher Startfrequenz bis welcher Endfrequenz."
    //
    // Sie war richtig, und das Funkgerät ist das falsche Werkzeug dafür.
    // Zwischen den Bändern darf es nicht senden, also blieb die Strecke
    // dort leer, und heraus kam ein Diagramm mit Löchern, das aussah wie
    // eine durchgehende Messung. Ein Messgerät, das lückenhaft misst und
    // vollständig aussieht, ist schlimmer als eines, das die Messung
    // verweigert.
    //
    // Für 1,8 bis 30 MHz am Stück gibt es den VNA — dessen .s1p kommt
    // über „VNA-Referenz…" in dasselbe Diagramm und ist dort dann auch
    // durchgehend, weil sie durchgehend gemessen wurde.
    //
    // Entfernt am 2026-08-15 auf Wunsch von OE5SOS: „Die Mehrband bitte
    // weg. Die Solo-Band bitte lassen."

    QSpinBox*     m_pointsBox{nullptr};
    QLabel*       m_powerLabel{nullptr};
    QLabel*       m_couplerLabel{nullptr};
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
