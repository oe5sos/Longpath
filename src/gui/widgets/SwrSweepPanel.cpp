// =================================================================
// src/gui/widgets/SwrSweepPanel.cpp  (NereusSDR)
// =================================================================
//
// See SwrSweepPanel.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-13 — Created by Ralph Martin Fischer (OE5SOS),
//                 AI-assisted implementation via Anthropic Claude
//                 (Cowork).
// =================================================================

#include "SwrSweepPanel.h"
#include "gui/styles/ThemeQss.h"
#include "SwrChartWidget.h"

#include "core/AppSettings.h"
#include "core/safety/RegionSetting.h"
#include "core/antenna/RadioSweep.h"
#include "gui/StyleConstants.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QMessageBox>
#include <QTimer>

namespace NereusSDR {

namespace {

// The sweepable bands, in dial order. 60 m is deliberately absent
// (channelized — design doc §Deferred).
const Band kSweepBands[] = {
    Band::Band160m, Band::Band80m, Band::Band40m, Band::Band30m,
    Band::Band20m,  Band::Band17m, Band::Band15m, Band::Band12m,
    Band::Band10m,  Band::Band6m,
};

} // namespace

SwrSweepPanel::SwrSweepPanel(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void SwrSweepPanel::buildUi()
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(12, 10, 12, 10);
    col->setSpacing(8);

    // ── Row 1: band, points, power, start/stop ───────────────────────
    auto* row = new QHBoxLayout;
    row->setSpacing(7);

    auto* bandCap = new QLabel(QStringLiteral("BAND"), this);
    bandCap->setStyleSheet(Style::themed(QStringLiteral("color:#8090a0;font-size:11px;")));
    row->addWidget(bandCap);

    m_bandBox = new QComboBox(this);
    for (Band b : kSweepBands) {
        m_bandBox->addItem(bandLabel(b), static_cast<int>(b));
    }
    m_bandBox->setCurrentIndex(4);   // 20 m
    row->addWidget(m_bandBox);

    // ── Ein Band, und nur ein Band ───────────────────────────────────
    //
    // OE5SOS, 2026-08-15: „Die Mehrband bitte weg. Die Solo-Band bitte
    // lassen."
    //
    // Hier stand eine Auswahl „Band / Bereich" mit zwei Frequenzfeldern.
    // Der Bereich versprach eine Kurve über mehrere Bänder und konnte
    // sie nicht halten: zwischen den Bändern darf nicht gesendet werden,
    // also blieb die Strecke dort leer. Was herauskam, war ein Diagramm
    // mit Löchern, das aussah wie eine Messung. SwrSweepPlan::forRange
    // weist mehrbandige Bereiche seit dem 2026-08-15 zurück — die
    // Auswahl stand aber noch da und bot etwas an, das nicht mehr ging.
    //
    // Für 1,8 bis 30 MHz am Stück gibt es den VNA. Die gemessene .s1p
    // kommt über „VNA-Referenz…" ins selbe Diagramm.

    auto* ptsCap = new QLabel(QStringLiteral("PUNKTE"), this);
    ptsCap->setStyleSheet(Style::themed(QStringLiteral("color:#8090a0;font-size:11px;")));
    row->addWidget(ptsCap);

    m_pointsBox = new QSpinBox(this);
    m_pointsBox->setRange(SwrSweepPlan::kMinPoints, SwrSweepPlan::kMaxPoints);
    m_pointsBox->setValue(51);
    m_pointsBox->setToolTip(QStringLiteral(
        "Messpunkte über das Band. 51 ≈ 17 s Sweep; mehr Punkte = "
        "feinere Kurve, längere Sendezeit."));
    row->addWidget(m_pointsBox);

    m_powerLabel = new QLabel(this);
    m_powerLabel->setStyleSheet(Style::themed(QStringLiteral("color:#c8d8e8;")));
    row->addWidget(m_powerLabel);

    // ── The coupler, live ────────────────────────────────────────────
    //
    // Two unscaled ADC counts, refreshed every second. Press TUNE and
    // watch them: forward should climb, reverse should climb a little.
    // Whichever stays put is the fault, and it is visible in one glance
    // without running a sweep, reading a status line or finding the PA
    // Values page in Setup — which has shown these numbers all along
    // and which nobody found in a day of looking.
    m_couplerLabel = new QLabel(this);
    m_couplerLabel->setToolTip(QStringLiteral(
        "Die Rohwerte des Richtkopplers, 0…4095, ungerechnet. Beim "
        "Tasten müssen beide steigen: der Vorlauf deutlich, der "
        "Rücklauf je nach Anpassung wenig bis deutlich. Bleibt einer "
        "stehen, meldet das Gerät ihn nicht — und dann kann daraus "
        "auch kein SWR entstehen."));
    row->addWidget(m_couplerLabel);

    row->addStretch(1);

    m_startBtn = new QPushButton(QStringLiteral("▶ Sweep starten"), this);
    m_startBtn->setStyleSheet(Style::buttonBaseStyle());
    m_startBtn->setEnabled(false);
    row->addWidget(m_startBtn);

    m_stopBtn = new QPushButton(QStringLiteral("■ Stopp"), this);
    m_stopBtn->setStyleSheet(Style::buttonBaseStyle());
    m_stopBtn->setEnabled(false);
    row->addWidget(m_stopBtn);

    col->addLayout(row);

    // ── Row 2: progress + status ─────────────────────────────────────
    auto* row2 = new QHBoxLayout;
    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    row2->addWidget(m_progress, 1);
    col->addLayout(row2);

    m_status = new QLabel(QStringLiteral(
        "Kein Funkgerät verbunden — der Sweep braucht die laufende "
        "Verbindung. Während des Sweeps sendet das Gerät mit "
        "Tune-Leistung; nicht am VFO drehen."), this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet(Style::themed(QStringLiteral("color:#8090a0;")));
    col->addWidget(m_status);

    // ── Chart ────────────────────────────────────────────────────────
    m_chart = new SwrChartWidget(this);
    col->addWidget(m_chart, 1);

    // ── Trace list + actions ─────────────────────────────────────────
    auto* row3 = new QHBoxLayout;
    m_traceList = new QListWidget(this);
    m_traceList->setMaximumHeight(84);
    m_traceList->setToolTip(QStringLiteral(
        "Ein Eintrag pro Sweep. Haken = sichtbar. Doppelklick benennt um."));
    connect(m_traceList, &QListWidget::itemChanged, this,
            [this](QListWidgetItem* item) {
        const int idx = m_traceList->row(item);
        m_chart->setTraceVisible(idx, item->checkState() == Qt::Checked);
        m_chart->renameTrace(idx, item->text());
    });
    row3->addWidget(m_traceList, 1);

    auto* btnCol = new QVBoxLayout;
    m_removeTraceBtn = new QPushButton(QStringLiteral("Entfernen"), this);
    m_removeTraceBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(m_removeTraceBtn, &QPushButton::clicked, this, [this]() {
        const int idx = m_traceList->currentRow();
        if (idx >= 0) {
            m_chart->removeTrace(idx);
            refreshTraceList();
        }
    });
    btnCol->addWidget(m_removeTraceBtn);

    m_exportBtn = new QPushButton(QStringLiteral("CSV exportieren"), this);
    m_exportBtn->setStyleSheet(Style::buttonBaseStyle());
    connect(m_exportBtn, &QPushButton::clicked,
            this, &SwrSweepPanel::exportCsv);
    btnCol->addWidget(m_exportBtn);
    btnCol->addStretch(1);
    row3->addLayout(btnCol);

    col->addLayout(row3);

    // `activated`, not `currentIndexChanged`: this must fire for a
    // choice the OPERATOR made and stay silent when the poll below
    // moves the combo to follow the radio. Otherwise the two chase each
    // other round the dial.
    connect(m_bandBox, QOverload<int>::of(&QComboBox::activated), this,
            [this](int) {
        refreshTunePowerLabel();
        if (m_backend.setRadioBand && !sweepRunning()) {
            m_backend.setRadioBand(
                static_cast<Band>(m_bandBox->currentData().toInt()));
        }
    });
    connect(m_bandBox, &QComboBox::currentIndexChanged, this,
            [this](int) { refreshTunePowerLabel(); });

    // Die drei Schlüssel der alten Bereichsauswahl räumen wir mit ab.
    // Ein Schlüssel ohne Verbraucher ist genau das, was in Aufgabe #81
    // steht — sieben Einstellungen, die niemand liest.
    {
        AppSettings& s = AppSettings::instance();
        s.remove(QStringLiteral("SweepRangeMode"));
        s.remove(QStringLiteral("SweepRangeFromMHz"));
        s.remove(QStringLiteral("SweepRangeToMHz"));
    }

    // See the note in the header. Only runs while the tab is on screen.
    m_powerPoll = new QTimer(this);
    m_powerPoll->setInterval(1000);
    connect(m_powerPoll, &QTimer::timeout, this, [this]() {
        followRadioBand();          // the radio leads
        refreshTunePowerLabel();
    });
}


void SwrSweepPanel::setCompact(bool compact)
{
    // The chart still exists and still receives the live points — the
    // trace list, the CSV export and the comparison against earlier
    // sweeps all hang off it. It is simply not the thing on screen any
    // more; the window's own curve is.
    if (m_chart)          { m_chart->setVisible(!compact); }
    if (m_traceList)      { m_traceList->setVisible(!compact); }
    if (m_removeTraceBtn) { m_removeTraceBtn->setVisible(!compact); }
    if (m_exportBtn)      { m_exportBtn->setVisible(!compact); }
    if (compact) {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    }
}

void SwrSweepPanel::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    followRadioBand();           // open the window already on the right band
    refreshTunePowerLabel();     // right on the first frame, not a second later
    if (m_powerPoll) { m_powerPoll->start(); }
}

void SwrSweepPanel::hideEvent(QHideEvent* e)
{
    if (m_powerPoll) { m_powerPoll->stop(); }
    QWidget::hideEvent(e);
}

void SwrSweepPanel::setBackend(const Backend& backend)
{
    m_backend = backend;
    const bool ready = m_backend.controller != nullptr
                       && m_backend.guard != nullptr;
    m_startBtn->setEnabled(ready);
    if (!ready) {
        return;
    }

    m_status->setText(QStringLiteral(
        "Bereit. Der Sweep sendet mit Tune-Leistung innerhalb der "
        "Bandgrenzen; nicht am VFO drehen, solange er läuft. Die "
        "Tune-Leistung muss hoch genug sein, dass der Richtkoppler "
        "etwas anzeigt — siehe die Zahl links."));

    connect(m_startBtn, &QPushButton::clicked,
            this, &SwrSweepPanel::startClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        m_backend.controller->abortSweep(QStringLiteral("Vom Operator gestoppt"));
    });

