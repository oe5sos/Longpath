// The logbook as Google Earth sees it. These pin the details that make
// a KML wrong quietly: coordinate order (longitude first), band
// folders, timestamps for the time slider, HTML escaping in what came
// from the log, and honest counting of what could not be placed.
// no-port-check: NereusSDR-original.

#include <QtTest/QtTest>
#include <QTimeZone>

#include "core/KmlExport.h"

using namespace NereusSDR;

namespace {

LogEntry entry(const char* call, const char* grid, const char* band,
               const char* mode = "SSB")
{
    LogEntry e;
    e.call = QString::fromLatin1(call);
    e.gridSquare = QString::fromLatin1(grid);
    e.band = QString::fromLatin1(band);
    e.mode = QString::fromLatin1(mode);
    e.timeOn = QDateTime(QDate(2026, 8, 10), QTime(12, 0, 0),
                         QTimeZone::UTC);
    return e;
}

} // namespace

class TstKmlExport : public QObject {
    Q_OBJECT
private slots:
    void coordinates_are_longitude_first();
    void bands_become_folders_with_counts();
    void timestamps_feed_the_time_slider();
    void log_text_is_escaped();
    void unplaceable_contacts_are_counted_not_invented();
    void fallback_positions_are_marked_as_guesses();
    void home_grid_adds_lines_and_a_home_pin();
    void an_empty_export_fails_instead_of_writing_a_blank_earth();
};

void TstKmlExport::coordinates_are_longitude_first()
{
    KmlExport::Options opt;
    const KmlExport::Result r =
        KmlExport::toKml({entry("OE1W", "JN88AA", "40m")}, opt);
    QCOMPARE(r.placed, 1);

    // JN88AA: longitude ≈ 16.04 E, latitude ≈ 48.02 N. If these arrive
    // the other way round, every pin lands in the Indian Ocean off
    // Somalia — the classic lat/lon swap, and the reason this test
    // checks values, not just structure.
    QVERIFY(r.kml.contains(QStringLiteral("<coordinates>16.")));
    QVERIFY(!r.kml.contains(QStringLiteral("<coordinates>48.")));
}

void TstKmlExport::bands_become_folders_with_counts()
{
    KmlExport::Options opt;
    const KmlExport::Result r = KmlExport::toKml(
        {entry("OE1A", "JN88AA", "40m"), entry("OE1B", "JN88AB", "40m"),
         entry("OE1C", "JN88AC", "20m")}, opt);
    QCOMPARE(r.placed, 3);
    QVERIFY(r.kml.contains(QStringLiteral("<Folder><name>40m (2)</name>")));
    QVERIFY(r.kml.contains(QStringLiteral("<Folder><name>20m (1)</name>")));

    // Dial order: 40m (7 MHz) before 20m (14 MHz).
    QVERIFY(r.kml.indexOf(QStringLiteral("40m (2)"))
            < r.kml.indexOf(QStringLiteral("20m (1)")));
}

void TstKmlExport::timestamps_feed_the_time_slider()
{
    KmlExport::Options opt;
    const KmlExport::Result r =
        KmlExport::toKml({entry("OE1W", "JN88AA", "40m")}, opt);
    QVERIFY(r.kml.contains(
        QStringLiteral("<TimeStamp><when>2026-08-10T12:00:00Z</when>")));
}

void TstKmlExport::log_text_is_escaped()
{
    LogEntry e = entry("OE1W", "JN88AA", "40m");
    e.name = QStringLiteral("A <B> & C");
    KmlExport::Options opt;
    const KmlExport::Result r = KmlExport::toKml({e}, opt);
    // Inside the CDATA description the text is HTML — the brackets from
    // the log must arrive as entities or they become (broken) markup.
    QVERIFY(r.kml.contains(QStringLiteral("A &lt;B&gt; &amp; C")));
}

void TstKmlExport::unplaceable_contacts_are_counted_not_invented()
{
    KmlExport::Options opt;   // no fallback
    const KmlExport::Result r = KmlExport::toKml(
        {entry("OE1A", "JN88AA", "40m"), entry("OE1B", "", "40m")}, opt);
    QCOMPARE(r.placed, 1);
    QCOMPARE(r.skipped, 1);
    QVERIFY(!r.kml.contains(QStringLiteral("OE1B")));
}

void TstKmlExport::fallback_positions_are_marked_as_guesses()
{
    KmlExport::Options opt;
    opt.fallback = [](const QString&, double& lat, double& lon) {
        lat = 36.0; lon = 138.0;   // middle of Japan
        return true;
    };
    const KmlExport::Result r =
        KmlExport::toKml({entry("JA1ABC", "", "20m")}, opt);
    QCOMPARE(r.placed, 1);
    // The tilde is the map's own convention for "country, not locator".
    QVERIFY(r.kml.contains(QStringLiteral("<name>~JA1ABC</name>")));
}

void TstKmlExport::home_grid_adds_lines_and_a_home_pin()
{
    KmlExport::Options opt;
    opt.myGrid = QStringLiteral("JN78AB");
    const KmlExport::Result r =
        KmlExport::toKml({entry("OE1W", "JN88AA", "40m")}, opt);
    QVERIFY(r.kml.contains(QStringLiteral("<LineString><tessellate>1")));
    QVERIFY(r.kml.contains(QStringLiteral("HOME JN78AB")));

    // Without a home grid: pins, no lines, no phantom home.
    KmlExport::Options bare;
    const KmlExport::Result r2 =
        KmlExport::toKml({entry("OE1W", "JN88AA", "40m")}, bare);
    QVERIFY(!r2.kml.contains(QStringLiteral("<LineString>")));
    QVERIFY(!r2.kml.contains(QStringLiteral("HOME")));
}

void TstKmlExport::an_empty_export_fails_instead_of_writing_a_blank_earth()
{
    KmlExport::Options opt;
    QString err;
    KmlExport::Result res;
    QVERIFY(!KmlExport::writeKml(QStringLiteral("/nonexistent/x.kml"),
                                 {entry("OE1B", "", "40m")}, opt, &err,
                                 &res));
    QCOMPARE(res.placed, 0);
    QVERIFY(!err.isEmpty());
}

QTEST_APPLESS_MAIN(TstKmlExport)
#include "tst_kml_export.moc"
