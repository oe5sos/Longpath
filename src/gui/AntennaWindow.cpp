// =================================================================
// src/gui/AntennaWindow.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See AntennaWindow.h for why the answer is at
// the top and the curve underneath it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/AntennaWindow.h"

#include "core/AppSettings.h"
#include "core/antenna/AntennaSweep.h"
#include "core/antenna/Feedline.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/SwrCurveWidget.h"
#include "gui/widgets/SwrSweepPanel.h"

#include <QTabWidget>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileDialog>
#include <QCoreApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace NereusSDR {
namespace {

const QString kKindKey   = QStringLiteral("AntennaTrimKind");
const QString kLengthKey = QStringLiteral("AntennaTrimLengthM");
const QString kRegionKey = QStringLiteral("AntennaBandRegion");
const QString kLimitKey  = QStringLiteral("AntennaSwrLimit");
const QString kDirKey    = QStringLiteral("AntennaSweepDir");
const QString kCableKey  = QStringLiteral("AntennaCableType");
const QString kCableLenKey = QStringLiteral("AntennaCableLengthM");
const QString kFromKey   = QStringLiteral("AntennaReadFromMHz");
const QString kToKey     = QStringLiteral("AntennaReadToMHz");
const QString kVfKey     = QStringLiteral("AntennaCableVf");
const QString kLossKey   = QStringLiteral("AntennaCableLossDb100m");
const QString kCoaxShownKey = QStringLiteral("AntennaCoaxRowShown");

QLabel* caption(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    QFont f = l->font();
    f.setPixelSize(9);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                         .arg(QLatin1String(Style::kTextScale)));
    return l;
}

// One of the three SWR tiles across the top.
QFrame* tile(QWidget* parent, QLabel** cap, QLabel** val,
             const QString& capText)
{
    auto* f = new QFrame(parent);
    f->setStyleSheet(QStringLiteral(
        "QFrame { background: %1; border: 1px solid %2; border-radius: 4px; }")
            .arg(QLatin1String(Style::kPanelBg),
                 QLatin1String(Style::kBorderSubtle)));
    auto* col = new QVBoxLayout(f);
    col->setContentsMargins(9, 7, 9, 7);
    col->setSpacing(1);

    *cap = caption(capText, f);
    (*cap)->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; border: none; }")
            .arg(QLatin1String(Style::kTextScale)));

    *val = new QLabel(QStringLiteral("—"), f);
    QFont vf = (*val)->font();
    vf.setPixelSize(19);
    vf.setFamily(QStringLiteral("Menlo"));
    (*val)->setFont(vf);
    (*val)->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; border: none; }")
            .arg(QLatin1String(Style::kTextPrimary)));

    col->addWidget(*cap);
    col->addWidget(*val);
    return f;
}

} // namespace

AntennaWindow::AntennaWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Antenna"));
    // Modeless: the whole workflow is measure, adjust the wire, measure
    // again. A window that has to be dismissed to reach anything else
    // would be closed after the first sweep and never reopened.
    setModal(false);
    // A sweep usually arrives as a file the operator just copied off an
    // SD card, and the shortest path from there is dragging it onto the
    // window.
    setAcceptDrops(true);
    // Taller than before: the sweep controls moved onto this page and
    // the curve must not be the thing that gives up the room. It is
    // what the window is for.
    resize(1000, 760);
    buildUi();
    refresh();
}

AntennaTrim::Kind AntennaWindow::currentKind() const
{
    return static_cast<AntennaTrim::Kind>(m_kindBox->currentData().toInt());
}

AmateurBands::Band AntennaWindow::readoutSpan() const
{
    const double from = m_fromBox ? m_fromBox->value() * 1e6 : 0.0;
    const double to   = m_toBox   ? m_toBox->value()   * 1e6 : 0.0;

    if (from > 0.0 && to > from) {
        AmateurBands::Band b;
        b.lowHz  = from;
        b.highHz = to;
        // Named, so nothing downstream mistakes it for a band plan
        // entry — the curve prints the name and a green bar labelled
        // "40 m" when it is really the operator's own 20 kHz would be a
        // small lie in a place that matters.
        b.name = QStringLiteral("your range");
        return b;
    }
    // One box filled and the other not is a half-typed instruction, not
    // an instruction. Fall back rather than guessing the missing end.
    return m_curve ? m_curve->shownBand() : AmateurBands::Band{};
}

AmateurBands::Region AntennaWindow::currentRegion() const
{
    return static_cast<AmateurBands::Region>(
        m_regionBox->currentData().toInt());
}

