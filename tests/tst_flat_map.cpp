// The flat map's one piece of real geometry: a great circle sampled in
// longitude/latitude, and the split where it crosses the date line.
// Drawn without that split, a path from Japan to California streaks all
// the way back across the map — the single most recognisable way a
// world map looks broken.
// no-port-check: NereusSDR-original.

#include <QtTest/QtTest>

#include "gui/widgets/FlatMapWidget.h"

#include <cmath>

using namespace NereusSDR;

namespace {

// Longitudes stay inside [-180, 180] after normalisation.
bool inRange(const QVector<QPointF>& pts)
{
    for (const QPointF& p : pts) {
        if (p.x() < -180.0 || p.x() > 180.0) { return false; }
        if (p.y() < -90.0  || p.y() > 90.0)  { return false; }
    }
    return true;
}

int totalPoints(const QVector<QVector<QPointF>>& runs)
{
    int n = 0;
    for (const QVector<QPointF>& r : runs) { n += r.size(); }
    return n;
}

} // namespace

class TstFlatMap : public QObject {
    Q_OBJECT
private slots:
    void samples_start_and_end_where_asked();
    void samples_stay_in_range();
    void same_place_gives_one_sample();
    void a_path_not_crossing_the_date_line_is_one_run();
    void a_path_crossing_the_date_line_is_split();
    void splitting_keeps_every_point();
    void each_run_has_no_internal_jump();
    void empty_input_is_no_runs();
    void a_pacific_path_curves_polewards();
};

void TstFlatMap::samples_start_and_end_where_asked()
{
    const auto s = FlatMapWidget::greatCircleSamples(48.3, 14.3,
                                                     40.9, -73.3, 32);
    QCOMPARE(s.size(), 33);
    QVERIFY(std::abs(s.first().y() - 48.3) < 0.01);
    QVERIFY(std::abs(s.first().x() - 14.3) < 0.01);
    QVERIFY(std::abs(s.last().y() - 40.9) < 0.01);
    QVERIFY(std::abs(s.last().x() + 73.3) < 0.01);
}

void TstFlatMap::samples_stay_in_range()
{
    // Including a path that runs the long way round the world.
    QVERIFY(inRange(FlatMapWidget::greatCircleSamples(35.7, 139.7,
                                                      37.8, -122.4, 64)));
    QVERIFY(inRange(FlatMapWidget::greatCircleSamples(-33.9, 151.2,
                                                      48.3, 14.3, 64)));
    QVERIFY(inRange(FlatMapWidget::greatCircleSamples(89.0, 0.0,
                                                      -89.0, 180.0, 64)));
}

void TstFlatMap::same_place_gives_one_sample()
{
    // Working somebody in your own grid: the slerp would divide by the
    // sine of a zero angle.
    const auto s = FlatMapWidget::greatCircleSamples(48.3, 14.3,
                                                     48.3, 14.3, 32);
    QCOMPARE(s.size(), 1);
    QVERIFY(!std::isnan(s.first().x()));
    QVERIFY(!std::isnan(s.first().y()));
}

void TstFlatMap::a_path_not_crossing_the_date_line_is_one_run()
{
    const auto s = FlatMapWidget::greatCircleSamples(48.3, 14.3,
                                                     40.9, -73.3, 64);
    const auto runs = FlatMapWidget::splitAtAntimeridian(s);
    QCOMPARE(runs.size(), 1);
}

void TstFlatMap::a_path_crossing_the_date_line_is_split()
{
    // Tokyo to San Francisco goes across the Pacific, over 180.
    const auto s = FlatMapWidget::greatCircleSamples(35.7, 139.7,
                                                     37.8, -122.4, 64);
    const auto runs = FlatMapWidget::splitAtAntimeridian(s);
    QCOMPARE(runs.size(), 2);
    QVERIFY(runs.at(0).size() > 1);
    QVERIFY(runs.at(1).size() > 1);

    // The break really is at the edge of the world, not somewhere in
    // the middle of the ocean.
    QVERIFY2(std::abs(runs.at(0).last().x()) > 150.0,
             qPrintable(QStringLiteral("%1").arg(runs.at(0).last().x())));
    QVERIFY2(std::abs(runs.at(1).first().x()) > 150.0,
             qPrintable(QStringLiteral("%1").arg(runs.at(1).first().x())));
}

void TstFlatMap::splitting_keeps_every_point()
{
    // Splitting must not lose samples — a dropped point is a notch in
    // the line that is easy to mistake for a rendering artefact.
    for (auto pair : {std::pair{139.7, -122.4}, std::pair{14.3, -73.3},
                      std::pair{-73.3, 151.2}}) {
        const auto s = FlatMapWidget::greatCircleSamples(35.7, pair.first,
                                                         37.8, pair.second, 64);
        QCOMPARE(totalPoints(FlatMapWidget::splitAtAntimeridian(s)), s.size());
    }
}

void TstFlatMap::each_run_has_no_internal_jump()
{
    const auto s = FlatMapWidget::greatCircleSamples(35.7, 139.7,
                                                     37.8, -122.4, 96);
    for (const auto& run : FlatMapWidget::splitAtAntimeridian(s)) {
        for (int i = 1; i < run.size(); ++i) {
            const double jump = std::abs(run.at(i).x() - run.at(i - 1).x());
            QVERIFY2(jump < 180.0,
                     qPrintable(QStringLiteral("jump of %1").arg(jump)));
        }
    }
}

void TstFlatMap::empty_input_is_no_runs()
{
    QVERIFY(FlatMapWidget::splitAtAntimeridian({}).isEmpty());
}

void TstFlatMap::a_pacific_path_curves_polewards()
{
    // Tokyo to San Francisco is a northern arc; both ends sit near 36-38
    // degrees but the middle goes well above 45. If the samples came out
    // as a straight interpolation the map would draw a route across the
    // mid Pacific that no signal takes.
    const auto s = FlatMapWidget::greatCircleSamples(35.7, 139.7,
                                                     37.8, -122.4, 64);
    double maxLat = -90.0;
    for (const QPointF& p : s) { maxLat = std::max(maxLat, p.y()); }
    QVERIFY2(maxLat > 45.0,
             qPrintable(QStringLiteral("peak latitude %1").arg(maxLat)));
}

QTEST_MAIN(TstFlatMap)
#include "tst_flat_map.moc"
