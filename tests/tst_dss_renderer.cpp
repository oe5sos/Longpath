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

QTEST_MAIN(TestDssRenderer)
#include "tst_dss_renderer.moc"
