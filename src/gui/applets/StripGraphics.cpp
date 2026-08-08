// =================================================================
// src/gui/applets/StripGraphics.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See StripGraphics.h for why these two pictures
// and not more.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/StripGraphics.h"

#include "gui/StyleConstants.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

constexpr int kTileW   = 84;
constexpr int kTileH   = 46;
constexpr int kTileGap = 6;

QColor c(const char* hex) { return QColor(QString::fromLatin1(hex)); }

} // namespace

// ── StripChainView ───────────────────────────────────────────────────

StripChainView::StripChainView(QWidget* parent)
    : QWidget(parent)
{
    m_reduction.fill(0.0);
    setMinimumHeight(kTileH + 8);
    setCursor(Qt::PointingHandCursor);
}

void StripChainView::setChain(StripChain* chain)
{
    m_chain = chain;
    update();
}

void StripChainView::setReduction(StripChain::Stage s, double db)
{
    const int i = static_cast<int>(s);
    if (i < 0 || i >= StripChain::kStageCount) { return; }
    // Only repaint when the picture would actually change. At 10 Hz
    // across eight tiles an unconditional update() is a repaint of the
    // whole row for a number that moved by a hundredth of a decibel.
    if (std::abs(m_reduction[static_cast<size_t>(i)] - db) < 0.05) { return; }
    m_reduction[static_cast<size_t>(i)] = db;
    update(tileRect(i));
}

QSize StripChainView::sizeHint() const
{
    const int n = StripChain::kStageCount;
    return QSize(n * kTileW + (n - 1) * kTileGap, kTileH + 8);
}

QRect StripChainView::tileRect(int index) const
{
    return QRect(index * (kTileW + kTileGap), 4, kTileW, kTileH);
}

void StripChainView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool master = m_chain && m_chain->isEnabled();

    for (int i = 0; i < StripChain::kStageCount; ++i) {
        const auto st = static_cast<StripChain::Stage>(i);
        const bool on = m_chain && m_chain->stageEnabled(st);
        const QRect r = tileRect(i);

        // Three states, drawn as three states. A stage can be switched
        // on and doing nothing because the master is off, and painting
        // that the same as "running" is how an operator concludes the
        // strip is broken.
        QColor bg, edge, text;
        if (on && master) {
            bg = c(Style::kButtonBg); edge = c(Style::kAccent);
            text = c(Style::kTextPrimary);
        } else if (on) {
            bg = c(Style::kInsetBg); edge = c(Style::kBorderSubtle);
            text = c(Style::kTextSecondary);
        } else {
            bg = c(Style::kInsetBg); edge = c(Style::kInsetBorder);
            text = c(Style::kTextInactive);
        }

        p.setPen(QPen(edge, 1));
        p.setBrush(bg);
        p.drawRoundedRect(r.adjusted(0, 0, -1, -1), 3, 3);

        // Gain reduction, as a bar growing downward from the top edge.
        // Downward because that is the direction the gain is going, and
        // an operator reading it sideways is reading it wrong.
        const double gr = -m_reduction[static_cast<size_t>(i)];
        if (on && master && gr > 0.1) {
            const double frac = std::clamp(gr / kBarFullScaleDb, 0.0, 1.0);
            const int barH = int(frac * (r.height() - 18));
            QRect bar(r.left() + 4, r.top() + 3, r.width() - 8, barH);
            p.setPen(Qt::NoPen);
            // Amber, not red: this is the stage working as intended,
            // not a fault. Red belongs to things that are wrong.
            p.setBrush(QColor(0xd0, 0x90, 0x20, 150));
            p.drawRect(bar);
        }

        p.setPen(text);
        QFont f = p.font();
        f.setPointSizeF(9.5);
        p.setFont(f);
        p.drawText(r.adjusted(2, 2, -2, -14), Qt::AlignCenter,
                   QString::fromLatin1(StripChain::stageName(st)));

        if (on && master && gr > 0.1) {
            QFont sf = f;
            sf.setPointSizeF(8.0);
            p.setFont(sf);
            p.setPen(c(Style::kTextSecondary));
            p.drawText(QRect(r.left(), r.bottom() - 14, r.width(), 13),
                       Qt::AlignCenter,
                       QStringLiteral("-%1 dB").arg(gr, 0, 'f', 1));
        }

        if (i < StripChain::kStageCount - 1) {
            p.setPen(c(Style::kTextInactive));
            p.drawText(QRect(r.right(), r.top(), kTileGap, r.height()),
                       Qt::AlignCenter, QStringLiteral("›"));
        }
    }
}

void StripChainView::mousePressEvent(QMouseEvent* ev)
{
    for (int i = 0; i < StripChain::kStageCount; ++i) {
        if (tileRect(i).contains(ev->pos())) {
            // Opens the stage. Deliberately not a bypass toggle: the
            // row is the thing an operator's eye rests on while
            // speaking, and a click that changes the audio is a click
            // that happens by accident.
            emit stageClicked(i);
            return;
        }
    }
}

