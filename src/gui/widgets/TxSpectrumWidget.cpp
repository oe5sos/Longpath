// =================================================================
// src/gui/widgets/TxSpectrumWidget.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See TxSpectrumWidget.h for why this is the one
// measurement in the channel strip that is about somebody else.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-11 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/widgets/TxSpectrumWidget.h"

#include "core/strip/MicSpectrum.h"
#include "gui/StyleConstants.h"

#include <QPainter>
#include <QPolygonF>
#include <QTimerEvent>

#include <algorithm>
#include <cmath>

namespace NereusSDR {
namespace {

// 4096 at 48 kHz is 11.7 Hz per bin — fine enough to place a band edge
// to better than a tenth of the tolerance anyone cares about, and small
// enough that the GUI thread can do it ten times a second without
// noticing.
constexpr int kFft = 4096;

// Ten a second. Faster tells you nothing new — the analysis averages
// over seconds — and costs a transform each time.
constexpr int kRedrawMs = 100;

// The vertical scale. Everything is relative to the peak, so 0 is the
// loudest bin and the floor is where the numbers stop meaning anything.
constexpr double kTopDb    =   3.0;
constexpr double kBottomDb = -80.0;

const QColor kCurve (Style::kAccent);
const QColor kHeld  (Style::kAmberText);
const QColor kEdge26(Style::kGreenText);
const QColor kEdge60(Style::kAmberWarn);
const QColor kGrid  (Style::kBorderSubtle);
const QColor kText  (Style::kTextPrimary);
const QColor kDim   (Style::kTextScale);
const QColor kBad   (Style::kRedBorder);

QString khz(double hz) { return QStringLiteral("%1").arg(hz / 1000.0, 0, 'f', 2); }

} // namespace

TxSpectrumWidget::TxSpectrumWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(280, 160);
}

QSize TxSpectrumWidget::sizeHint()        const { return {600, 300}; }
QSize TxSpectrumWidget::minimumSizeHint() const { return {280, 160}; }

void TxSpectrumWidget::setSource(const MicSpectrum* ring)
{
    m_ring = ring;
    m_lastSeen = 0;
    update();
}

void TxSpectrumWidget::setWindowSeconds(double s)
{
    m_windowSeconds = std::clamp(s, 0.25, 15.0);
}

void TxSpectrumWidget::setHold(bool on)
{
    if (m_hold == on) { return; }
    m_hold = on;
    // Turning hold ON starts a fresh maximum. Keeping whatever was
    // there from a previous session would show a peak the operator
    // never asked to remember.
    if (on) { resetHold(); }
    update();
}

void TxSpectrumWidget::resetHold()
{
    m_held.clear();
    m_heldOcc = {};
    update();
}

void TxSpectrumWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    // Only measure while visible. An FFT ten times a second behind a
    // tab nobody is looking at is a transform nobody asked for.
    if (m_timer == 0) { m_timer = startTimer(kRedrawMs); }
}

void TxSpectrumWidget::hideEvent(QHideEvent* e)
{
    QWidget::hideEvent(e);
    if (m_timer != 0) { killTimer(m_timer); m_timer = 0; }
}

void TxSpectrumWidget::timerEvent(QTimerEvent* e)
{
    if (e->timerId() != m_timer) { QWidget::timerEvent(e); return; }
    recompute();
}

