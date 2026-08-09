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
#include "core/strip/StripTargets.h"
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
    // Hold no longer captures — the average is always being captured.
    // It just stops the capture, which is what "hold" should have meant
    // all along.
    m_held = on;
    if (on) { captureHold(); }
    update();
}

void StripEqCurve::setSmoothing(bool on)
{
    m_smooth = on;
    applySmoothing();
    update();
}

void StripEqCurve::setShowTarget(bool on) { m_showTarget = on; update(); }
void StripEqCurve::setShowResult(bool on) { m_showResult = on; update(); }
void StripEqCurve::setShowBands(bool on) { m_showBands = on; update(); }

// ── The take ─────────────────────────────────────────────────────────

void StripEqCurve::startTake()
{
    m_recording  = true;
    m_haveTake   = false;
    m_takeLenSec = 0.0;
    m_takeMag.clear();
    m_takeClock.restart();
    emit takeStateChanged();
    update();
}

void StripEqCurve::stopTake()
{
    if (!m_recording) { return; }
    m_recording  = false;
    m_takeLenSec = m_takeClock.elapsed() / 1000.0;
    captureTake();
    emit takeStateChanged();
    update();
}

double StripEqCurve::takeSeconds() const
{
    if (m_recording) { return m_takeClock.elapsed() / 1000.0; }
    return m_takeLenSec;
}

void StripEqCurve::captureTake()
{
    // Exactly the audio between start and stop, taken from the ring at
    // the end rather than accumulated as it arrived. The ring holds
    // sixteen seconds and a take is capped at fifteen, so the whole take
    // is certainly still in there — but only just, which is why the cap
    // is not fifteen point five.
    if (!m_spec) { m_takeLenSec = 0.0; return; }

    const int rate = m_spec->sampleRate();
    const int want = int(std::min(m_takeLenSec,
                                  double(MicSpectrum::kHoldSeconds)) * rate);
    if (want < kFft * 2) {
        // Under about two-tenths of a second there is nothing to
        // average. Refused rather than shown as a jagged line that
        // looks like a measurement.
        m_takeLenSec = 0.0;
        return;
    }

    std::vector<float> buf(static_cast<size_t>(want), 0.0f);
    const int got = m_spec->snapshot(buf.data(), want);
    if (got < kFft * 2) { m_takeLenSec = 0.0; return; }

    const int bins = kFft / 2;
    std::vector<double> power(static_cast<size_t>(bins), 0.0);
    int windows = 0;
    std::vector<std::complex<double>> a(kFft);
    for (int start = 0; start + kFft <= got; start += kFft / 2) {
        for (int i = 0; i < kFft; ++i) {
            const double w =
                0.5 * (1.0 - std::cos(2.0 * M_PI * i / (kFft - 1)));
            a[static_cast<size_t>(i)] = std::complex<double>(
                buf[static_cast<size_t>(start + i)] * w, 0.0);
        }
        fftInPlace(a);
        for (int i = 0; i < bins; ++i) {
            const double m = std::abs(a[static_cast<size_t>(i)]) / (kFft / 4.0);
            power[static_cast<size_t>(i)] += m * m;
        }
        ++windows;
    }
    if (windows == 0) { m_takeLenSec = 0.0; return; }

    // Power domain, as everywhere else in this file: averaging decibels
    // would give the pauses the same weight as the words.
    m_heldMag.assign(static_cast<size_t>(bins), -120.0);
    for (int i = 0; i < bins; ++i) {
        const double mean = power[static_cast<size_t>(i)] / windows;
        m_heldMag[static_cast<size_t>(i)] =
            mean > 1e-18 ? 10.0 * std::log10(mean) : -120.0;
    }
    applySmoothing();

    // The take becomes the held curve and the reference for everything
    // else on the plot, so there is one measured state rather than two
    // that can disagree.
    m_takeMag  = m_heldShown.empty() ? m_heldMag : m_heldShown;
    m_haveTake = true;
    m_haveHold = true;
    m_held     = true;
}

void StripEqCurve::setProfile(const QString& name)
{
    m_profile = name;
    update();
}

void StripEqCurve::captureHold()
{
    m_heldMag.clear();
    m_heldShown.clear();
    if (!m_spec) { return; }

    const int rate = m_spec->sampleRate();
    const int want = rate * MicSpectrum::kHoldSeconds;
    std::vector<float> buf(static_cast<size_t>(want), 0.0f);
    const int got = m_spec->snapshot(buf.data(), want);
    if (got < kFft * 2) { return; }    // not enough to average anything

    // Overlapping windows, half a window apart. Averaged in the POWER
    // domain, not in decibels: averaging logarithms gives a quiet
    // window the same weight as a loud one, so the pauses between words
    // pull the whole curve down and flatten exactly the peaks that
    // matter.
    const int bins = kFft / 2;
    std::vector<double> power(static_cast<size_t>(bins), 0.0);
    int windows = 0;

    std::vector<std::complex<double>> a(kFft);
    for (int start = 0; start + kFft <= got; start += kFft / 2) {
        for (int i = 0; i < kFft; ++i) {
            const double w =
                0.5 * (1.0 - std::cos(2.0 * M_PI * i / (kFft - 1)));
            a[static_cast<size_t>(i)] =
                std::complex<double>(buf[static_cast<size_t>(start + i)] * w,
                                     0.0);
        }
        fftInPlace(a);
        for (int i = 0; i < bins; ++i) {
            const double m = std::abs(a[static_cast<size_t>(i)]) / (kFft / 4.0);
            power[static_cast<size_t>(i)] += m * m;
        }
        ++windows;
    }
    if (windows == 0) { return; }

    m_haveHold = true;
    m_heldMag.assign(static_cast<size_t>(bins), -120.0);
    for (int i = 0; i < bins; ++i) {
        const double mean = power[static_cast<size_t>(i)] / windows;
        m_heldMag[static_cast<size_t>(i)] =
            mean > 1e-18 ? 10.0 * std::log10(mean) : -120.0;
    }
    applySmoothing();
}