    SwrSweepController* ctl = m_backend.controller;
    connect(ctl, &SwrSweepController::sweepStarted, this,
            [this](const SwrSweepPlan& plan) {
        m_startBtn->setEnabled(false);
        m_stopBtn->setEnabled(true);
        m_progress->setRange(0, plan.points);
        m_progress->setValue(0);
        const QString name = QStringLiteral("%1 %2")
            .arg(bandLabel(plan.band),
                 QDateTime::currentDateTime().toString(
                     QStringLiteral("HH:mm")));
        m_chart->beginLiveTrace(name, plan.startHz, plan.stopHz);
        m_status->setText(QStringLiteral(
            "Sweep läuft — %1, %2 Punkte …")
                .arg(bandLabel(plan.band)).arg(plan.points));

        // Tell the analysis half that the band under the needle has
        // changed, so it can step back rather than go on describing the
        // previous one in full confidence. See the signal's docstring.
        emit sweepStartedFor(bandLabel(plan.band),
                             static_cast<double>(plan.startHz),
                             static_cast<double>(plan.stopHz));
    });
    connect(ctl, &SwrSweepController::pointReady, this,
            [this](int, quint64 freqHz, double swr, double) {
        if (swr > 0.0) {
            m_chart->appendLivePoint(freqHz, swr);
        }
    });
    connect(ctl, &SwrSweepController::progressChanged, this,
            [this](int done, int) { m_progress->setValue(done); });
    connect(ctl, &SwrSweepController::sweepFinished, this,
            [this](const SwrSweepResult& result) {
        m_startBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);

        // points.size() counts the ATTEMPTS. A sweep where the bridge
        // read nothing comes back with fifty-one of them and not one
        // measurement, and the old test kept that as a trace: an entry
        // in the list, a tick beside it, and an empty chart. Count the
        // measurements.
        // A trace of fifty-one identical 1.00s is not a measurement and
        // must not be kept as one — saved, exported to CSV, compared
        // against next week's. Dropped for the same reason an empty one
        // is.
        if (result.validPoints() == 0 || result.reverseNeverMoved) {
            m_chart->dropLiveTrace();
        } else {
            m_chart->finishLiveTrace();
            // Hand the measurement to the analysis half of the window,
            // so a finished radio sweep reads like a loaded file:
            // band shaded and named, SWR at start / middle / end, the
            // usable span, the best match. Only for a sweep that is
            // actually a measurement — see the signal's docstring.
            emit analysisReady(RadioSweep::fromResult(result));
        }
        refreshTraceList();

        if (result.completed && result.reverseNeverMoved) {
            // ── The answer that looks right and is not ───────────────
            //
            // Every point 1.00, a flat line along the bottom, and a
            // "resonance" at whichever frequency happened to come
            // first. SWR is (1+γ)/(1−γ) with γ = √(rev/fwd): reverse
            // reading zero gives exactly 1.00 everywhere, always.
            //
            // A dead forward channel draws no curve and gets noticed.
            // A dead reverse channel draws a perfect one. This is the
            // only failure in the feature an operator would act on —
            // by leaving an antenna alone that needs work — so it is
            // said plainly rather than shown as a result.
            m_status->setText(QStringLiteral(
                "Vorlauf gemessen, Rücklauf nicht: rückwärtiger ADC "
                "ruhend %1, beim Senden höchstens %2 — er hat sich "
                "nicht bewegt.\n\n"
                "Damit ist jedes SWR hier rechnerisch 1,00 und keines "
                "gemessen. Die flache Linie bedeutet NICHT, dass die "
                "Antenne perfekt ist. Entweder liegt am Rücklaufzweig "
                "des Kopplers nichts an, oder das Gerät meldet ihn "
                "nicht.")
                    .arg(result.baselineRevRaw)
                    .arg(result.maxRevRaw));
        } else if (result.completed && result.validPoints() > 0) {
            const quint64 res = result.resonanceHz();
            m_status->setText(res > 0
                ? QStringLiteral("Fertig. Resonanz bei %1 MHz, SWR %2 "
                                 "(%3 von %4 Punkten gemessen).")
                      .arg(static_cast<double>(res) / 1e6, 0, 'f', 3)
                      .arg(result.minSwr(), 0, 'f', 2)
                      .arg(result.validPoints())
                      .arg(result.points.size())
                : QStringLiteral("Fertig, aber ohne verwertbares Minimum."));
        } else if (result.completed) {
            // Completed and measured nothing. Say the watts — "zu
            // niedrig?" was a guess, and the numbers were there all
            // along.
            //
            // The fork used to be `maxFwdW < kSilentBridgeW` — on the
            // watts. Same mistake as in the controller, found the same
            // way: an ADC pinned at 41 counts scales to a confident
            // 5 W, so the operator was told to turn the power up
            // against a fault that has nothing to do with power.
            //
            // Decide on the counts when there are counts. Watts are the
            // fallback for a run that never got any.
            const bool sawCounts =
                (result.maxFwdRaw > 0 || result.baselineRaw > 0);
            const bool adcAsleep =
                sawCounts
                    ? (result.maxFwdRaw
                       <= result.baselineRaw + SwrSweepController::kMinRawRise)
                    : (result.maxFwdW < SwrSweepController::kSilentBridgeW);
            m_status->setText(
                adcAsleep
                ? QStringLiteral(
                      "Der Richtkoppler hat nicht reagiert. ADC ruhend "
                      "%1, beim Senden höchstens %2 — der Wert ist "
                      "beim Tasten nicht gestiegen.\n\n"
                      "Die Ruhezahl kommt von deinem Gerät, kurz vor "
                      "dem Sweep, mit ausgeschaltetem Sender. Es geht "
                      "also nicht um zu wenig Leistung, sondern darum, "
                      "dass sich beim Tasten nichts geändert hat: "
                      "entweder entsteht keine HF, oder der Koppler "
                      "meldet sie nicht.\n\n"
                      "Ohne Senden messen geht über den Reiter "
                      "„Datei (VNA)“ — ein NanoVNA misst mit "
                      "Mikrowatt, weil er einen Empfänger benutzt "
                      "statt einer Diode.")
                      .arg(result.baselineRaw)
                      .arg(result.maxFwdRaw)
                : QStringLiteral(
                      "Kein einziger Punkt gemessen. Der Richtkoppler "
                      "meldete höchstens %1 W Vorlauf; ab %2 W ist die "
                      "Anzeige eine Messung. Tune-Leistung höher "
                      "stellen.")
                      .arg(result.maxFwdW, 0, 'f', 2)
                      .arg(SwrSweepController::kMinFwdW, 0, 'f', 1));
        } else {
            m_status->setText(QStringLiteral("Abgebrochen: %1")
                                  .arg(result.abortReason));
        }
    });
    connect(ctl, &SwrSweepController::reasonChanged, this,
            [this](const QString& reason) { m_status->setText(reason); });

    refreshTunePowerLabel();
}

