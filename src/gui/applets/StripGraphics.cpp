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

#include "core/VoiceAnalyzer.h"
#include "core/strip/ClientTube.h"
#include "core/strip/ClientPudu.h"
#include "core/strip/ClientDeEss.h"
#include "core/strip/ClientGate.h"
#include "core/strip/ClientComp.h"
#include "core/strip/ClientFinalLimiter.h"

#include "gui/StyleConstants.h"

#include <QEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <complex>

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

// ── StripDynamicsCurve ───────────────────────────────────────────────

StripDynamicsCurve::StripDynamicsCurve(Stage s, QWidget* parent)
    : QWidget(parent), m_stage(s)
{
    setMinimumHeight(150);
    setMouseTracking(true);

    // The ball runs on its own 30 Hz clock, not on the window's 10 Hz
    // meter tick. At 10 Hz a smoothed ball is visibly steppy — it is
    // gliding, and you can count the steps. AetherSDR uses 33 ms and
    // that is what makes the motion read as motion.
    m_ballTimer = new QTimer(this);
    m_ballTimer->setInterval(kBallTimerMs);
    connect(m_ballTimer, &QTimer::timeout, this, [this]() {
        if (!m_chain || !isVisible()) { return; }
        double target = -120.0;
        double out    = -120.0;
        switch (m_stage) {
        case Stage::Gate:
            target = double(m_chain->gate().inputPeakDb());
            out    = double(m_chain->gate().outputPeakDb());
            break;
        case Stage::Compressor:
            target = double(m_chain->comp().inputPeakDb());
            out    = double(m_chain->comp().outputPeakDb());
            break;
        case Stage::Limiter:
            target = double(m_chain->limiter().inputPeakDb());
            out    = double(m_chain->limiter().outputPeakDb());
            break;
        }
        // One pole, both directions. The point is that there is never a
        // jump for the eye to chase.
        m_liveIn  += kBallSmoothAlpha * (target - m_liveIn);
        m_liveOut += kBallSmoothAlpha * (out - m_liveOut);
        update();
    });
    m_ballTimer->start();
}

void StripDynamicsCurve::mouseMoveEvent(QMouseEvent* ev)
{
    m_cursor     = ev->pos();
    m_haveCursor = true;
    update();
}

void StripDynamicsCurve::leaveEvent(QEvent*)
{
    m_haveCursor = false;
    update();
}

namespace {

// ── The recessed frame ───────────────────────────────────────────────
//
// The same sunken glass as the frequency display: a dark inner edge at
// the top and a faint lit one at the bottom. Two lines, not a
// stylesheet, because these widgets paint themselves.
void drawInset(QPainter& p, const QRect& r)
{
    p.fillRect(r, QColor(0x08, 0x08, 0x10));
    p.setPen(QPen(QColor(0x16, 0x20, 0x2e), 1));
    p.drawRect(r);
    p.setPen(QPen(QColor(0, 0, 0, 190), 1));
    p.drawLine(r.left() + 1, r.top() + 1, r.right() - 1, r.top() + 1);
    p.setPen(QPen(QColor(0x50, 0x78, 0x96, 40), 1));
    p.drawLine(r.left() + 1, r.bottom() - 1, r.right() - 1, r.bottom() - 1);
}

// The readout box, bottom-right so it never sits under the pointer.
void drawReadout(QPainter& p, const QRect& r, const QString& text)
{
    QFont rf = p.font();
    rf.setPointSizeF(8.0);
    p.setFont(rf);
    const int w = QFontMetrics(rf).horizontalAdvance(text) + 12;
    const QRect box(r.right() - w - 4, r.bottom() - 20, w, 16);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 170));
    p.drawRoundedRect(box, 3, 3);
    p.setPen(c(Style::kTextPrimary));
    p.drawText(box, Qt::AlignCenter, text);
}

} // namespace

void StripDynamicsCurve::setChain(StripChain* chain)
{
    m_chain = chain;
    update();
}

QSize StripDynamicsCurve::sizeHint() const { return QSize(220, 170); }

void StripDynamicsCurve::refresh()
{
    // The ball has its own timer; this only exists so the parameters
    // that came from a knob turn are redrawn promptly.
    update();
}