void StripEqCurve::applySmoothing()
{
    m_heldShown = m_heldMag;
    if (!m_smooth || m_heldMag.empty() || !m_spec) { return; }

    // A third of an octave, which is roughly how finely the ear
    // separates tone. Narrower than that and the picture shows the
    // harmonics of the voice — real, but not something an equaliser
    // should be aimed at, because they move with every note the
    // operator speaks on.
    const double binHz = m_spec->sampleRate() / double(kFft);
    const double factor = std::pow(2.0, 1.0 / 6.0);   // ± 1/6 octave
    const int n = int(m_heldMag.size());

    for (int i = 1; i < n; ++i) {
        const double hz = i * binHz;
        const int lo = std::max(1, int(hz / factor / binHz));
        const int hi = std::min(n - 1, int(hz * factor / binHz));
        double sum = 0.0; int cnt = 0;
        for (int k = lo; k <= hi; ++k) {
            sum += m_heldMag[static_cast<size_t>(k)];
            ++cnt;
        }
        m_heldShown[static_cast<size_t>(i)] =
            cnt ? sum / cnt : m_heldMag[static_cast<size_t>(i)];
    }
}

QSize StripEqCurve::sizeHint() const { return QSize(560, 230); }

void StripEqCurve::tick()
{
    // Stop itself at the limit. Fifteen seconds is what the ring can
    // certainly still hold, and it is long enough that the average is
    // of a person talking rather than of two vowels.
    if (m_recording
        && m_takeClock.elapsed() >= MicSpectrum::kHoldSeconds * 1000) {
        stopTake();
    }

    // A finished take is the measured state and nothing may overwrite
    // it. Without this the rolling average — which runs once a second —
    // would replace the take about a second after it was made, and the
    // operator would watch their careful fifteen-second measurement
    // quietly turn back into a rolling average with no indication that
    // anything had happened.
    if (m_haveTake) { update(); return; }

    if (m_held) { update(); return; }

    recomputeSpectrum();

    // With no take yet, the fifteen-second average still runs on its
    // own so that opening the window shows something rather than an
    // empty plot. It is a preview, not a measurement, and the plot says
    // which of the two it is showing.
    if (++m_sinceCapture >= 10) {
        m_sinceCapture = 0;
        captureHold();
    }
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

std::vector<int> StripEqCurve::handleBands() const
{
    std::vector<int> v;
    if (!m_chain) { return v; }
    const int n = m_chain->eq().activeBandCount();
    if (n > 0) { v.push_back(0); }                 // the high-pass
    std::vector<int> tone;
    for (int b = kFirstToneBand; b < n; ++b) { tone.push_back(b); }
    // Left to right, so the numbers over the handles count along the
    // axis. Slot order and frequency order are the same only until the
    // first drag, and a knot numbered 7 sitting between 3 and 4 is a
    // label that makes the picture harder to read than no label.
    std::sort(tone.begin(), tone.end(), [this](int a, int b) {
        return m_chain->eq().band(a).freqHz < m_chain->eq().band(b).freqHz;
    });
    v.insert(v.end(), tone.begin(), tone.end());
    return v;
}

int StripEqCurve::handleAt(const QPoint& pt) const
{
    if (!m_chain) { return -1; }
    const QRect r = plotRect();
    constexpr double kGrabPx = 14.0;
    int best = -1;
    double bestD = kGrabPx;
    for (int b : handleBands()) {
        const QPointF h = handlePos(b, r);
        const double d = std::hypot(h.x() - pt.x(), h.y() - pt.y());
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

// ── Mouse ────────────────────────────────────────────────────────────

bool StripEqCurve::editingTarget() const
{
    return m_showTarget && m_haveHold
           && m_profile == QLatin1String(StripTargets::kUserProfileName);
}

QPointF StripEqCurve::targetPointPos(int idx, const QRect& r,
                                     double ref) const
{
    const double* f = StripTargets::userPointFreqs();
    const QVector<double> v = StripTargets::userTarget();
    const double hz = f[idx];
    // The rose line is drawn as target-minus-measured, so its handle
    // must sit on that same curve — not on the target in isolation, or
    // the dot would be nowhere near the line it belongs to.
    const double measured = ref;
    Q_UNUSED(measured);
    const double db = (idx < v.size() ? v.at(idx) : 0.0);
    return QPointF(xForHz(hz, r), yForDb(db - m_targetRef, r));
}

int StripEqCurve::targetPointAt(const QPoint& pt) const
{
    if (!editingTarget()) { return -1; }
    const QRect r = plotRect();
    constexpr double kGrab = 12.0;
    int best = -1;
    double bestD = kGrab;
    for (int i = 0; i < StripTargets::kUserPointCount; ++i) {
        const QPointF h = targetPointPos(i, r, 0.0);
        const double d = std::hypot(h.x() - pt.x(), h.y() - pt.y());
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void StripEqCurve::mousePressEvent(QMouseEvent* ev)
{
    // The equaliser's own handles win a tie: they are what the operator
    // is usually reaching for, and the target's are only live at all
    // when the profile is theirs.
    m_dragBand = handleAt(ev->pos());
    if (m_dragBand >= 0) { setCursor(Qt::ClosedHandCursor); return; }
    m_dragTarget = targetPointAt(ev->pos());
    if (m_dragTarget >= 0) { setCursor(Qt::ClosedHandCursor); }
}

void StripEqCurve::mouseMoveEvent(QMouseEvent* ev)
{
    m_cursor     = ev->pos();
    m_haveCursor = true;

    if (m_dragTarget >= 0) {
        const QRect r = plotRect();
        QVector<double> v = StripTargets::userTarget();
        while (v.size() < StripTargets::kUserPointCount) { v.append(0.0); }
        // Only up and down. The frequencies are fixed so that the saved
        // file and the handles agree without storing a frequency beside
        // every gain, and so that two points cannot be dragged past
        // each other into a curve that doubles back on itself.
        v[m_dragTarget] = std::clamp(
            dbForY(ev->position().y(), r) + m_targetRef, -30.0, 12.0);
        StripTargets::setUserTarget(v);
        emit bandChanged(-1);     // the window persists and redraws
        update();
        return;
    }
    if (m_dragBand < 0) {
        const int h = handleAt(ev->pos());
        if (h != m_hoverBand) {
            m_hoverBand = h;
            setCursor(h >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
        }
        // Repaint on every move, not only when the hovered band
        // changes: the readout follows the pointer and a readout that
        // only updates when you cross a handle is worse than none.
        update();
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

void StripEqCurve::wheelEvent(QWheelEvent* ev)
{
    // The wheel over a handle changes its width, which is the third
    // thing an equaliser has after frequency and gain and the one no
    // slider was ever going to make intuitive. Narrow for a notch,
    // wide for a tilt; the shape follows under the pointer.
    const int band = m_hoverBand >= 0 ? m_hoverBand : handleAt(ev->position().toPoint());
    if (band < 0 || !m_chain) { ev->ignore(); return; }

    ClientEq::BandParams p = m_chain->eq().band(band);
    if (p.type == ClientEq::FilterType::HighPass) {
        // A high-pass has slope rather than Q, and the four the filter
        // supports are a list rather than a continuum.
        static const int kSlopes[] = {12, 24, 36, 48};
        int idx = 1;
        for (int i = 0; i < 4; ++i) { if (kSlopes[i] == p.slopeDbPerOct) { idx = i; } }
        idx = std::clamp(idx + (ev->angleDelta().y() > 0 ? 1 : -1), 0, 3);
        p.slopeDbPerOct = kSlopes[idx];
    } else {
        const double step = ev->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        p.q = float(std::clamp(double(p.q) * step, 0.3, 8.0));
    }
    p.enabled = true;
    m_chain->eq().setBand(band, p);
    emit bandChanged(band);
    update();
    ev->accept();
}

void StripEqCurve::mouseDoubleClickEvent(QMouseEvent* ev)
{
    if (!m_chain) { return; }
    const int band = handleAt(ev->pos());

    if (band < 0) {
        // Empty space: add a band here. The count is the third thing an
        // equaliser has, and an operator who can see a problem the
        // existing bands cannot reach should be able to put one where
        // it is rather than moving two others to approximate it.
        const int n = m_chain->eq().activeBandCount();
        if (n >= ClientEq::kMaxBands) { return; }
        const QRect r = plotRect();
        ClientEq::BandParams p;
        p.type    = ClientEq::FilterType::Peak;
        p.freqHz  = float(std::clamp(hzForX(ev->position().x(), r),
                                     20.0, 12000.0));
        p.gainDb  = float(std::clamp(dbForY(ev->position().y(), r),
                                     -kRangeDb, kRangeDb));
        p.q       = 1.0f;
        p.enabled = true;
        m_chain->eq().setBand(n, p);
        m_chain->eq().setActiveBandCount(n + 1);
        emit bandChanged(n);
        update();
        return;
    }

    // On a handle: cycle its shape. A peak fixes one spot, a shelf
    // tilts everything past it, a notch removes a tone — three
    // different jobs that no amount of dragging a peak will do.
    ClientEq::BandParams p = m_chain->eq().band(band);
    switch (p.type) {
    case ClientEq::FilterType::Peak:
        p.type = ClientEq::FilterType::LowShelf;  break;
    case ClientEq::FilterType::LowShelf:
        p.type = ClientEq::FilterType::HighShelf; break;
    case ClientEq::FilterType::HighShelf:
        // Round trip back to a peak, but narrow — the notch a peak
        // becomes when you pull it down hard, offered directly.
        p.type = ClientEq::FilterType::Peak;
        p.q    = 6.0f;
        break;
    case ClientEq::FilterType::HighPass:
    case ClientEq::FilterType::LowPass:
        // The high-pass keeps its job. Turning the one control that
        // removes rumble into a shelf by mis-clicking would be a poor
        // trade for the convenience.
        return;
    }
    p.enabled = true;
    m_chain->eq().setBand(band, p);
    emit bandChanged(band);
    update();
}

void StripEqCurve::contextMenuEvent(QContextMenuEvent* ev)
{
    if (!m_chain) { return; }
    const int band = handleAt(ev->pos());
    // Only the bands added on top of the fixed layout can go. The
    // high-pass, the three mains notches and the six shaping bands are
    // referred to by index from the panel, the tuner and the settings
    // file; removing one would renumber the rest and quietly move
    // somebody else's setting.
    if (band < kFirstToneBand + 6) { ev->ignore(); return; }

    const int n = m_chain->eq().activeBandCount();
    if (band != n - 1) {
        // Only the last one, for the same reason.
        ev->ignore();
        return;
    }
    m_chain->eq().setActiveBandCount(n - 1);
    emit bandChanged(band);
    update();
    ev->accept();
}

void StripEqCurve::mouseReleaseEvent(QMouseEvent*)
{
    if (m_dragTarget >= 0) {
        m_dragTarget = -1;
        setCursor(Qt::OpenHandCursor);
    }
    if (m_dragBand >= 0) {
        m_dragBand = -1;
        setCursor(Qt::OpenHandCursor);
    }
}

void StripEqCurve::leaveEvent(QEvent*)
{
    m_hoverBand  = -1;
    m_haveCursor = false;
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

QVector<double> StripEqCurve::measuredAtTargetPoints() const
{
    QVector<double> out;
    const std::vector<double>& mag =
        m_heldShown.empty() ? m_heldMag : m_heldShown;
    if (!m_haveHold || mag.empty() || !m_spec) { return out; }

    const double binHz = m_spec->sampleRate() / double(kFft);
    const auto b1k = static_cast<size_t>(1000.0 / binHz);
    if (b1k >= mag.size()) { return out; }
    const double ref = mag[b1k];

    const double* f = StripTargets::userPointFreqs();
    for (int i = 0; i < StripTargets::kUserPointCount; ++i) {
        const auto bin = static_cast<size_t>(f[i] / binHz);
        out.append(bin < mag.size() ? mag[bin] - ref : 0.0);
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
    // The fifteen-second average when there is one, live otherwise.
    // The average is what an equaliser can be aimed at; the live trace
    // is only there for the first few seconds after the window opens,
    // so that something moves and the operator knows the microphone is
    // arriving.
    const std::vector<double>& mag =
        m_haveHold ? (m_heldShown.empty() ? m_heldMag : m_heldShown) : m_mag;
    // Kept outside the block so the result curve, drawn after the
    // equaliser further down, can sit on the same measurement rather
    // than recomputing a reference that would differ by a fraction of a
    // decibel and make the two curves disagree about where 0 dB is.
    double voiceRef  = -1000.0;
    bool   haveVoice = false;
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
        // Between words the reference collapses toward the noise floor
        // and the shape would leap or vanish. Keep the last one that
        // came from actual speech, and let it fall only slowly, so the
        // picture stays still while the level moves — which is what
        // makes it possible to look at the shape at all.
        if (ref > -70.0) { m_lastRef = ref; }
        else if (m_lastRef > -1000.0) { m_lastRef -= 0.3; ref = m_lastRef; }
        if (ref > -100.0) {
            voiceRef  = ref;
            haveVoice = true;
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
            p.setPen(QPen(m_held ? QColor(0xd0, 0x90, 0x20, 220)
                                 : QColor(0x60, 0x80, 0xa0, 130),
                          m_held ? 2.0 : 1.0));
            p.drawPath(sp);

            // ── Where the blue line should go ────────────────────
            //
            // Not the target for the voice — the CORRECTION. Rose is
            // target minus measured, which is exactly the equaliser
            // curve that would put this voice on this profile. So the
            // instruction is one sentence with no arithmetic in it:
            // lay the blue line on the rose line.
            //
            // Drawing the voice target instead, as this did at first,
            // asks the operator to do the subtraction by eye across a
            // log axis — and the answer they arrive at is wrong in the
            // direction that flatters whatever they already have.
            //
            // Dashed, and its own colour, because it is not a
            // measurement: it is an opinion about what the chosen
            // profile wants, and must never be mistaken for the voice.
            if (m_showTarget && m_haveHold) {
                QPainterPath tp;
                bool begun = false;
                for (int x = r.left(); x <= r.right(); ++x) {
                    const double hz = hzForX(x, r);
                    const auto bin = static_cast<size_t>(hz / binHz);
                    if (bin < 1 || bin >= mag.size()) { continue; }
                    const double measured = mag[bin] - ref;
                    const double want = StripTargets::targetDb(m_profile, hz);
                    const QPointF pt(x, yForDb(want - measured, r));
                    if (!begun) { tp.moveTo(pt); begun = true; }
                    else        { tp.lineTo(pt); }
                }
                QPen rose(QColor(0xe8, 0x78, 0xb0, 210), 1.8);
                rose.setStyle(Qt::DashLine);
                p.setPen(rose);
                p.drawPath(tp);

                // Handles on the target itself, when it is the
                // operator's own. Five built-in curves are five
                // opinions; the sixth option is that the operator draws
                // it, and then the rose line stops being something to
                // argue with and becomes something to aim at.
                if (editingTarget()) {
                    // The rose is drawn relative to the measured curve,
                    // so a handle at the point's own dB has to be
                    // offset by the same amount. Stored from here
                    // because the hit test runs outside paint.
                    const auto b1k = static_cast<size_t>(1000.0 / binHz);
                    const_cast<StripEqCurve*>(this)->m_targetRef =
                        (b1k < mag.size()) ? (mag[b1k] - ref) : 0.0;

                    p.setPen(QPen(QColor(0xe8, 0x78, 0xb0), 1.5));
                    p.setBrush(QColor(0x30, 0x18, 0x24));
                    for (int i = 0; i < StripTargets::kUserPointCount; ++i) {
                        p.drawEllipse(targetPointPos(i, r, ref), 3.5, 3.5);
                    }
                }
            }
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

    // ── Each band on its own, behind the sum ─────────────────────────
    //
    // Ten overlapping bands make a composite curve that cannot be read
    // backwards: a dip at 700 Hz is either one band cutting or two
    // neighbours boosting around it, and those want opposite
    // corrections. Faint, so they inform without competing with the
    // line that says what actually happens; the one under the pointer
    // brightens, which is how you find out which handle owns a feature.
    p.setClipRect(r);
    if (m_showBands && eqOn) {
        for (int b : handleBands()) {
            if (b >= bands) { continue; }
            const ClientEq::BandParams bp = eq.band(b);
            if (!bp.enabled) { continue; }
            // A band sitting at unity draws a straight line along zero
            // and adds nothing but clutter.
            if (bp.type != ClientEq::FilterType::HighPass
                && std::abs(double(bp.gainDb)) < 0.05) { continue; }

            QPainterPath bpath;
            for (int x = r.left(); x <= r.right(); ++x) {
                const double hz = hzForX(x, r);
                const double db = double(ClientEq::bandMagnitudeDb(
                    bp, float(hz), eq.sampleRate(), eq.filterFamily()));
                const QPointF pt(x, yForDb(db, r));
                if (x == r.left()) { bpath.moveTo(pt); }
                else               { bpath.lineTo(pt); }
            }
            const bool lit = (b == m_dragBand) || (b == m_hoverBand);
            QColor col = c(Style::kAccent);
            col.setAlpha(lit ? 190 : 70);
            p.setPen(QPen(col, lit ? 1.6 : 1.0));
            p.drawPath(bpath);
        }
    }

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

    // Filled back to the zero line, so boost and cut read as areas
    // rather than as a wiggle to be traced. Faint, and only when the
    // equaliser is actually in circuit — a filled shape is a claim that
    // something is happening.
    if (eqOn) {
        const double zeroY = yForDb(0.0, r);
        QPainterPath fill = path;
        fill.lineTo(QPointF(r.right(), zeroY));
        fill.lineTo(QPointF(r.left(), zeroY));
        fill.closeSubpath();
        QLinearGradient g(0, r.top(), 0, r.bottom());
        QColor top = c(Style::kAccent);   top.setAlpha(70);
        QColor mid = c(Style::kAccent);   mid.setAlpha(10);
        g.setColorAt(0.0, top);
        g.setColorAt(0.5, mid);
        g.setColorAt(1.0, top);
        p.fillPath(fill, g);
    }

    p.setPen(QPen(eqOn ? c(Style::kAccent) : c(Style::kTextInactive),
                  eqOn ? 2.0 : 1.0));
    p.drawPath(path);
    p.setClipping(false);

    // ── Where you will actually end up ───────────────────────────────
    //
    // Measurement plus equaliser, in one line. Drawn last of the three
    // so it sits on top, and only when the equaliser is in circuit:
    // with the strip bypassed the result IS the measurement, and two
    // curves lying exactly on top of each other is a picture that
    // invites the operator to hunt for a difference that is not there.
    //
    // Green, and solid, because unlike the rose line this is not an
    // opinion — given the measurement and the filter it is arithmetic,
    // and the same arithmetic the filter itself will do.
    if (m_showResult && haveVoice && eqOn && !mag.empty() && m_spec) {
        const double binHz = m_spec->sampleRate() / double(kFft);
        QPainterPath rp;
        bool begun = false;
        for (int x = r.left(); x <= r.right(); ++x) {
            const double hz  = hzForX(x, r);
            const auto   bin = static_cast<size_t>(hz / binHz);
            if (bin < 1 || bin >= mag.size()) { continue; }
            double db = mag[bin] - voiceRef;
            for (int b = 0; b < bands; ++b) {
                db += double(ClientEq::bandMagnitudeDb(
                    eq.band(b), float(hz), eq.sampleRate(),
                    eq.filterFamily()));
            }
            const QPointF pt(x, yForDb(db, r));
            if (!begun) { rp.moveTo(pt); begun = true; }
            else        { rp.lineTo(pt); }
        }
        p.setClipRect(r);
        p.setPen(QPen(QColor(0x50, 0xd0, 0x80, 230), 2.0));
        p.drawPath(rp);
        p.setClipping(false);
    }

    // ── Handles ──────────────────────────────────────────────────────
    //
    // Numbered, the way a channel strip's knots always are, so the
    // picture and the numbers underneath refer to each other without
    // the operator having to count along the axis.
    int knot = 0;
    for (int b : handleBands()) {
        if (b >= bands) { continue; }
        ++knot;
        const QPointF h = handlePos(b, r);
        {
            QFont nf = p.font();
            nf.setPointSizeF(7.5);
            p.setFont(nf);
            p.setPen(c(Style::kTextInactive));
            p.drawText(QRectF(h.x() - 10, r.top() + 1, 20, 11),
                       Qt::AlignCenter, QString::number(knot));
        }
        const bool active = (b == m_dragBand) || (b == m_hoverBand);
        p.setPen(QPen(c(Style::kAccent), active ? 2.0 : 1.0));
        p.setBrush(active ? c(Style::kAccent) : c(Style::kInsetBg));
        p.drawEllipse(h, active ? 6.0 : 4.5, active ? 6.0 : 4.5);

        if (active) {
            const ClientEq::BandParams bp = eq.band(b);

            // ── How wide is this band? ───────────────────────────
            //
            // Q is a number nobody has an intuition for. Drawn as the
            // span between the half-gain points it becomes the thing
            // it actually is: how much of the voice this handle
            // touches. Only on the active band — ten brackets at once
            // would be a picture of brackets.
            if (bp.type == ClientEq::FilterType::Peak
                && std::abs(double(bp.gainDb)) > 0.05) {
                // Bandwidth in octaves from Q, the standard relation.
                const double q  = std::max(0.1, double(bp.q));
                const double bw = (2.0 / std::log(2.0))
                    * std::asinh(1.0 / (2.0 * q));
                const double lo = double(bp.freqHz) * std::pow(2.0, -bw / 2.0);
                const double hi = double(bp.freqHz) * std::pow(2.0,  bw / 2.0);
                const double y  = yForDb(double(bp.gainDb) / 2.0, r);
                const double x1 = xForHz(std::max(kMinHz, lo), r);
                const double x2 = xForHz(std::min(kMaxHz, hi), r);
                QColor bracket = c(Style::kAccent);
                bracket.setAlpha(150);
                p.setPen(QPen(bracket, 1.0));
                p.drawLine(QPointF(x1, y), QPointF(x2, y));
                p.drawLine(QPointF(x1, y - 3), QPointF(x1, y + 3));
                p.drawLine(QPointF(x2, y - 3), QPointF(x2, y + 3));
            }
            auto shapeName = [](ClientEq::FilterType t) {
                switch (t) {
                case ClientEq::FilterType::Peak:      return "peak";
                case ClientEq::FilterType::LowShelf:  return "low shelf";
                case ClientEq::FilterType::HighShelf: return "high shelf";
                case ClientEq::FilterType::HighPass:  return "high-pass";
                case ClientEq::FilterType::LowPass:   return "low-pass";
                }
                return "";
            };
            const QString label = bp.type == ClientEq::FilterType::HighPass
                ? QStringLiteral("high-pass  %1 Hz  %2 dB/oct")
                      .arg(bp.freqHz, 0, 'f', 0).arg(bp.slopeDbPerOct)
                : QStringLiteral("%1  %2 Hz  %3 dB  Q %4")
                      .arg(QString::fromLatin1(shapeName(bp.type)))
                      .arg(bp.freqHz, 0, 'f', 0).arg(bp.gainDb, 0, 'f', 1)
                      .arg(bp.q, 0, 'f', 2);
            p.setPen(c(Style::kTextPrimary));
            p.drawText(QRectF(h.x() - 60, h.y() - 24, 120, 14),
                       Qt::AlignCenter, label);
        }
    }

    // ── What is this curve? ──────────────────────────────────────────
    //
    // A measured take and a rolling preview look identical, and acting
    // on the second while believing it is the first is the whole reason
    // the take exists. So the plot says which one it is drawing, every
    // frame, in the corner.
    {
        QString badge;
        QColor  badgeCol = QColor(0xd0, 0x90, 0x20);
        if (m_recording) {
            badge = QStringLiteral("● RECORDING  %1 s")
                        .arg(takeSeconds(), 0, 'f', 1);
            badgeCol = QColor(0xe0, 0x50, 0x50);
        } else if (m_haveTake) {
            badge = QStringLiteral("MEASURED  %1 s")
                        .arg(m_takeLenSec, 0, 'f', 1);
        } else if (m_held) {
            badge = QStringLiteral("HOLD");
        } else if (m_haveHold) {
            badge = QStringLiteral("preview — press Record to measure");
            badgeCol = c(Style::kTextInactive);
        }
        if (!badge.isEmpty()) {
            p.setPen(badgeCol);
            p.drawText(r.adjusted(6, 2, 0, 0), Qt::AlignLeft | Qt::AlignTop,
                       badge);
        }
    }
    if (!eqOn) {
        p.setPen(c(Style::kTextSecondary));
        p.drawText(r.adjusted(0, 0, -6, -4), Qt::AlignRight | Qt::AlignBottom,
                   QStringLiteral("not in circuit"));
    }

    // ── What is under the pointer ────────────────────────────────────
    //
    // A frequency axis that spans nine octaves in five hundred pixels
    // cannot be read by eye: an estimate off a log scale is routinely a
    // third of an octave out, and it is worst in the middle of the range
    // where every decision gets made. So the picture says where the
    // pointer is, in the two units the operator is thinking in.
    if (m_haveCursor && r.contains(m_cursor)) {
        p.setClipRect(r);
        QColor guide = c(Style::kTextInactive);
        guide.setAlpha(90);
        p.setPen(QPen(guide, 1, Qt::DotLine));
        p.drawLine(m_cursor.x(), r.top(), m_cursor.x(), r.bottom());
        p.drawLine(r.left(), m_cursor.y(), r.right(), m_cursor.y());
        p.setClipping(false);

        const double hz = hzForX(m_cursor.x(), r);
        const QString text = QStringLiteral("%1  %2 dB")
            .arg(hz >= 1000.0
                     ? QStringLiteral("%1 kHz").arg(hz / 1000.0, 0, 'f', 2)
                     : QStringLiteral("%1 Hz").arg(hz, 0, 'f', 0))
            .arg(dbForY(m_cursor.y(), r), 0, 'f', 1);

        QFont rf = p.font();
        rf.setPointSizeF(8.5);
        p.setFont(rf);
        const int w = QFontMetrics(rf).horizontalAdvance(text) + 12;
        const QRect box(r.right() - w - 4, r.top() + 3, w, 16);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 150));
        p.drawRoundedRect(box, 3, 3);
        p.setPen(c(Style::kTextPrimary));
        p.drawText(box, Qt::AlignCenter, text);
    }

    // ── The trim the equaliser is taking off ─────────────────────────
    //
    // Shown because it is a gain change nobody asked for by hand. It is
    // the right thing to do — see EqLoudness.h — but a control that
    // quietly moves the level and does not say so is a control that
    // gets blamed for something else later.
    if (eqOn) {
        const double trim = 20.0 * std::log10(
            std::max(1e-6, double(eq.masterGain())));
        if (std::abs(trim) > 0.05) {
            QFont tf = p.font();
            tf.setPointSizeF(8.0);
            p.setFont(tf);
            p.setPen(c(Style::kTextInactive));
            // Below where the pointer readout sits, so the two never
            // overlap when the operator is hovering — which is exactly
            // when both are wanted.
            p.drawText(QRect(r.right() - 180, r.top() + 21, 176, 14),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QStringLiteral("loudness matched  %1%2 dB")
                           .arg(trim > 0 ? QStringLiteral("+")
                                         : QString())
                           .arg(trim, 0, 'f', 1));
        }
    }

    // A key, because three curves in three colours is two more than
    // anyone remembers between sessions.
    {
        QFont kf = p.font();
        kf.setPointSizeF(8.0);
        p.setFont(kf);
        int x = r.left() + 6;
        const int y = r.bottom() - 14;
        auto swatch = [&](const QColor& col, const QString& text,
                          bool dashed) {
            QPen pen(col, 2.0);
            if (dashed) { pen.setStyle(Qt::DashLine); }
            p.setPen(pen);
            p.drawLine(x, y + 6, x + 16, y + 6);
            p.setPen(c(Style::kTextSecondary));
            const int w = QFontMetrics(kf).horizontalAdvance(text) + 6;
            p.drawText(QRect(x + 20, y, w + 4, 13),
                       Qt::AlignLeft | Qt::AlignVCenter, text);
            x += 20 + w + 10;
        };
        swatch(QColor(0xd0, 0x90, 0x20, 220),
               QStringLiteral("your voice, %1 s").arg(MicSpectrum::kHoldSeconds),
               false);
        swatch(QColor(0xe8, 0x78, 0xb0, 210),
               QStringLiteral("put the blue here"), true);
        swatch(c(Style::kAccent), QStringLiteral("equaliser"), false);
        if (m_showResult) {
            swatch(QColor(0x50, 0xd0, 0x80, 230),
                   QStringLiteral("result"), false);
        }
    }
}


// ── StripDynamicsCurve ───────────────────────────────────────────────

StripDynamicsCurve::StripDynamicsCurve(Stage s, QWidget* parent)
    : QWidget(parent), m_stage(s)
{
    setMinimumHeight(150);
    setMouseTracking(true);
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
    if (!m_chain) { return; }
    switch (m_stage) {
    case Stage::Gate:
        m_liveIn  = double(m_chain->gate().inputPeakDb());
        m_liveOut = double(m_chain->gate().outputPeakDb());
        break;
    case Stage::Compressor:
        m_liveIn  = double(m_chain->comp().inputPeakDb());
        m_liveOut = double(m_chain->comp().outputPeakDb());
        break;
    case Stage::Limiter:
        m_liveIn  = double(m_chain->limiter().inputPeakDb());
        m_liveOut = double(m_chain->limiter().outputPeakDb());
        break;
    }

    // Peak hold, 20 dB/s, the same constant the level bars use.
    constexpr double kDecayDbPerTick = 2.0;   // at the 10 Hz meter timer
    m_holdIn  = std::max(m_liveIn,  m_holdIn  - kDecayDbPerTick);
    m_holdOut = std::max(m_liveOut, m_holdOut - kDecayDbPerTick);

    // The trail records where the signal ACTUALLY went, not where the
    // hold says it has been — a trail of held values would be a trail of
    // the same decaying point and would show nothing.
    if (m_liveIn > kMinDb + 1.0) {
        m_trail[static_cast<size_t>(m_trailNext)] =
            QPointF(m_liveIn, m_liveOut);
        m_trailNext = (m_trailNext + 1) % kTrail;
        m_trailCount = std::min(m_trailCount + 1, kTrail);
    }
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

double StripDynamicsCurve::xFor(double db, const QRect& r) const
{
    const double t = (db - kMinDb) / (kMaxDb - kMinDb);
    return r.left() + std::clamp(t, 0.0, 1.0) * r.width();
}

double StripDynamicsCurve::yFor(double db, const QRect& r) const
{
    const double t = (db - kMinDb) / (kMaxDb - kMinDb);
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

    // Grid every 12 dB, and the unity diagonal dashed. The distance
    // between the curve and that diagonal is what the stage is doing.
    p.setPen(QPen(c(Style::kGroove), 1, Qt::DotLine));
    for (double db = -48.0; db < 0.0; db += 12.0) {
        const int x = int(xFor(db, r));
        const int y = int(yFor(db, r));
        p.drawLine(x, r.top(), x, r.bottom());
        p.drawLine(r.left(), y, r.right(), y);
    }
    p.setPen(QPen(c(Style::kTextInactive), 1, Qt::DashLine));
    p.drawLine(QPointF(xFor(kMinDb, r), yFor(kMinDb, r)),
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

    // The gate's hysteresis: it closes at one level and re-opens at a
    // higher one, and the gap between them is the whole reason a gate
    // does not chatter on a breath. Two lines, because one would be a
    // lie about a stage that deliberately has two.
    if (m_stage == Stage::Gate) {
        const double ret = double(m_chain->gate().returnDb());
        if (std::abs(ret - thr) > 0.1) {
            p.setPen(QPen(QColor(0x50, 0xd0, 0x80, 130), 1, Qt::DotLine));
            const int rx = int(xFor(ret, r));
            p.drawLine(rx, r.top(), rx, r.bottom());
        }
    }

    QPainterPath path;
    for (int x = r.left(); x <= r.right(); ++x) {
        const double inDb = kMinDb
            + (double(x - r.left()) / std::max(1, r.width()))
              * (kMaxDb - kMinDb);
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
    p.setPen(QPen(QColor(0x00, 0xe5, 0xff), 2.1));
    p.drawPath(path);

    // ── Where the signal is, right now ───────────────────────────────
    //
    // The dot is the point of the whole picture. A transfer curve on its
    // own is a manual page; the dot turns it into an instrument, because
    // it answers the only question the operator actually has — am I
    // hitting this stage at all, and where.
    // The range the signal has swept recently, oldest faintest. On a
    // transfer curve this is the useful part: a single point says where
    // you are, the trail says how much of the curve you are using.
    for (int i = 0; i < m_trailCount; ++i) {
        const int age = (m_trailCount - 1)
            - ((i - m_trailNext + kTrail) % kTrail);
        const QPointF& v = m_trail[static_cast<size_t>(i)];
        if (v.x() <= kMinDb + 1.0) { continue; }
        const int alpha = 12 + 60 * (kTrail - std::min(age, kTrail)) / kTrail;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xff, 0xb8, 0x00, alpha));
        p.drawEllipse(QPointF(xFor(v.x(), r), yFor(v.y(), r)), 2.4, 2.4);
    }

    if (m_holdIn > kMinDb + 1.0) {
        const QPointF dot(xFor(m_holdIn, r), yFor(m_holdOut, r));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xff, 0xb8, 0x00, 40));
        p.drawEllipse(dot, 10.0, 10.0);
        p.setPen(QPen(QColor(0xff, 0xb8, 0x00), 1.6));
        p.setBrush(QColor(0xff, 0xb8, 0x00, 210));
        p.drawEllipse(dot, 4.5, 4.5);
    }

    // ── The scale, only when asked for ───────────────────────────────
    if (m_haveCursor && r.contains(m_cursor)) {
        QColor guide = c(Style::kTextInactive);
        guide.setAlpha(110);
        p.setPen(QPen(guide, 1, Qt::DotLine));
        p.drawLine(m_cursor.x(), r.top(), m_cursor.x(), r.bottom());
        p.drawLine(r.left(), m_cursor.y(), r.right(), m_cursor.y());
        p.setClipping(false);

        const double inDb = kMinDb + (double(m_cursor.x() - r.left())
            / std::max(1, r.width())) * (kMaxDb - kMinDb);
        drawReadout(p, r, QStringLiteral("%1 → %2 dB")
                              .arg(inDb, 0, 'f', 1)
                              .arg(outputDb(inDb), 0, 'f', 1));
    }
    p.setClipping(false);

    p.setPen(c(Style::kTextInactive));
    p.drawText(r.adjusted(3, 0, 0, -1), Qt::AlignLeft | Qt::AlignBottom,
               QStringLiteral("in"));
    p.drawText(r.adjusted(0, 2, -3, 0), Qt::AlignRight | Qt::AlignTop,
               QStringLiteral("out"));
}

// ── StripShaperCurve ─────────────────────────────────────────────────

StripShaperCurve::StripShaperCurve(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(150);
}

void StripShaperCurve::setChain(StripChain* chain) { m_chain = chain; update(); }
QSize StripShaperCurve::sizeHint() const { return QSize(220, 170); }

void StripShaperCurve::refresh()
{
    if (m_chain) { m_livePeak = double(m_chain->tube().inputPeakDb()); }
    constexpr double kDecayDbPerTick = 2.0;
    m_holdPeak = std::max(m_livePeak, m_holdPeak - kDecayDbPerTick);
    update();
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
    if (m_holdPeak > -60.0) {
        const double amp = std::pow(10.0, m_holdPeak / 20.0);
        for (double sgn : {-1.0, 1.0}) {
            const double x = sgn * amp;
            const double px = r.left()
                + (x + 1.5) / 3.0 * r.width();
            const double y = double(ClientTube::shapeAt(float(x * drive),
                                                        float(bias), model));
            const double py = r.center().y() - std::clamp(y, -1.6, 1.6)
                              * (r.height() / 3.2);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0xff, 0xb8, 0x00, 36));
            p.drawEllipse(QPointF(px, py), 8.0, 8.0);
            p.setPen(QPen(QColor(0xff, 0xb8, 0x00), 1.5));
            p.setBrush(QColor(0xff, 0xb8, 0x00, 200));
            p.drawEllipse(QPointF(px, py), 3.5, 3.5);
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
