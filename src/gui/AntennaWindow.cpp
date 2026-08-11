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

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileDialog>
#include <QFileInfo>
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
    resize(880, 620);
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

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(12, 10, 12, 10);
    col->setSpacing(9);

    AppSettings& s = AppSettings::instance();

    // ── Inputs ───────────────────────────────────────────────────────
    auto* top = new QHBoxLayout;
    top->setSpacing(7);

    m_openBtn = new QPushButton(QStringLiteral("Open sweep…"), this);
    m_openBtn->setStyleSheet(Style::buttonBaseStyle());
    m_openBtn->setToolTip(QStringLiteral(
        "A Touchstone .s1p file. Every analyser writes them — on a "
        "NanoVNA, save the sweep to the SD card."));
    top->addWidget(m_openBtn);

    top->addWidget(caption(QStringLiteral("ANTENNA"), this));
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
    top->addWidget(m_kindBox);

    top->addWidget(caption(QStringLiteral("LENGTH"), this));
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
    top->addWidget(m_lengthBox);

    m_estimateBtn = new QPushButton(QStringLiteral("estimate"), this);
    m_estimateBtn->setStyleSheet(Style::buttonBaseStyle());
    m_estimateBtn->setToolTip(QStringLiteral(
        "Fill in a half-wave for the target frequency, assuming a "
        "velocity factor of 0.95. A starting point, not a measurement — "
        "measure your own wire if you can."));
    top->addWidget(m_estimateBtn);

    top->addWidget(caption(QStringLiteral("TARGET"), this));
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
    top->addWidget(m_targetBox);

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
    row2->addWidget(caption(QStringLiteral("READ FROM"), this));
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
    row2->addWidget(m_fromBox);

    row2->addWidget(caption(QStringLiteral("TO"), this));
    m_toBox = new QDoubleSpinBox(this);
    m_toBox->setRange(0.0, 500.0);
    m_toBox->setDecimals(3);
    m_toBox->setSingleStep(0.005);
    m_toBox->setSuffix(QStringLiteral(" MHz"));
    m_toBox->setSpecialValueText(QStringLiteral("whole band"));
    m_toBox->setValue(s.value(kToKey, 0.0).toDouble());
    row2->addWidget(m_toBox);

    // ── The coax between the analyser and the antenna ────────────
    //
    // Zero for anyone who calibrated at the far end, which is the right
    // way to do it and costs nothing here. For everyone else this is
    // the difference between measuring the antenna and measuring the
    // antenna plus a transformer of unknown ratio.
    row2->addWidget(caption(QStringLiteral("COAX"), this));
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
    row2->addWidget(m_cableBox);

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
    row2->addWidget(m_cableLenBox);

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
    head->addWidget(actionFrame, 1);

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
    col->addLayout(head);

    m_caution = new QLabel(QString{}, this);
    m_caution->setWordWrap(true);
    m_caution->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: %2; border-radius: 4px; "
        "padding: 6px 9px; font-size: 11px; }")
            .arg(QLatin1String(Style::kAmberText),
                 QLatin1String(Style::kAmberBg)));
    m_caution->setVisible(false);
    col->addWidget(m_caution);

    // ── The evidence ─────────────────────────────────────────────────
    m_curve = new SwrCurveWidget(this);
    col->addWidget(m_curve, 1);

    m_explain = new QLabel(QString{}, this);
    m_explain->setWordWrap(true);
    m_explain->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
            .arg(QLatin1String(Style::kTextSecondary)));
    col->addWidget(m_explain);

    // ── One row per band the sweep touches ───────────────────────────
    //
    // A dipole sweep touches one band and the three tiles above already
    // say everything; the table stays hidden. An end-fed swept across
    // HF touches eight, and "where is it resonant" is then a list, not
    // a number.
    m_bandTable = new QTableWidget(0, 5, this);
    m_bandTable->setHorizontalHeaderLabels({
        QStringLiteral("Band"), QStringLiteral("start"),
        QStringLiteral("middle"), QStringLiteral("end"),
        QStringLiteral("resonant at")});
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
    col->addLayout(learnRow);

    m_source = new QLabel(QString{}, this);
    m_source->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }")
            .arg(QLatin1String(Style::kTextScale)));
    col->addWidget(m_source);

    // ── Wiring ───────────────────────────────────────────────────────
    connect(m_openBtn, &QPushButton::clicked,
            this, &AntennaWindow::chooseFile);
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
        applyFeedline();
        refresh();
    });
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
    setSweep(Touchstone::readS1p(path));
}