// ── StripLevelBars ───────────────────────────────────────────────────

StripLevelBars::StripLevelBars(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(52);
}

void StripLevelBars::setChain(StripChain* chain) { m_chain = chain; update(); }

QSize StripLevelBars::sizeHint() const { return QSize(320, 56); }

void StripLevelBars::tick()
{
    if (!m_chain) { return; }
    m_in  = m_chain->inputPeakDb();
    m_out = m_chain->outputPeakDb();

    // 20 dB per second of decay: fast enough that the hold tracks
    // speech, slow enough that a single syllable can be read.
    constexpr double kDecayDbPerTick = 2.0;   // at the 10 Hz meter timer
    m_inHold  = std::max(m_in,  m_inHold  - kDecayDbPerTick);
    m_outHold = std::max(m_out, m_outHold - kDecayDbPerTick);
    update();
}

void StripLevelBars::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), c(Style::kInsetBg));

    QFont f = p.font();
    f.setPointSizeF(8.5);
    p.setFont(f);

    const int labelW = 30;
    const int barH   = 12;
    const int gap    = 6;
    const QRect area = rect().adjusted(labelW + 4, 6, -70, -6);

    auto drawBar = [&](int y, const QString& name, double db, double hold,
                       const QColor& fill) {
        p.setPen(c(Style::kTextSecondary));
        p.drawText(QRect(2, y, labelW, barH), Qt::AlignRight | Qt::AlignVCenter,
                   name);

        const QRect track(area.left(), y, area.width(), barH);
        p.setPen(QPen(c(Style::kInsetBorder), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(track);

        auto xOf = [&](double v) {
            const double t = std::clamp((v - kFloorDb) / -kFloorDb, 0.0, 1.0);
            return track.left() + 1 + t * (track.width() - 2);
        };
        if (db > kFloorDb) {
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawRect(QRectF(track.left() + 1, y + 1,
                              xOf(db) - track.left() - 1, barH - 2));
        }
        if (hold > kFloorDb) {
            p.setPen(QPen(c(Style::kTextPrimary), 1));
            const int hx = int(xOf(hold));
            p.drawLine(hx, y + 1, hx, y + barH - 1);
        }
        // -6 dBFS mark: the level above which an SSB transmitter has
        // nothing left to give and the ALC starts making the decisions.
        p.setPen(QPen(QColor(0xd0, 0x60, 0x40), 1, Qt::DotLine));
        const int wx = int(xOf(-6.0));
        p.drawLine(wx, y, wx, y + barH);

        p.setPen(c(Style::kTextSecondary));
        p.drawText(QRect(track.right() + 6, y, 62, barH),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   db > kFloorDb ? QStringLiteral("%1 dB").arg(db, 0, 'f', 1)
                                 : QStringLiteral("--"));
    };

    const bool running = m_chain && m_chain->isEnabled();
    if (!running) {
        p.setPen(c(Style::kTextInactive));
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("strip off — nothing to measure"));
        return;
    }

    drawBar(6, QStringLiteral("in"), m_in, m_inHold,
            QColor(0x00, 0xb4, 0xd8, 170));
    drawBar(6 + barH + gap, QStringLiteral("out"), m_out, m_outHold,
            QColor(0x40, 0xc0, 0x80, 170));

    // The one number an operator actually needs from this picture: did
    // the strip make them louder, and by how much. Everything above the
    // limiter can add gain, and the total is not obvious from eight
    // separate settings.
    if (m_in > kFloorDb && m_out > kFloorDb) {
        const double delta = m_out - m_in;
        p.setPen(delta > 6.0 ? QColor(0xd0, 0x60, 0x40)
                             : c(Style::kTextSecondary));
        p.drawText(rect().adjusted(0, 0, -4, -2),
                   Qt::AlignRight | Qt::AlignBottom,
                   QStringLiteral("%1%2 dB")
                       .arg(delta >= 0 ? QStringLiteral("+") : QString())
                       .arg(delta, 0, 'f', 1));
    }
}

// ── StripEqCurve ─────────────────────────────────────────────────────

StripEqCurve::StripEqCurve(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(130);
}

void StripEqCurve::setChain(StripChain* chain)
{
    m_chain = chain;
    update();
}

void StripEqCurve::refresh() { update(); }

QSize StripEqCurve::sizeHint() const { return QSize(520, 150); }

double StripEqCurve::xForHz(double hz, const QRect& r) const
{
    // Log axis, because hearing is logarithmic and because a linear one
    // spends 90% of its width above 2 kHz, where nothing in a voice
    // needs adjusting.
    const double t = (std::log10(hz) - std::log10(kMinHz))
                   / (std::log10(kMaxHz) - std::log10(kMinHz));
    return r.left() + t * r.width();
}

