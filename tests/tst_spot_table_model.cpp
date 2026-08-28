// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Test references real DXCC entity callsigns as
// fixtures. Precedent: B2-B6, C1-C4, D1.

#include <QtTest>

#include "models/SpotTableModel.h"
#include "models/BandFilterProxy.h"
#include "core/DxSpot.h"

using namespace Longpath;

class TestSpotTableModel : public QObject {
    Q_OBJECT
private slots:
    void initialState();
    void addSpotIncrementsRowCount();
    void columnCountIsTen();
    void dataRoundTrip();
    void referenceAndEntityRoundTrip();
    void freqAtRowMatchesSource();
    void setMaxSpotsBounds();
    void bandFilterHidesMatchingBand();
    void bandFilterShowsAfterUnhide();
    void modeFilterHidesMatchingMode();
    void searchTextMatchesAnyOfSeveralColumns();
    void distanceAndBearingBlankWithoutGrids();
    void distanceAndBearingComputedFromGrids();
    void watchTermMatchTintsBackground();
    void watchTermMatchKeepsDxCallReadable();
};

static DxSpot makeSpot(const QString& call, double mhz, const QString& band = {}) {
    Q_UNUSED(band);
    DxSpot s;
    s.dxCall = call;
    s.freqMhz = mhz;
    s.spotterCall = "TEST";
    s.comment = "fixture";
    s.utcTime = QTime(12, 0);
    s.source = "Cluster";
    return s;
}

void TestSpotTableModel::initialState() {
    SpotTableModel m;
    QCOMPARE(m.rowCount(), 0);
    QCOMPARE(m.columnCount(), int(SpotTableModel::ColCount));
}

void TestSpotTableModel::addSpotIncrementsRowCount() {
    SpotTableModel m;
    m.addSpot(makeSpot("W1AW", 14.250));
    QCOMPARE(m.rowCount(), 1);
}

void TestSpotTableModel::columnCountIsTen() {
    // 2026-08-26: was 8 before the NereusSDR-native ColReference /
    // ColEntity columns; 2026-08-27: 12 after ColDistance / ColBearing
    // (SpotHub POTA improvement pass) -- see SpotTableModel.h
    // modification history. Test name kept for history's sake even
    // though the count has moved on twice since.
    SpotTableModel m;
    QCOMPARE(m.columnCount(), 12);
}

void TestSpotTableModel::dataRoundTrip() {
    SpotTableModel m;
    m.addSpot(makeSpot("W1AW", 14.250));
    QCOMPARE(m.data(m.index(0, SpotTableModel::ColDxCall), Qt::DisplayRole).toString(),
             QStringLiteral("W1AW"));
}

void TestSpotTableModel::referenceAndEntityRoundTrip() {
    SpotTableModel m;
    DxSpot s = makeSpot("KB9LBE", 14.283);
    s.reference = "US-1772";
    s.entity = "US";
    m.addSpot(s);
    QCOMPARE(m.data(m.index(0, SpotTableModel::ColReference), Qt::DisplayRole).toString(),
             QStringLiteral("US-1772"));
    QCOMPARE(m.data(m.index(0, SpotTableModel::ColEntity), Qt::DisplayRole).toString(),
             QStringLiteral("US"));
    // Sources without a reference (e.g. plain DX cluster) leave both
    // columns empty rather than showing a placeholder.
    SpotTableModel m2;
    m2.addSpot(makeSpot("W1AW", 14.250));
    QVERIFY(m2.data(m2.index(0, SpotTableModel::ColReference), Qt::DisplayRole).toString().isEmpty());
    QVERIFY(m2.data(m2.index(0, SpotTableModel::ColEntity), Qt::DisplayRole).toString().isEmpty());
}

void TestSpotTableModel::freqAtRowMatchesSource() {
    SpotTableModel m;
    m.addSpot(makeSpot("W1AW", 14.250));
    QCOMPARE(m.freqAtRow(0), 14.250);
}

void TestSpotTableModel::setMaxSpotsBounds() {
    SpotTableModel m;
    m.setMaxSpots(3);
    for (int i = 0; i < 10; ++i)
        m.addSpot(makeSpot(QString("CALL%1").arg(i), 14.0 + i*0.001));
    QCOMPARE(m.rowCount(), 3);
}