void AntennaWindow::buildUi()
{
    setStyleSheet(QStringLiteral("QDialog { background: %1; }")
                      .arg(QLatin1String(Style::kAppBg)));

    // 2026-08-13: the window grew a second life — the radio-as-antenna-
    // analyzer sweep (design doc 2026-08-13-swr-sweep-analyzer-design).
    // The existing file-based analyser becomes tab one, byte-for-byte
    // unchanged below (`col` now targets the tab page instead of the
    // dialog); the sweep panel is tab two and receives its backend from
    // MainWindow via sweepPanel()->setBackend().
    // ── One window ───────────────────────────────────────────────────
    //
    // "hätte ich gerne alles auf einem fenster. sprich sweep und
    //  auswertung."
    //
    // It was two tabs: measure on one, read the answer on the other,
    // with a second and poorer chart on the measuring side. Nobody
    // wants to change tabs to see what their antenna just did. The
    // sweep panel is now the control strip along the top, in compact
    // form, and there is one curve underneath it.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* filePage = new QWidget(this);
    outer->addWidget(filePage);

    m_sweepPanel = new SwrSweepPanel(this);
    m_sweepPanel->setCompact(true);

    // ── One analysis, two sources ────────────────────────────────────
    //
    // The window had grown two halves that did not know about each
    // other: a file gave you the band shaded and named, SWR at start,
    // middle and end, the usable span and the trim advice, while the
    // radio's own sweep gave a bare line on a second chart.
    //
    // "der sweep sollte nach beendigung genauso wie das beispiel
    //  aussehen." Quite. A finished sweep now goes through the same
    // analysis a loaded .s1p does, and the tab is called Auswertung
    // rather than Datei (VNA) because it is no longer only about files.
    //
    // It also brings the operator here to look at it, which is the
    // point — the answer is on this page, not on the one with the
    // Start button.
    connect(m_sweepPanel, &SwrSweepPanel::analysisReady,
            this, [this](const Sweep& s) { setSweep(s); });

    // ── The other edge, the one that was missing ─────────────────────
    //
    // analysisReady arrives when a sweep FINISHES. For the seventeen to
    // thirty seconds before that, the panel above already named the new
    // band while everything below went on describing the old one — two
    // halves of one page, both current, about different bands.
    connect(m_sweepPanel, &SwrSweepPanel::sweepStartedFor,
            this, &AntennaWindow::onSweepStartedFor);

    auto* col = new QVBoxLayout(filePage);
    col->setContentsMargins(12, 10, 12, 10);
    col->setSpacing(9);

    // The measuring controls, first thing on the page: band, points,
    // tune power, the live coupler counts, Start and Stop. Everything
    // below reads whatever they produced.
    col->addWidget(m_sweepPanel);

    AppSettings& s = AppSettings::instance();

    // ── What is no longer on screen, and why it still exists ─────────
    //
    // "ist die drahtlänge usw. unwichtig im diagramm, dass kanns du
    //  löschen, es soll lediglich eine perfekte kurve zeigen, wie und
    //  wo das swr ist." And: "lösche auch coax kabel."
    //
    // So the antenna kind, the wire length, the estimate button, the
    // target frequency, the read-from/to span, the coax de-embedding,
    // the trim headline, the caution paragraph, the learned-exponent
    // note, the forget button and the multi-band table all come off the
    // page. What is left is a curve and the numbers that describe it.
    //
    // They are still CONSTRUCTED and parented into this hidden holder
    // rather than deleted, because refresh() reads every one of them
    // and gutting both at once is how a working window becomes a broken
    // one at the end of a long day. Hidden now, removed for good once
    // the new layout has settled — with the trim maths kept, since it
    // is tested and correct and belongs in a .s1p-only view later.
    auto* retired = new QWidget(this);
    retired->hide();
    auto* retiredBox = new QVBoxLayout(retired);
    retiredBox->setContentsMargins(0, 0, 0, 0);

    // ── Inputs ───────────────────────────────────────────────────────
    auto* top = new QHBoxLayout;
    top->setSpacing(7);

    m_openBtn = new QPushButton(QStringLiteral("Open sweep…"), this);
    m_openBtn->setStyleSheet(Style::buttonBaseStyle());
    m_openBtn->setToolTip(QStringLiteral(
        "A Touchstone .s1p file. Every analyser writes them — on a "
        "NanoVNA, save the sweep to the SD card."));
    top->addWidget(m_openBtn);

    // ── One click to a curve ─────────────────────────────────────────
    //
    // Two synthetic sweeps have shipped in docs/antenna/samples since
    // the window was written, and the README says exactly what they
    // must produce. Nobody found them: the operator spent a day on the
    // radio sweep, tried to open one from a shell prompt, got
    // "permission denied", and concluded the window draws no chart at
    // all.
    //
    // A file that has to be hunted for is a file that does not exist.
    // This loads it, so the very first thing the window can do is draw
    // a real curve with a known answer — which is also the fastest way
    // to tell "the display is broken" from "my radio is not measuring".
    m_demoBtn = new QPushButton(QStringLiteral("Beispiel"), this);
    m_demoBtn->setStyleSheet(Style::buttonBaseStyle());
    m_demoBtn->setToolTip(QStringLiteral(
        "Lädt einen mitgelieferten 40-m-Dipol-Sweep mit bekanntem "
        "Ergebnis: resonant bei 7.183 MHz, 55 Ω, SWR 1.10. Zeigt das "
        "Fenster etwas anderes, liegt es am Fenster — zeigt es das, "
        "funktioniert die Darstellung und es liegt an der Messung."));
    top->addWidget(m_demoBtn);

    // ── The whole range, from the instrument that may sweep it ───────
    //
    // See AntennaWindow.h for why the radio cannot. This pins a VNA
    // sweep behind the live curve so it survives every radio sweep
    // afterwards, rather than being pushed out by the next one.
    m_pinBtn = new QPushButton(QStringLiteral("VNA-Referenz…"), this);
    m_pinBtn->setStyleSheet(Style::buttonBaseStyle());
    m_pinBtn->setToolTip(QStringLiteral(
        "Eine .s1p vom VNA als durchgehende Vergleichskurve dahinter "
        "legen. Sie bleibt liegen, wenn du danach mit dem Funkgerät "
        "misst.\n\n"
        "Das Funkgerät kann den ganzen Bereich nicht messen: zwischen "
        "den Bändern darf es nicht senden, und der Durchlauf sendet. "
        "Der VNA darf — er ist ein Messgerät mit Mikrowatt."));
    top->addWidget(m_pinBtn);

    top->addStretch(1);

    retiredBox->addWidget(caption(QStringLiteral("ANTENNA"), this));
    m_kindBox = new QComboBox(this);
    m_kindBox->addItem(QStringLiteral("Dipole"),
                       int(AntennaTrim::Kind::Dipole));
    m_kindBox->addItem(QStringLiteral("End-fed half-wave"),
                       int(AntennaTrim::Kind::EndFedHalfWave));
    m_kindBox->addItem(QStringLiteral("Beam — driven element"),
                       int(AntennaTrim::Kind::YagiDrivenElement));
    m_kindBox->addItem(QStringLiteral("Vertical"),
                       int(AntennaTrim::Kind::VerticalRadiator));
    m_kindBox->addItem(QStringLiteral("Loop"),
                       int(AntennaTrim::Kind::Loop));
    // ── Stored by VALUE, not by index ────────────────────────────
    //
    // It used to save the combo index. Inserting "Beam" at position two
    // then shifted Vertical from 2 to 3 and Loop from 3 to 4, so an
    // operator who had chosen Vertical would silently have come back to
    // a beam — with a halved per-element figure and the wrong warnings.
    // A saved index is a saved position in a list that changes; a saved
    // enum value is not.
    {
        const int want = s.value(kKindKey, int(AntennaTrim::Kind::Dipole))
                             .toInt();
        const int at = m_kindBox->findData(want);
        m_kindBox->setCurrentIndex(at >= 0 ? at : 0);
    }
    m_kindBox->setToolTip(QStringLiteral(
        "Decides how the change is shared out. A centre-fed dipole gets "
        "half on each leg; everything else takes it all in one place."));
    retiredBox->addWidget(m_kindBox);

    retiredBox->addWidget(caption(QStringLiteral("LENGTH"), this));
    m_lengthBox = new QDoubleSpinBox(this);
    m_lengthBox->setRange(0.0, 500.0);
    m_lengthBox->setDecimals(2);
    m_lengthBox->setSingleStep(0.1);
    m_lengthBox->setSuffix(QStringLiteral(" m"));
    m_lengthBox->setSpecialValueText(QStringLiteral("unknown"));
    m_lengthBox->setValue(s.value(kLengthKey, 0.0).toDouble());
    m_lengthBox->setToolTip(QStringLiteral(
        "Total wire length as it is now. Without it the answer can only "
        "be a percentage."));
    retiredBox->addWidget(m_lengthBox);

    m_estimateBtn = new QPushButton(QStringLiteral("estimate"), this);
    m_estimateBtn->setStyleSheet(Style::buttonBaseStyle());
    m_estimateBtn->setToolTip(QStringLiteral(
        "Fill in a half-wave for the target frequency, assuming a "
        "velocity factor of 0.95. A starting point, not a measurement — "
        "measure your own wire if you can."));
    retiredBox->addWidget(m_estimateBtn);

    retiredBox->addWidget(caption(QStringLiteral("TARGET"), this));
    m_targetBox = new QDoubleSpinBox(this);
    m_targetBox->setRange(0.0, 500.0);
    m_targetBox->setDecimals(3);
    m_targetBox->setSingleStep(0.005);
    m_targetBox->setSuffix(QStringLiteral(" MHz"));
    // Zero is not a frequency, it is "you have not chosen". Reading
    // "0.000 MHz" as a target would be a number that means nothing.
    m_targetBox->setSpecialValueText(QStringLiteral("band middle"));
    m_targetBox->setToolTip(QStringLiteral(
        "Where you want it resonant. Defaults to the middle of the band "
        "until you choose."));
    retiredBox->addWidget(m_targetBox);

    top->addStretch(1);
    col->addLayout(top);

    // ── Second row: what is being measured, rather than what is on
    //    the mast. Two rows because ten controls on one line wrap into
    //    unreadability on a laptop. ──────────────────────────────────
    auto* row2 = new QHBoxLayout;
    row2->setSpacing(7);

    // The exact span to read off. A band is a reasonable default and
    // not always the question — a CW operator cares about 7.020 to
    // 7.040, not about the whole of 40 m.
    retiredBox->addWidget(caption(QStringLiteral("READ FROM"), this));
    m_fromBox = new QDoubleSpinBox(this);
    m_fromBox->setRange(0.0, 500.0);
    m_fromBox->setDecimals(3);
    m_fromBox->setSingleStep(0.005);
    m_fromBox->setSuffix(QStringLiteral(" MHz"));
    m_fromBox->setSpecialValueText(QStringLiteral("whole band"));
    m_fromBox->setValue(s.value(kFromKey, 0.0).toDouble());
    m_fromBox->setToolTip(QStringLiteral(
        "Read the numbers off this span instead of the whole band. "
        "Leave both at 'whole band' to use the band edges."));
    retiredBox->addWidget(m_fromBox);

    retiredBox->addWidget(caption(QStringLiteral("TO"), this));
    m_toBox = new QDoubleSpinBox(this);
    m_toBox->setRange(0.0, 500.0);
    m_toBox->setDecimals(3);
    m_toBox->setSingleStep(0.005);
    m_toBox->setSuffix(QStringLiteral(" MHz"));
    m_toBox->setSpecialValueText(QStringLiteral("whole band"));
    m_toBox->setValue(s.value(kToKey, 0.0).toDouble());
    retiredBox->addWidget(m_toBox);

    // ── The coax between the analyser and the antenna ────────────
    //
    // Zero for anyone who calibrated at the far end, which is the right
    // way to do it and costs nothing here. For everyone else this is
    // the difference between measuring the antenna and measuring the
    // antenna plus a transformer of unknown ratio.
    // ── Folded away by default ───────────────────────────────────
    //
    // De-embedding a feedline matters, and it is not what most people
    // open this window for. Anyone who calibrated at the antenna end —
    // the right way — never needs it. So it sits behind a toggle
    // instead of taking four controls' worth of attention from the
    // question actually being asked.
    m_coaxToggle = new QPushButton(QStringLiteral("coax…"), this);
    m_coaxToggle->setCheckable(true);
    m_coaxToggle->setStyleSheet(Style::buttonBaseStyle());
    m_coaxToggle->setToolTip(QStringLiteral(
        "Only needed if you calibrated at the analyser rather than at "
        "the antenna. A length of coax rotates the impedance and hides "
        "the real resonance."));
    retiredBox->addWidget(m_coaxToggle);

    m_coaxGroup = new QWidget(this);
    auto* coax = new QHBoxLayout(m_coaxGroup);
    coax->setContentsMargins(0, 0, 0, 0);
    coax->setSpacing(7);
    retiredBox->addWidget(m_coaxGroup);

    coax->addWidget(caption(QStringLiteral("COAX"), m_coaxGroup));
    m_cableBox = new QComboBox(this);
    for (int i = 0; i < Feedline::catalogue().size(); ++i) {
        m_cableBox->addItem(Feedline::catalogue().at(i).name, i);
    }
    {
        // The catalogue is a list that will gain entries, so the same
        // rule again — remember the cable by NAME.
        const QString want = s.value(kCableKey, QString{}).toString();
        const int at = want.isEmpty() ? 0 : m_cableBox->findText(want);
        m_cableBox->setCurrentIndex(at >= 0 ? at : 0);
    }
    m_cableBox->setToolTip(QStringLiteral(
        "The cable between the analyser and the antenna. Nominal "
        "figures — real cable varies by make and by age."));
    coax->addWidget(m_cableBox);

    m_cableLenBox = new QDoubleSpinBox(this);
    m_cableLenBox->setRange(0.0, 300.0);
    m_cableLenBox->setDecimals(1);
    m_cableLenBox->setSingleStep(0.5);
    m_cableLenBox->setSuffix(QStringLiteral(" m"));
    m_cableLenBox->setSpecialValueText(QStringLiteral("none"));
    m_cableLenBox->setValue(s.value(kCableLenKey, 0.0).toDouble());
    m_cableLenBox->setToolTip(QStringLiteral(
        "How much of it. Leave at none if you calibrated at the antenna "
        "end — then there is nothing to take out."));
    coax->addWidget(m_cableLenBox);

    // Only meaningful for the "Custom" entry, and hidden otherwise —
    // two spin boxes that silently do nothing next to eight named
    // cables would be worse than the dead menu item they replace.
    m_vfBox = new QDoubleSpinBox(this);
    m_vfBox->setRange(0.05, 1.0);
    m_vfBox->setDecimals(2);
    m_vfBox->setSingleStep(0.01);
    m_vfBox->setPrefix(QStringLiteral("vf "));
    m_vfBox->setValue(s.value(kVfKey, 0.66).toDouble());
    m_vfBox->setToolTip(QStringLiteral(
        "Velocity factor. 0.66 for solid polyethylene, about 0.82 for "
        "foam, 0.85 for LMR-400."));
    coax->addWidget(m_vfBox);

    m_lossBox = new QDoubleSpinBox(this);
    m_lossBox->setRange(0.0, 100.0);
    m_lossBox->setDecimals(1);
    m_lossBox->setSingleStep(0.5);
    m_lossBox->setSuffix(QStringLiteral(" dB/100m"));
    m_lossBox->setValue(s.value(kLossKey, 4.5).toDouble());
    m_lossBox->setToolTip(QStringLiteral(
        "Matched loss per 100 m at 10 MHz. Scaled as the square root of "
        "frequency from there."));
    coax->addWidget(m_lossBox);

    row2->addStretch(1);

    row2->addWidget(caption(QStringLiteral("SWR MAX"), this));
    m_limitBox = new QDoubleSpinBox(this);
    m_limitBox->setRange(1.1, 5.0);
    m_limitBox->setDecimals(1);
    m_limitBox->setSingleStep(0.1);
    m_limitBox->setValue(s.value(kLimitKey, 2.0).toDouble());
    row2->addWidget(m_limitBox);

    m_regionBox = new QComboBox(this);
    m_regionBox->addItem(QStringLiteral("Region 1"),
                         int(AmateurBands::Region::One));
    m_regionBox->addItem(QStringLiteral("Region 2"),
                         int(AmateurBands::Region::Two));
    m_regionBox->addItem(QStringLiteral("Region 3"),
                         int(AmateurBands::Region::Three));
    {
        // Same rule as the antenna kind: the enum value, never the
        // position in the list.
        const int want = s.value(kRegionKey,
                                 int(AmateurBands::Region::One)).toInt();
        const int at = m_regionBox->findData(want);
        m_regionBox->setCurrentIndex(at >= 0 ? at : 0);
    }
    m_regionBox->setToolTip(QStringLiteral(
        "Which IARU band plan draws the band edges. It is a plan, not a "
        "licence — your own allocation may be narrower."));
    row2->addWidget(m_regionBox);

    col->addLayout(row2);

    // ── The answer, large ────────────────────────────────────────────
    auto* head = new QHBoxLayout;
    head->setSpacing(8);

    auto* actionFrame = new QFrame(this);
    actionFrame->setStyleSheet(QStringLiteral(
        "QFrame { background: %1; border: 1px solid %2; border-radius: 6px; }")
            .arg(QLatin1String(Style::kPanelBg),
                 QLatin1String(Style::kBorder)));
    auto* ac = new QVBoxLayout(actionFrame);
    ac->setContentsMargins(14, 10, 14, 10);
    ac->setSpacing(2);

    m_action = new QLabel(QStringLiteral("—"), actionFrame);
    {
        QFont f = m_action->font();
        f.setPixelSize(30);
        f.setFamily(QStringLiteral("Menlo"));
        m_action->setFont(f);
    }
    m_action->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; border: none; }")
            .arg(QLatin1String(Style::kGreenText)));
    m_action->setWordWrap(true);

    m_actionSub = new QLabel(QString{}, actionFrame);
    m_actionSub->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 12px; border: none; }")
            .arg(QLatin1String(Style::kTextPrimary)));
    m_actionSub->setWordWrap(true);

    ac->addWidget(m_action);
    ac->addWidget(m_actionSub);

    head->addWidget(tile(this, &m_startCap, &m_startVal,
                         QStringLiteral("BAND START")));
    head->addWidget(tile(this, &m_midCap, &m_midVal,
                         QStringLiteral("BAND MIDDLE")));
    head->addWidget(tile(this, &m_endCap, &m_endVal,
                         QStringLiteral("BAND END")));
    // The other half of "bandwidth": not the span you asked about, but
    // the span you have. usableSpan() has existed since the first
    // commit and nothing was showing it.
    head->addWidget(tile(this, &m_spanCap, &m_spanVal,
                         QStringLiteral("USABLE")));
    // Held so the whole row can come off for a multi-band sweep, where
    // four tiles describing one arbitrary band are worse than nothing.
    m_tileRow = new QWidget(this);
    m_tileRow->setLayout(head);
    col->addWidget(m_tileRow);
    // The trim headline used to sit here, above the tiles. It said
    // "shorten by 49 %" and "enter the wire length for centimetres" —
    // advice, not measurement, and explicitly unwanted now. Retired
    // with the rest; the tiles and the curve are the answer.
    retiredBox->addWidget(actionFrame);

    m_caution = new QLabel(QString{}, this);
    m_caution->setWordWrap(true);
    m_caution->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: %2; border-radius: 4px; "
        "padding: 6px 9px; font-size: 11px; }")
            .arg(QLatin1String(Style::kAmberText),
                 QLatin1String(Style::kAmberBg)));
    m_caution->setVisible(false);
    retiredBox->addWidget(m_caution);

    // ── The evidence ─────────────────────────────────────────────────
    m_curve = new SwrCurveWidget(this);
    // The one thing on the page that should take every pixel going.
    m_curve->setMinimumHeight(320);
    m_curve->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    col->addWidget(m_curve, 1);

    m_explain = new QLabel(QString{}, this);
    m_explain->setWordWrap(true);
    m_explain->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
            .arg(QLatin1String(Style::kTextSecondary)));
    retiredBox->addWidget(m_explain);

    // ── One row per band the sweep touches ───────────────────────────
    //
    // A dipole sweep touches one band and the three tiles above already
    // say everything; the table stays hidden. An end-fed swept across
    // HF touches eight, and "where is it resonant" is then a list, not
    // a number.
    m_bandTable = new QTableWidget(0, 6, this);
    m_bandTable->setHorizontalHeaderLabels({
        QStringLiteral("Band"), QStringLiteral("start"),
        QStringLiteral("middle"), QStringLiteral("end"),
        QStringLiteral("bestes SWR"), QStringLiteral("resonant bei")});
    m_bandTable->verticalHeader()->setVisible(false);
    m_bandTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_bandTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_bandTable->setFocusPolicy(Qt::NoFocus);
    m_bandTable->horizontalHeader()->setStretchLastSection(true);
    m_bandTable->setStyleSheet(QStringLiteral(
        "QTableWidget { background: %1; color: %2; border: 1px solid %3;"
        "  gridline-color: %3; font-size: 11px; }"
        "QHeaderView::section { background: %4; color: %5; border: none;"
        "  border-bottom: 1px solid %3; padding: 2px 6px; font-size: 9px; }"
    ).arg(QLatin1String(Style::kInsetBg),
          QLatin1String(Style::kTextPrimary),
          QLatin1String(Style::kBorderSubtle),
          QLatin1String(Style::kButtonBg),
          QLatin1String(Style::kTextScale)));
    m_bandTable->setVisible(false);
    // ── Out of the retired holder ────────────────────────────────────
    //
    // It was parented into the hidden widget with the wire length and
    // the coax boxes, which meant setVisible(true) on the table itself
    // could never show it: a child of a hidden parent stays hidden.
    // So refreshBandTable() has been filling in a table nobody could
    // see, and a range sweep over nine bands showed four tiles for
    // whichever single band bestOverlap happened to pick — 10 m, with
    // "USABLE none", while 40 m sat at 1.10.
    col->addWidget(m_bandTable);

    // What the last change actually did, against what was predicted.
    // Sits under the curve because it is a statement about the pair of
    // curves above it.
    auto* learnRow = new QHBoxLayout;
    learnRow->setSpacing(7);
    m_learned = new QLabel(QString{}, this);
    m_learned->setWordWrap(true);
    m_learned->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
            .arg(QLatin1String(Style::kGreenText)));
    learnRow->addWidget(m_learned, 1);

    m_forgetBtn = new QPushButton(QStringLiteral("Forget history"), this);
    m_forgetBtn->setStyleSheet(Style::buttonBaseStyle());
    m_forgetBtn->setToolTip(QStringLiteral(
        "Start again. Do this when you move the antenna, change its "
        "height, or start on a different band — none of those keep the "
        "old measurements comparable."));
    learnRow->addWidget(m_forgetBtn);
    retiredBox->addLayout(learnRow);

    m_source = new QLabel(QString{}, this);
    m_source->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }")
            .arg(QLatin1String(Style::kTextScale)));
    col->addWidget(m_source);

    // ── Wiring ───────────────────────────────────────────────────────
    connect(m_openBtn, &QPushButton::clicked,
            this, &AntennaWindow::chooseFile);
    connect(m_demoBtn, &QPushButton::clicked,
            this, &AntennaWindow::loadSample);
    connect(m_pinBtn, &QPushButton::clicked,
            this, &AntennaWindow::choosePinned);
    connect(m_estimateBtn, &QPushButton::clicked,
            this, &AntennaWindow::estimateLength);
    connect(m_forgetBtn, &QPushButton::clicked, this, [this]() {
        m_session.clear();
        m_previous = Sweep{};
        m_previousLabel.clear();
        m_curve->clearReference();
        refresh();
    });

    connect(m_kindBox, &QComboBox::currentIndexChanged, this, [this](int) {
        AppSettings::instance().setValue(kKindKey,
                                         m_kindBox->currentData().toInt());
        refresh();
    });
    connect(m_regionBox, &QComboBox::currentIndexChanged, this, [this](int) {
        AppSettings::instance().setValue(kRegionKey,
                                         m_regionBox->currentData().toInt());
        // The band may now be a different one, so the target that was
        // defaulted to the old band centre is no longer right.
        if (!m_targetChosen) { m_targetBox->setValue(0.0); }
        refresh();
    });
    connect(m_lengthBox, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        AppSettings::instance().setValue(kLengthKey, v);
        // The natural order is: cut the wire, load the new sweep, then
        // remember to type the new length. So a length typed after a
        // load belongs to the measurement currently on screen.
        m_session.updateLastLength(v);
        refresh();
    });
    connect(m_limitBox, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        AppSettings::instance().setValue(kLimitKey, v);
        refresh();
    });
    connect(m_fromBox, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        AppSettings::instance().setValue(kFromKey, v);
        refresh();
    });
    connect(m_toBox, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        AppSettings::instance().setValue(kToKey, v);
        refresh();
    });
    connect(m_cableBox, &QComboBox::currentIndexChanged, this, [this](int) {
        AppSettings::instance().setValue(kCableKey,
                                         m_cableBox->currentText());
        const bool custom =
            m_cableBox->currentText() == QStringLiteral("Custom");
        m_vfBox->setVisible(custom);
        m_lossBox->setVisible(custom);
        applyFeedline();
        refresh();
    });
    for (QDoubleSpinBox* b : {m_vfBox, m_lossBox}) {
        connect(b, &QDoubleSpinBox::valueChanged, this, [this]() {
            AppSettings::instance().setValue(kVfKey, m_vfBox->value());
            AppSettings::instance().setValue(kLossKey, m_lossBox->value());
            applyFeedline();
            refresh();
        });
    }
    {
        const bool custom =
            m_cableBox->currentText() == QStringLiteral("Custom");
        m_vfBox->setVisible(custom);
        m_lossBox->setVisible(custom);
    }

    connect(m_coaxToggle, &QPushButton::toggled, this, [this](bool on) {
        AppSettings::instance().setValue(kCoaxShownKey, on);
        m_coaxGroup->setVisible(on);
        // Folding it away does NOT quietly stop de-embedding — a hidden
        // control that still acts is worse than a visible one. The
        // length is zeroed so what is on screen matches what is being
        // computed.
        if (!on && m_cableLenBox->value() > 0.0) {
            m_cableLenBox->setValue(0.0);
        }
    });
    {
        // Shown again if it was left open, or if a cable length
        // survived in the settings — hiding a control that is actively
        // changing the answer would be the same lie the other way
        // round.
        const bool open = s.value(kCoaxShownKey, false).toBool()
                          || m_cableLenBox->value() > 0.0;
        m_coaxToggle->setChecked(open);
        m_coaxGroup->setVisible(open);
    }
    connect(m_cableLenBox, &QDoubleSpinBox::valueChanged,
            this, [this](double v) {
        AppSettings::instance().setValue(kCableLenKey, v);
        applyFeedline();
        refresh();
    });
    connect(m_targetBox, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        // Only a value the operator put there counts as chosen — the
        // band-centre default is written through the same setter.
        if (v > 0.0 && m_targetBox->hasFocus()) { m_targetChosen = true; }
        refresh();
    });
}

