// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - PotaParkInfoClient JSON parser tests
//
// NereusSDR-native (operator-requested follow-up, 2026-08-27). Pins
// the contract that PotaParkInfoClient parses api.pota.app/park/{ref}
// JSON responses. Fixture is a trimmed but verbatim copy of the real
// response fetched live for US-4558 (Continental Divide Trail) during
// development -- see PotaParkInfoClient.h/.cpp modification history.

#include <QtTest>

#include "core/PotaParkInfoClient.h"

using namespace Longpath;

class TestPotaParkInfoClient : public QObject {
    Q_OBJECT
private slots:
    void parsesParkInfoResponse();
    void rejectsMalformedJson();
    void rejectsResponseWithoutReference();
};

void TestPotaParkInfoClient::parsesParkInfoResponse() {
    QByteArray json = R"({
        "parkId": 4558, "reference": "US-4558",
        "name": "Continental Divide Trail",
        "latitude": 32.3252, "longitude": -108.726,
        "grid4": "DM52", "grid6": "DM52ph",
        "parktypeDesc": "National Scenic Trail",
        "parkComments": "This is a Trail park.",
        "accessMethods": "Automobile,Foot,Other",
        "activationMethods": "Automobile,Cabin,Campground,Other,Pedestrian,Shelter",
        "agencies": "Continental Divide Trail Coalition",
        "agencyURLs": "https://continentaldividetrail.org",
        "website": "https://www.fs.usda.gov/managing-land/trails/cdt",
        "locationDesc": "US-CO,US-ID,US-MT,US-NM,US-WY",
        "locationName": "Colorado,Idaho,Montana,New Mexico,Wyoming",
        "entityId": 291, "entityName": "United States of America",
        "referencePrefix": "US", "entityDeleted": 0,
        "firstActivator": "W5ESE", "firstActivationDate": "2006-09-05"
    })";

    auto info = PotaParkInfoClient::parseParkInfoForTest(json);
    QVERIFY(info.has_value());
    QCOMPARE(info->reference, QStringLiteral("US-4558"));
    QCOMPARE(info->name, QStringLiteral("Continental Divide Trail"));
    QCOMPARE(info->grid6, QStringLiteral("DM52ph"));
    QCOMPARE(info->entityName, QStringLiteral("United States of America"));
    QCOMPARE(info->referencePrefix, QStringLiteral("US"));
    QCOMPARE(info->locationName, QStringLiteral("Colorado,Idaho,Montana,New Mexico,Wyoming"));
    QCOMPARE(info->firstActivator, QStringLiteral("W5ESE"));
    QCOMPARE(info->firstActivationDate, QStringLiteral("2006-09-05"));
    QVERIFY(info->active);  // no "active" field in this fixture -> defaults true
    QCOMPARE(info->latitude, 32.3252);
    QCOMPARE(info->longitude, -108.726);
}

void TestPotaParkInfoClient::rejectsMalformedJson() {
    QVERIFY(!PotaParkInfoClient::parseParkInfoForTest(QByteArray("not json")).has_value());
    QVERIFY(!PotaParkInfoClient::parseParkInfoForTest(QByteArray("")).has_value());
    QVERIFY(!PotaParkInfoClient::parseParkInfoForTest(QByteArray("[1,2,3]")).has_value());
}

void TestPotaParkInfoClient::rejectsResponseWithoutReference() {
    // The live API returns {} for an unknown reference.
    QVERIFY(!PotaParkInfoClient::parseParkInfoForTest(QByteArray("{}")).has_value());
}

QTEST_GUILESS_MAIN(TestPotaParkInfoClient)
#include "tst_pota_park_info_client.moc"