double StripEqCurve::yForDb(double db, const QRect& r) const
{
    const double t = (kRangeDb - db) / (2.0 * kRangeDb);
    return r.top() + std::clamp(t, 0.0, 1.0) * r.height();
}

void StripEqCurve::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect().adjusted(30, 8, -8, -18);
    p.fillRect(rect(), c(Style::kInsetBg));
    p.setPen(QPen(c(Style::kInsetBorder), 1));
    p.drawRect(r.adjusted(0, 0, -1, -1));

    // ── Grid ─────────────────────────────────────────────────────────
    QFont f = p.font();
    f.setPointSizeF(8.0);
    p.setFont(f);

    for (double hz : {50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0,
                      5000.0, 10000.0}) {
        const int x = int(xForHz(hz, r));
        p.setPen(QPen(c(Style::kGroove), 1, Qt::DotLine));
        p.drawLine(x, r.top(), x, r.bottom());
        p.setPen(c(Style::kTextInactive));
        p.drawText(QRect(x - 20, r.bottom() + 2, 40, 14), Qt::AlignCenter,
                   hz >= 1000.0 ? QStringLiteral("%1k").arg(hz / 1000.0, 0, 'g', 2)
                                : QStringLiteral("%1").arg(hz, 0, 'f', 0));
    }
    for (double db : {-12.0, -6.0, 0.0, 6.0, 12.0}) {
        const int y = int(yForDb(db, r));
        p.setPen(QPen(db == 0.0 ? c(Style::kBorder) : c(Style::kGroove), 1,
                      db == 0.0 ? Qt::SolidLine : Qt::DotLine));
        p.drawLine(r.left(), y, r.right(), y);
        p.setPen(c(Style::kTextInactive));
        p.drawText(QRect(0, y - 7, 26, 14), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("%1").arg(db, 0, 'f', 0));
    }

    // ── The speech band ──────────────────────────────────────────────
    //
    // Marked because every decision on this curve is really a decision
    // about what happens between 300 Hz and 3 kHz. Outside it, the
    // transmit filter has the last word whatever the EQ does.
    {
        const int x1 = int(xForHz(300.0, r));
        const int x2 = int(xForHz(3000.0, r));
        p.fillRect(QRect(x1, r.top(), x2 - x1, r.height()),
                   QColor(0x00, 0xb4, 0xd8, 18));
        p.setPen(c(Style::kTextInactive));
        p.drawText(QRect(x1, r.top() + 2, x2 - x1, 12), Qt::AlignCenter,
                   QStringLiteral("speech"));
    }

    if (!m_chain) {
        p.setPen(c(Style::kTextSecondary));
        p.drawText(r, Qt::AlignCenter,
                   QStringLiteral("Not connected"));
        return;
    }

    // ── The response ─────────────────────────────────────────────────
    //
    // Summed from ClientEq's own analytic magnitude, band by band. Using
    // the filter's function rather than a second implementation is the
    // point: a curve drawn from a model of the filter is a curve that
    // can be wrong in a way nobody notices until someone measures the
    // transmitter.
    ClientEq& eq = m_chain->eq();
    const int bands = eq.activeBandCount();
    const bool eqOn = m_chain->stageEnabled(StripChain::Stage::Eq)
                      && m_chain->isEnabled();

    QPainterPath path;
    const int steps = std::max(2, r.width());
    for (int i = 0; i <= steps; ++i) {
        const double t = double(i) / steps;
        const double hz = std::pow(10.0,
            std::log10(kMinHz)
            + t * (std::log10(kMaxHz) - std::log10(kMinHz)));

        double db = 0.0;
        for (int b = 0; b < bands; ++b) {
            db += double(ClientEq::bandMagnitudeDb(
                eq.band(b), float(hz), eq.sampleRate(), eq.filterFamily()));
        }
        const QPointF pt(xForHz(hz, r), yForDb(db, r));
        if (i == 0) { path.moveTo(pt); } else { path.lineTo(pt); }
    }

    // Dim when the EQ is not actually in circuit, rather than hidden:
    // the operator is often shaping the curve before switching it on,
    // and a blank panel would look broken.
    p.setPen(QPen(eqOn ? c(Style::kAccent) : c(Style::kTextInactive),
                  eqOn ? 2.0 : 1.0));
    p.setClipRect(r);
    p.drawPath(path);
    p.setClipping(false);

    if (!eqOn) {
        p.setPen(c(Style::kTextSecondary));
        p.drawText(r.adjusted(0, 0, -6, -4), Qt::AlignRight | Qt::AlignBottom,
                   QStringLiteral("not in circuit"));
    }
}

} // namespace NereusSDR