void AntennaWindow::dragEnterEvent(QDragEnterEvent* e)
{
    // Accept only what can actually be opened. Accepting everything and
    // then failing on the drop is a worse answer than the cursor saying
    // no in the first place.
    if (!e->mimeData()->hasUrls()) { return; }
    for (const QUrl& u : e->mimeData()->urls()) {
        if (u.isLocalFile()
            && u.toLocalFile().endsWith(QStringLiteral(".s1p"),
                                        Qt::CaseInsensitive)) {
            e->acceptProposedAction();
            return;
        }
    }
}

void AntennaWindow::dropEvent(QDropEvent* e)
{
    for (const QUrl& u : e->mimeData()->urls()) {
        if (!u.isLocalFile()) { continue; }
        const QString path = u.toLocalFile();
        if (!path.endsWith(QStringLiteral(".s1p"), Qt::CaseInsensitive)) {
            continue;
        }
        e->acceptProposedAction();
        AppSettings::instance().setValue(kDirKey,
                                         QFileInfo(path).absolutePath());
        openFile(path);
        return;      // one sweep at a time; the rest would just flash past
    }
}

void AntennaWindow::chooseFile()
{
    AppSettings& s = AppSettings::instance();
    const QString start = s.value(kDirKey, QString{}).toString();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open a sweep"), start,
        QStringLiteral("Touchstone one-port (*.s1p *.S1P);;All files (*)"));
    if (path.isEmpty()) { return; }
    s.setValue(kDirKey, QFileInfo(path).absolutePath());
    openFile(path);
}