double StripDynamicsCurve::outputDb(double inDb) const
{
    if (!m_chain) { return inDb; }

    switch (m_stage) {
    case Stage::Gate: {
        // A downward expander. Above the threshold it is a straight
        // wire; below it, every decibel down costs `ratio` decibels,
        // and the total attenuation stops at the floor.
        ClientGate& g = m_chain->gate();
        const double t = double(g.thresholdDb());
        const double r = std::max(1.0, double(g.ratio()));
        const double floorDb = double(g.floorDb());
        if (inDb >= t) { return inDb; }
        const double cut = std::max((inDb - t) * (r - 1.0), floorDb);
        return inDb + cut;
    }
    case Stage::Compressor: {
        // Soft knee, the standard formulation: straight below the knee,
        // quadratic through it, straight again above.
        ClientComp& c = m_chain->comp();
        const double t = double(c.thresholdDb());
        const double r = std::max(1.0, double(c.ratio()));
        const double w = std::max(0.0, double(c.kneeDb()));
        const double m = double(c.makeupDb());
        double y;
        if (w > 0.0 && std::abs(inDb - t) * 2.0 <= w) {
            const double d = inDb - t + w / 2.0;
            y = inDb + (1.0 / r - 1.0) * d * d / (2.0 * w);
        } else if (inDb < t) {
            y = inDb;
        } else {
            y = t + (inDb - t) / r;
        }
        return y + m;
    }
    case Stage::Limiter: {
        const double ceil = double(m_chain->limiter().ceilingDb());
        const double trim = double(m_chain->limiter().outputTrimDb());
        return std::min(inDb, ceil) + trim;
    }
    }
    return inDb;
}

QString StripDynamicsCurve::explain() const
{
    if (!m_chain) { return QStringLiteral("No radio connected."); }

    const double in  = m_liveIn;
    const bool quiet = in < minDb() + 3.0;

    switch (m_stage) {
    case Stage::Gate: {
        ClientGate& g = m_chain->gate();
        const double thr = double(g.thresholdDb());
        const double ret = double(g.returnDb());
        const double gr  = double(g.gainReductionDb());
        if (quiet) {
            return QStringLiteral(
                "<b style='color:#607080'>Silent</b><br>Nothing is arriving "
                "at the gate. Talk, or check the microphone.");
        }
        if (in >= thr) {
            return QStringLiteral(
                "<b style='color:#00ff88'>▲ Open</b><br>You are "
                "<b>%1 dB above</b> the threshold. The gate is passing your "
                "voice and taking nothing off.")
                .arg(in - thr, 0, 'f', 1);
        }
        if (in >= thr - ret) {
            return QStringLiteral(
                "<b style='color:#ffb800'>◆ In the sticky zone</b><br>"
                "%1 dB below the threshold but still inside the deadband, "
                "so the gate holds whatever it was doing. This is the part "
                "that stops it chattering on a breath.")
                .arg(thr - in, 0, 'f', 1);
        }
        return QStringLiteral(
            "<b style='color:#e05050'>▼ Closing</b><br>%1 dB below the "
            "threshold; the gate is taking off <b>%2 dB</b>. If that is "
            "your voice rather than the room, the threshold is too high.")
            .arg(thr - in, 0, 'f', 1).arg(gr, 0, 'f', 1);
    }
    case Stage::Compressor: {
        ClientComp& c = m_chain->comp();
        const double thr = double(c.thresholdDb());
        const double gr  = double(c.gainReductionDb());
        if (quiet) {
            return QStringLiteral(
                "<b style='color:#607080'>Silent</b><br>Nothing is reaching "
                "the compressor.");
        }
        if (gr < 0.3) {
            return QStringLiteral(
                "<b style='color:#8090a0'>Not working</b><br>You are "
                "%1 dB below the threshold, so the compressor is a straight "
                "wire right now. Lower the threshold if you wanted it to do "
                "something.")
                .arg(thr - in, 0, 'f', 1);
        }
        if (gr > 12.0) {
            return QStringLiteral(
                "<b style='color:#e05050'>Working hard</b><br>Taking off "
                "<b>%1 dB</b>. Past about 12 dB a compressor stops evening "
                "a voice out and starts being the loudest thing about it.")
                .arg(gr, 0, 'f', 1);
        }
        return QStringLiteral(
            "<b style='color:#00ff88'>Working</b><br>Taking off "
            "<b>%1 dB</b> on the peaks. That is the useful range: audibly "
            "steadier, still recognisably a voice.")
            .arg(gr, 0, 'f', 1);
    }
    case Stage::Limiter: {
        const double ceil = double(m_chain->limiter().ceilingDb());
        const double gr   = double(m_chain->limiter().gainReductionDb());
        if (quiet) {
            return QStringLiteral(
                "<b style='color:#607080'>Silent</b><br>Nothing to limit.");
        }
        if (gr < 0.2) {
            return QStringLiteral(
                "<b style='color:#00ff88'>Idle — which is correct</b><br>"
                "You are %1 dB under the ceiling. A brickwall that never "
                "acts is a brickwall doing its job.")
                .arg(ceil - in, 0, 'f', 1);
        }
        return QStringLiteral(
            "<b style='color:#ffb800'>Catching peaks</b><br>Holding back "
            "<b>%1 dB</b>. Occasional is fine; if this is constant, the "
            "limiter is doing the compressor's job and doing it worse.")
            .arg(gr, 0, 'f', 1);
    }
    }
    return {};
}