void TestSpotTableModel::bandFilterHidesMatchingBand() {
    auto* src = new SpotTableModel;
    BandFilterProxy proxy;
    proxy.setSourceModel(src);

    src->addSpot(makeSpot("W1AW", 14.250));  // 20m
    src->addSpot(makeSpot("VK6APH", 7.150)); // 40m
    QCOMPARE(proxy.rowCount(), 2);

    proxy.setBandVisible("20m", false);
    QCOMPARE(proxy.rowCount(), 1);
}

void TestSpotTableModel::bandFilterShowsAfterUnhide() {
    auto* src = new SpotTableModel;
    BandFilterProxy proxy;
    proxy.setSourceModel(src);

    src->addSpot(makeSpot("W1AW", 14.250));
    proxy.setBandVisible("20m", false);
    QCOMPARE(proxy.rowCount(), 0);
    proxy.setBandVisible("20m", true);
    QCOMPARE(proxy.rowCount(), 1);
}

void TestSpotTableModel::modeFilterHidesMatchingMode() {
    // 2026-08-27 (operator-requested follow-up): mode filter mirrors
    // the band/entity filters, over SpotTableModel::ColMode.
    auto* src = new SpotTableModel;
    BandFilterProxy proxy;
    proxy.setSourceModel(src);

    DxSpot cw = makeSpot("W1AW", 14.250);
    cw.comment = "CW big signal";  // extractMode() reads the leading word
    DxSpot ssb = makeSpot("VK6APH", 7.150);
    ssb.comment = "SSB nice audio";
    src->addSpot(cw);
    src->addSpot(ssb);
    QCOMPARE(proxy.rowCount(), 2);

    proxy.setModeVisible("CW", false);
    QCOMPARE(proxy.rowCount(), 1);
    proxy.setModeVisible("CW", true);
    QCOMPARE(proxy.rowCount(), 2);
}

void TestSpotTableModel::searchTextMatchesAnyOfSeveralColumns() {
    // 2026-08-27 (operator-requested follow-up, SOTAwatch3-style
    // free-text box): unlike the exact-match predicates, this is a
    // loose OR-across-columns substring search, and matching via the
    // Reference column specifically needs the row to actually carry
    // one (Cluster spots don't).
    auto* src = new SpotTableModel;
    BandFilterProxy proxy;
    proxy.setSourceModel(src);

    DxSpot potaSpot = makeSpot("KB9LBE", 14.283);
    potaSpot.reference = "US-1772";
    potaSpot.comment = "Mark Twain State Park SSB";
    DxSpot other = makeSpot("VK6APH", 7.150);
    other.comment = "CW big signal";
    src->addSpot(potaSpot);
    src->addSpot(other);
    QCOMPARE(proxy.rowCount(), 2);

    proxy.setSearchText("us-1772");  // case-insensitive, matches Reference
    QCOMPARE(proxy.rowCount(), 1);

    proxy.setSearchText("twain");    // matches Comment
    QCOMPARE(proxy.rowCount(), 1);

    proxy.setSearchText("vk6aph");   // matches DxCall on the OTHER row
    QCOMPARE(proxy.rowCount(), 1);

    proxy.setSearchText("");         // empty -> no filtering
    QCOMPARE(proxy.rowCount(), 2);
}

void TestSpotTableModel::distanceAndBearingBlankWithoutGrids() {
    // 2026-08-27 (operator-requested follow-up): Dist/Brg need both
    // the operator's own grid (setOurGridSquare) and the spot's own
    // (DxSpot::grid) -- blank whenever either is missing.
    SpotTableModel m;
    DxSpot noGrid = makeSpot("W1AW", 14.250);
    m.addSpot(noGrid);
    QVERIFY(m.data(m.index(0, SpotTableModel::ColDistance), Qt::DisplayRole).toString().isEmpty());
    QVERIFY(m.data(m.index(0, SpotTableModel::ColBearing), Qt::DisplayRole).toString().isEmpty());

    m.setOurGridSquare("JN67VV");  // operator grid known, spot grid still isn't
    QVERIFY(m.data(m.index(0, SpotTableModel::ColDistance), Qt::DisplayRole).toString().isEmpty());
    QVERIFY(m.data(m.index(0, SpotTableModel::ColBearing), Qt::DisplayRole).toString().isEmpty());
}

