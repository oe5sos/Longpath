#include "DssRenderer.h"

#include <QVector>

#include <cmath>
#include <cstdlib>
#include <vector>

// =================================================================
// src/gui/DssRenderer.cpp  (NereusSDR)
// =================================================================
// Ported from AetherSDR `src/gui/DssRenderer.cpp` (AetherSDR 31b29583),
// reduced scope — see the header for what was left out and why.
//
// rebuild() no longer paints through QPainter. AetherSDR's CPU fallback
// fills one trapezoid per column and strokes one anti-aliased line per
// column, back to front: 96 rows x 767 polygons + 767 lines = ~147k
// QPainter calls per frame, and every pixel is overpainted up to 96
// times. Measured on an Apple-silicon MacBook Air (-O3, 2026-09-03):
// 620 ms per rebuild at 1400x450 px, 1320 ms at Retina 2800x900 px --
// about one frame per second, with the GUI thread frozen in between.
// AetherSDR only ever runs that path when its GPU mesh cannot be created.
//
// The rasteriser below produces an equivalent picture (same geometry, same
// per-column colours, the crest's pen width reproduced as coverage) at
// ~7-10 ms:
// traces are walked FRONT to back and a per-x "horizon" remembers how far
// up a nearer trace has already claimed the column, so a farther trace
// only fills the part of its curtain that is still visible -- each curtain
// pixel is written once. Because every curtain reaches the plot floor,
// "the last trace painted back-to-front" and "the nearest trace whose
// ridge lies above the pixel" are the same trace, so the result matches
// the painter's-algorithm original. The crest's partially covered pixels
// (the pen's anti-aliasing) are the one thing that cannot be resolved
// front to back; they are collected and blended at the end in painter's
// order (see PartialCrest in the header).
//
// Modification history (NereusSDR):
//   2026-09-03 — Ported in C++20/Qt6 for NereusSDR by Martin Fischer
//                 (OE5SOS), AI-assisted via Anthropic Claude Code.
//   2026-09-03 — rebuild(): QPainter polygon/line painting replaced by a
//                 direct horizon rasteriser with coverage-based crest
//                 anti-aliasing; geometry and colour formulas unchanged.
//                 Martin Fischer (OE5SOS), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