QString StripDynamicsCurve::legend() const
{
    QString band;
    if (m_stage == Stage::Gate) {
        band = QStringLiteral(
            "<span style='color:#00b4d8'>▬</span> the sticky zone — inside "
            "it the gate keeps its current state<br>");
    }
    return QStringLiteral(
        "<span style='color:#c8d8e8'>●</span> your signal: across is what "
        "goes in, up is what comes out<br>"
        "<span style='color:#405060'>╱</span> dashed: where nothing "
        "happens<br>%1"
        "<span style='color:#ffb800'>┊</span> the threshold you set")
        .arg(band);
}

double StripDynamicsCurve::xFor(double db, const QRect& r) const
{
    const double t = (db - minDb()) / (kMaxDb - minDb());
    return r.left() + std::clamp(t, 0.0, 1.0) * r.width();
}

double StripDynamicsCurve::yFor(double db, const QRect& r) const
{
    const double t = (db - minDb()) / (kMaxDb - minDb());
    return r.bottom() - std::clamp(t, 0.0, 1.0) * r.height();
}

void StripDynamicsCurve::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect().adjusted(4, 4, -5, -5);
    p.fillRect(rect(), c(Style::kAppBg));
    drawInset(p, r);

    QFont f = p.font();
    f.setPointSizeF(7.5);
    p.setFont(f);

    // ── The grid, and why it is labelled ─────────────────────────────
    //
    // "You sometimes cannot tell what you are looking at" — and the
    // reason was that I left the axes unlabelled on purpose, reasoning
    // that Zeus does. AetherSDR, the thing actually named as the
    // reference, labels them: majors every 12 dB on BOTH axes, minors
    // every 12 dB in between, unity dashed. That turns two anonymous
    // directions into decibels, and it is the single change that makes
    // the picture readable rather than decorative.
    //
    // Both axes, not one. The horizontal is input and the vertical is
    // output; labelling only one leaves the reader to assume the other
    // matches, which is exactly the assumption a compressor breaks.
    p.setPen(QPen(c(Style::kGroove), 1, Qt::DotLine));
    for (double db = minDb() + 6.0; db < 0.0; db += 12.0) {
        const int x = int(xFor(db, r));
        const int y = int(yFor(db, r));
        p.drawLine(x, r.top(), x, r.bottom());
        p.drawLine(r.left(), y, r.right(), y);
    }
    p.setPen(QPen(c(Style::kBorderSubtle), 1));
    for (double db = minDb(); db <= 0.0; db += 12.0) {
        const int x = int(xFor(db, r));
        const int y = int(yFor(db, r));
        p.drawLine(x, r.top(), x, r.bottom());
        p.drawLine(r.left(), y, r.right(), y);
        if (db > minDb() && db < 0.0) {
            p.setPen(c(Style::kTextScale));
            p.drawText(QRectF(x + 2, r.bottom() - 12, 24, 10),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QString::number(int(db)));
            p.drawText(QRectF(r.left() + 2, y - 11, 24, 10),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QString::number(int(db)));
            p.setPen(QPen(c(Style::kBorderSubtle), 1));
        }
    }
    p.setPen(QPen(c(Style::kTextInactive), 1, Qt::DashLine));
    p.drawLine(QPointF(xFor(minDb(), r), yFor(minDb(), r)),
               QPointF(xFor(kMaxDb, r), yFor(kMaxDb, r)));

    if (!m_chain) {
        p.setPen(c(Style::kTextSecondary));
        p.drawText(r, Qt::AlignCenter, QStringLiteral("Not connected"));
        return;
    }

    // The threshold, marked. It is the one number on this picture the
    // operator actually sets, so it gets a line rather than being left
    // to be inferred from where the curve bends.
    double thr = 0.0;
    bool haveThr = true;
    switch (m_stage) {
    case Stage::Gate:       thr = double(m_chain->gate().thresholdDb()); break;
    case Stage::Compressor: thr = double(m_chain->comp().thresholdDb()); break;
    case Stage::Limiter:    thr = double(m_chain->limiter().ceilingDb()); break;
    }
    if (haveThr) {
        p.setPen(QPen(QColor(0xd0, 0x90, 0x20, 160), 1, Qt::DashLine));
        const int tx = int(xFor(thr, r));
        p.drawLine(tx, r.top(), tx, r.bottom());
    }

    // ── The gate's deadband, as a band ───────────────────────────────
    //
    // I drew this as a second dotted line. AetherSDR shades the region
    // between (threshold − return) and threshold instead, and that is
    // better for a reason worth writing down: the operator's question is
    // not "where are the two levels" but "is my voice inside the sticky
    // zone right now", and a filled region answers it by containing the
    // ball or not containing it. Two lines make you measure.
    if (m_stage == Stage::Gate) {
        const double ret = double(m_chain->gate().returnDb());
        if (ret > 0.05) {
            const double xR = xFor(thr, r);
            const double xL = xFor(thr - ret, r);
            if (xR > xL) {
                QColor band = c(Style::kAccent);
                band.setAlpha(45);
                p.fillRect(QRectF(xL, r.top(), xR - xL, r.height()), band);
            }
        }
    }

    QPainterPath path;
    for (int x = r.left(); x <= r.right(); ++x) {
        const double inDb = minDb()
            + (double(x - r.left()) / std::max(1, r.width()))
              * (kMaxDb - minDb());
        const QPointF pt(x, yFor(outputDb(inDb), r));
        if (x == r.left()) { path.moveTo(pt); } else { path.lineTo(pt); }
    }
    p.setClipRect(r);

    // Filled to the floor, so the shape reads as an area rather than as
    // a line to be traced.
    {
        QPainterPath fill = path;
        fill.lineTo(QPointF(r.right(), r.bottom()));
        fill.lineTo(QPointF(r.left(), r.bottom()));
        fill.closeSubpath();
        QLinearGradient g(0, r.top(), 0, r.bottom());
        QColor top = c(Style::kAccent); top.setAlpha(76);
        QColor bot = c(Style::kAccent); bot.setAlpha(0);
        g.setColorAt(0.0, top);
        g.setColorAt(1.0, bot);
        p.fillPath(fill, g);
    }
    // Amber for the gate, cyan for the others — AetherSDR's convention,
    // and it works: the colour says which kind of stage you are looking
    // at before you have read the title.
    const QColor curveCol = (m_stage == Stage::Gate)
        ? QColor(0xf0, 0x9a, 0x30) : QColor(0x00, 0xe5, 0xff);
    QPen curvePen(curveCol, 2.1);
    curvePen.setJoinStyle(Qt::RoundJoin);
    curvePen.setCapStyle(Qt::RoundCap);
    p.setPen(curvePen);
    p.drawPath(path);

    // A dot where the threshold meets the curve, so the eye finds the
    // knee without tracing the dashed line down to the axis.
    {
        p.setPen(Qt::NoPen);
        p.setBrush(curveCol);
        p.drawEllipse(QPointF(xFor(thr, r), yFor(outputDb(thr), r)), 3.0, 3.0);
    }

    // ── Where the signal is, right now ───────────────────────────────
    //
    // The dot is the point of the whole picture. A transfer curve on its
    // own is a manual page; the dot turns it into an instrument, because
    // it answers the only question the operator actually has — am I
    // hitting this stage at all, and where.
    // A radial gradient, so the ball reads as a light source rather than
    // as a translucent disc, and a white core inside it. My first
    // version was a flat amber circle with a flat amber halo, and it
    // disappeared against the amber threshold line. The white core is
    // what makes it findable against anything.
    if (m_liveIn > minDb() + 1.0) {
        const QPointF dot(xFor(std::clamp(m_liveIn, minDb(), kMaxDb), r),
                          yFor(std::clamp(m_liveOut, minDb(), kMaxDb), r));
        constexpr double kGlow = 11.0;
        QRadialGradient g(dot, kGlow);
        QColor glow(0xff, 0xb8, 0x00);
        glow.setAlpha(200); g.setColorAt(0.0, glow);
        glow.setAlpha(0);   g.setColorAt(1.0, glow);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(dot, kGlow, kGlow);
        p.setBrush(c(Style::kTextPrimary));
        p.drawEllipse(dot, 3.5, 3.5);
    }

    // ── The scale, only when asked for ───────────────────────────────
    if (m_haveCursor && r.contains(m_cursor)) {
        QColor guide = c(Style::kTextInactive);
        guide.setAlpha(110);
        p.setPen(QPen(guide, 1, Qt::DotLine));
        p.drawLine(m_cursor.x(), r.top(), m_cursor.x(), r.bottom());
        p.drawLine(r.left(), m_cursor.y(), r.right(), m_cursor.y());
        p.setClipping(false);

        const double inDb = minDb() + (double(m_cursor.x() - r.left())
            / std::max(1, r.width())) * (kMaxDb - minDb());
        drawReadout(p, r, QStringLiteral("%1 → %2 dB")
                              .arg(inDb, 0, 'f', 1)
                              .arg(outputDb(inDb), 0, 'f', 1));
    }
    p.setClipping(false);

    p.setPen(c(Style::kTextScale));
    p.drawText(r.adjusted(3, 0, 0, -1), Qt::AlignRight | Qt::AlignBottom,
               QStringLiteral("input dBFS →"));
    p.save();
    p.translate(r.left() + 11, r.top() + 4);
    p.rotate(90);
    p.drawText(0, 0, QStringLiteral("output dBFS →"));
    p.restore();
}

