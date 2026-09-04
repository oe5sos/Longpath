// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - DssRenderer (3D stacked-trace surface) tests
//
// Pins the contract DssRenderer.h documents: strength = clamp((dbm -
// floorDbm) / rangeDb, 0, 1) drives ridge height, the front row's baseline
// sits at the bottom of the plot rect, and image() is a no-op (background
// only) until at least one row has been pushed.

#include <QtTest>

#include "gui/DssRenderer.h"

using namespace Longpath;

namespace {
QVector<float> constantRow(int n, float value)
{
    QVector<float> row(n);
    row.fill(value);
    return row;
}
}  // namespace

class TestDssRenderer : public QObject {
    Q_OBJECT
private slots:
    void emptyRendererHasNoData();
    void pushRowMakesDataAvailable();
    void visibleRowCountCapsAtKVisibleRows();
    void clearResetsState();
    void imageIsBackgroundOnlyWithoutData();
    void imageHeightFollowsStrength();
    void nonStandardBinCountIsAccepted();
    void nearerTraceOccludesFartherOne();
    void crestBandLightensTheRidgeRows();
    void higherNearerTraceHidesTheFartherOneCompletely();
    void deferredCrestPartialBlendsOverTheFartherCurtain();
    void scaleStripStaysTransparent();
};

void TestDssRenderer::emptyRendererHasNoData()
{
    DssRenderer dss;
    QVERIFY(!dss.hasData());
    QCOMPARE(dss.visibleRowCount(), 0);
}

void TestDssRenderer::pushRowMakesDataAvailable()
{
    DssRenderer dss;
    dss.pushRow(constantRow(DssRenderer::kCols, -90.0f));
    QVERIFY(dss.hasData());
    QCOMPARE(dss.visibleRowCount(), 1);
}

void TestDssRenderer::visibleRowCountCapsAtKVisibleRows()
{
    DssRenderer dss;
    for (int i = 0; i < DssRenderer::kRows + 10; ++i) {
        dss.pushRow(constantRow(DssRenderer::kCols, -90.0f));
    }
    QCOMPARE(dss.visibleRowCount(), DssRenderer::kVisibleRows);
}

void TestDssRenderer::clearResetsState()
{
    DssRenderer dss;
    dss.pushRow(constantRow(DssRenderer::kCols, -90.0f));
    QVERIFY(dss.hasData());
    dss.clear();
    QVERIFY(!dss.hasData());
    QCOMPARE(dss.visibleRowCount(), 0);
}

void TestDssRenderer::imageIsBackgroundOnlyWithoutData()
{
    DssRenderer dss;
    const QColor bg(10, 20, 30);
    auto palette = [](float) -> QRgb { return qRgb(255, 0, 0); };
    const QImage& img = dss.image(QSize(200, 100), 0, -100.0f, 50.0f, 1.0f,
                                  palette, 1, bg);
    QCOMPARE(img.size(), QSize(200, 100));
    QCOMPARE(QColor(img.pixel(100, 50)), bg);
}

