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

#include <QEvent>
#include <QMouseEvent>
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

// ── StripEqCurve ─────────────────────────────────────────────────────

namespace {

// A small radix-2 FFT, in place. The same choice as VoiceAnalyzer and
// for the same reason: this runs ten times a second on the GUI thread,
// where microseconds do not matter, and keeping it local means the
// picture has no dependency on the DSP build.
void fftInPlace(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) { j ^= bit; }
        j ^= bit;
        if (i < j) { std::swap(a[i], a[j]); }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * M_PI / double(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<double> u = a[i + k];
                const std::complex<double> v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

} // namespace

StripEqCurve::StripEqCurve(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(210);
    setMouseTracking(true);
}

void StripEqCurve::setChain(StripChain* chain) { m_chain = chain; update(); }
void StripEqCurve::setSpectrum(const MicSpectrum* spec) { m_spec = spec; }
void StripEqCurve::refresh() { update(); }

void StripEqCurve::setHeld(bool on)
{
    m_held = on;
    if (on && m_haveMag) { m_heldMag = m_mag; }
    update();
}

QSize StripEqCurve::sizeHint() const { return QSize(560, 230); }

void StripEqCurve::tick()
{
    if (!m_held) { recomputeSpectrum(); }
    update();
}

void StripEqCurve::recomputeSpectrum()
{
    if (!m_spec) { return; }

    static std::vector<float> buf;
    buf.resize(kFft);
    const int got = m_spec->snapshot(buf.data(), kFft);
    if (got < kFft) { return; }        // not enough audio yet

    std::vector<std::complex<double>> a(kFft);
    for (int i = 0; i < kFft; ++i) {
        // Hann. Without it every block boundary is a step, and the
        // leakage from those steps buries the differences this picture
        // exists to show.
        const double w = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (kFft - 1)));
        a[static_cast<size_t>(i)] = std::complex<double>(buf[i] * w, 0.0);
    }
    fftInPlace(a);

    const int bins = kFft / 2;
    if (static_cast<int>(m_mag.size()) != bins) {
        m_mag.assign(static_cast<size_t>(bins), -120.0);
    }
    // Exponential average. 0.25 is about a third of a second at 10 Hz —
    // slow enough to settle into the shape of a voice, fast enough that
    // moving the microphone is visible while it is still in your hand.
    constexpr double kAlpha = 0.25;
    for (int i = 0; i < bins; ++i) {
        const double m = std::abs(a[static_cast<size_t>(i)]) / (kFft / 4.0);
        const double db = m > 1e-9 ? 20.0 * std::log10(m) : -120.0;
        m_mag[static_cast<size_t>(i)] =
            (1.0 - kAlpha) * m_mag[static_cast<size_t>(i)] + kAlpha * db;
    }
    m_haveMag = true;
}

// ── Geometry ─────────────────────────────────────────────────────────

QRect StripEqCurve::plotRect() const
{
    return rect().adjusted(32, 10, -10, -20);
}

double StripEqCurve::xForHz(double hz, const QRect& r) const
{
    const double t = (std::log10(std::max(hz, 1.0)) - std::log10(kMinHz))
                   / (std::log10(kMaxHz) - std::log10(kMinHz));
    return r.left() + std::clamp(t, 0.0, 1.0) * r.width();
}

double StripEqCurve::hzForX(double x, const QRect& r) const
{
    const double t = std::clamp((x - r.left()) / std::max(1, r.width()),
                                0.0, 1.0);
    return std::pow(10.0, std::log10(kMinHz)
                          + t * (std::log10(kMaxHz) - std::log10(kMinHz)));
}

double StripEqCurve::yForDb(double db, const QRect& r) const
{
    const double t = (kRangeDb - db) / (2.0 * kRangeDb);
    return r.top() + std::clamp(t, 0.0, 1.0) * r.height();
}

double StripEqCurve::dbForY(double y, const QRect& r) const
{
    const double t = std::clamp((y - r.top()) / std::max(1, r.height()),
                                0.0, 1.0);
    return kRangeDb - t * 2.0 * kRangeDb;
}

