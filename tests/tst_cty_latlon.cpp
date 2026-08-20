// Verify that CtyDatParser now captures entity coordinates, that the
// cty.dat positive-west longitude convention is flipped to positive
// east on read, and that gridSquareFromLatLon round-trips.
// no-port-check: CtyDatParser ported from AetherSDR (attributed in the
// file); the lat/lon capture and the grid encoder are NereusSDR
// additions.

#include <QtTest/QtTest>
#include <QTemporaryFile>
#include "core/CtyDatParser.h"
#include "core/Maidenhead.h"

using namespace Longpath;

namespace {

// Two real cty.dat header lines. Note the longitude column: cty.dat
// writes it POSITIVE WEST, so Austria (14.5 deg EAST) appears as
// -14.50 and Canada (95 deg WEST) as +95.00.
const char* kSample =
    "Austria:                  15:  28:  EU:   47.33:   -13.33:    -1.0:  OE:\n"
    "    OE;\n"
    "Canada:                    5:  09:  NA:   44.35:    78.75:     5.0:  VE:\n"
    "    VE,VA;\n"
    "England:                  14:  27:  EU:   52.17:     0.00:     0.0:  G:\n"
    "    G,M,2E;\n";

QString writeSample()
{
    auto* f = new QTemporaryFile;
    f->setAutoRemove(false);
    f->open();
    f->write(kSample);
    f->close();
    return f->fileName();
}

} // namespace

class TstCtyLatLon : public QObject {
    Q_OBJECT
private slots:
    void entity_coordinates_are_captured();
    void longitude_is_flipped_to_positive_east();
    void grid_square_from_latlon_round_trips();
    void grid_from_latlon_handles_both_hemispheres();
    void callsign_resolves_to_entity_with_position();
};

void TstCtyLatLon::entity_coordinates_are_captured()
{
    CtyDatParser p;
    QVERIFY(p.loadFromFile(writeSample()));

    const DxccEntity* oe = p.entityByPrefix(QStringLiteral("OE"));
    QVERIFY(oe);
    QVERIFY(oe->hasLatLon);
    QVERIFY(qAbs(oe->latitude - 47.33) < 0.01);
}

void TstCtyLatLon::longitude_is_flipped_to_positive_east()
{
    CtyDatParser p;
    QVERIFY(p.loadFromFile(writeSample()));

    // Austria is EAST of Greenwich. cty.dat says -13.33 (positive west),
    // so after the flip it must be +13.33. Getting this backwards
    // mirrors every bearing about the prime meridian.
    const DxccEntity* oe = p.entityByPrefix(QStringLiteral("OE"));
    QVERIFY(oe);
    QVERIFY2(oe->longitude > 0,
             qPrintable(QStringLiteral("Austria should be east, got %1")
                            .arg(oe->longitude)));
    QVERIFY(qAbs(oe->longitude - 13.33) < 0.01);

    // Canada is WEST. cty.dat says +78.75, so it must become negative.
    const DxccEntity* ve = p.entityByPrefix(QStringLiteral("VE"));
    QVERIFY(ve);
    QVERIFY2(ve->longitude < 0,
             qPrintable(QStringLiteral("Canada should be west, got %1")
                            .arg(ve->longitude)));
}

void TstCtyLatLon::grid_square_from_latlon_round_trips()
{
    // Linz is JN78. Encode its centre and decode it again; the position
    // must come back within the size of a sub-square.
    const QString grid = gridSquareFromLatLon(48.30, 14.29);
    QVERIFY2(grid.startsWith(QStringLiteral("JN78")),
             qPrintable(QStringLiteral("got %1").arg(grid)));
    QVERIFY(isValidGridSquare(grid));

    double lat = 0, lon = 0;
    calculateLatLonFromGridSquare(grid, lat, lon);
    QVERIFY(qAbs(lat - 48.30) < 0.05);
    QVERIFY(qAbs(lon - 14.29) < 0.10);
}

void TstCtyLatLon::grid_from_latlon_handles_both_hemispheres()
{
    // Sydney: southern and eastern.
    QVERIFY(gridSquareFromLatLon(-33.87, 151.21).startsWith(QStringLiteral("QF56")));
    // New York: northern and western — the sign case a naive encoder
    // gets wrong.
    QVERIFY(gridSquareFromLatLon(40.71, -74.01).startsWith(QStringLiteral("FN20")));
    // Every output must be a locator the rest of the code accepts.
    for (double la : {-89.0, -45.0, 0.0, 45.0, 89.0}) {
        for (double lo : {-179.0, -90.0, 0.0, 90.0, 179.0}) {
            QVERIFY2(isValidGridSquare(gridSquareFromLatLon(la, lo)),
                     qPrintable(QStringLiteral("%1,%2 -> %3")
                                    .arg(la).arg(lo)
                                    .arg(gridSquareFromLatLon(la, lo))));
        }
    }
}

void TstCtyLatLon::callsign_resolves_to_entity_with_position()
{
    CtyDatParser p;
    QVERIFY(p.loadFromFile(writeSample()));

    const QString prefix = p.resolvePrimaryPrefix(QStringLiteral("OE5SOS"));
    QCOMPARE(prefix, QStringLiteral("OE"));

    const DxccEntity* ent = p.entityByPrefix(prefix);
    QVERIFY(ent && ent->hasLatLon);

    // The full chain the rotator dial walks: callsign to a bearing.
    const QString entGrid = gridSquareFromLatLon(ent->latitude, ent->longitude);
    const double deg = calculateBearingInDegrees(QStringLiteral("JN67VV"),
                                                 entGrid);
    QVERIFY(deg >= 0.0 && deg < 360.0);
}

QTEST_APPLESS_MAIN(TstCtyLatLon)
#include "tst_cty_latlon.moc"
