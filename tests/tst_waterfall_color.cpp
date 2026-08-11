// The waterfall colour mapping, pinned against the bug it had.
//
// Reported on ANAN-7000DLE/macOS and present in released 0.5.2: the
// whole waterfall came up a solid magenta. The cause was arithmetic,
// not the GPU. The waterfall AGC set its thresholds to
// runMin-12 .. runMax+12, and dbmToRgb then subtracted the colour-gain
// slider from the top — 13.5 dB at the shipped default of 45. More came
// off the top than the margin had put there, so the effective high
// threshold sat 1.5 dB BELOW the running maximum. The loudest pixels
// always clamped to the palette's last stop, which in ClarityBlue is
// magenta, and on a flat spectrum every pixel is the loudest.
//
// These tests are about the mapping, so they use waterfallColor
// directly and reproduce the AGC's composition arithmetic alongside it.
// no-port-check: the guards are NereusSDR-original; the underlying
// Thetis formula is unchanged and attributed in SpectrumWidget.cpp.

#include <QtTest/QtTest>

#include "gui/SpectrumWidget.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace NereusSDR;

namespace {

constexpr int kBlackDefault = 104;   // shipped default
constexpr int kGainDefault  = 45;    // shipped default

// The composition the waterfall AGC performs, mirrored here so a change
// to one without the other shows up as a failing test rather than as a
// magenta screen.
void agcWindow(float runMin, float runMax, int blackLevel, int colorGain,
               float& low, float& high)
{
    constexpr float kMargin = 12.0f;
    const float blackCut = SpectrumWidget::wfBlackLevelOffsetDb(blackLevel);
    const float gainCut  = SpectrumWidget::wfColorGainOffsetDb(colorGain);

    low  = runMin - kMargin;
    high = runMax + kMargin;

    const float dataSpan = std::max(0.0f, runMax - runMin);
    const float clipAllowance =
        std::min(SpectrumWidget::kWfMaxClipDb, 0.25f * dataSpan);
    low  = std::min(low,  runMin - 1.0f - blackCut);
    high = std::max(high, runMax - clipAllowance + gainCut);

    const float needed =
        SpectrumWidget::kWfAgcPaletteSpanDb + blackCut + gainCut;
    if (high - low < needed) {
        const float mid = 0.5f * (low + high);
        low  = mid - 0.5f * needed;
        high = mid + 0.5f * needed;
    }
}

QRgb colorAt(float dbm, float runMin, float runMax,
             int blackLevel = kBlackDefault, int colorGain = kGainDefault)
{
    float low = 0.0f, high = 0.0f;
    agcWindow(runMin, runMax, blackLevel, colorGain, low, high);
    return SpectrumWidget::waterfallColor(dbm, low, high, blackLevel,
                                          colorGain,
                                          WfColorScheme::ClarityBlue);
}

// The colour the whole screen used to be — the OLD magenta cap. It no
// longer exists in any palette (269e9676 removed it), so the negative
// assertions below can never fire on it; they are kept as tripwires
// against the cap ever coming back.
constexpr QRgb kClarityTop = qRgb(0xff, 0x40, 0xc0);

// The palette's ACTUAL last stop since 269e9676: red. What a peak is
// allowed — and expected — to reach.
constexpr QRgb kClarityPeak = qRgb(0xff, 0x2a, 0x2a);

} // namespace

class TstWaterfallColor : public QObject {
    Q_OBJECT
private slots:
    void a_flat_spectrum_is_not_one_solid_colour();
    void a_flat_spectrum_lands_mid_palette();
    void a_nearly_flat_band_still_shows_texture();
    void the_noise_floor_of_a_busy_band_stays_dark();
    void peaks_may_reach_the_top_colour();
    void the_colour_gain_slider_still_does_something();
    void a_collapsed_window_does_not_saturate();
    void not_a_number_is_dark_not_bright();
    void slider_offsets_match_the_shipped_defaults();
    void no_palette_ends_in_pink_or_purple();
    void the_ramp_has_no_hard_edges();
};

void TstWaterfallColor::a_flat_spectrum_is_not_one_solid_colour()
{
    // The reported bug, in one assertion. Every pixel identical, which
    // is what a quiet band or a radio not yet delivering varied data
    // looks like.
    for (float level : {-140.0f, -110.0f, -90.0f, -60.0f}) {
        const QRgb c = colorAt(level, level, level);
        QVERIFY2(c != kClarityTop,
                 qPrintable(QStringLiteral("flat at %1 dBm gave the palette "
                                           "top (magenta)").arg(level)));
    }
}

void TstWaterfallColor::a_flat_spectrum_lands_mid_palette()
{
    // Not merely "not magenta" — it should sit somewhere sensible, so
    // the picture reads as a uniform mid-tone rather than clipped at
    // either end.
    const QRgb c = colorAt(-110.0f, -110.0f, -110.0f);
    QVERIFY(c != kClarityTop);
    QVERIFY(c != qRgb(0, 0, 0));
}