void SwrSweepPanel::startClicked()
{
    if (!m_backend.controller || !m_backend.guard) {
        return;
    }
    const Band band =
        static_cast<Band>(m_bandBox->currentData().toInt());

    // ── Where the operator actually is ───────────────────────────────
    //
    // This read the key "BandPlanRegion" and defaulted to UnitedStates.
    // Nothing in the program has ever written that key — Setup →
    // General → Region writes the display string under "Region" — so
    // the default was the only value it ever saw, and an 80 m sweep in
    // Austria was clipped against the US plan and transmitted to
    // 4.000 MHz. See core/safety/RegionSetting.h.
    const safety::RegionChoice choice = safety::configuredRegion();
    const std::optional<safety::Region> region =
        choice.configured ? std::optional<safety::Region>(choice.region)
                          : std::nullopt;
    const DSPMode mode =
        m_backend.txMode ? m_backend.txMode() : DSPMode::USB;

    // ── Ein Band ─────────────────────────────────────────────────────
    //
    // Hier stand daneben ein Zweig für den Bereich über mehrere Bänder.
    // Er ist mit der Auswahl im Kopf des Fensters weggefallen;
    // SwrSweepPlan::forRange bleibt bestehen und weist mehrbandige
    // Bereiche zurück, wird von hier aus aber nicht mehr aufgerufen.
    SwrSweepPlan plan = SwrSweepPlan::forBand(band);
    plan.points = m_pointsBox->value();

    if (!plan.isValid()
        || !plan.clipToGuard(*m_backend.guard, region, mode)) {
        m_status->setText(QStringLiteral(
            "Band %1 ist mit dem aktuellen Bandplan/Modus nicht "
            "sweepbar.").arg(bandLabel(band)));
        return;
    }

    // Wie lange getastet wird, bevor getastet wird und nicht danach.
    // Das stand vorher nur im Bereichsmodus da — als hätte man bei
    // einem einzelnen Band ein Recht darauf, es nicht zu erfahren. 99
    // Punkte sind auch auf einem Band eine halbe Minute Sendezeit.
    {
        const double seconds =
            plan.points * (plan.settleMs + plan.dwellMs) / 1000.0;
        m_status->setText(QStringLiteral(
            "%1 bis %2 MHz, %3 Punkte, etwa %4 s Sendezeit.")
                .arg(plan.startHz / 1e6, 0, 'f', 3)
                .arg(plan.stopHz  / 1e6, 0, 'f', 3)
                .arg(plan.points)
                .arg(seconds, 0, 'f', 0));
    }

    // Say which plan trimmed it, and say when nobody told us. Silence
    // here is what let a US-clipped sweep look normal for weeks.
    if (!choice.configured) {
        m_status->setText(QStringLiteral(
            "Keine Region eingestellt (Einstellungen → General → "
            "Region). Der Sweep bleibt vorsichtshalber in dem Bereich, "
            "den JEDER Bandplan erlaubt: %1–%2 MHz.")
                .arg(plan.startHz / 1e6, 0, 'f', 3)
                .arg(plan.stopHz  / 1e6, 0, 'f', 3));
    }

    // ── Refuse before keying, not after ──────────────────────────────
    //
    // 2026-08-14 at the bench: Tune Pwr sat at 1 W, the sweep keyed all
    // fifty-one points, and the verdict arrived seventeen seconds later
    // as "keine gültigen Messpunkte (Vorlaufleistung zu niedrig?)". The
    // question mark is the tell — the panel had every number it needed
    // to know that BEFORE it transmitted, and asked instead of checking.
    //
    // The tune power is on screen two centimetres away. Read it.
    // ── No longer a refusal ──────────────────────────────────────────
    //
    // This blocked the sweep below kMinUsefulTuneW, and that figure was
    // MINE — I picked five watts as a plausible-sounding floor for a
    // coupler I have never measured. It then stood between the operator
    // and his own transmitter for half a morning while he was telling
    // me, correctly, that he wants to work at low power.
    //
    // A guard built on a number I invented is not a safety feature, it
    // is an opinion with a lock on it. The controller's dead-run abort
    // is the real protection: it is driven by what the bridge actually
    // reports and gives up after five points either way. So this warns
    // and gets out of the way.
    if (m_backend.tuneDrive) {
        const TuneDrive drive = m_backend.tuneDrive(band);
        if (drive.watts < SwrSweepController::kMinUsefulTuneW) {
            m_status->setText(QStringLiteral(
                "Läuft mit %1 W aus dem %2. Wenig für einen "
                "Richtkoppler — wenn nichts herauskommt, steht der "
                "Grund gleich hier.")
                    .arg(drive.watts).arg(drive.sourceLabel));
        }
    }

    m_backend.controller->startSweep(plan);
}