// ── StripShaperCurve ─────────────────────────────────────────────────

StripShaperCurve::StripShaperCurve(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(150);
    m_ballTimer = new QTimer(this);
    m_ballTimer->setInterval(33);
    connect(m_ballTimer, &QTimer::timeout, this, [this]() {
        if (!m_chain || !isVisible()) { return; }
        const double target = double(m_chain->tube().inputPeakDb());
        m_livePeak += 0.30 * (target - m_livePeak);
        update();
    });
    m_ballTimer->start();
}

void StripShaperCurve::setChain(StripChain* chain) { m_chain = chain; update(); }
QSize StripShaperCurve::sizeHint() const { return QSize(220, 170); }

void StripShaperCurve::refresh() { update(); }

QString StripShaperCurve::explain() const
{
    if (!m_chain) { return QStringLiteral("No radio connected."); }
    ClientTube& t = m_chain->tube();
    const double drive = double(t.driveDb());

    if (m_livePeak < -55.0) {
        return QStringLiteral(
            "<b style='color:#607080'>Silent</b><br>Nothing is reaching the "
            "tube.");
    }
    // How far up the curve the signal actually gets. This is the whole
    // question for a waveshaper and it is not answerable from the knobs.
    const double reach = std::pow(10.0, m_livePeak / 20.0)
                       * std::pow(10.0, drive / 20.0);
    if (reach < 0.25) {
        return QStringLiteral(
            "<b style='color:#8090a0'>A straight wire</b><br>Your voice is "
            "only reaching the flat middle of the curve, so this stage is "
            "doing nothing audible whatever the knobs say. More drive, or "
            "more level ahead of it.");
    }
    if (reach > 1.4) {
        return QStringLiteral(
            "<b style='color:#e05050'>Deep in the bend</b><br>You are well "
            "into the flat top of the curve. That is heavy distortion — it "
            "will be loud, and it will be heard as distortion rather than "
            "as warmth.");
    }
    return QStringLiteral(
        "<b style='color:#00ff88'>In the bend</b><br>Your voice is reaching "
        "the curved part, which is where harmonics come from. The gap "
        "between the orange curve and the dashed line is how much is being "
        "added.");
}