void AntennaWindow::openFile(const QString& path)
{
    const Sweep s = Touchstone::readS1p(path);
    // A file that could not be read used to vanish without a word: the
    // note went into the Sweep and the Sweep went nowhere, so the
    // window looked exactly as it had before. Say so.
    if (s.points.isEmpty() && !s.note.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Sweep"), s.note);
        return;
    }
    setSweep(s);
}

void AntennaWindow::choosePinned()
{
    // Already holding one — the button is the way to put it down again.
    if (!m_pinned.isEmpty()) {
        m_pinned = Sweep{};
        m_pinnedLabel.clear();
        m_pinBtn->setText(QStringLiteral("VNA-Referenz…"));
        applyReference();
        return;
    }

    AppSettings& s = AppSettings::instance();
    const QString start = s.value(kDirKey, QString{}).toString();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("VNA-Sweep als Referenz"), start,
        QStringLiteral("Touchstone one-port (*.s1p *.S1P);;All files (*)"));
    if (path.isEmpty()) { return; }
    s.setValue(kDirKey, QFileInfo(path).absolutePath());

    const Sweep ref = Touchstone::readS1p(path);
    if (ref.points.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("VNA-Referenz"),
            ref.note.isEmpty()
                ? QStringLiteral("Die Datei enthält keine Punkte.")
                : ref.note);
        return;
    }
    m_pinned = ref;
    m_pinnedLabel = QFileInfo(path).completeBaseName();
    // Say what it costs to press again, so the button is not a trap.
    m_pinBtn->setText(QStringLiteral("Referenz ✕"));
    applyReference();
}