bool SwrSweepPanel::sweepRunning() const
{
    return m_backend.controller && m_backend.controller->isSweeping();
}

void SwrSweepPanel::followRadioBand()
{
    if (!m_backend.radioBand || sweepRunning()) { return; }
    // Not while the list is open: yanking the selection out from under
    // a hand that is halfway to choosing is worse than a second's lag.
    if (m_bandBox->view() && m_bandBox->view()->isVisible()) { return; }

    const int want = m_bandBox->findData(
        static_cast<int>(m_backend.radioBand()));
    // -1 means the radio is on something this panel cannot sweep — 60 m,
    // 6 m on some boards, a transverter. Leave the combo alone rather
    // than snapping it to an unrelated band.
    if (want < 0 || want == m_bandBox->currentIndex()) { return; }

    const QSignalBlocker block(m_bandBox);
    m_bandBox->setCurrentIndex(want);
    refreshTunePowerLabel();   // tune power is per band
}

void SwrSweepPanel::refreshTunePowerLabel()
{
    // The coupler readout rides the same one-second poll.
    if (m_couplerLabel) {
        if (m_backend.rawAdc) {
            const auto raw = m_backend.rawAdc();
            QString text =
                QStringLiteral("   Koppler-ADC:  VOR %1  ·  RÜCK %2")
                    .arg(raw.first, 4).arg(raw.second, 4);
            if (m_backend.couplerProfile) {
                const QString prof = m_backend.couplerProfile();
                if (!prof.isEmpty()) {
                    text += QStringLiteral("  ·  Profil %1").arg(prof);
                }
            }
            m_couplerLabel->setText(text);
            // Amber while either sits at the bottom of its range: that
            // is the state in which no SWR can be computed, and it
            // should be visible before a sweep rather than after.
            const bool quiet = (raw.first < 100 || raw.second < 20);
            m_couplerLabel->setStyleSheet(
                QStringLiteral("color:%1; font-family: monospace;")
                    .arg(QString::fromLatin1(
                        quiet ? Style::kTextSecondary : Style::kGreenText)));
        } else {
            m_couplerLabel->clear();
        }
    }

    if (!m_backend.tuneDrive) {
        m_powerLabel->clear();
        return;
    }
    const Band band =
        static_cast<Band>(m_bandBox->currentData().toInt());
    const TuneDrive drive = m_backend.tuneDrive(band);
    const int watts = drive.watts;

    // Two ways to have it wrong and only one of them was shown. Too
    // much power is a warning; too little is the reason the sweep
    // measures nothing, and it stayed invisible until after the radio
    // had transmitted fifty-one times.
    //
    // "zu wenig zum Messen — mindestens 5 W" was a false statement.
    // 2026-08-14: the label said it at 3 W while the sweep beside it had
    // just measured 51 of 51 points on 20 m. Five watts is a number I
    // picked for a coupler I have never measured, and the operator had
    // already told me twice that he works at low power — a QRP rig on
    // a summit tunes at one watt and has no choice about it.
    //
    // Whether the bridge can see the drive is not knowable from the
    // slider; it depends on the coupler, and only the measurement finds
    // out. So this warns that it MIGHT not be enough and says where the
    // answer will appear, instead of asserting a failure that has not
    // happened.
    QString note;
    QString colour = QString::fromLatin1(Style::kTextPrimary);
    if (watts < SwrSweepController::kMinUsefulTuneW) {
        note   = QStringLiteral("  ⚠ wenig — falls nichts herauskommt, "
                                "steht der Grund unten");
        colour = QString::fromLatin1(Style::kAmberText);
    } else if (watts > 10) {
        note   = QStringLiteral("  ⚠ >10 W");
        colour = QString::fromLatin1(Style::kAmberText);
    }
    m_powerLabel->setStyleSheet(QStringLiteral("color:%1;").arg(colour));
    // The source is named on the face of the label, not hidden in the
    // tooltip. Which slider feeds TUNE is a setting most operators have
    // never looked at, and not naming it is what made a whole morning
    // disappear into raising the wrong one.
    // Hier stand ein Zusatz „pro Band verschieden" für den Bereich über
    // mehrere Bänder. Der Sweep bleibt jetzt in einem Band, und in einem
    // Band gilt eine Tune-Leistung — der Hinweis ist mit der Sache
    // weggefallen, für die er galt.
    m_powerLabel->setText(QStringLiteral("Tune-Leistung: %1 W (%2)%3")
                              .arg(watts).arg(drive.sourceLabel)
                              .arg(note));

    m_powerLabel->setToolTip(QStringLiteral(
        "Die Leistung, mit der der Sweep sendet — dieselbe, die TUNE "
        "benutzt.\n\n"
        "Welcher Regler das ist, steht in Klammern: NereusSDR kennt "
        "drei Quellen für die Tune-Leistung (RF-Power-Regler, "
        "Tune-Pwr-Regler, fester Wert aus dem Setup), und in der "
        "Voreinstellung ist es der RF-Power-Regler — der Tune-Pwr-"
        "Regler tut dann gar nichts.\n\n"
        "Der Richtkoppler ist ein Dämpfungsglied vor einer Diode und "
        "hat unten eine Schwelle: unter %1 W Vorlauf ist die Anzeige "
        "Rauschen und der Punkt wird verworfen. Mehr als nötig bringt "
        "nichts — SWR ist ein Verhältnis und ändert sich mit der "
        "Leistung nicht.")
            .arg(SwrSweepController::kMinFwdW, 0, 'f', 1));
}

void SwrSweepPanel::refreshTraceList()
{
    m_traceList->blockSignals(true);
    m_traceList->clear();
    for (const SwrChartWidget::Trace& t : m_chart->traces()) {
        auto* item = new QListWidgetItem(t.name, m_traceList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable
                       | Qt::ItemIsEditable);
        item->setCheckState(t.visible ? Qt::Checked : Qt::Unchecked);
        item->setForeground(t.color);
    }
    m_traceList->blockSignals(false);
}

void SwrSweepPanel::exportCsv()
{
    const auto& traces = m_chart->traces();
    if (traces.isEmpty()) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("SWR-Sweeps exportieren"),
        QStringLiteral("swr-sweeps.csv"),
        QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Export"),
                             QStringLiteral("Datei nicht schreibbar."));
        return;
    }
    QTextStream out(&f);
    out << "trace,freq_hz,swr\n";
    for (const SwrChartWidget::Trace& t : traces) {
        for (const SwrSweepPoint& p : t.points) {
            if (p.swr > 0.0) {
                out << '"' << t.name << '"' << ',' << p.freqHz << ','
                    << QString::number(p.swr, 'f', 3) << '\n';
            }
        }
    }
}

} // namespace NereusSDR