QString StripShaperCurve::legend() const
{
    return QStringLiteral(
        "<span style='color:#c8d8e8'>●</span> how far up the curve your "
        "voice reaches, either side of zero<br>"
        "<span style='color:#f09a30'>▬</span> the shaper — the audio "
        "thread's own function, not a drawing of it<br>"
        "<span style='color:#405060'>╱</span> dashed: no distortion. The "
        "distance between the two IS the distortion.");
}

void StripShaperCurve::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect().adjusted(4, 4, -5, -5);
    p.fillRect(rect(), c(Style::kAppBg));
    drawInset(p, r);

    p.setPen(QPen(c(Style::kGroove), 1, Qt::DotLine));
    p.drawLine(r.left(), r.center().y(), r.right(), r.center().y());
    p.drawLine(r.center().x(), r.top(), r.center().x(), r.bottom());

    // Unity, dashed. The gap between it and the curve IS the distortion.
    p.setPen(QPen(c(Style::kTextInactive), 1, Qt::DashLine));
    p.drawLine(QPointF(r.left(), r.bottom()), QPointF(r.right(), r.top()));

    // Say what the axes are. Without this the picture is two curves and
    // a dot, and "what am I looking at" is a fair question.
    {
        QFont sf = p.font();
        sf.setPointSizeF(7.5);
        p.setFont(sf);
        p.setPen(c(Style::kTextScale));
        p.drawText(r.adjusted(3, 0, -3, -1), Qt::AlignRight | Qt::AlignBottom,
                   QStringLiteral("input →"));
        p.drawText(r.adjusted(3, 3, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                   QStringLiteral("output"));
        p.drawText(r.adjusted(0, 3, -3, 0), Qt::AlignRight | Qt::AlignTop,
                   QStringLiteral("dashed = no distortion"));
    }

    if (!m_chain) {
        p.setPen(c(Style::kTextSecondary));
        p.drawText(r, Qt::AlignCenter, QStringLiteral("Not connected"));
        return;
    }

    ClientTube& t = m_chain->tube();
    const double drive = std::pow(10.0, double(t.driveDb()) / 20.0);
    const double bias  = double(t.biasAmount());
    const ClientTube::Model model = t.model();

    // Drawn from ClientTube::shapeAt — the function the audio thread
    // runs. Not a lookalike.
    QPainterPath path;
    for (int px = r.left(); px <= r.right(); ++px) {
        const double x =
            -1.5 + 3.0 * double(px - r.left()) / std::max(1, r.width());
        const double y = double(ClientTube::shapeAt(float(x * drive),
                                                    float(bias), model));
        const double yy = r.center().y() - std::clamp(y, -1.6, 1.6)
                          * (r.height() / 3.2);
        const QPointF pt(px, yy);
        if (px == r.left()) { path.moveTo(pt); } else { path.lineTo(pt); }
    }
    p.setClipRect(r);
    p.setPen(QPen(QColor(0xf0, 0x9a, 0x30), 2.0));
    p.drawPath(path);

    // How far up the curve the signal is actually reaching. A shaper
    // driven at -40 dBFS is a straight wire no matter how the knobs are
    // set, and the picture should say so rather than implying warmth
    // that is not happening.
    if (m_livePeak > -60.0) {
        const double amp = std::pow(10.0, m_livePeak / 20.0);
        for (double sgn : {-1.0, 1.0}) {
            const double x = sgn * amp;
            const double px = r.left()
                + (x + 1.5) / 3.0 * r.width();
            const double y = double(ClientTube::shapeAt(float(x * drive),
                                                        float(bias), model));
            const double py = r.center().y() - std::clamp(y, -1.6, 1.6)
                              * (r.height() / 3.2);
            QRadialGradient g(QPointF(px, py), 10.0);
            QColor glow(0xff, 0xb8, 0x00);
            glow.setAlpha(200); g.setColorAt(0.0, glow);
            glow.setAlpha(0);   g.setColorAt(1.0, glow);
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawEllipse(QPointF(px, py), 10.0, 10.0);
            p.setBrush(c(Style::kTextPrimary));
            p.drawEllipse(QPointF(px, py), 3.0, 3.0);
        }
    }
    p.setClipping(false);
}