// One row, two halves: columns < kCols/2 at full strength (floorDbm +
// rangeDb), columns >= kCols/2 pinned exactly at floorDbm (zero strength).
// zCurve=1.0 removes the pow() curve so the geometry is exact:
//   front row (age 0) => depthFrac 0 => baseline at H, ridge = H * 0.46.
// A point between the loud ridge top and H must show the loud colour; the
// same point above the (zero-height) quiet trapezoid must still be the
// untouched background.
void TestDssRenderer::imageHeightFollowsStrength()
{
    DssRenderer dss;
    const float floorDbm = -100.0f;
    const float rangeDb  = 50.0f;
    QVector<float> row(DssRenderer::kCols);
    for (int c = 0; c < DssRenderer::kCols; ++c) {
        row[c] = (c < DssRenderer::kCols / 2) ? (floorDbm + rangeDb) : floorDbm;
    }
    dss.pushRow(row);

    const QColor bg(10, 20, 30);
    const QColor loud(255, 0, 0);
    const QColor quiet(0, 255, 0);
    auto palette = [&](float dbm) -> QRgb {
        return (dbm > floorDbm + rangeDb / 2.0f) ? loud.rgb() : quiet.rgb();
    };

    // W == kCols so column c maps to x == c (depthFrac 0 => no inset).
    const int H = 300;
    const QImage& img = dss.image(QSize(DssRenderer::kCols, H), 0,
                                  floorDbm, rangeDb, 1.0f, palette, 1, bg);
    QCOMPARE(img.size(), QSize(DssRenderer::kCols, H));

    // Front-row ridge top for full strength: H - H*0.46 = 0.54*H = 162.
    // y=250 sits well below that (inside the loud trapezoid) and well
    // above H (so still outside the quiet column's ~zero-height one).
    const int yBelowLoudRidge = 250;
    const int xLoud  = 100;  // well inside the loud half, away from the seam
    const int xQuiet = 600;  // well inside the quiet half

    QCOMPARE(QColor(img.pixel(xLoud, yBelowLoudRidge)), loud);
    QCOMPARE(QColor(img.pixel(xQuiet, yBelowLoudRidge)), bg);
}

void TestDssRenderer::nonStandardBinCountIsAccepted()
{
    DssRenderer dss;
    dss.pushRow(constantRow(DssRenderer::kCols, -90.0f));
    dss.pushRow(constantRow(137, -80.0f));  // arbitrary FFT bin count, not kCols
    QVERIFY(dss.hasData());
    QCOMPARE(dss.visibleRowCount(), 2);

    auto palette = [](float) -> QRgb { return qRgb(200, 200, 200); };
    const QImage& img = dss.image(QSize(400, 200), 0, -100.0f, 50.0f, 0.7f,
                                  palette, 1, QColor(0, 0, 0));
    QCOMPARE(img.size(), QSize(400, 200));
}

// Painter's-algorithm contract: a nearer (newer) trace hides everything
// below its ridge, and a farther trace stays visible above it. One full-
// strength row pushed first (ends up at age 1, behind) and one half-
// strength row pushed last (age 0, in front). pushRow's temporal blend
// (0.6 new + 0.4 previous) turns the front row's -75 dBm into -65 dBm,
// i.e. strength 0.7; the far row keeps its -50 dBm (strength 1.0).
//
// With H = 400 and zCurve = 1.0:
//   front ridge (age 0): H - 0.46 H * 0.7             = 271
//   far ridge   (age 1): 0.994 H - 0.46 H * 0.996 * 1 ~= 214
// So at mid-width, y = 240 lies below the far ridge but above the front
// one (far trace visible), and y = 350 lies inside the front curtain
// (near trace wins). Colour checks are tolerant: the far trace is hazed
// and dimmed by depth (a few units), never recoloured.
void TestDssRenderer::nearerTraceOccludesFartherOne()
{
    DssRenderer dss;
    const float floorDbm = -100.0f;
    const float rangeDb  = 50.0f;
    dss.pushRow(constantRow(DssRenderer::kCols, floorDbm + rangeDb));         // far, full
    dss.pushRow(constantRow(DssRenderer::kCols, floorDbm + rangeDb / 2.0f));  // near, half

    const QColor bg(10, 20, 30);
    auto palette = [&](float dbm) -> QRgb {
        return (dbm > floorDbm + rangeDb * 0.75f) ? qRgb(255, 0, 0)   // loud: red
                                                  : qRgb(0, 255, 0);  // quiet: green
    };

    const int W = DssRenderer::kCols;
    const int H = 400;
    const QImage& img = dss.image(QSize(W, H), 0, floorDbm, rangeDb, 1.0f,
                                  palette, 1, bg);
    QCOMPARE(img.size(), QSize(W, H));

    const int x = W / 2;
    const QColor aboveNearRidge(img.pixel(x, 240));
    const QColor insideNearCurtain(img.pixel(x, 350));
    QVERIFY2(aboveNearRidge.red() > 200 && aboveNearRidge.green() < 40,
             qPrintable(QStringLiteral("far trace must show above the near ridge, got %1")
                            .arg(aboveNearRidge.name())));
    QVERIFY2(insideNearCurtain.green() > 200 && insideNearCurtain.red() < 40,
             qPrintable(QStringLiteral("near trace must hide the far one below its ridge, got %1")
                            .arg(insideNearCurtain.name())));
    // And the floor beyond the front ridge's reach is still background:
    // nothing draws above the far ridge at all.
    QCOMPARE(QColor(img.pixel(x, 150)), bg);
}