void TxSpectrumWidget::recompute()
{
    if (m_ring == nullptr) { return; }

    const unsigned long long seen = m_ring->framesSeen();
    if (seen == m_lastSeen) {
        // Nothing new since last time: the transmit chain is not
        // running. Leave the last picture up rather than blanking it —
        // what you were transmitting a second ago is still the useful
        // thing to look at after you unkey.
        return;
    }
    m_lastSeen = seen;

    const int rate  = m_ring->sampleRate() > 0 ? m_ring->sampleRate() : 48000;
    const int want  = std::max(kFft,
                               int(m_windowSeconds * double(rate)));
    if (int(m_scratch.size()) < want) { m_scratch.resize(size_t(want)); }

    const int got = m_ring->snapshot(m_scratch.data(), want);
    if (got < kFft) { return; }   // ring has not filled yet

    const std::vector<float> block(m_scratch.begin(),
                                   m_scratch.begin() + got);
    std::vector<double> mag = TxSpectrumAnalysis::ltasDb(block, kFft);
    if (mag.empty()) {
        // Silence, or a block too short. ltasDb refuses rather than
        // returning a floor of noise that looks like a measurement.
        return;
    }

    const double binHz = double(rate) / double(kFft);
    m_mag = std::move(mag);
    m_occ = TxSpectrumAnalysis::occupiedBandwidth(m_mag, binHz);
    m_everMeasured = true;

    // ── Peak hold, per bin ───────────────────────────────────────────
    //
    // The maximum of each bin over the hold period, not the widest
    // single sweep. A speech spectrum moves constantly and the thing
    // that matters is the envelope of everything said, which is what a
    // per-bin maximum is.
    if (m_hold) {
        if (m_held.size() != m_mag.size()) {
            m_held = m_mag;
        } else {
            for (size_t i = 0; i < m_mag.size(); ++i) {
                m_held[i] = std::max(m_held[i], m_mag[i]);
            }
        }
        m_heldOcc = TxSpectrumAnalysis::occupiedBandwidth(m_held, binHz);
    }

    emit measured(m_hold && m_heldOcc.valid ? m_heldOcc : m_occ);
    update();
}

double TxSpectrumWidget::xFor(double hz) const
{
    const double t = std::clamp(hz / 6000.0, 0.0, 1.0);
    return m_plotL + t * (m_plotR - m_plotL);
}

double TxSpectrumWidget::yFor(double db) const
{
    const double v = std::clamp(db, kBottomDb, kTopDb);
    const double t = (v - kBottomDb) / (kTopDb - kBottomDb);
    return m_plotB - t * (m_plotB - m_plotT);
}

void TxSpectrumWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(Style::kPanelBg));

    m_plotL = 40.0;
    m_plotR = width() - 12.0;
    m_plotT = 24.0;
    m_plotB = height() - 42.0;
    const QRectF plot(m_plotL, m_plotT, m_plotR - m_plotL, m_plotB - m_plotT);

    p.setPen(QPen(kGrid, 1));
    p.setBrush(QColor(Style::kInsetBg));
    p.drawRect(plot);

    QFont small = p.font();
    small.setPixelSize(10);
    p.setFont(small);
    const QFontMetrics sfm(small);

    if (plot.width() < 20 || plot.height() < 20) { return; }

    if (m_ring == nullptr) {
        p.setPen(kDim);
        p.drawText(plot, Qt::AlignCenter | Qt::TextWordWrap,
                   QStringLiteral("No transmitter connected."));
        return;
    }
    if (!m_everMeasured) {
        p.setPen(kDim);
        p.drawText(plot, Qt::AlignCenter | Qt::TextWordWrap,
                   QStringLiteral("Nothing measured yet.\n\nSpeak with the "
                                  "transmitter running — the off-air "
                                  "monitor is enough, it does not have to "
                                  "go on the air."));
        return;
    }

    // ── Grid ─────────────────────────────────────────────────────────
    for (double db = 0.0; db >= kBottomDb + 1.0; db -= 20.0) {
        const double y = yFor(db);
        p.setPen(QPen(kGrid, 1, Qt::DotLine));
        p.drawLine(QPointF(m_plotL, y), QPointF(m_plotR, y));
        p.setPen(kDim);
        const QString t = QString::number(int(db));
        p.drawText(QPointF(m_plotL - 6.0 - sfm.horizontalAdvance(t),
                           y + 3.0), t);
    }
    for (double hz = 1000.0; hz < 6000.0; hz += 1000.0) {
        const double x = xFor(hz);
        p.setPen(QPen(kGrid, 1, Qt::DotLine));
        p.drawLine(QPointF(x, m_plotT), QPointF(x, m_plotB));
        p.setPen(kDim);
        p.drawText(QPointF(x - 6.0, m_plotB + 13.0),
                   QString::number(int(hz / 1000.0)));
    }
    p.setPen(kDim);
    p.drawText(QPointF(m_plotR - sfm.horizontalAdvance(
                           QStringLiteral("kHz")), m_plotB + 13.0),
               QStringLiteral("kHz"));

    const int rate = m_ring->sampleRate() > 0 ? m_ring->sampleRate() : 48000;
    const double binHz = double(rate) / double(kFft);

    auto drawCurve = [&](const std::vector<double>& mag, const QColor& col,
                         double widthPx, Qt::PenStyle style) {
        if (mag.empty()) { return; }
        QPolygonF line;
        line.reserve(int(mag.size()));
        for (size_t i = 0; i < mag.size(); ++i) {
            const double hz = double(i) * binHz;
            if (hz > 6000.0) { break; }
            line << QPointF(xFor(hz), yFor(mag[i]));
        }
        p.setClipRect(plot.adjusted(1, 1, -1, -1));
        p.setPen(QPen(col, widthPx, style));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(line);
        p.setClipping(false);
    };

    if (m_hold && !m_held.empty()) {
        drawCurve(m_held, kHeld, 2.0, Qt::SolidLine);
        drawCurve(m_mag,  kCurve, 1.2, Qt::SolidLine);
    } else {
        drawCurve(m_mag, kCurve, 2.0, Qt::SolidLine);
    }

    // ── The edges ────────────────────────────────────────────────────
    const TxSpectrumAnalysis::Occupancy& occ =
        (m_hold && m_heldOcc.valid) ? m_heldOcc : m_occ;

    if (!occ.valid) {
        p.setPen(kDim);
        p.drawText(QPointF(m_plotL, m_plotT - 8.0),
                   occ.note.isEmpty()
                       ? QStringLiteral("No usable measurement")
                       : occ.note);
        return;
    }

    auto mark = [&](double hz, const QColor& col, const QString& label) {
        if (hz <= 0.0 || hz > 6000.0) { return; }
        const double x = xFor(hz);
        p.setPen(QPen(col, 1.2, Qt::DashLine));
        p.drawLine(QPointF(x, m_plotT), QPointF(x, m_plotB));
        p.setPen(col);
        const double tw = sfm.horizontalAdvance(label);
        const bool leftOf = x + 4.0 + tw > m_plotR;
        p.drawText(QPointF(leftOf ? x - 4.0 - tw : x + 4.0, m_plotB - 4.0),
                   label);
    };
    mark(occ.lowHz60,  kEdge60, QStringLiteral("−60"));
    mark(occ.highHz60, kEdge60, QStringLiteral("−60"));
    mark(occ.lowHz26,  kEdge26, QStringLiteral("−26"));
    mark(occ.highHz26, kEdge26, QStringLiteral("−26"));

    // ── The numbers ──────────────────────────────────────────────────
    p.setPen(kText);
    QFont head = p.font();
    head.setPixelSize(11);
    p.setFont(head);

    // 2.7 kHz is the usual SSB filter. Wider than that at −26 dBc and
    // the emission is broader than the mode is supposed to be, which is
    // the point at which it stops being taste.
    const bool wide = occ.bandwidth26Hz > 3000.0;
    p.setPen(wide ? kBad : kText);
    p.drawText(QPointF(m_plotL, m_plotT - 8.0),
               QStringLiteral("%1 kHz at −26 dBc   ·   %2 kHz at −60   ·   "
                              "%3 to %4 kHz")
                   .arg(occ.bandwidth26Hz / 1000.0, 0, 'f', 2)
                   .arg(occ.bandwidth60Hz / 1000.0, 0, 'f', 2)
                   .arg(khz(occ.lowHz26), khz(occ.highHz26)));

    // ── What it is and is not ────────────────────────────────────────
    p.setFont(small);
    p.setPen(kDim);
    p.drawText(QPointF(m_plotL, m_plotB + 28.0),
               m_hold
                   ? QStringLiteral("Peak hold. Post-modulator siphon — "
                                    "before the amplifier and the antenna.")
                   : QStringLiteral("Live. Post-modulator siphon — before "
                                    "the amplifier and the antenna."));
}

} // namespace NereusSDR
