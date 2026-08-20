// =================================================================
// src/core/KmlExport.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see KmlExport.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-10 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "KmlExport.h"

#include "core/AdifLog.h"
#include "core/Maidenhead.h"

#include <QFile>
#include <QMap>
#include <QTimeZone>

#include <algorithm>

namespace Longpath {
namespace KmlExport {

namespace {

// KML colours are aabbggrr — the reverse of HTML's rrggbb, a detail
// that has produced a generation of accidentally pink maps.
QString bandColour(const QString& band)
{
    static const QMap<QString, QString> kColours = {
        {QStringLiteral("160m"), QStringLiteral("ff4040c0")},  // dark red
        {QStringLiteral("80m"),  QStringLiteral("ff4070ff")},  // orange-red
        {QStringLiteral("60m"),  QStringLiteral("ff40a0ff")},  // orange
        {QStringLiteral("40m"),  QStringLiteral("ff40d0ff")},  // amber
        {QStringLiteral("30m"),  QStringLiteral("ff40ffff")},  // yellow
        {QStringLiteral("20m"),  QStringLiteral("ff40ff40")},  // green
        {QStringLiteral("17m"),  QStringLiteral("ffb0ff40")},  // teal-green
        {QStringLiteral("15m"),  QStringLiteral("ffffd040")},  // cyan-blue
        {QStringLiteral("12m"),  QStringLiteral("ffff8040")},  // blue
        {QStringLiteral("10m"),  QStringLiteral("ffff40a0")},  // violet
        {QStringLiteral("6m"),   QStringLiteral("ffff40ff")},  // magenta
        {QStringLiteral("2m"),   QStringLiteral("ffc040ff")},  // pink
    };
    return kColours.value(band.trimmed().toLower(),
                          QStringLiteral("ffcccccc"));  // grey: unknown band
}

QString esc(const QString& s)
{
    return s.toHtmlEscaped();
}

// "lon,lat,0" — KML wants longitude first, which is the other classic
// way a map export goes quietly wrong.
QString coord(double lat, double lon)
{
    return QStringLiteral("%1,%2,0")
        .arg(lon, 0, 'f', 5).arg(lat, 0, 'f', 5);
}

QString describe(const LogEntry& e)
{
    // A small HTML table, because this is what the operator sees when
    // they click the pin — it should read like a QSL card, not a dump.
    QString rows;
    auto row = [&rows](const QString& k, const QString& v) {
        if (v.trimmed().isEmpty()) { return; }
        rows += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td></tr>")
                    .arg(k, esc(v));
    };

    const QDateTime utc = e.timeOn.toUTC();
    if (utc.isValid()) {
        row(QStringLiteral("When"),
            utc.toString(QStringLiteral("yyyy-MM-dd hh:mm")) +
                QStringLiteral(" UTC"));
    }
    row(QStringLiteral("Name"), e.name);
    row(QStringLiteral("QTH"), e.qth);
    row(QStringLiteral("Country"), e.country);
    row(QStringLiteral("Locator"), e.gridSquare);
    row(QStringLiteral("Band"), e.band);
    row(QStringLiteral("Mode"),
        e.submode.isEmpty() ? e.mode
                            : e.mode + QStringLiteral(" / ") + e.submode);
    if (!e.rstSent.trimmed().isEmpty() || !e.rstRcvd.trimmed().isEmpty()) {
        row(QStringLiteral("RST"),
            QStringLiteral("sent %1 · rcvd %2").arg(e.rstSent, e.rstRcvd));
    }
    if (e.freqMHz > 0.0) {
        row(QStringLiteral("Freq"),
            QStringLiteral("%1 MHz").arg(e.freqMHz, 0, 'f', 4));
    }
    if (e.distanceKm > 0.0) {
        row(QStringLiteral("Distance"),
            QStringLiteral("%1 km · %2°")
                .arg(e.distanceKm, 0, 'f', 0)
                .arg(e.bearingDeg, 0, 'f', 0));
    }
    row(QStringLiteral("Comment"), e.comment);

    return QStringLiteral("<![CDATA[<table>%1</table>]]>").arg(rows);
}

} // namespace

Result toKml(const QVector<LogEntry>& entries, const Options& opt)
{
    Result r;

    const QString myGrid = opt.myGrid.trimmed().toUpper();
    const bool haveHome = isValidGridSquare(myGrid);
    double homeLat = 0.0, homeLon = 0.0;
    if (haveHome) {
        calculateLatLonFromGridSquare(myGrid, homeLat, homeLon);
    }

    // Band → placemark blocks. QMap so the iteration order is stable;
    // the sidebar order itself is fixed afterwards by frequency, via
    // AdifLog::bandSortKeyMHz — "10m, 160m, 17m" is the alphabetical
    // trap the sort key exists to avoid.
    QMap<QString, QStringList> byBand;
    QMap<QString, int> countByBand;

    for (const LogEntry& e : entries) {
        if (!e.isValid()) { continue; }

        double lat = 0.0, lon = 0.0;
        bool approximate = false;
        if (isValidGridSquare(e.gridSquare)) {
            calculateLatLonFromGridSquare(e.gridSquare.trimmed().toUpper(),
                                          lat, lon);
        } else if (opt.fallback && opt.fallback(e.call, lat, lon)) {
            approximate = true;
        } else {
            ++r.skipped;
            continue;
        }

        const QString band = e.band.trimmed().isEmpty()
            ? QStringLiteral("no band") : e.band.trimmed().toLower();

        QString geometry;
        if (haveHome) {
            // Two endpoints only: with tessellate on, Google Earth bends
            // the segment along the surface itself, so sampling the
            // great circle here would just bloat the file.
            geometry = QStringLiteral(
                "<MultiGeometry><Point><coordinates>%1</coordinates>"
                "</Point><LineString><tessellate>1</tessellate>"
                "<coordinates>%2 %1</coordinates></LineString>"
                "</MultiGeometry>")
                .arg(coord(lat, lon), coord(homeLat, homeLon));
        } else {
            geometry = QStringLiteral(
                "<Point><coordinates>%1</coordinates></Point>")
                .arg(coord(lat, lon));
        }

        const QDateTime utc = e.timeOn.toUTC();
        const QString when = utc.isValid()
            ? QStringLiteral("<TimeStamp><when>%1</when></TimeStamp>")
                  .arg(utc.toString(Qt::ISODate))
            : QString{};

        const QString name = approximate
            ? QStringLiteral("~") + e.call.trimmed().toUpper()
            : e.call.trimmed().toUpper();

        byBand[band] << QStringLiteral(
            "<Placemark><name>%1</name>%2"
            "<styleUrl>#band-%3</styleUrl>"
            "<description>%4</description>%5</Placemark>")
            .arg(esc(name), when, esc(band), describe(e), geometry);
        ++countByBand[band];
        ++r.placed;
    }

    if (r.placed == 0) { return r; }

    QString body;
    for (const QString& band : byBand.keys()) {
        body += QStringLiteral(
            "<Style id=\"band-%1\">"
            "<IconStyle><color>%2</color><scale>0.9</scale>"
            "<Icon><href>http://maps.google.com/mapfiles/kml/shapes/"
            "placemark_circle.png</href></Icon></IconStyle>"
            "<LineStyle><color>%2</color><width>1.5</width></LineStyle>"
            "<LabelStyle><scale>0.7</scale></LabelStyle>"
            "</Style>")
            .arg(esc(band), bandColour(band));
    }

    // Bands in dial order, not string order.
    QStringList bands = byBand.keys();
    std::sort(bands.begin(), bands.end(),
              [](const QString& a, const QString& b) {
        return AdifLog::bandSortKeyMHz(a) < AdifLog::bandSortKeyMHz(b);
    });

    for (const QString& band : bands) {
        body += QStringLiteral("<Folder><name>%1 (%2)</name>%3</Folder>")
            .arg(esc(band))
            .arg(countByBand.value(band))
            .arg(byBand.value(band).join(QString{}));
    }

    if (haveHome) {
        body += QStringLiteral(
            "<Placemark><name>HOME %1</name>"
            "<Style><IconStyle><color>ff40ff40</color>"
            "<Icon><href>http://maps.google.com/mapfiles/kml/shapes/"
            "ranger_station.png</href></Icon></IconStyle></Style>"
            "<Point><coordinates>%2</coordinates></Point></Placemark>")
            .arg(esc(myGrid), coord(homeLat, homeLon));
    }

    r.kml = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<kml xmlns=\"http://www.opengis.net/kml/2.2\">"
        "<Document><name>Longpath logbook</name>%1</Document></kml>\n")
        .arg(body);
    return r;
}

bool writeKml(const QString& path, const QVector<LogEntry>& entries,
              const Options& opt, QString* error, Result* result)
{
    const Result r = toKml(entries, opt);
    if (result) { *result = r; }

    if (r.placed == 0) {
        if (error) {
            *error = QStringLiteral(
                "None of these contacts carries a locator, so there is "
                "nothing to place on the Earth.");
        }
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) { *error = f.errorString(); }
        return false;
    }
    f.write(r.kml.toUtf8());
    f.close();
    return true;
}

} // namespace KmlExport
} // namespace Longpath