// The crest: a pen of width 1.6 px (front trace) centred on the ridge,
// reproduced as row coverage. One full-strength row, H = 400, zCurve 1.0:
// ridge y = 400 - 0.46 * 400 = 216.0 exactly, so the band [215.2, 216.8]
// covers row 215 (0.8, above the curtain -> deferred partial over the
// background) and row 216 (0.8, first curtain row -> blended over the
// fill). Row 217 is untouched curtain, row 214 untouched background. The
// palette returns pure red; QColor::lighter(165) turns that into a pink
// (255,165,165), so "crest present" shows up as a green channel well above
// the curtain's zero.
void TestDssRenderer::crestBandLightensTheRidgeRows()
{
    DssRenderer dss;
    const float floorDbm = -100.0f;
    const float rangeDb  = 50.0f;
    dss.pushRow(constantRow(DssRenderer::kCols, floorDbm + rangeDb));
    const QColor bg(10, 20, 30);
    auto palette = [](float) -> QRgb { return qRgb(255, 0, 0); };
    const QImage& img = dss.image(QSize(DssRenderer::kCols, 400), 0, floorDbm,
                                  rangeDb, 1.0f, palette, 1, bg);
    const int x = 100;
    const QColor above(img.pixel(x, 214));
    const QColor partial(img.pixel(x, 215));
    const QColor first(img.pixel(x, 216));
    const QColor curtain(img.pixel(x, 217));
    QCOMPARE(above, bg);
    QVERIFY2(partial != bg && partial.green() > 100,
             qPrintable(QStringLiteral("row 215 must carry the crest's partial coverage, got %1").arg(partial.name())));
    QVERIFY2(first.red() == 255 && first.green() > 100 && first.green() < 165,
             qPrintable(QStringLiteral("row 216 must be crest blended over the fill, got %1").arg(first.name())));
    QCOMPARE(curtain, QColor(255, 0, 0));
}

// The other half of the occlusion contract: a nearer trace whose ridge is
// HIGHER than the farther one hides it completely, crest included. Far row
// half strength (green), near row pushed last: 0.6 * -50 + 0.4 * -75 =
// -60 dBm -> strength 0.8 (red). Near ridge 400 - 184 * 0.8 = 252.8, far
// ridge ~306 -- everything of the far trace lies under the near curtain.
// A horizon rasteriser that let a far crest through would show green here.
void TestDssRenderer::higherNearerTraceHidesTheFartherOneCompletely()
{
    DssRenderer dss;
    const float floorDbm = -100.0f;
    const float rangeDb  = 50.0f;
    dss.pushRow(constantRow(DssRenderer::kCols, floorDbm + rangeDb / 2.0f));  // far, half
    dss.pushRow(constantRow(DssRenderer::kCols, floorDbm + rangeDb));         // near, full
    const QColor bg(10, 20, 30);
    auto palette = [&](float dbm) -> QRgb {
        return (dbm > floorDbm + rangeDb * 0.75f) ? qRgb(255, 0, 0) : qRgb(0, 255, 0);
    };
    const int H = 400;
    const QImage& img = dss.image(QSize(DssRenderer::kCols, H), 0, floorDbm,
                                  rangeDb, 1.0f, palette, 1, bg);
    const int x = DssRenderer::kCols / 2;
    for (int y = 0; y < H; ++y) {
        const QColor c(img.pixel(x, y));
        QVERIFY2(!(c.green() > 200 && c.red() < 40),
                 qPrintable(QStringLiteral("far (green) trace leaked through at row %1: %2").arg(y).arg(c.name())));
    }
    QCOMPARE(QColor(img.pixel(x, 350)), QColor(255, 0, 0));
    QCOMPARE(QColor(img.pixel(x, 200)), bg);
}