void AntennaWindow::applyFeedline()
{
    const double len = m_cableLenBox ? m_cableLenBox->value() : 0.0;
    const int idx = m_cableBox ? m_cableBox->currentIndex() : 0;
    if (len <= 0.0 || idx < 0 || idx >= Feedline::catalogue().size()) {
        m_sweep = m_measured;
        return;
    }
    const auto out = Feedline::deEmbed(m_measured, len,
                                       Feedline::catalogue().at(idx));
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

    m_curve->setReference(m_previous, m_previousLabel);

    // Record it against the length currently entered. If the operator
    // types the new length afterwards — which is the usual order — the
    // length box's handler corrects this entry.
    const auto res = AntennaSweep::nearestResonance(
        m_sweep, m_targetBox->value() * 1e6);
    if (res.found) {
        m_session.record(m_lengthBox->value(), res.freqHz, s.source);
    }
    // A newly loaded sweep may be a different band, so the defaulted
    // target has to be recomputed. A target the operator typed is left
    // alone — see m_targetChosen.
    if (!m_targetChosen) { m_targetBox->setValue(0.0); }
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
        return;
    }

    const double limit = m_limitBox->value();
    m_bandTable->setRowCount(bands.size());

    for (int r = 0; r < bands.size(); ++r) {
        const AmateurBands::Band& b = bands.at(r);

        auto put = [this, r](int c, const QString& text, const QColor& col) {
            auto* it = new QTableWidgetItem(text);
            it->setForeground(col);
            m_bandTable->setItem(r, c, it);
            return it;
        };

        put(0, b.name, QColor(Style::kAccent));

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
                    QColor(v > limit ? Style::kRedBorder
                                     : Style::kTextPrimary));
            }
        }

        // A resonance inside this band is the thing an end-fed owner is
        // looking for. Saying "none" is as useful as saying where.
        QString where = QStringLiteral("—");
        QColor  col(Style::kTextInactive);
        for (const auto& c : res) {
            if (!b.contains(c.freqHz)) { continue; }
            where = QStringLiteral("%1 MHz · %2 Ω")
                        .arg(c.freqHz / 1e6, 0, 'f', 3)
                        .arg(c.resistanceOhms, 0, 'f', 0);
            col = QColor(Style::kAmberText);
            break;
        }
        put(4, where, col);
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
        ? QStringLiteral("at the antenna, %1 m of %2 removed")
              .arg(cableLen, 0, 'f', 1)
              .arg(Feedline::catalogue().at(cableIdx).name)
        : QStringLiteral("at the analyser port");

    m_source->setText(m_sweep.source.isEmpty()
        ? QString{}
        : QStringLiteral("%1 · %2 points · %3 to %4 MHz · %5")
              .arg(m_sweep.source)
              .arg(m_sweep.points.size())
              .arg(m_sweep.startHz() / 1e6, 0, 'f', 3)
              .arg(m_sweep.stopHz()  / 1e6, 0, 'f', 3)
              .arg(plane));

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
        val->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; border: none; }")
                .arg(QLatin1String(s > limit ? Style::kRedBorder
                                             : Style::kTextPrimary)));
    };
    const QString what = custom ? QStringLiteral("RANGE")
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
                    .arg(QLatin1String(covers ? Style::kGreenText
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

    const auto res = AntennaSweep::nearestResonance(m_sweep, targetHz);
    if (!res.found) {
        m_action->setText(QStringLiteral("?"));
        m_actionSub->setText(QStringLiteral(
            "No resonance in this sweep — the reactance never reaches "
            "zero. Sweep wider to find it before trimming anything."));
        m_caution->setVisible(false);
        return;
    }
    if (targetHz <= 0.0) {
        m_action->setText(QStringLiteral("—"));
        m_actionSub->setText(QStringLiteral(
            "Resonant at %1 MHz. Set a target frequency to get the "
            "change in centimetres.").arg(res.freqHz / 1e6, 0, 'f', 3));
        m_caution->setVisible(false);
        return;
    }

    const AntennaTrim::Kind kind = currentKind();
    // Through the session, so a measured exponent is used when there is
    // one. With fewer than two usable measurements this is exactly
    // AntennaTrim::compute with the textbook rule.
    const auto t = AntennaTrim::compute(kind, res.freqHz, targetHz,
                                        m_lengthBox->value(),
                                        session.valid ? session.exponent
                                                      : 1.0);

    if (std::abs(t.percentChange) < 0.1) {
        m_action->setText(QStringLiteral("nothing"));
        m_actionSub->setText(QStringLiteral(
            "Already resonant where you want it. Leave the wire alone."));
        m_caution->setVisible(false);
        return;
    }

    if (t.totalChangeM == 0.0) {
        m_action->setText(QStringLiteral("%1 %2 %")
                              .arg(t.lengthen ? QStringLiteral("+")
                                              : QStringLiteral("−"))
                              .arg(std::abs(t.percentChange), 0, 'f', 2));
        m_actionSub->setText(QStringLiteral(
            "%1. Enter the current length to get centimetres.")
                .arg(t.lengthen ? QStringLiteral("Longer")
                                : QStringLiteral("Shorter")));
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
    QStringList notes;
    if (t.halved) {
        notes << QStringLiteral(
            "This is half of the %1 cm the arithmetic asks for. Cut it in "
            "two passes and measure between — wire does not grow back.")
                  .arg(std::abs(t.perElementM) * 100.0, 0, 'f', 1);
    }
    if (!t.caution.isEmpty()) { notes << t.caution; }
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