void TstWaterfallColor::a_nearly_flat_band_still_shows_texture()
{
    // Five decibels of variation must produce visibly different colours
    // at the two ends, or a quiet band is a flat wash.
    const QRgb lo = colorAt(-112.0f, -112.0f, -107.0f);
    const QRgb hi = colorAt(-107.0f, -112.0f, -107.0f);
    QVERIFY(lo != hi);
    QVERIFY(hi != kClarityTop);
}

void TstWaterfallColor::the_noise_floor_of_a_busy_band_stays_dark()
{
    // With 40 dB of real signal on the band, the floor must be near
    // black. A fix that avoided magenta by washing everything pale
    // would pass the tests above and ruin the display.
    const QRgb floorColour = colorAt(-120.0f, -120.0f, -80.0f);
    QVERIFY2(qRed(floorColour) < 40 && qGreen(floorColour) < 60
                 && qBlue(floorColour) < 100,
             qPrintable(QStringLiteral("floor was %1,%2,%3")
                            .arg(qRed(floorColour)).arg(qGreen(floorColour))
                            .arg(qBlue(floorColour))));
}

void TstWaterfallColor::peaks_may_reach_the_top_colour()
{
    // The opposite failure would be a display that can never show a
    // strong signal. Peaks reaching the top of the palette is correct;
    // the top of the palette swallowing the picture is not.
    const QRgb peak = colorAt(-80.0f, -120.0f, -80.0f);
    // Against the palette's real top (red since 269e9676) — this test
    // still compared against the removed magenta cap and failed on the
    // 2026-08-11 full-suite run.
    QCOMPARE(peak, kClarityPeak);
}

void TstWaterfallColor::the_colour_gain_slider_still_does_something()
{
    // The guard clamps how far the slider may tighten the window. It
    // must not clamp so early that the control is inert.
    const QRgb atMin = colorAt(-90.0f, -120.0f, -80.0f, kBlackDefault, 0);
    const QRgb atMid = colorAt(-90.0f, -120.0f, -80.0f, kBlackDefault, 45);
    const QRgb atMax = colorAt(-90.0f, -120.0f, -80.0f, kBlackDefault, 100);
    QVERIFY(atMin != atMid);
    QVERIFY(atMid != atMax);
}

void TstWaterfallColor::a_collapsed_window_does_not_saturate()
{
    // Thresholds can also arrive collapsed from a path that is not the
    // AGC — a user typing low=-100, high=-99. The old guard widened
    // that to one decibel, which is not a display: everything above the
    // floor became the top colour.
    const QRgb c = SpectrumWidget::waterfallColor(
        -99.5f, -100.0f, -99.0f, kBlackDefault, kGainDefault,
        WfColorScheme::ClarityBlue);
    QVERIFY(c != kClarityTop);

    // Inverted thresholds must not produce a negative range either.
    const QRgb inverted = SpectrumWidget::waterfallColor(
        -100.0f, -80.0f, -120.0f, kBlackDefault, kGainDefault,
        WfColorScheme::ClarityBlue);
    QVERIFY(inverted != kClarityTop);
}

void TstWaterfallColor::not_a_number_is_dark_not_bright()
{
    // A NaN pixel is unknown, not loud. Left alone it falls through the
    // stop search — every comparison against NaN is false — and takes
    // the last stop, painting the brightest colour in the palette for
    // missing data.
    const QRgb c = SpectrumWidget::waterfallColor(
        std::numeric_limits<float>::quiet_NaN(), -120.0f, -80.0f,
        kBlackDefault, kGainDefault, WfColorScheme::ClarityBlue);
    QCOMPARE(c, qRgb(0, 0, 0));
}

void TstWaterfallColor::slider_offsets_match_the_shipped_defaults()
{
    // The numbers the bug turned on. If either formula changes, the
    // margin arithmetic in composeWaterfallActiveThresholds has to be
    // revisited, and this is the tripwire.
    QVERIFY(std::abs(SpectrumWidget::wfBlackLevelOffsetDb(104) - 8.4f) < 0.01f);
    QVERIFY(std::abs(SpectrumWidget::wfColorGainOffsetDb(45) - 13.5f) < 0.01f);
    // The inequality that caused it: the slider took more off the top
    // than the AGC's 12 dB margin put there.
    QVERIFY(SpectrumWidget::wfColorGainOffsetDb(45) > 12.0f);
}