void TestSpotTableModel::distanceAndBearingComputedFromGrids() {
    SpotTableModel m;
    DxSpot spot = makeSpot("W1AW", 14.250);
    spot.grid = "FN31pr";  // Newington, CT -- real W1AW grid square
    m.addSpot(spot);

    // No operator grid yet -> still blank.
    QVERIFY(m.data(m.index(0, SpotTableModel::ColDistance), Qt::DisplayRole).toString().isEmpty());

    m.setOurGridSquare("JN67VV");  // Oberwart, AT area (this session's OE5SOS grid)
    const QString distStr = m.data(m.index(0, SpotTableModel::ColDistance), Qt::DisplayRole).toString();
    const QString brgStr  = m.data(m.index(0, SpotTableModel::ColBearing), Qt::DisplayRole).toString();
    QVERIFY(!distStr.isEmpty());
    bool ok = false;
    const int distKm = distStr.toInt(&ok);
    QVERIFY(ok);
    QVERIFY(distKm > 5000 && distKm < 9000);  // Austria <-> Connecticut, sanity range
    QVERIFY(brgStr.contains(QChar(0x00B0)));   // degree sign
    bool degOk = false;
    const int brgDeg = brgStr.left(3).toInt(&degOk);
    QVERIFY(degOk);
    QVERIFY(brgDeg > 250 && brgDeg < 350);  // broadly westerly, Austria -> New England

    // setOurGridSquare must refresh already-inserted rows, not just future ones.
    m.setOurGridSquare("");
    QVERIFY(m.data(m.index(0, SpotTableModel::ColDistance), Qt::DisplayRole).toString().isEmpty());
}

void TestSpotTableModel::watchTermMatchTintsBackground() {
    SpotTableModel m;
    DxSpot watched = makeSpot("W1AW", 14.250);
    watched.reference = "US-1772";
    DxSpot other = makeSpot("VK6APH", 7.150);
    m.addSpot(other);   // row 1 after the next addSpot (prepend)
    m.addSpot(watched); // row 0

    // No terms/color configured yet -> no highlight anywhere.
    QVERIFY(!m.data(m.index(0, SpotTableModel::ColDxCall), Qt::BackgroundRole).isValid());

    const QColor watchColor(0x00, 0xb4, 0xd8);
    m.setWatchColor(watchColor);
    m.setWatchTerms({"US-1772"});  // matches by reference, not callsign
    QCOMPARE(m.data(m.index(0, SpotTableModel::ColDxCall), Qt::BackgroundRole).value<QColor>(),
             watchColor);
    // The non-matching row stays untinted.
    QVERIFY(!m.data(m.index(1, SpotTableModel::ColDxCall), Qt::BackgroundRole).isValid());

    // Matching by callsign works too.
    m.setWatchTerms({"vk6aph"});  // case-insensitive
    QCOMPARE(m.data(m.index(1, SpotTableModel::ColDxCall), Qt::BackgroundRole).value<QColor>(),
             watchColor);
    QVERIFY(!m.data(m.index(0, SpotTableModel::ColDxCall), Qt::BackgroundRole).isValid());
}

void TestSpotTableModel::watchTermMatchKeepsDxCallReadable() {
    // 2026-08-27: caught on first live test -- the default watchlist
    // highlight color and the DxCall column's fixed accent foreground
    // color were both #00b4d8, making the callsign invisible in a
    // matched row. A match must switch DxCall's foreground to the
    // dark text used elsewhere for text-on-accent-background.
    SpotTableModel m;
    DxSpot watched = makeSpot("W1AW", 14.250);
    m.addSpot(watched);

    const QColor accentCyan(0x00, 0xb4, 0xd8);
    QCOMPARE(m.data(m.index(0, SpotTableModel::ColDxCall), Qt::ForegroundRole).value<QColor>(),
             accentCyan);  // unmatched: still the normal accent color

    m.setWatchColor(accentCyan);   // the actual default watch color
    m.setWatchTerms({"W1AW"});
    const QVariant fg = m.data(m.index(0, SpotTableModel::ColDxCall), Qt::ForegroundRole);
    QVERIFY(fg.value<QColor>() != accentCyan);  // must not collide with the background
}

QTEST_GUILESS_MAIN(TestSpotTableModel)
#include "tst_spot_table_model.moc"