void AntennaWindow::applyReference()
{
    // A pinned VNA sweep outranks the previous radio sweep. Both drawn
    // at once would be two faint dashed curves behind one solid one,
    // and the eye cannot tell which faint line is which.
    if (!m_pinned.isEmpty()) {
        m_curve->setReference(m_pinned, m_pinnedLabel.isEmpty()
            ? QStringLiteral("VNA")
            : QStringLiteral("VNA · %1").arg(m_pinnedLabel));
    } else {
        m_curve->setReference(m_previous, m_previousLabel);
    }
}

void AntennaWindow::loadSample()
{
    // The samples live in the source tree, not in the bundle. Rather
    // than install them, look in the places they can be relative to a
    // running binary — build/NereusSDR.app/Contents/MacOS is three
    // levels under the repository root, and a plain build is one.
    const QString rel =
        QStringLiteral("docs/antenna/samples/dipole-40m-before.s1p");
    const QString base = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        base + QStringLiteral("/") + rel,
        base + QStringLiteral("/../") + rel,
        base + QStringLiteral("/../../") + rel,
        base + QStringLiteral("/../../../") + rel,
        base + QStringLiteral("/../../../../") + rel,
        base + QStringLiteral("/../../../../../") + rel,
    };
    for (const QString& c : candidates) {
        const QFileInfo fi(c);
        if (fi.exists() && fi.isReadable()) {
            openFile(fi.absoluteFilePath());
            return;
        }
    }
    QMessageBox::information(this, QStringLiteral("Beispiel"),
        QStringLiteral(
            "Die Beispieldatei wurde nicht gefunden. Sie liegt im "
            "Quellbaum unter\n\n    %1\n\nund lässt sich mit "
            "„Open sweep…“ von dort öffnen.").arg(rel));
}

Feedline::Cable AntennaWindow::currentCable() const
{
    const int idx = m_cableBox ? m_cableBox->currentIndex() : 0;
    if (idx < 0 || idx >= Feedline::catalogue().size()) { return {}; }

    Feedline::Cable c = Feedline::catalogue().at(idx);
    // "Custom" is the last entry and the only one whose numbers come
    // from the operator. Everything else keeps its nominal figures,
    // which the tooltip already says are nominal.
    if (c.name == QStringLiteral("Custom") && m_vfBox && m_lossBox) {
        c.velocityFactor = m_vfBox->value();
        c.lossDb100m     = m_lossBox->value();
        c.refHz          = 10e6;
    }
    return c;
}

void AntennaWindow::applyFeedline()
{
    const double len = m_cableLenBox ? m_cableLenBox->value() : 0.0;
    const int idx = m_cableBox ? m_cableBox->currentIndex() : 0;
    if (len <= 0.0 || idx <= 0 || idx >= Feedline::catalogue().size()) {
        // Index 0 is "None — calibrated at the antenna", which is not a
        // cable to remove.
        m_sweep = m_measured;
        return;
    }
    const auto out = Feedline::deEmbed(m_measured, len, currentCable());
    m_sweep = out.sweep;
}

void AntennaWindow::setSweep(const Sweep& s)
{
    // The sweep being replaced becomes the faint one behind the new
    // curve. Only when there was one and it is not this one again —
    // reloading the same file should not make it its own reference.
    if (!m_measured.isEmpty() && m_measured.source != s.source) {
        m_previous = m_sweep;
        m_previousLabel = m_measured.source;
    }

    m_measured = s;
    applyFeedline();

    applyReference();

    // Record it against the length currently entered. If the operator
    // types the new length afterwards — which is the usual order — the
    // length box's handler corrects this entry.
    const auto res = AntennaSweep::nearestResonance(
        m_sweep, m_targetBox->value() * 1e6);
    if (res.found) {
        m_session.record(m_lengthBox->value(), res.freqHz, s.source);
    }
    // ── A target outside the new sweep is not a target ───────────────
    //
    // The defaulted one was already recomputed. The one the operator
    // typed was left alone, on the reasoning that a choice should
    // survive — and that is right until the choice stops making sense.
    //
    // Bench, 2026-08-14: a 20 m sweep left the target at 14.175, then
    // an 80 m sweep came in. The window shaded and labelled 20 m over a
    // 3.5–4.0 MHz curve, and the three tiles read "not swept" while the
    // usable span collapsed to 3 kHz. Everything on screen described a
    // band that had not been measured.
    //
    // A target the sweep does not cover cannot be honoured, so it is
    // dropped back to the band middle rather than silently misdescribing
    // the measurement.
    if (m_targetChosen && !m_sweep.isEmpty()) {
        const double t = m_targetBox->value() * 1e6;
        if (t > 0.0 && (t < m_sweep.startHz() || t > m_sweep.stopHz())) {
            m_targetChosen = false;
        }
    }
    if (!m_targetChosen) { m_targetBox->setValue(0.0); }

    // A real measurement is the only way back out of the superseded
    // state — see onSweepStartedFor(). Recorded before refresh(),
    // because refresh() is what pushes the state into the curve and the
    // readouts.
    m_analysisStale = false;
    m_analysisLoHz  = m_sweep.isEmpty() ? 0.0 : m_sweep.startHz();
    m_analysisHiHz  = m_sweep.isEmpty() ? 0.0 : m_sweep.stopHz();
    m_analysisBandName.clear();

    refresh();
}