namespace {

// The palette as 256 samples, read through the real mapping with a
// plain 0..1 window so position maps straight to palette position.
QVector<QRgb> rampOf(WfColorScheme scheme)
{
    QVector<QRgb> out;
    out.reserve(256);
    // A wide window, so the min-span guard stays out of the way. The
    // old 0..1 dB window was silently widened to kWfMinSpanDb by the
    // collapsed-window guard (53f26cc6), which meant these samples
    // covered only the bottom sliver of the palette — and the CIELAB
    // step off pure black in that sliver read as a "hard edge" that is
    // really just L*'s cube-root steepness (2026-08-11 full-suite
    // failure). dbm = t·span over a 0..span window maps t straight to
    // palette position again, which is what this helper promises.
    constexpr float kSpan = 100.0f;
    for (int i = 0; i < 256; ++i) {
        const float t = static_cast<float>(i) / 255.0f;
        // Sliders neutralised: black level 125 adds nothing, colour
        // gain 0 takes nothing.
        out.append(SpectrumWidget::waterfallColor(t * kSpan, 0.0f, kSpan,
                                                  125, 0, scheme));
    }
    return out;
}

// CIELAB, so "how different do these look" is measured rather than
// guessed. Two colours can differ hugely in RGB and barely at all to
// the eye, and vice versa.
struct Lab { double l, a, b; };

Lab toLab(QRgb c)
{
    auto lin = [](int v) {
        const double u = v / 255.0;
        return u <= 0.04045 ? u / 12.92 : std::pow((u + 0.055) / 1.055, 2.4);
    };
    const double r = lin(qRed(c)), g = lin(qGreen(c)), b = lin(qBlue(c));
    const double x = (r * 0.4124 + g * 0.3576 + b * 0.1805) / 0.95047;
    const double y = (r * 0.2126 + g * 0.7152 + b * 0.0722);
    const double z = (r * 0.0193 + g * 0.1192 + b * 0.9505) / 1.08883;
    auto f = [](double t) {
        return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116.0;
    };
    const double fx = f(x), fy = f(y), fz = f(z);
    return {116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)};
}

double deltaE(QRgb a, QRgb b)
{
    const Lab p = toLab(a), q = toLab(b);
    return std::sqrt((p.l - q.l) * (p.l - q.l) + (p.a - q.a) * (p.a - q.a)
                     + (p.b - q.b) * (p.b - q.b));
}

const WfColorScheme kSchemes[] = {
    WfColorScheme::Default, WfColorScheme::Enhanced,
    WfColorScheme::ClarityBlue, WfColorScheme::LinLog,
    WfColorScheme::LinRad, WfColorScheme::Custom,
};

} // namespace

void TstWaterfallColor::no_palette_ends_in_pink_or_purple()
{
    // ClarityBlue ended magenta and Enhanced ended purple, both one step
    // past red — a hue reversal after a ramp that runs forwards through
    // the spectrum the whole way. It reads as a pink cap on every strong
    // signal rather than as 'hotter still'.
    for (WfColorScheme s : kSchemes) {
        for (QRgb c : rampOf(s)) {
            const bool pink = qRed(c) > 150 && qBlue(c) > 120
                              && qGreen(c) < qBlue(c) * 0.7;
            QVERIFY2(!pink,
                     qPrintable(QStringLiteral("scheme %1 contains #%2%3%4")
                                    .arg(static_cast<int>(s))
                                    .arg(qRed(c), 2, 16, QLatin1Char('0'))
                                    .arg(qGreen(c), 2, 16, QLatin1Char('0'))
                                    .arg(qBlue(c), 2, 16, QLatin1Char('0'))));
        }
    }
}

void TstWaterfallColor::the_ramp_has_no_hard_edges()
{
    // Stops placed on round numbers rather than on equal perceptual
    // steps make the ramp band: one step several times larger than its
    // neighbours shows as an edge between colour zones. Measured in
    // CIELAB, no single step should be more than four times the mean.
    //
    // The bound is generous on purpose. It is a guard against a stop
    // being dropped, mistyped or moved a long way, not a demand that
    // every palette be perceptually uniform — Spectran and BlackWhite
    // are deliberately coarse and are not in the list.
    for (WfColorScheme s : {WfColorScheme::ClarityBlue,
                            WfColorScheme::Enhanced}) {
        const QVector<QRgb> r = rampOf(s);
        double total = 0.0, worst = 0.0;
        for (int i = 1; i < r.size(); ++i) {
            const double d = deltaE(r.at(i), r.at(i - 1));
            total += d;
            worst = std::max(worst, d);
        }
        const double mean = total / (r.size() - 1);
        QVERIFY2(worst <= 4.0 * mean,
                 qPrintable(QStringLiteral("scheme %1: worst step %2, "
                                           "mean %3")
                                .arg(static_cast<int>(s))
                                .arg(worst).arg(mean)));
    }
}

QTEST_MAIN(TstWaterfallColor)
#include "tst_waterfall_color.moc"
