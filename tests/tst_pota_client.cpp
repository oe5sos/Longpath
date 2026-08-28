// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - PotaClient JSON parser + dedup tests
//
// Phase 3J-2 Task B2. Pins the contract that PotaClient parses
// api.pota.app/spot/activator JSON responses and dedups across
// consecutive polls via spotId.

#include <QtTest>

#include "core/PotaClient.h"
#include "core/DxSpot.h"

using namespace Longpath;

class TestPotaClient : public QObject {
    Q_OBJECT
private slots:
    void parsesPotaJsonResponse();
    void dedupsAcrossPolls();
    void rejectsMalformedJson();
    void fallsBackToParkNameWhenCommentsEmpty();
};

void TestPotaClient::parsesPotaJsonResponse() {
    QByteArray json = R"([{
        "spotId": 12345,
        "activator": "K3POTA",
        "frequency": "14260.0",
        "mode": "SSB",
        "reference": "K-1234",
        "name": "Acadia NP",
        "spotTime": "2026-05-10T17:08:00Z",
        "spotter": "W1AW",
        "comments": "POTA K-1234",
        "grid6": "FN31pr"
    }])";

    PotaClient client;
    auto spots = client.parseJsonForTest(json);
    QCOMPARE(spots.size(), 1);
    QCOMPARE(spots[0].dxCall, QStringLiteral("K3POTA"));
    QCOMPARE(spots[0].freqMhz, 14.260);
    QCOMPARE(spots[0].source, QStringLiteral("POTA"));
    // 2026-08-26 (SpotHub POTA improvement pass, no upstream
    // equivalent): reference/entity are now structured fields instead
    // of being folded into `comment` -- see DxSpot.h.
    QCOMPARE(spots[0].reference, QStringLiteral("K-1234"));
    QCOMPARE(spots[0].entity, QStringLiteral("K"));
    // 2026-08-27 (operator-requested follow-up): grid6 feeds
    // SpotTableModel's distance/bearing columns, no extra lookup.
    QCOMPARE(spots[0].grid, QStringLiteral("FN31pr"));
    // `comment` carries the real operator note (API's `comments`
    // field) plus the mode suffix that SpotTableModel::extractMode
    // relies on -- no longer a synthesized "ref park mode" string.
    QCOMPARE(spots[0].comment, QStringLiteral("POTA K-1234 SSB"));
}

void TestPotaClient::fallsBackToParkNameWhenCommentsEmpty() {
    // 2026-08-26: when the API's `comments` field is empty, `comment`
    // falls back to the park name (still with the mode suffix) rather
    // than being left blank.
    QByteArray json = R"([{
        "spotId": 99,
        "activator": "KB9LBE",
        "frequency": "14283.0",
        "mode": "SSB",
        "reference": "US-1772",
        "name": "Mark Twain State Park",
        "spotTime": "2026-08-26T20:07:22",
        "spotter": "KC1VIO",
        "comments": ""
    }])";

    PotaClient client;
    auto spots = client.parseJsonForTest(json);
    QCOMPARE(spots.size(), 1);
    QCOMPARE(spots[0].reference, QStringLiteral("US-1772"));
    QCOMPARE(spots[0].entity, QStringLiteral("US"));
    QCOMPARE(spots[0].comment, QStringLiteral("Mark Twain State Park SSB"));
}

void TestPotaClient::dedupsAcrossPolls() {
    PotaClient client;
    QByteArray json1 = R"([{"spotId":1,"activator":"A","frequency":"7000","mode":"SSB","spotTime":"2026-05-10T17:00:00Z","spotter":"S","comments":""}])";
    QByteArray json2 = R"([{"spotId":1,"activator":"A","frequency":"7000","mode":"SSB","spotTime":"2026-05-10T17:00:00Z","spotter":"S","comments":""},
                          {"spotId":2,"activator":"B","frequency":"7100","mode":"SSB","spotTime":"2026-05-10T17:01:00Z","spotter":"S","comments":""}])";
    QCOMPARE(client.parseJsonForTest(json1).size(), 1);
    QCOMPARE(client.parseJsonForTest(json2).size(), 1);  // only spotId=2 is new (spotId=1 already seen)
}

void TestPotaClient::rejectsMalformedJson() {
    PotaClient client;
    QCOMPARE(client.parseJsonForTest(QByteArray("not json")).size(), 0);
    QCOMPARE(client.parseJsonForTest(QByteArray("")).size(), 0);
    QCOMPARE(client.parseJsonForTest(QByteArray("{\"not\":\"array\"}")).size(), 0);
}

QTEST_GUILESS_MAIN(TestPotaClient)
#include "tst_pota_client.moc"
