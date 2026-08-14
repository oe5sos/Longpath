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
#include "SwrChartWidget.h"

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"

#include <QComboBox>
#include <QSpinBox>
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
    bandCap->setStyleSheet(QStringLiteral("color:#8090a0;font-size:10px;"));
    row->addWidget(bandCap);

    m_bandBox = new QComboBox(this);
    for (Band b : kSweepBands) {
        m_bandBox->addItem(bandLabel(b), static_cast<int>(b));
    }
    m_bandBox->setCurrentIndex(4);   // 20 m
    row->addWidget(m_bandBox);

    auto* ptsCap = new QLabel(QStringLiteral("PUNKTE"), this);
    ptsCap->setStyleSheet(QStringLiteral("color:#8090a0;font-size:10px;"));
    row->addWidget(ptsCap);

    m_pointsBox = new QSpinBox(this);
    m_pointsBox->setRange(SwrSweepPlan::kMinPoints, SwrSweepPlan::kMaxPoints);
    m_pointsBox->setValue(51);
    m_pointsBox->setToolTip(QStringLiteral(
        "Messpunkte über das Band. 51 ≈ 17 s Sweep; mehr Punkte = "
        "feinere Kurve, längere Sendezeit."));
    row->addWidget(m_pointsBox);

    m_powerLabel = new QLabel(this);
    m_powerLabel->setStyleSheet(QStringLiteral("color:#c8d8e8;"));
    row->addWidget(m_powerLabel);

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
    m_status->setStyleSheet(QStringLiteral("color:#8090a0;"));
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

    connect(m_bandBox, &QComboBox::currentIndexChanged, this,
            [this](int) { refreshTunePowerLabel(); });

    // See the note in the header. Only runs while the tab is on screen.
    m_powerPoll = new QTimer(this);
    m_powerPoll->setInterval(1000);
    connect(m_powerPoll, &QTimer::timeout,
            this, &SwrSweepPanel::refreshTunePowerLabel);
}

void SwrSweepPanel::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
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
        if (result.validPoints() == 0) {
            m_chart->dropLiveTrace();
        } else {
            m_chart->finishLiveTrace();
        }
        refreshTraceList();

        if (result.completed && result.validPoints() > 0) {
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
            m_status->setText(
                result.maxFwdW < SwrSweepController::kSilentBridgeW
                ? QStringLiteral(
                      "Kein einziger Punkt gemessen. Höchstens %1 W, "
                      "roher ADC-Höchstwert %2 von 4095 — das ist der "
                      "Wert, den das Gerät selbst gesendet hat. "
                      "Wiederhole den Sweep mit deutlich mehr Leistung: "
                      "steigt die Rohzahl nicht mit, entsteht keine HF "
                      "oder der Koppler meldet nichts, und weder "
                      "Skalierung noch Sweep sind daran schuld.")
                      .arg(result.maxFwdW, 0, 'f', 2)
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

    SwrSweepPlan plan = SwrSweepPlan::forBand(band);
    plan.points = m_pointsBox->value();

    const int regionInt = AppSettings::instance()
        .value(QStringLiteral("BandPlanRegion"),
               QString::number(static_cast<int>(
                   safety::Region::UnitedStates)))
        .toInt();
    const auto region = static_cast<safety::Region>(regionInt);
    const DSPMode mode =
        m_backend.txMode ? m_backend.txMode() : DSPMode::USB;

    if (!plan.isValid()
        || !plan.clipToGuard(*m_backend.guard, region, mode)) {
        m_status->setText(QStringLiteral(
            "Band %1 ist mit dem aktuellen Bandplan/Modus nicht "
            "sweepbar.").arg(bandLabel(band)));
        return;
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
    if (m_backend.tuneDrive) {
        const TuneDrive drive = m_backend.tuneDrive(band);
        if (drive.watts < SwrSweepController::kMinUsefulTuneW) {
            // Names the control, not just the number. "Tune-Leistung
            // steht auf 1 W" sent the operator to the Tune Pwr slider
            // twice while TUNE was reading the RF Power one.
            m_status->setText(QStringLiteral(
                "TUNE würde mit %1 W senden — der Wert kommt aus dem "
                "%2. Damit misst der Richtkoppler nichts (er braucht "
                "mindestens %3 W Vorlauf, brauchbar ab etwa %4 W). "
                "Stell GENAU diesen Regler höher und starte neu. Es "
                "wurde nichts gesendet.")
                    .arg(drive.watts)
                    .arg(drive.sourceLabel)
                    .arg(SwrSweepController::kMinFwdW, 0, 'f', 1)
                    .arg(SwrSweepController::kMinUsefulTuneW));
            return;
        }
    }

    m_backend.controller->startSweep(plan);
}

void SwrSweepPanel::refreshTunePowerLabel()
{
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
    QString note;
    QString colour = QString::fromLatin1(Style::kTextPrimary);
    if (watts < SwrSweepController::kMinUsefulTuneW) {
        note   = QStringLiteral("  ⚠ zu wenig zum Messen — mindestens %1 W")
                     .arg(SwrSweepController::kMinUsefulTuneW);
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
    m_powerLabel->setText(QStringLiteral("Tune-Leistung: %1 W (%2)%3")
                              .arg(watts).arg(drive.sourceLabel).arg(note));

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