bool AntennaWindow::sameBandSpan(double aLoHz, double aHiHz,
                                 double bLoHz, double bHiHz)
{
    const double aw = aHiHz - aLoHz;
    const double bw = bHiHz - bLoHz;
    if (aw <= 0.0 || bw <= 0.0) { return false; }
    const double overlap = std::min(aHiHz, bHiHz) - std::max(aLoHz, bLoHz);
    // Half the narrower run. A second sweep of 20 m — perhaps with more
    // points, perhaps after moving the feedpoint — is the same band and
    // must not make the picture step back one second before it replaces
    // it anyway. 80 m after 20 m shares nothing and is not.
    return overlap > 0.5 * std::min(aw, bw);
}

void AntennaWindow::onSweepStartedFor(const QString& bandName,
                                      double loHz, double hiHz)
{
    // Nothing on screen to overtake.
    if (m_sweep.isEmpty() || m_analysisHiHz <= m_analysisLoHz) { return; }
    if (sameBandSpan(m_analysisLoHz, m_analysisHiHz, loHz, hiHz)) { return; }

    m_analysisStale = true;
    // Prefer the name the analysis worked out for itself; fall back to
    // the one that came with the sweep. bandName is the band being
    // measured NOW and is deliberately not used here — the capsule has
    // to say which band the numbers under it belong to, and naming the
    // other one is the whole confusion this is here to end.
    Q_UNUSED(bandName);
    refresh();
}

void AntennaWindow::estimateLength()
{
    double target = m_targetBox->value() * 1e6;
    if (target <= 0.0) {
        const AmateurBands::Band b = m_curve->shownBand();
        if (b.isValid()) { target = b.centreHz(); }
    }
    if (target <= 0.0) { return; }
    m_lengthBox->setValue(AntennaTrim::halfWaveEstimateM(target));
}

void AntennaWindow::refreshBandTable()
{
    const auto bands = m_curve->shownBands();
    const auto res   = m_curve->resonances();

    // One band is what the tiles at the top already show. A one-row
    // table there is furniture.
    if (bands.size() < 2) {
        m_bandTable->setVisible(false);
        if (m_tileRow) { m_tileRow->setVisible(true); }
        return;
    }
    // Several bands: the tiles come off. They describe exactly one, and
    // which one is decided by widest overlap — on a 1.8-to-30 sweep
    // that is 10 m, so a nine-band measurement was headlined "BAND
    // START 28.000 · USABLE none" while 40 m sat at 1.10.
    if (m_tileRow) { m_tileRow->setVisible(false); }

    // ── What "resonant" can honestly mean here ───────────────────────
    //
    // A file from a VNA carries phase, so a true series resonance —
    // reactance crossing zero — is available, and it is NOT the same
    // frequency as the SWR minimum. A sweep from the radio is
    // magnitude-only: forward and reflected power, no phase, no
    // reactance, no resonance. AntennaSweep::resonances() correctly
    // returns nothing for it.
    //
    // For a radio sweep the useful per-band answer is the dip: lowest
    // SWR in the band and where it sits. So that column is always
    // filled, and the resonance column is hidden rather than printed
    // as a row of dashes that look like a fault.
    m_bandTable->setColumnHidden(5, m_sweep.magnitudeOnly);

    const double limit = m_limitBox->value();
    m_bandTable->setRowCount(bands.size());

    // Ink for everything in the table that is NOT a red over-limit
    // value: it steps back with the rest of the analysis when that has
    // been superseded. Red is passed through untouched at the call
    // sites below, which is the whole distinction.
    const bool stale = m_analysisStale;
    auto ink = [stale](const char* c) {
        return QColor(stale ? Style::kTextInactive : c);
    };

    for (int r = 0; r < bands.size(); ++r) {
        const AmateurBands::Band& b = bands.at(r);

        auto put = [this, r](int c, const QString& text, const QColor& col) {
            auto* it = new QTableWidgetItem(text);
            it->setForeground(col);
            m_bandTable->setItem(r, c, it);
            return it;
        };

        put(0, b.name, ink(Style::kAccent));

        const double at[3] = {b.lowHz, b.centreHz(), b.highHz};
        for (int i = 0; i < 3; ++i) {
            const double v = AntennaSweep::swrAt(m_sweep, at[i]);
            if (v <= 0.0) {
                // The edge falls outside the swept range. Blank would
                // read as fine.
                put(1 + i, QStringLiteral("not swept"),
                    QColor(Style::kTextInactive));
            } else {
                put(1 + i, QStringLiteral("%1").arg(v, 0, 'f', 2),
                    v > limit ? QColor(Style::kRedBorder)
                              : ink(Style::kTextPrimary));
            }
        }

        // ── The dip, which every sweep can answer ────────────────────
        //
        // Searched over the measured points inside the band, not by
        // sampling the interpolation on a grid: the points ARE the
        // measurement, and a grid can walk straight past a sharp
        // minimum between two of them.
        double bestSwr = 0.0;
        double bestAt  = 0.0;
        for (const SweepPoint& p : m_sweep.points) {
            if (!b.contains(p.freqHz)) { continue; }
            const double v = AntennaSweep::swr(p.gamma);
            if (v <= 0.0) { continue; }
            if (bestSwr <= 0.0 || v < bestSwr) { bestSwr = v; bestAt = p.freqHz; }
        }
        if (bestSwr > 0.0) {
            put(4, QStringLiteral("%1 bei %2 MHz")
                       .arg(bestSwr, 0, 'f', 2)
                       .arg(bestAt / 1e6, 0, 'f', 3),
                bestSwr > limit ? QColor(Style::kRedBorder)
                                : ink(Style::kGreenText));
        } else {
            put(4, QStringLiteral("nicht gemessen"),
                QColor(Style::kTextInactive));
        }

        // A resonance inside this band is the thing an end-fed owner is
        // looking for. Saying "none" is as useful as saying where.
        // Only meaningful with phase — see the note above; the column
        // is hidden for a magnitude-only sweep.
        QString where = QStringLiteral("—");
        QColor  col(Style::kTextInactive);
        for (const auto& c : res) {
            if (!b.contains(c.freqHz)) { continue; }
            where = QStringLiteral("%1 MHz · %2 Ω")
                        .arg(c.freqHz / 1e6, 0, 'f', 3)
                        .arg(c.resistanceOhms, 0, 'f', 0);
            col = ink(Style::kAmberText);
            break;
        }
        put(5, where, col);
    }

    m_bandTable->resizeColumnsToContents();
    // Tall enough for its rows and no taller — it sits between the
    // curve and the advice, and neither should be pushed off screen by
    // an eight-band sweep.
    const int rowH = m_bandTable->rowHeight(0);
    // int(), because QVector::size() is a qsizetype and std::min will
    // not deduce a common type with a plain 6.
    m_bandTable->setFixedHeight(
        std::min(int(bands.size()), 6) * rowH
        + m_bandTable->horizontalHeader()->height() + 4);
    m_bandTable->setVisible(true);
}