QPointF StripEqCurve::handlePos(int band, const QRect& r) const
{
    if (!m_chain) { return QPointF(); }
    const ClientEq::BandParams p = m_chain->eq().band(band);
    // The high-pass has no gain, so its handle rides on the 0 dB line
    // and only moves sideways. Putting it at its own -3 dB point would
    // be more correct and much harder to grab.
    const double db = p.type == ClientEq::FilterType::HighPass
                          ? 0.0 : double(p.gainDb);
    return QPointF(xForHz(double(p.freqHz), r), yForDb(db, r));
}

int StripEqCurve::handleAt(const QPoint& pt) const
{
    if (!m_chain) { return -1; }
    const QRect r = plotRect();
    constexpr double kGrabPx = 14.0;
    int best = -1;
    double bestD = kGrabPx;
    for (int b : kHandleBands) {
        const QPointF h = handlePos(b, r);
        const double d = std::hypot(h.x() - pt.x(), h.y() - pt.y());
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

// ── Mouse ────────────────────────────────────────────────────────────

void StripEqCurve::mousePressEvent(QMouseEvent* ev)
{
    m_dragBand = handleAt(ev->pos());
    if (m_dragBand >= 0) { setCursor(Qt::ClosedHandCursor); }
}

void StripEqCurve::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_dragBand < 0) {
        const int h = handleAt(ev->pos());
        if (h != m_hoverBand) {
            m_hoverBand = h;
            setCursor(h >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
            update();
        }
        return;
    }
    if (!m_chain) { return; }

    const QRect r = plotRect();
    ClientEq::BandParams p = m_chain->eq().band(m_dragBand);

    p.freqHz = float(std::clamp(hzForX(ev->position().x(), r),
                                20.0, 12000.0));
    if (p.type != ClientEq::FilterType::HighPass) {
        // Cuts and boosts both, here. The measured suggestion only ever
        // cuts — for good reasons written down in VoiceAnalyzer — but
        // this is the operator's own hand on the curve, and refusing to
        // let them lift a band they can see is missing would be the
        // software second-guessing someone who is looking at the
        // evidence.
        p.gainDb = float(std::clamp(dbForY(ev->position().y(), r),
                                    -kRangeDb, kRangeDb));
    }
    p.enabled = true;
    m_chain->eq().setBand(m_dragBand, p);
    emit bandChanged(m_dragBand);
    update();
}

void StripEqCurve::mouseReleaseEvent(QMouseEvent*)
{
    if (m_dragBand >= 0) {
        m_dragBand = -1;
        setCursor(Qt::OpenHandCursor);
    }
}

void StripEqCurve::leaveEvent(QEvent*)
{
    m_hoverBand = -1;
    setCursor(Qt::ArrowCursor);
    update();
}

// ── Tips ─────────────────────────────────────────────────────────────

QStringList StripEqCurve::tips() const
{
    QStringList out;
    if (!m_held || m_heldMag.empty() || !m_chain) { return out; }

    // Six bands wide enough to name in words. Narrower than this and
    // the advice stops being actionable: "3.15 kHz is 4 dB high" is a
    // number, "there is too much presence" is something to do.
    struct Zone { const char* name; double lo, hi; const char* tooMuch;
                  const char* tooLittle; };
    static const Zone kZones[] = {
        {"rumble",   20.0,   90.0,
         "Rumble under 90 Hz. The transmitter will not send it, but it "
         "eats compressor headroom first — raise the high-pass.",
         ""},
        {"body",     90.0,  250.0,
         "Heavy in the body, 90-250 Hz. Usually proximity effect: back "
         "off the microphone a little, or pull this band down.",
         "Thin in the body, 90-250 Hz. A small lift here adds warmth "
         "without costing intelligibility."},
        {"mud",     250.0,  600.0,
         "Boxy around 250-600 Hz. This is the band that makes a voice "
         "sound like a telephone; a cut here usually helps more than a "
         "presence boost.",
         ""},
        {"clarity", 600.0, 1600.0,
         "Strong through 600-1600 Hz — this can sound hard on a long "
         "contact.",
         "Soft through 600-1600 Hz, which is where most of the words "
         "are."},
        {"presence",1600.0, 3200.0,
         "Bright at 1.6-3.2 kHz. Carries well and tires the listener; "
         "worth having for DX, worth less for a ragchew.",
         "Dull at 1.6-3.2 kHz. This is the band that decides whether "
         "you are understood — a couple of decibels here is worth ten "
         "anywhere else."},
        {"air",    3200.0, 8000.0,
         "Energy above 3.2 kHz, where the transmit filter is already "
         "closing. It costs ALC and the far end never hears it.", ""},
    };

    const double rate = m_spec ? m_spec->sampleRate() : 48000;
    const double binHz = rate / kFft;

    auto avgDb = [&](double lo, double hi) {
        double sum = 0.0; int n = 0;
        for (size_t i = 1; i < m_heldMag.size(); ++i) {
            const double f = i * binHz;
            if (f >= lo && f < hi) { sum += m_heldMag[i]; ++n; }
        }
        return n ? sum / n : -120.0;
    };

    // Everything relative to the clarity band, which is the closest
    // thing a voice has to a reference: it is where the fundamental
    // energy of speech lives whoever is speaking.
    const double ref = avgDb(600.0, 1600.0);
    if (ref < -100.0) { return out; }

    struct Finding { double deviation; QString text; };
    std::vector<Finding> found;
    for (const Zone& z : kZones) {
        const double d = avgDb(z.lo, z.hi) - ref;
        // The target shape, expressed as how far each zone should sit
        // from the clarity band for a voice that carries.
        double want = 0.0;
        if (z.lo < 90.0)        { want = -18.0; }
        else if (z.lo < 250.0)  { want = -4.0; }
        else if (z.lo < 600.0)  { want = -3.0; }
        else if (z.lo < 1600.0) { want = 0.0; }
        else if (z.lo < 3200.0) { want = 1.0; }
        else                    { want = -8.0; }

        const double err = d - want;
        if (std::abs(err) < 3.0) { continue; }   // not worth saying
        const char* txt = err > 0 ? z.tooMuch : z.tooLittle;
        if (!txt || !*txt) { continue; }
        found.push_back({std::abs(err),
                         QStringLiteral("%1 (%2 dB)")
                             .arg(QString::fromLatin1(txt))
                             .arg(err, 0, 'f', 1)});
    }

    std::sort(found.begin(), found.end(),
              [](const Finding& a, const Finding& b) {
        return a.deviation > b.deviation;
    });
    // Three at most. A list of six things to fix is a list nobody
    // starts.
    for (int i = 0; i < int(found.size()) && i < 3; ++i) {
        out << found[static_cast<size_t>(i)].text;
    }
    return out;
}

// ── Paint ────────────────────────────────────────────────────────────

void StripEqCurve::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = plotRect();
    p.fillRect(rect(), c(Style::kInsetBg));
    p.setPen(QPen(c(Style::kInsetBorder), 1));
    p.drawRect(r.adjusted(0, 0, -1, -1));

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
                   hz >= 1000.0
                       ? QStringLiteral("%1k").arg(hz / 1000.0, 0, 'g', 2)
                       : QStringLiteral("%1").arg(hz, 0, 'f', 0));
    }
    for (double db : {-12.0, -6.0, 0.0, 6.0, 12.0}) {
        const int y = int(yForDb(db, r));
        p.setPen(QPen(db == 0.0 ? c(Style::kBorder) : c(Style::kGroove), 1,
                      db == 0.0 ? Qt::SolidLine : Qt::DotLine));
        p.drawLine(r.left(), y, r.right(), y);
        p.setPen(c(Style::kTextInactive));
        p.drawText(QRect(0, y - 7, 28, 14), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("%1").arg(db, 0, 'f', 0));
    }

    {
        const int x1 = int(xForHz(300.0, r));
        const int x2 = int(xForHz(3000.0, r));
        p.fillRect(QRect(x1, r.top(), x2 - x1, r.height()),
                   QColor(0x00, 0xb4, 0xd8, 16));
        p.setPen(c(Style::kTextInactive));
        p.drawText(QRect(x1, r.top() + 2, x2 - x1, 12), Qt::AlignCenter,
                   QStringLiteral("speech"));
    }

    // ── The voice ────────────────────────────────────────────────────
    //
    // Drawn first and dimly: it is what the curve is aimed AT, not the
    // thing being set. Scaled so the clarity band sits near the 0 dB
    // line, because the absolute level is a microphone-gain question
    // and this picture is about shape.
    const std::vector<double>& mag = m_held ? m_heldMag : m_mag;
    if (!mag.empty() && m_spec) {
        const double binHz = m_spec->sampleRate() / double(kFft);
        double ref = -120.0;
        {
            double sum = 0.0; int n = 0;
            for (size_t i = 1; i < mag.size(); ++i) {
                const double hz = i * binHz;
                if (hz >= 600.0 && hz < 1600.0) { sum += mag[i]; ++n; }
            }
            if (n) { ref = sum / n; }
        }
        if (ref > -100.0) {
            QPainterPath sp;
            bool started = false;
            for (int x = r.left(); x <= r.right(); ++x) {
                const double hz = hzForX(x, r);
                const auto bin = static_cast<size_t>(hz / binHz);
                if (bin < 1 || bin >= mag.size()) { continue; }
                const double db = mag[bin] - ref;
                const QPointF pt(x, yForDb(db, r));
                if (!started) { sp.moveTo(pt); started = true; }
                else          { sp.lineTo(pt); }
            }
            p.setClipRect(r);
            p.setPen(QPen(m_held ? QColor(0xd0, 0x90, 0x20, 200)
                                 : QColor(0x60, 0x80, 0xa0, 130), 1));
            p.drawPath(sp);
            p.setClipping(false);
        }
    }

    if (!m_chain) {
        p.setPen(c(Style::kTextSecondary));
        p.drawText(r, Qt::AlignCenter, QStringLiteral("Not connected"));
        return;
    }

    // ── The equaliser ────────────────────────────────────────────────
    ClientEq& eq = m_chain->eq();
    const int bands = eq.activeBandCount();
    const bool eqOn = m_chain->stageEnabled(StripChain::Stage::Eq)
                      && m_chain->isEnabled();

    QPainterPath path;
    const int steps = std::max(2, r.width());
    for (int i = 0; i <= steps; ++i) {
        const double hz = hzForX(r.left() + double(i) * r.width() / steps, r);
        double db = 0.0;
        for (int b = 0; b < bands; ++b) {
            db += double(ClientEq::bandMagnitudeDb(
                eq.band(b), float(hz), eq.sampleRate(), eq.filterFamily()));
        }
        const QPointF pt(xForHz(hz, r), yForDb(db, r));
        if (i == 0) { path.moveTo(pt); } else { path.lineTo(pt); }
    }
    p.setClipRect(r);
    p.setPen(QPen(eqOn ? c(Style::kAccent) : c(Style::kTextInactive),
                  eqOn ? 2.0 : 1.0));
    p.drawPath(path);
    p.setClipping(false);

    // ── Handles ──────────────────────────────────────────────────────
    for (int b : kHandleBands) {
        if (b >= bands) { continue; }
        const QPointF h = handlePos(b, r);
        const bool active = (b == m_dragBand) || (b == m_hoverBand);
        p.setPen(QPen(c(Style::kAccent), active ? 2.0 : 1.0));
        p.setBrush(active ? c(Style::kAccent) : c(Style::kInsetBg));
        p.drawEllipse(h, active ? 6.0 : 4.5, active ? 6.0 : 4.5);

        if (active) {
            const ClientEq::BandParams bp = eq.band(b);
            const QString label = bp.type == ClientEq::FilterType::HighPass
                ? QStringLiteral("%1 Hz").arg(bp.freqHz, 0, 'f', 0)
                : QStringLiteral("%1 Hz  %2 dB")
                      .arg(bp.freqHz, 0, 'f', 0).arg(bp.gainDb, 0, 'f', 1);
            p.setPen(c(Style::kTextPrimary));
            p.drawText(QRectF(h.x() - 60, h.y() - 24, 120, 14),
                       Qt::AlignCenter, label);
        }
    }

    if (m_held) {
        p.setPen(QColor(0xd0, 0x90, 0x20));
        p.drawText(r.adjusted(6, 2, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                   QStringLiteral("HOLD"));
    }
    if (!eqOn) {
        p.setPen(c(Style::kTextSecondary));
        p.drawText(r.adjusted(0, 0, -6, -4), Qt::AlignRight | Qt::AlignBottom,
                   QStringLiteral("not in circuit"));
    }
}

} // namespace NereusSDR