namespace Longpath {

namespace {

// CPU-only tunables. The perspective geometry (back-width / depth-span /
// front-ridge / haze) lives in DssRenderer.h as shared constants so the GPU
// mesh uses the same values; these are extra CPU-render touches the GPU frag
// doesn't replicate (depth dimming floor, smoothing, slope shading).
//   [verbatim from AetherSDR DssRenderer.cpp:12-15 @31b29583; Longpath has
//    no GPU mesh yet -- the constants are shared so a later port can't drift]
constexpr double kMinDim        = 0.50;  // depth dimming never falls below this
constexpr float  kTemporalAlpha = 0.60f; // temporal IIR: fraction of the new row
constexpr double kSlopeGain     = 0.55;  // slope shading strength
constexpr double kShadeLo       = 0.68;
constexpr double kShadeHi       = 1.32;

inline int chan(double v) { return static_cast<int>(std::clamp(v, 0.0, 255.0)); }

inline float median3(float a, float b, float c)
{
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

inline QColor scaled(const QColor& c, double f)
{
    f = std::max(0.0, f);
    return QColor(chan(c.red() * f), chan(c.green() * f), chan(c.blue() * f));
}

// Linear blend c -> t by f in [0,1].
inline QColor lerpColor(const QColor& c, const QColor& t, double f)
{
    f = std::clamp(f, 0.0, 1.0);
    return QColor(chan(c.red()   + (t.red()   - c.red())   * f),
                  chan(c.green() + (t.green() - c.green()) * f),
                  chan(c.blue()  + (t.blue()  - c.blue())  * f));
}

// One opaque pixel in QImage::Format_RGBA8888_Premultiplied memory order
// (bytes R, G, B, A). Written byte-wise so it is endian-safe; opaque means
// premultiplied and straight are the same value.
inline quint32 packOpaque(QRgb c)
{
    quint32 v = 0;
    auto* b = reinterpret_cast<uchar*>(&v);
    b[0] = static_cast<uchar>(qRed(c));
    b[1] = static_cast<uchar>(qGreen(c));
    b[2] = static_cast<uchar>(qBlue(c));
    b[3] = 255;
    return v;
}

// dst = src * cov + dst * (1 - cov), per channel, opaque result. Used for
// the crest's partial pixels (its anti-aliasing) and nothing else.
inline void blendInto(quint32* dst, quint32 src, double cov)
{
    const int a = std::clamp(static_cast<int>(cov * 256.0 + 0.5), 0, 256);
    auto*       d = reinterpret_cast<uchar*>(dst);
    const auto* s = reinterpret_cast<const uchar*>(&src);
    for (int i = 0; i < 3; ++i) {
        d[i] = static_cast<uchar>((s[i] * a + d[i] * (256 - a)) >> 8);
    }
    d[3] = 255;
}

std::array<float, DssRenderer::kCols> resampledRawRow(
    const QVector<float>& binsDbm, float fallback)
{
    std::array<float, DssRenderer::kCols> row;
    const int n = binsDbm.size();
    if (n <= 0) {
        row.fill(fallback);
        return row;
    }

    if (n == DssRenderer::kCols) {
        for (int c = 0; c < DssRenderer::kCols; ++c) {
            row[c] = std::isfinite(binsDbm[c]) ? binsDbm[c] : fallback;
        }
    } else {
        // Peak-preserving downsample (or upsample by nearest-max over a
        // fractional window): a narrow carrier landing between display
        // columns must not vanish because a plain average smeared it.
        const double step = static_cast<double>(n) / DssRenderer::kCols;
        for (int c = 0; c < DssRenderer::kCols; ++c) {
            int i0 = static_cast<int>(std::floor(c * step));
            int i1 = static_cast<int>(std::ceil((c + 1) * step));
            i0 = std::clamp(i0, 0, n - 1);
            i1 = std::clamp(i1, i0 + 1, n);
            float mx = std::isfinite(binsDbm[i0]) ? binsDbm[i0] : fallback;
            for (int i = i0 + 1; i < i1; ++i) {
                if (std::isfinite(binsDbm[i])) {
                    mx = std::max(mx, binsDbm[i]);
                }
            }
            row[c] = mx;
        }
    }

    return row;
}

std::array<float, DssRenderer::kCols> smoothDssRow(
    const std::array<float, DssRenderer::kCols>& raw,
    std::array<float, DssRenderer::kCols>& rawPrev1,
    std::array<float, DssRenderer::kCols>& rawPrev2,
    int& rawHistCount,
    const std::array<float, DssRenderer::kCols>* previousSmoothed)
{
    std::array<float, DssRenderer::kCols> row = raw;
    if (rawHistCount >= 2) {
        for (int c = 0; c < DssRenderer::kCols; ++c) {
            row[c] = median3(raw[c], rawPrev1[c], rawPrev2[c]);
        }
    }

    rawPrev2 = rawPrev1;
    rawPrev1 = raw;
    rawHistCount = std::min(rawHistCount + 1, 2);

    std::array<float, DssRenderer::kCols> smoothed = row;
    for (int c = 0; c < DssRenderer::kCols; ++c) {
        const float a = row[std::max(0, c - 1)];
        const float b = row[c];
        const float d = row[std::min(DssRenderer::kCols - 1, c + 1)];
        smoothed[c] = 0.25f * a + 0.5f * b + 0.25f * d;
    }
    if (previousSmoothed != nullptr) {
        for (int c = 0; c < DssRenderer::kCols; ++c) {
            smoothed[c] = kTemporalAlpha * smoothed[c]
                + (1.0f - kTemporalAlpha) * (*previousSmoothed)[c];
        }
    }
    return smoothed;
}

}  // namespace

void DssRenderer::clear()
{
    m_head  = 0;
    m_count = 0;
    m_rawHistCount = 0;
    m_dirty = true;
}

const std::array<float, DssRenderer::kCols>& DssRenderer::rowAt(int age) const
{
    const int idx = (m_head + age) % kRows;
    return m_rows[idx];
}

void DssRenderer::pushRow(const QVector<float>& binsDbm)
{
    const std::array<float, kCols> raw = resampledRawRow(binsDbm, -200.0f);
    const std::array<float, kCols>* previous =
        (m_count > 0) ? &m_rows[m_head] : nullptr;
    const std::array<float, kCols> smoothed =
        smoothDssRow(raw, m_rawPrev1, m_rawPrev2, m_rawHistCount, previous);

    m_head = (m_head - 1 + kRows) % kRows;
    m_rows[m_head] = smoothed;
    m_count = std::min(m_count + 1, kRows);
    m_dirty = true;
}

const QImage& DssRenderer::image(const QSize& px, int scaleStripPx,
                                 float floorDbm, float rangeDb, float zCurve,
                                 const PaletteFn& palette,
                                 quint64 paletteToken,
                                 const QColor& bgFill)
{
    // A palette that calls back into image() (nothing in Longpath does, but
    // the callback is host-injected) would otherwise reassign m_cache under
    // the frame being rasterised. Hand it what there is.
    if (m_rebuilding) {
        return m_cache;
    }

    const bool changed = m_dirty
        || px != m_cacheSize
        || scaleStripPx != m_cacheScaleStrip
        || floorDbm != m_cacheFloor
        || rangeDb != m_cacheRange
        || zCurve != m_cacheZCurve
        || paletteToken != m_cachePaletteToken;

    if (changed) {
        rebuild(px, scaleStripPx, floorDbm, rangeDb, zCurve, palette, bgFill);
        m_cacheSize         = px;
        m_cacheScaleStrip   = scaleStripPx;
        m_cacheFloor        = floorDbm;
        m_cacheRange        = rangeDb;
        m_cacheZCurve       = zCurve;
        m_cachePaletteToken = paletteToken;
        m_dirty             = false;
    }
    return m_cache;
}

void DssRenderer::rebuild(const QSize& px, int scaleStripPx, float floorDbm,
                          float rangeDb, float zCurve, const PaletteFn& palette,
                          const QColor& bgFill)
{
    struct RebuildScope {
        bool& flag;
        explicit RebuildScope(bool& f) : flag(f) { flag = true; }
        ~RebuildScope() { flag = false; }
    };
    const RebuildScope scope(m_rebuilding);

    // Never carry the deferred crest pixels of a frame that did not finish
    // (a throwing palette, std::bad_alloc) into the next one: their x/y
    // belong to that frame's size.
    m_partials.clear();

    const int W    = px.width();
    const int Htot = px.height();
    if (W <= 0 || Htot <= 0) {
        m_cache = QImage();
        return;
    }

    if (m_cache.size() != px || m_cache.format() != QImage::Format_RGBA8888_Premultiplied) {
        m_cache = QImage(px, QImage::Format_RGBA8888_Premultiplied);
    }
    if (m_cache.isNull()) {
        // QImage refused the allocation (absurd size, or out of memory):
        // bits() would be nullptr. Nothing to draw into.
        return;
    }
    m_cache.fill(Qt::transparent);

    // Plot region is everything above the (transparent) scale strip:
    // pixel rows [0, H). Every write below stays inside [0, W) x [0, H).
    const int H = std::clamp(Htot - std::max(0, scaleStripPx), 1, Htot);

    uchar* const    bits   = m_cache.bits();
    const qsizetype stride = m_cache.bytesPerLine();
    const auto rowPixels = [bits, stride](int y) {
        return reinterpret_cast<quint32*>(bits + static_cast<qsizetype>(y) * stride);
    };

    const quint32 bgPx = packOpaque(bgFill.rgb());
    for (int y = 0; y < H; ++y) {
        std::fill_n(rowPixels(y), W, bgPx);
    }

    // NaN/inf in the anchor or range would pass std::clamp unchanged and end
    // in static_cast<int>(NaN) below (undefined behaviour) -- treat like
    // "no usable mapping" and leave the background.
    if (m_count <= 0 || !palette || !(rangeDb > 0.0f)
        || !std::isfinite(rangeDb) || !std::isfinite(floorDbm)) {
        return;
    }

    const double zc            = std::max(0.05, static_cast<double>(zCurve));
    const double bottomY       = H;                       // plot floor
    const double depthSpan     = H * kDepthSpanFrac;
    const double frontMaxRidge = H * kFrontMaxRidgeFrac;
    // Match dss_mesh.vert's depth parametrization exactly (v = rr / rows), so
    // the CPU fallback and the GPU mesh place rows at the same depth.
    //   [original inline comment from AetherSDR DssRenderer.cpp:811-812]
    const double denom         = kVisibleRows;

    std::array<double,  kCols> ys;       // ridge y per column (px, down = +)
    std::array<quint32, kCols> fillPx;   // depth/slope-shaded curtain colour
    std::array<quint32, kCols> crestPx;  // bright rim colour

    // Horizon: per x pixel, the topmost row (exclusive limit) a NEARER
    // trace has already claimed. Everything from there down is hidden for
    // the traces behind it. Starts at H: nothing claimed, floor exposed.
    std::vector<int> horizon(static_cast<size_t>(W), H);

    // Front (newest) → back. Nearer traces are wider, sit lower, and fill
    // to the floor, so they occlude farther ones; the horizon does the
    // occlusion here instead of overpainting.
    for (int age = 0; age < visibleRowCount(); ++age) {
        const double depthFrac    = age / denom;
        const double rowWidthFrac = 1.0 - depthFrac * (1.0 - kBackWidthFrac);
        const double inset        = W * (1.0 - rowWidthFrac) * 0.5;
        const double rowW         = W - 2.0 * inset;
        const double baselineY    = bottomY - depthFrac * depthSpan;
        const double maxRidge     = frontMaxRidge * rowWidthFrac;
        const double dim          = kMinDim + (1.0 - kMinDim) * (1.0 - depthFrac);

        const auto& row = rowAt(age);
        // Pass 1: geometry — noise-floor-anchored ridge heights, with the
        // same pow(s, zCurve) floor-lift a GPU shader would apply.
        for (int c = 0; c < kCols; ++c) {
            double strength = std::clamp((row[c] - floorDbm) / rangeDb, 0.0f, 1.0f);
            strength = std::pow(strength, zc);
            ys[c] = baselineY - strength * maxRidge;
        }
        // Pass 2: colour — palette by amplitude, hazed by depth, lit by
        // slope for the curtain; the crest is the palette colour lightened.
        const double slopeScale = (maxRidge > 1.0) ? maxRidge : 1.0;
        for (int c = 0; c < kCols; ++c) {
            const int cl = std::max(0, c - 1);
            const int cr = std::min(kCols - 1, c + 1);
            const double slope = (ys[cl] - ys[cr]) / slopeScale; // +: rises to right
            const double shade = std::clamp(1.0 + kSlopeGain * slope, kShadeLo, kShadeHi);
            const QColor pal = QColor(palette(row[c]));
            const QColor base = lerpColor(pal, bgFill, depthFrac * kHaze);
            fillPx[c] = packOpaque(scaled(base, dim * shade).rgb());
            const QColor rim = lerpColor(pal.lighter(165), bgFill, depthFrac * kHaze);
            crestPx[c] = packOpaque(scaled(rim, dim).rgb());
        }

        // Pass 3: rasterise. Pixel-centre rule like a non-AA polygon fill:
        // pixel x belongs to the trace iff its centre lies in
        // [inset, inset + rowW]; the ridge between two columns is linear,
        // the curtain colour is flat per column (the original trapezoids).
        const int x0 = std::max(0,     static_cast<int>(std::ceil(inset - 0.5)));
        const int x1 = std::min(W - 1, static_cast<int>(std::floor(inset + rowW - 0.5)));
        const double colPerPx = (kCols - 1) / std::max(rowW, 1e-9);
        // Crest pen width of the original: 1.6 px on the front trace, 1.0
        // behind. Reproduced as coverage, never as whole pixels.
        const double crestW = (age == 0) ? 1.6 : 1.0;

        // One crest pixel. colTop is the first row of THIS trace's curtain
        // in that column, colLimit the row from which a nearer trace owns
        // the column. Rows inside our own curtain blend right away, as the
        // pen did over the freshly filled trapezoid. A fully covered row
        // above the curtain is solid and claimed. A PARTIALLY covered row
        // above the curtain is deferred: in painter's order it blends over
        // whatever lies behind us, and that is drawn later -- so it stays
        // unclaimed (farther curtains fill it first) and composites at the
        // end.
        const auto paintCrest = [&](int col, int r, int colTop, int colLimit,
                                    double cov, quint32 rgba) {
            if (r < 0 || r >= colLimit || cov <= 0.0) {
                return;
            }
            if (r >= colTop) {
                blendInto(&rowPixels(r)[col], rgba, cov);
            } else if (cov >= 0.999) {
                rowPixels(r)[col] = rgba;
                horizon[static_cast<size_t>(col)] =
                    std::min(horizon[static_cast<size_t>(col)], r);
            } else {
                m_partials.push_back({col, r, rgba, static_cast<float>(cov)});
            }
        };

        double prevY     = 0.0;
        int    prevTop   = -1;
        int    prevLimit = 0;
        for (int x = x0; x <= x1; ++x) {
            const double u = (x + 0.5 - inset) * colPerPx;
            const int    c = std::clamp(static_cast<int>(u), 0, kCols - 2);
            const double t = std::clamp(u - c, 0.0, 1.0);
            const double y = ys[c] + t * (ys[c + 1] - ys[c]);
            // First pixel row whose centre lies at or below the ridge.
            const int top   = std::clamp(static_cast<int>(std::ceil(y - 0.5)), 0, H);
            const int limit = horizon[static_cast<size_t>(x)];   // exclusive

            // Curtain: ridge down to where a nearer trace takes over. The
            // curtain claims the column from the ridge; paintCrest() may
            // raise that claim for solid crest pixels above it.
            for (int yy = top; yy < limit; ++yy) {
                rowPixels(yy)[x] = fillPx[c];
            }
            horizon[static_cast<size_t>(x)] = std::min(top, limit);

            // Crest band: the rim as a crestW-high band centred on the ridge.
            {
                const double bandLo = y - crestW * 0.5;
                const double bandHi = y + crestW * 0.5;
                const int rLo = std::max(0,     static_cast<int>(std::floor(bandLo)));
                const int rHi = std::min(H - 1, static_cast<int>(std::floor(bandHi)));
                for (int r = rLo; r <= rHi; ++r) {
                    const double cov = std::clamp(
                        std::min(r + 1.0, bandHi) - std::max(static_cast<double>(r), bandLo),
                        0.0, 1.0);
                    paintCrest(x, r, top, limit, cov, crestPx[c]);
                }
            }

            // Flank: where the ridge jumps between neighbouring pixel
            // columns, the original's pen ran as a steep hairline from
            // (x - 0.5, prevY) to (x + 0.5, y), its width spread over the
            // two columns it drifts across. Same here, row by row.
            if (prevTop >= 0 && std::abs(top - prevTop) >= 2) {
                const double dy  = y - prevY;
                const double yLo = std::min(y, prevY);
                const double yHi = std::max(y, prevY);
                const int rA = std::max(0,     static_cast<int>(std::ceil(yLo - 0.5)));
                const int rB = std::min(H - 1, static_cast<int>(std::floor(yHi - 0.5)));
                for (int r = rA; r <= rB; ++r) {
                    const double xr = (x - 0.5) + ((r + 0.5) - prevY) / dy;
                    const double lo = xr - crestW * 0.5;
                    const double hi = xr + crestW * 0.5;
                    const double covL = std::clamp(
                        std::min(hi, static_cast<double>(x)) - std::max(lo, x - 1.0), 0.0, 1.0);
                    const double covR = std::clamp(
                        std::min(hi, x + 1.0) - std::max(lo, static_cast<double>(x)), 0.0, 1.0);
                    if (x - 1 >= 0) {
                        paintCrest(x - 1, r, prevTop, prevLimit, covL, crestPx[c]);
                    }
                    paintCrest(x, r, top, limit, covR, crestPx[c]);
                }
            }

            prevY     = y;
            prevTop   = top;
            prevLimit = limit;
        }
    }

    // Deferred crest anti-aliasing, composited in painter's order: the
    // entries were pushed front to back, so walk them back to front and the
    // nearest trace's rim ends up on top, over whatever was drawn behind it.
    for (auto it = m_partials.rbegin(); it != m_partials.rend(); ++it) {
        blendInto(&rowPixels(it->y)[it->x], it->rgba, it->cov);
    }
    m_partials.clear();
}

}  // namespace Longpath