void AntennaWindow::refresh()
{
    m_curve->setRegion(currentRegion());
    m_curve->setSwrLimit(m_limitBox->value());
    m_curve->setSweep(m_sweep);

    // A typed range overrides the band, and the curve is told so its
    // three verticals land on the same span the tiles describe.
    const AmateurBands::Band typed = readoutSpan();
    const bool custom = typed.isValid()
                        && typed.name == QStringLiteral("your range");
    m_curve->setBand(custom ? typed : AmateurBands::Band{});

    const AmateurBands::Band band = custom ? typed : m_curve->shownBand();

    // Remember what these numbers are about while they are still
    // current, so the capsule can name it after they stop being. Worked
    // out from the sweep rather than taken from the sweep's source
    // string: a .s1p from a VNA has no band in its name and this way
    // both kinds of measurement carry one.
    if (!m_analysisStale) {
        m_analysisBandName = band.isValid() ? band.name : QString{};
    }
    m_curve->setSuperseded(m_analysisStale, m_analysisBandName);

    // Default the target to the middle of the band. Written through the
    // spin box so the operator can see what it decided, and only while
    // they have not chosen for themselves.
    if (!m_targetChosen && band.isValid()
        && m_targetBox->value() <= 0.0) {
        QSignalBlocker block(m_targetBox);
        m_targetBox->setValue(band.centreHz() / 1e6);
    }
    const double targetHz = m_targetBox->value() * 1e6;
    m_curve->setTargetHz(targetHz);

    // Where the numbers claim to have been measured. An operator
    // looking at a de-embedded curve and one looking at a raw curve are
    // looking at different antennas, and the line has to say which.
    const double cableLen = m_cableLenBox->value();
    const int    cableIdx = m_cableBox->currentIndex();
    const QString plane = (cableLen > 0.0 && cableIdx > 0)
        ? QStringLiteral("at the antenna, %1 m of %2 removed (vf %3, "
                         "%4 dB/100m at 10 MHz)")
              .arg(cableLen, 0, 'f', 1)
              .arg(currentCable().name)
              .arg(currentCable().velocityFactor, 0, 'f', 2)
              .arg(currentCable().lossDb100m, 0, 'f', 1)
        : QStringLiteral("at the analyser port");

    // ── What the antenna is doing at the target ──────────────────────
    //
    // R + jX and the return loss, which is what a VNA screen shows and
    // what somebody comparing against their instrument will look for.
    // impedanceAt() and returnLossDb() were written on the first day
    // and nothing was calling either of them — an audit found them
    // dead, and the fix is to show them rather than delete them.
    QString atTarget;
    if (!m_sweep.isEmpty() && targetHz > 0.0) {
        const auto z = AntennaSweep::impedanceAt(m_sweep, targetHz);
        if (z != std::complex<double>{}) {
            const double rl =
                AntennaSweep::returnLossDb(
                    (std::complex<double>(z.real(), z.imag())
                        - m_sweep.referenceOhms)
                    / (std::complex<double>(z.real(), z.imag())
                        + m_sweep.referenceOhms));
            atTarget = QStringLiteral(" · at %1: %2 %3 j%4 Ω, %5 dB return")
                           .arg(targetHz / 1e6, 0, 'f', 3)
                           .arg(z.real(), 0, 'f', 0)
                           .arg(z.imag() < 0.0 ? QStringLiteral("−")
                                               : QStringLiteral("+"))
                           .arg(std::abs(z.imag()), 0, 'f', 0)
                           .arg(rl, 0, 'f', 1);
        }
    }

    m_source->setText(m_sweep.source.isEmpty()
        ? QString{}
        : QStringLiteral("%1 · %2 points · %3 to %4 MHz · %5%6")
              .arg(m_sweep.source)
              .arg(m_sweep.points.size())
              .arg(m_sweep.startHz() / 1e6, 0, 'f', 3)
              .arg(m_sweep.stopHz()  / 1e6, 0, 'f', 3)
              .arg(plane, atTarget));

    // ── The three tiles ──────────────────────────────────────────────
    const double limit = m_limitBox->value();
    auto setTile = [this, limit](QLabel* val, QLabel* cap,
                                 double hz, const QString& capText) {
        if (hz <= 0.0) {
            val->setText(QStringLiteral("—"));
            cap->setText(capText);
            val->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; border: none; }")
                    .arg(QLatin1String(Style::kTextInactive)));
            return;
        }
        cap->setText(QStringLiteral("%1  %2")
                         .arg(capText).arg(hz / 1e6, 0, 'f', 3));
        const double s = AntennaSweep::swrAt(m_sweep, hz);
        if (s <= 0.0) {
            // Outside the swept range. Saying nothing would let this
            // read as measured and fine.
            val->setText(QStringLiteral("not swept"));
            val->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 13px; border: none; }")
                    .arg(QLatin1String(Style::kTextInactive)));
            return;
        }
        val->setText(QStringLiteral("%1").arg(s, 0, 'f', 2));
        // Over the limit stays red at full strength even when the
        // analysis has been superseded — the same rule the curve keeps,
        // for the same reason. Everything else steps back.
        val->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; border: none; }")
                .arg(QLatin1String(s > limit    ? Style::kRedBorder
                                 : m_analysisStale ? Style::kTextInactive
                                                   : Style::kTextPrimary)));
    };
    // The caption carries the band once the numbers stop being about the
    // one on the radio: "160M START" rather than "BAND START", so the
    // tiles cannot be read as describing whatever the head names.
    const QString what =
        custom ? QStringLiteral("RANGE")
      : (m_analysisStale && !m_analysisBandName.isEmpty())
               ? m_analysisBandName.toUpper()
               : QStringLiteral("BAND");
    setTile(m_startVal, m_startCap, band.isValid() ? band.lowHz : 0.0,
            what + QStringLiteral(" START"));
    setTile(m_midVal, m_midCap, band.isValid() ? band.centreHz() : 0.0,
            what + QStringLiteral(" MIDDLE"));
    setTile(m_endVal, m_endCap, band.isValid() ? band.highHz : 0.0,
            what + QStringLiteral(" END"));

    // ── How wide it actually is ──────────────────────────────────────
    //
    // Grown outwards from the target — or the middle of the span when
    // no target was chosen — because a multiband antenna has several
    // runs under the limit and only the one you are standing in counts.
    {
        const double around = targetHz > 0.0 ? targetHz
                            : band.isValid() ? band.centreHz()
                                             : 0.0;
        const auto span = AntennaSweep::usableSpan(m_sweep, limit, around);
        if (!span.found || around <= 0.0) {
            m_spanCap->setText(QStringLiteral("USABLE"));
            m_spanVal->setText(m_sweep.isEmpty() ? QStringLiteral("—")
                                                 : QStringLiteral("none"));
            m_spanVal->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 15px; border: none; }")
                    .arg(QLatin1String(m_sweep.isEmpty()
                                           ? Style::kTextInactive
                                           : Style::kRedBorder)));
        } else {
            const double kHz = span.widthHz() / 1000.0;
            m_spanCap->setText(QStringLiteral("USABLE  %1–%2")
                                   .arg(span.lowHz  / 1e6, 0, 'f', 3)
                                   .arg(span.highHz / 1e6, 0, 'f', 3));
            m_spanVal->setText(kHz >= 1000.0
                ? QStringLiteral("%1 MHz").arg(kHz / 1000.0, 0, 'f', 2)
                : QStringLiteral("%1 kHz").arg(kHz, 0, 'f', 0));
            // Green only when it covers the whole span being read off —
            // "300 kHz" is meaningless without knowing whether that is
            // enough for the band in hand.
            const bool covers = band.isValid()
                                && span.lowHz  <= band.lowHz  + 1.0
                                && span.highHz >= band.highHz - 1.0;
            m_spanVal->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; border: none; }")
                    .arg(QLatin1String(
                        m_analysisStale ? Style::kTextInactive
                      : covers          ? Style::kGreenText
                                        : Style::kTextPrimary)));
        }
    }

    refreshBandTable();

    // ── What the last change actually did ────────────────────────────
    //
    // Shown whether or not it produced a usable exponent: "these two
    // are too close together to learn from" is itself worth reading,
    // and so is "the resonance went the wrong way".
    const auto session = m_session.learned();
    m_learned->setText(m_session.count() >= 2 ? session.note : QString{});
    m_learned->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
            .arg(QLatin1String(session.valid ? Style::kGreenText
                                             : Style::kAmberWarn)));
    m_learned->setVisible(m_session.count() >= 2);
    m_forgetBtn->setVisible(m_session.count() >= 1);

    // ── The answer ───────────────────────────────────────────────────
    m_explain->setText(AntennaSweep::describe(m_sweep, targetHz));

    if (m_sweep.isEmpty()) {
        m_action->setText(QStringLiteral("—"));
        m_actionSub->setText(QStringLiteral(
            "Open a .s1p sweep from your analyser."));
        m_caution->setVisible(false);
        return;
    }

    // ── A sweep with no phase leads with the match ───────────────────
    //
    // A radio's coupler measures how much comes back, not when. The
    // headline below would call that "not resonant" in red, which is a
    // verdict on the antenna where the truth is a limitation of the
    // instrument — and the operator would go and cut wire over it.
    //
    // So say what WAS measured: where in the band the match is best.
    // That is most of what a SOTA operator needs, and the missing part
    // is named rather than left as an absence.
    if (m_sweep.magnitudeOnly) {
        const auto best = AntennaSweep::bestMatch(m_sweep);
        if (best.found) {
            m_action->setText(QStringLiteral("%1 MHz")
                                  .arg(best.freqHz / 1e6, 0, 'f', 3));
            m_action->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; border: none; }")
                    .arg(QLatin1String(best.swr <= limit
                                           ? Style::kGreenText
                                           : Style::kAmberText)));
            m_actionSub->setText(QStringLiteral(
                "bester Anpassungspunkt · SWR %1 — gemessen vom "
                "Funkgerät.\n"
                "Wo genau die Antenne resonant ist und wie viele "
                "Zentimeter Draht fehlen, kann diese Messung nicht "
                "sagen: der Richtkoppler misst nur, wie viel "
                "zurückkommt, nicht mit welcher Phase. Dafür braucht "
                "es einen Sweep mit Phase (.s1p).")
                    .arg(best.swr, 0, 'f', 2));
        } else {
            m_action->setText(QStringLiteral("—"));
            m_actionSub->setText(QStringLiteral(
                "Keine verwertbaren Punkte in dieser Messung."));
        }
        m_caution->setVisible(false);
        return;
    }

    const auto res = AntennaSweep::nearestResonance(m_sweep, targetHz);
    if (!res.found) {
        m_action->setText(QStringLiteral("not resonant"));
        m_action->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; border: none; }")
                .arg(QLatin1String(Style::kRedBorder)));
        m_actionSub->setText(QStringLiteral(
            "Not between %1 and %2 MHz, anyway — the reactance never "
            "reaches zero. Sweep wider.")
                .arg(m_sweep.startHz() / 1e6, 0, 'f', 3)
                .arg(m_sweep.stopHz()  / 1e6, 0, 'f', 3));
        m_caution->setVisible(false);
        return;
    }
    // ── The headline is the answer to the question asked ─────────────
    //
    // Most people opening this window want one thing: where is my
    // antenna resonant. Cable loss, velocity factors and learned
    // exponents are refinements for somebody trimming to a target, and
    // leading with "—" until they fill in three more boxes buries the
    // answer they came for.
    //
    // So the big number is the RESONANCE until there is both a target
    // and a length, and only then becomes the centimetres.
    auto showResonance = [this, &res](const QString& extra) {
        m_action->setText(QStringLiteral("%1 MHz")
                              .arg(res.freqHz / 1e6, 0, 'f', 3));
        m_action->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; border: none; }")
                .arg(QLatin1String(Style::kAmberText)));
        QString sub = QStringLiteral("resonant · %1 Ω · SWR %2")
                          .arg(res.resistanceOhms, 0, 'f', 0)
                          .arg(res.swrThere, 0, 'f', 2);
        if (!extra.isEmpty()) { sub += QStringLiteral(" · ") + extra; }
        m_actionSub->setText(sub);
    };

    if (targetHz <= 0.0) {
        showResonance(QStringLiteral("set a target to get centimetres"));
        m_caution->setVisible(false);
        return;
    }

    const AntennaTrim::Kind kind = currentKind();

    // ── One path, and it took an audit to notice ─────────────────────
    //
    // This used to call AntennaTrim::compute directly and pass the
    // exponent itself, which is what TrimSession::recommend does. Two
    // implementations of the same decision — and the tests exercised
    // recommend(), so the suite was green about a function the program
    // never ran.
    //
    // The observation is refreshed first because the window looks for
    // the crossing NEAREST THE TARGET: change the target and a
    // different one can legitimately win, so what was recorded at load
    // time is no longer what is on screen.
    m_session.updateLastResonance(res.freqHz);
    const auto t = m_session.recommend(kind, targetHz,
                                       m_lengthBox->value());

    if (std::abs(t.percentChange) < 0.1) {
        showResonance(QStringLiteral("already where you want it — leave "
                                     "the wire alone"));
        m_action->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; border: none; }")
                .arg(QLatin1String(Style::kGreenText)));
        m_caution->setVisible(false);
        return;
    }

    if (t.totalChangeM == 0.0) {
        // A percentage is not an instruction anybody can act on with a
        // tape measure, so the resonance stays the headline and the
        // percentage goes underneath with what is missing.
        showResonance(QStringLiteral("%1 %2 % — enter the wire length "
                                     "for centimetres")
                          .arg(t.lengthen ? QStringLiteral("+")
                                          : QStringLiteral("−"))
                          .arg(std::abs(t.percentChange), 0, 'f', 2));
    } else {
        m_action->setText(QStringLiteral("%1 %2 cm")
                              .arg(t.lengthen ? QStringLiteral("+")
                                              : QStringLiteral("−"))
                              .arg(std::abs(t.firstStepM) * 100.0, 0, 'f', 1));
        QString sub = QStringLiteral("%1 · %2, resonant %3 → %4 MHz")
                          .arg(AntennaTrim::kindWhere(kind),
                               AntennaTrim::kindName(kind))
                          .arg(res.freqHz / 1e6, 0, 'f', 3)
                          .arg(targetHz / 1e6, 0, 'f', 3);
        if (band.isValid()) {
            sub = band.name + QStringLiteral(" · ") + sub;
        }
        m_actionSub->setText(sub);
    }
    m_action->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; border: none; }")
            .arg(QLatin1String(t.lengthen ? Style::kGreenText
                                          : Style::kAmberText)));

    // ── Warnings, only when there are any ────────────────────────────
    // AntennaTrim::instruction() is the authoritative sentence and it
    // already carries the halving note and the large-change caution.
    // The window used to rebuild both from the same fields in slightly
    // different words — two versions of one sentence, and the tests
    // checked the version nobody saw.
    QStringList notes;
    notes << AntennaTrim::instruction(t, kind);
    if (kind == AntennaTrim::Kind::EndFedHalfWave) {
        notes << QStringLiteral(
            "On an end-fed, the harmonic bands do not all move by the "
            "same amount. Re-measure each band rather than assuming.");
    }
    if (kind == AntennaTrim::Kind::YagiDrivenElement) {
        // The two ways to ruin a beam while the SWR meter applauds.
        notes << QStringLiteral(
            "This changes the MATCH only. The gain and the front-to-back "
            "live in the reflector and directors — never trim those to "
            "fix SWR.");
        notes << QStringLiteral(
            "If the beam is fed through a gamma, hairpin or beta match, "
            "the adjustment is in the match and not in the element. "
            "Cutting the driven element there makes it worse.");
    }
    if (kind == AntennaTrim::Kind::VerticalRadiator) {
        notes << QStringLiteral(
            "The radiator sets the resonance; the radials set the feed "
            "resistance. A high SWR at resonance is a radial problem, "
            "not a length problem.");
    }
    notes << QStringLiteral(
        "Height above ground shifts the resonance. Adjust at the height "
        "the antenna will actually work at.");

    if (session.valid) {
        notes << QStringLiteral(
            "Scaled from your own measurements: this antenna moves as "
            "L^-%1, not the textbook L^-1.")
                  .arg(session.exponent, 0, 'f', 2);
    }

    m_caution->setText(notes.join(QStringLiteral("  ")));
    m_caution->setVisible(true);
}

} // namespace NereusSDR