// ── StripBandCurve ───────────────────────────────────────────────────

StripBandCurve::StripBandCurve(Stage s, QWidget* parent)
    : QWidget(parent), m_stage(s)
{
    setMinimumHeight(150);
}

void StripBandCurve::setChain(StripChain* chain) { m_chain = chain; update(); }
QSize StripBandCurve::sizeHint() const { return QSize(220, 170); }

void StripBandCurve::refresh()
{
    if (m_chain && m_stage == Stage::DeEsser) {
        m_liveGr = double(m_chain->deEss().gainReductionDb());
    }
    update();
}

double StripBandCurve::xForHz(double hz, const QRect& r) const
{
    const double lo = std::log10(100.0);
    const double hi = std::log10(12000.0);
    const double t  = (std::log10(std::max(20.0, hz)) - lo) / (hi - lo);
    return r.left() + std::clamp(t, 0.0, 1.0) * r.width();
}

QString StripBandCurve::explain() const
{
    if (!m_chain) { return QStringLiteral("No radio connected."); }

    if (m_stage == Stage::DeEsser) {
        ClientDeEss& d = m_chain->deEss();
        const double f0 = double(d.frequencyHz());
        if (m_liveGr > 0.3) {
            return QStringLiteral(
                "<b style='color:#ffb800'>Working</b><br>Taking off "
                "<b>%1 dB</b> around %2 Hz. It only acts on that band, so "
                "the rest of your voice is untouched.")
                .arg(m_liveGr, 0, 'f', 1).arg(f0, 0, 'f', 0);
        }
        return QStringLiteral(
            "<b style='color:#8090a0'>Listening, not acting</b><br>Nothing "
            "loud enough at %1 Hz to trigger it. Either you are not "
            "sibilant, or it is pointed at the wrong band — that is the "
            "commonest way to end up with a de-esser that does nothing.")
            .arg(f0, 0, 'f', 0);
    }

    ClientPudu& p = m_chain->pudu();
    const double lo = double(p.pooMix());
    const double hi = double(p.dooMix());
    if (lo < 0.02 && hi < 0.02) {
        return QStringLiteral(
            "<b style='color:#8090a0'>Nothing mixed in</b><br>Both "
            "generators are at zero, so the stage is running and silent.");
    }
    return QStringLiteral(
        "<b style='color:#00ff88'>Generating</b><br>Low at %1 Hz mixed "
        "<b>%2%</b>, high at %3 Hz mixed <b>%4%</b>. This stage invents "
        "content that was not in your voice; past a point it stops "
        "sounding like you.")
        .arg(double(p.pooTuneHz()), 0, 'f', 0).arg(int(lo * 100.0))
        .arg(double(p.dooTuneHz()), 0, 'f', 0).arg(int(hi * 100.0));
}

