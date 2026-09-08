// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - SotaClient JSON parser + dedup tests
//
// Same shape as tst_pota_client.cpp: pins the contract that SotaClient
// parses api2.sota.org.uk/api/spots/... JSON responses and dedups
// across consecutive polls via id. The sample payloads below mirror a
// real response fetched live from the API while writing this class
// (2026-09-03) -- including the two fields that are NOT always present
// ("frequency" can be null, "mode" can be an empty string).

#include <QtTest>

#include "core/SotaClient.h"
#include "core/DxSpot.h"
#include "core/Maidenhead.h"

using namespace Longpath;

class TestSotaClient : public QObject {
    Q_OBJECT
private slots:
    void parsesSotaJsonResponse();
    void entityIsTheAssociationPrefix();
    void fallsBackToSummitNameWhenCommentsEmpty();
    void nullFrequencyIsRejected();
    void emptyModeIsToleratedNotFatal();
    void dedupsAcrossPolls();
    void rejectsMalformedJson();
    void minIntervalIsSixtySeconds();
};

void TestSotaClient::parsesSotaJsonResponse()
{
    // Field values lifted from a live response fetched 2026-09-03.
    QByteArray json = R"([{
        "id": 383706,
        "timeStamp": "2026-09-03T14:46:02.768450Z",
        "activatorCallsign": "DL5AZZ/P",
        "callsign": "RBNHOLE",
        "comments": "[RBNHole] at IK7YTT 25 WPM 20 dB SNR",
        "frequency": 10.1254,
        "mode": "CW",
        "summitCode": "DL/MF-117",
        "summitName": "Schwarzenberg",
        "latitude": 47.641658782958984,
        "longitude": 12.13634967803955
    }])";

    SotaClient client;
    const auto spots = client.parseJsonForTest(json);
    QCOMPARE(spots.size(), 1);
    QCOMPARE(spots[0].dxCall, QStringLiteral("DL5AZZ/P"));
    QCOMPARE(spots[0].spotterCall, QStringLiteral("RBNHOLE"));
    QCOMPARE(spots[0].source, QStringLiteral("SOTA"));
    // Already MHz on this API -- no kHz conversion like POTA's.
    QVERIFY(qAbs(spots[0].freqMhz - 10.1254) < 1e-6);
    QCOMPARE(spots[0].reference, QStringLiteral("DL/MF-117"));
    QCOMPARE(spots[0].comment,
             QStringLiteral("[RBNHole] at IK7YTT 25 WPM 20 dB SNR CW"));
    // lat/lon -> grid must go through the same helper the logbook and
    // FreeDV Reporter use, with the arguments in the right order.
    QCOMPARE(spots[0].grid,
             gridSquareFromLatLon(47.641658782958984, 12.13634967803955));
}

void TestSotaClient::entityIsTheAssociationPrefix()
{
    // SOTA references split on '/' (association/region-number), unlike
    // POTA's '-' (prefix-number) -- this is the one field-mapping
    // difference from PotaClient::parseAndCollect worth its own test.
    QByteArray json = R"([{
        "id": 1, "activatorCallsign": "G0ABC", "callsign": "S",
        "frequency": 7.032, "mode": "CW",
        "summitCode": "G/LD-003", "summitName": "Scafell Pike",
        "comments": "", "timeStamp": "2026-09-03T10:00:00Z"
    }])";
    SotaClient client;
    const auto spots = client.parseJsonForTest(json);
    QCOMPARE(spots.size(), 1);
    QCOMPARE(spots[0].reference, QStringLiteral("G/LD-003"));
    QCOMPARE(spots[0].entity, QStringLiteral("G"));
}

void TestSotaClient::fallsBackToSummitNameWhenCommentsEmpty()
{
    QByteArray json = R"([{
        "id": 2, "activatorCallsign": "OE5SOS", "callsign": "S",
        "frequency": 14.285, "mode": "SSB",
        "summitCode": "OE/OO-123", "summitName": "Testgipfel",
        "comments": "", "timeStamp": "2026-09-03T10:00:00Z"
    }])";
    SotaClient client;
    const auto spots = client.parseJsonForTest(json);
    QCOMPARE(spots.size(), 1);
    QCOMPARE(spots[0].comment, QStringLiteral("Testgipfel SSB"));
}

void TestSotaClient::nullFrequencyIsRejected()
{
    // Confirmed live (2026-09-03): "frequency" can legally be JSON
    // null, not just absent. A spot with no frequency is useless to a
    // panadapter overlay, so it is dropped -- same
    // "freqMhz <= 0.0 || dxCall.isEmpty()" reject rule PotaClient uses.
    QByteArray json = R"([{
        "id": 3, "activatorCallsign": "M0ABC", "callsign": "S",
        "frequency": null, "mode": "",
        "summitCode": "G/LD-005", "summitName": "No Freq Yet",
        "comments": "", "timeStamp": "2026-09-03T10:00:00Z"
    }])";
    SotaClient client;
    QCOMPARE(client.parseJsonForTest(json).size(), 0);
}

void TestSotaClient::emptyModeIsToleratedNotFatal()
{
    // Confirmed live: "mode" can be an empty string. Must not crash and
    // must not append a stray trailing space to the comment.
    QByteArray json = R"([{
        "id": 4, "activatorCallsign": "M0ABC", "callsign": "S",
        "frequency": 7.033, "mode": "",
        "summitCode": "G/LD-006", "summitName": "No Mode Yet",
        "comments": "on air", "timeStamp": "2026-09-03T10:00:00Z"
    }])";
    SotaClient client;
    const auto spots = client.parseJsonForTest(json);
    QCOMPARE(spots.size(), 1);
    QCOMPARE(spots[0].comment, QStringLiteral("on air"));
}

void TestSotaClient::dedupsAcrossPolls()
{
    SotaClient client;
    QByteArray json1 = R"([{"id":1,"activatorCallsign":"A","callsign":"S","frequency":7.0,"mode":"CW","summitCode":"G/LD-001","summitName":"","comments":"","timeStamp":"2026-09-03T10:00:00Z"}])";
    QByteArray json2 = R"([{"id":1,"activatorCallsign":"A","callsign":"S","frequency":7.0,"mode":"CW","summitCode":"G/LD-001","summitName":"","comments":"","timeStamp":"2026-09-03T10:00:00Z"},
                          {"id":2,"activatorCallsign":"B","callsign":"S","frequency":7.1,"mode":"CW","summitCode":"G/LD-002","summitName":"","comments":"","timeStamp":"2026-09-03T10:01:00Z"}])";
    QCOMPARE(client.parseJsonForTest(json1).size(), 1);
    QCOMPARE(client.parseJsonForTest(json2).size(), 1);  // only id=2 is new
}

void TestSotaClient::rejectsMalformedJson()
{
    SotaClient client;
    QCOMPARE(client.parseJsonForTest(QByteArray("not json")).size(), 0);
    QCOMPARE(client.parseJsonForTest(QByteArray("")).size(), 0);
    QCOMPARE(client.parseJsonForTest(QByteArray("{\"not\":\"array\"}")).size(), 0);
}

void TestSotaClient::minIntervalIsSixtySeconds()
{
    // SOTA's own published guidance: no more than one request every 60
    // seconds. Unlike PotaClient, this floor is enforced by the class
    // itself, not left to whoever calls startPolling().
    QCOMPARE(SotaClient::kMinIntervalSec, 60);
}

QTEST_GUILESS_MAIN(TestSotaClient)
#include "tst_sota_client.moc"
