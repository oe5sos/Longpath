// =================================================================
// tests/tst_lan_discovery_regex.cpp  (Longpath)
// =================================================================
//
// Unit tests for LanDiscovery regex parsing and deduplication.
// Verifies that the official FlexRadio announcement format is
// parsed correctly, malformed lines are rejected, and duplicate
// serials are deduplicated.

#include <QtTest>
#include "core/LanDiscovery.h"

class LanDiscoveryRegexTest : public QObject {
    Q_OBJECT
private slots:
    void parsesValidAnnouncement();
    void rejectsMalformedLine();
    void dedupsBySerial();
    void acceptsExtraFieldFromNewerFirmware();
    void acceptsReorderedFields();
    void stillRejectsAnnouncementWithoutSerial();
};

void LanDiscoveryRegexTest::parsesValidAnnouncement() {
    Longpath::LanDiscovery d;
    QSignalSpy spy(&d, &Longpath::LanDiscovery::deviceDiscovered);
    d.injectDatagramForTesting("PowerGeniusXL ip=192.168.1.43 v=3.8.9 serial=PGXL5678 nickname=ShackAmp");
    QCOMPARE(spy.count(), 1);
    auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("PowerGeniusXL"));
    QCOMPARE(args.at(1).toString(), QString("192.168.1.43"));
    QCOMPARE(args.at(3).toString(), QString("3.8.9"));
    QCOMPARE(args.at(4).toString(), QString("PGXL5678"));
    QCOMPARE(args.at(5).toString(), QString("ShackAmp"));
}

void LanDiscoveryRegexTest::rejectsMalformedLine() {
    Longpath::LanDiscovery d;
    QSignalSpy spy(&d, &Longpath::LanDiscovery::deviceDiscovered);
    d.injectDatagramForTesting("garbage with no fields");
    d.injectDatagramForTesting("PowerGeniusXL ip=invalid v=3.8.9 serial=X nickname=Y");
    QCOMPARE(spy.count(), 0);
}

void LanDiscoveryRegexTest::dedupsBySerial() {
    Longpath::LanDiscovery d;
    QSignalSpy spy(&d, &Longpath::LanDiscovery::deviceDiscovered);
    QString line = "PowerGeniusXL ip=192.168.1.43 v=3.8.9 serial=PGXL5678 nickname=ShackAmp";
    d.injectDatagramForTesting(line);
    d.injectDatagramForTesting(line);
    d.injectDatagramForTesting(line);
    QCOMPARE(spy.count(), 1);
}

// Tuner Genius XL firmware 1.2.44 appends a field to the announcement.
// The old anchored regex ended in nickname=(?<nick>\S+)$ and therefore
// dropped the whole datagram, leaving the tuner undiscoverable -- the
// bug upstream NereusSDR fixed after its 2026.10.0 release and that we
// inherited with the regex.  Counter-check: run this case against the
// previous parseAnnouncement and it fails on spy.count() == 0.
void LanDiscoveryRegexTest::acceptsExtraFieldFromNewerFirmware() {
    Longpath::LanDiscovery d;
    QSignalSpy spy(&d, &Longpath::LanDiscovery::deviceDiscovered);
    d.injectDatagramForTesting(
        "TunerGeniusXL ip=192.168.1.44 v=1.2.44 serial=TGXL1234 "
        "nickname=ShackTuner model_variant=2");
    QCOMPARE(spy.count(), 1);
    auto args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("TunerGeniusXL"));
    QCOMPARE(args.at(1).toString(), QString("192.168.1.44"));
    QCOMPARE(args.at(3).toString(), QString("1.2.44"));
    QCOMPARE(args.at(4).toString(), QString("TGXL1234"));
    QCOMPARE(args.at(5).toString(), QString("ShackTuner"));
}

// Field order is the firmware's business, not ours.
void LanDiscoveryRegexTest::acceptsReorderedFields() {
    Longpath::LanDiscovery d;
    QSignalSpy spy(&d, &Longpath::LanDiscovery::deviceDiscovered);
    d.injectDatagramForTesting(
        "TunerGeniusXL serial=TGXL9999 nickname=Tuner2 ip=10.0.0.7 v=1.2.44");
    QCOMPARE(spy.count(), 1);
    auto args = spy.takeFirst();
    QCOMPARE(args.at(1).toString(), QString("10.0.0.7"));
    QCOMPARE(args.at(4).toString(), QString("TGXL9999"));
}

// Tolerant does not mean indiscriminate: without a serial there is
// nothing to deduplicate on and nothing to pair with, so the datagram
// is still refused.
void LanDiscoveryRegexTest::stillRejectsAnnouncementWithoutSerial() {
    Longpath::LanDiscovery d;
    QSignalSpy spy(&d, &Longpath::LanDiscovery::deviceDiscovered);
    d.injectDatagramForTesting("TunerGeniusXL ip=192.168.1.44 v=1.2.44 nickname=NoSerial");
    d.injectDatagramForTesting("ip=192.168.1.44 v=1.2.44 serial=NoModel nickname=X");
    QCOMPARE(spy.count(), 0);
}

QTEST_GUILESS_MAIN(LanDiscoveryRegexTest)
#include "tst_lan_discovery_regex.moc"