QString StripBandCurve::legend() const
{
    if (m_stage == Stage::DeEsser) {
        return QStringLiteral(
            "<span style='color:#f09a30'>▬</span> the band it LISTENS to — "
            "not the cut it makes<br>"
            "<span style='color:#f09a30'>┊</span> where it is centred<br>"
            "<span style='color:#ffb800'>▐</span> right-hand bar: how hard "
            "it is working now");
    }
    return QStringLiteral(
        "<span style='color:#50a0f0'>▬</span> the low generator<br>"
        "<span style='color:#f09a30'>▬</span> the high generator<br>"
        "height is how much is mixed in, position is where it is tuned");
}

void StripBandCurve::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect().adjusted(4, 4, -5, -18);
    p.fillRect(rect(), c(Style::kAppBg));
    drawInset(p, r);

    QFont f = p.font();
    f.setPointSizeF(7.5);
    p.setFont(f);

    for (double hz : {100.0, 300.0, 1000.0, 3000.0, 10000.0}) {
        const int x = int(xForHz(hz, r));
        p.setPen(QPen(c(Style::kGroove), 1, Qt::DotLine));
        p.drawLine(x, r.top(), x, r.bottom());
        p.setPen(c(Style::kTextInactive));
        p.drawText(QRect(x - 20, r.bottom() + 2, 40, 12), Qt::AlignCenter,
                   hz >= 1000.0
                       ? QStringLiteral("%1k").arg(hz / 1000.0, 0, 'g', 2)
                       : QStringLiteral("%1").arg(hz, 0, 'f', 0));
    }

    if (!m_chain) {
        p.setPen(c(Style::kTextSecondary));
        p.drawText(r, Qt::AlignCenter, QStringLiteral("Not connected"));
        return;
    }

    p.setClipRect(r);

    if (m_stage == Stage::DeEsser) {
        ClientDeEss& d = m_chain->deEss();
        const double f0 = double(d.frequencyHz());
        const double q  = std::max(0.2, double(d.q()));

        // The sidechain's own response — the band the de-esser LISTENS
        // to, not the cut it applies. Which band it is listening to is
        // the setting people get wrong, and it is invisible otherwise.
        QPainterPath band;
        for (int x = r.left(); x <= r.right(); ++x) {
            const double lo = std::log10(100.0), hi = std::log10(12000.0);
            const double hz = std::pow(10.0, lo
                + (hi - lo) * double(x - r.left())
                  / std::max(1, r.width()));
            const double rel = hz / f0;
            const double mag = 1.0 / std::sqrt(1.0
                + std::pow(q * (rel - 1.0 / rel), 2.0));
            const QPointF pt(x, r.bottom() - mag * r.height() * 0.85);
            if (x == r.left()) { band.moveTo(pt); } else { band.lineTo(pt); }
        }
        QPainterPath fill = band;
        fill.lineTo(QPointF(r.right(), r.bottom()));
        fill.lineTo(QPointF(r.left(), r.bottom()));
        fill.closeSubpath();
        QLinearGradient g(0, r.top(), 0, r.bottom());
        g.setColorAt(0.0, QColor(0xf0, 0x9a, 0x30, 90));
        g.setColorAt(1.0, QColor(0xf0, 0x9a, 0x30, 10));
        p.fillPath(fill, g);
        p.setPen(QPen(QColor(0xf0, 0x9a, 0x30), 1.8));
        p.drawPath(band);

        const int fx = int(xForHz(f0, r));
        p.setPen(QPen(QColor(0xf0, 0x9a, 0x30, 200), 1, Qt::DashLine));
        p.drawLine(fx, r.top(), fx, r.bottom());
        p.setPen(c(Style::kTextPrimary));
        p.drawText(QRect(fx - 34, r.top() + 2, 68, 12), Qt::AlignCenter,
                   QStringLiteral("%1 Hz").arg(f0, 0, 'f', 0));
        p.setPen(c(Style::kTextScale));
        p.drawText(r.adjusted(4, 3, -4, 0), Qt::AlignLeft | Qt::AlignTop,
                   QStringLiteral("what it listens to"));
        p.drawText(r.adjusted(4, 0, -4, -2), Qt::AlignRight | Qt::AlignBottom,
                   QStringLiteral("bar right = working now"));

        // How hard it is working, now. Without this the picture says
        // where it is listening and never whether it heard anything.
        if (m_liveGr > 0.1) {
            const double h = std::clamp(m_liveGr / 12.0, 0.0, 1.0)
                             * r.height();
            p.fillRect(QRectF(r.right() - 8, r.bottom() - h, 6, h),
                       QColor(0xff, 0xb8, 0x00, 200));
        }
    } else {
        // The exciter's two tunings: where the low generator works and
        // where the high one does.
        ClientPudu& pu = m_chain->pudu();
        struct Mark { double hz; double mix; QColor col; QString name; };
        const Mark marks[2] = {
            {double(pu.pooTuneHz()), double(pu.pooMix()),
             QColor(0x50, 0xa0, 0xf0), QStringLiteral("low")},
            {double(pu.dooTuneHz()), double(pu.dooMix()),
             QColor(0xf0, 0x9a, 0x30), QStringLiteral("high")},
        };
        for (const Mark& m : marks) {
            if (m.hz <= 0.0) { continue; }
            const int x = int(xForHz(m.hz, r));
            const double h = std::clamp(m.mix, 0.0, 1.0) * r.height() * 0.8;
            QColor fillCol = m.col;
            fillCol.setAlpha(70);
            p.fillRect(QRectF(x - 10, r.bottom() - h, 20, h), fillCol);
            p.setPen(QPen(m.col, 1.5));
            p.drawLine(x, int(r.bottom() - h), x, r.bottom());
            p.setPen(c(Style::kTextInactive));
            p.drawText(QRect(x - 30, r.top() + 2, 60, 12), Qt::AlignCenter,
                       QStringLiteral("%1 %2 Hz").arg(m.name)
                           .arg(m.hz, 0, 'f', 0));
        }
    }
    p.setClipping(false);
}

} // namespace NereusSDR