// Same setup as nearerTraceOccludesFartherOne (far full-strength red at
// ridge ~214, near strength 0.7 green at ridge 271.2). The near crest band
// [270.4, 272.0] leaves a 0.6 partial on row 270 -- ABOVE the near curtain,
// where the far curtain is. In painter's order that partial blends over the
// far curtain; a rasteriser that blended it over the background instead
// (or claimed the row so the far curtain never got there) would lose the
// red. Expected ~ 0.6 * pink-green crest + 0.4 * red curtain.
void TestDssRenderer::deferredCrestPartialBlendsOverTheFartherCurtain()
{
    DssRenderer dss;
    const float floorDbm = -100.0f;
    const float rangeDb  = 50.0f;
    dss.pushRow(constantRow(DssRenderer::kCols, floorDbm + rangeDb));         // far, full
    dss.pushRow(constantRow(DssRenderer::kCols, floorDbm + rangeDb / 2.0f));  // near, half
    const QColor bg(10, 20, 30);
    auto palette = [&](float dbm) -> QRgb {
        return (dbm > floorDbm + rangeDb * 0.75f) ? qRgb(255, 0, 0) : qRgb(0, 255, 0);
    };
    const QImage& img = dss.image(QSize(DssRenderer::kCols, 400), 0, floorDbm,
                                  rangeDb, 1.0f, palette, 1, bg);
    const int x = DssRenderer::kCols / 2;
    const QColor farCurtain(img.pixel(x, 269));
    const QColor partial(img.pixel(x, 270));
    const QColor nearCrest(img.pixel(x, 271));
    const QColor nearCurtain(img.pixel(x, 272));
    QVERIFY2(farCurtain.red() > 200 && farCurtain.green() < 40,
             qPrintable(QStringLiteral("row 269 should be the far curtain, got %1").arg(farCurtain.name())));
    QVERIFY2(partial.red() > 90 && partial.green() > 100,
             qPrintable(QStringLiteral("row 270 must mix the near crest over the FAR curtain, got %1").arg(partial.name())));
    QVERIFY2(nearCrest.green() > 200 && nearCrest.red() > 100,
             qPrintable(QStringLiteral("row 271 should be the near crest over its own fill, got %1").arg(nearCrest.name())));
    QCOMPARE(nearCurtain, QColor(0, 255, 0));
}

// scaleStripPx rows stay transparent and the plot shrinks to fit above
// them; a strip as tall as the image leaves a one-row plot and no crash.
void TestDssRenderer::scaleStripStaysTransparent()
{
    DssRenderer dss;
    dss.pushRow(constantRow(DssRenderer::kCols, -50.0f));
    const QColor bg(10, 20, 30);
    auto palette = [](float) -> QRgb { return qRgb(255, 0, 0); };

    const QImage& img = dss.image(QSize(DssRenderer::kCols, 400), 100, -100.0f,
                                  50.0f, 1.0f, palette, 1, bg);
    for (int y = 300; y < 400; y += 9) {
        QCOMPARE(qAlpha(img.pixel(100, y)), 0);
        QCOMPARE(qAlpha(img.pixel(DssRenderer::kCols - 1, y)), 0);
    }
    QCOMPARE(qAlpha(img.pixel(100, 299)), 255);
    // plot height 300: ridge at 300 - 138 = 162 -> row 200 is curtain
    QCOMPARE(QColor(img.pixel(100, 200)), QColor(255, 0, 0));

    const QImage& tall = dss.image(QSize(DssRenderer::kCols, 400), 400, -100.0f,
                                   50.0f, 1.0f, palette, 2, bg);
    QCOMPARE(tall.size(), QSize(DssRenderer::kCols, 400));
    QCOMPARE(qAlpha(tall.pixel(100, 0)), 255);
    QCOMPARE(qAlpha(tall.pixel(100, 1)), 0);
    QCOMPARE(qAlpha(tall.pixel(100, 399)), 0);
}

QTEST_MAIN(TestDssRenderer)
#include "tst_dss_renderer.moc"
