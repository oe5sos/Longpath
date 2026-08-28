// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - PotaAlertsClient JSON parser tests
//
// NereusSDR-native (operator-requested follow-up, 2026-08-27). Pins
// the contract that PotaAlertsClient parses api.pota.app/activation
// JSON responses. Fixture rows are trimmed but verbatim copies of the
// real response fetched live during development.

#include <QtTest>

#include "core/PotaAlertsClient.h"

using namespace Longpath;

class TestPotaAlertsClient : public QObject {
    Q_OBJECT
private slots:
    void parsesAlertsResponse();
    void rejectsMalformedJson();
    void skipsRowsMissingActivatorOrReference();
};

void TestPotaAlertsClient::parsesAlertsResponse() {
    QByteArray json = R"([
        {
            "scheduledActivitiesId": 141809, "schedulerUserId": 87305,
            "activator": "K2AYE", "name": "Fleming Wildlife Management Area",
            "reference": "US-3812", "locationDesc": "US-KY",
            "activityStart": null, "activityEnd": null,
            "startDate": "2026-02-15", "endDate": "2026-10-10",
            "startTime": "20:25", "endTime": "18:59",
            "frequencies": "20 meter", "comments": ""
        },
        {
            "scheduledActivitiesId": 152751, "schedulerUserId": 25292,
            "activator": "LW5DR", "name": "Humedal Cuenca Quequen Grande",
            "reference": "AR-0373", "locationDesc": "AR-B",
            "activityStart": null, "activityEnd": null,
            "startDate": "2026-06-01", "endDate": "2026-12-31",
            "startTime": "00:00", "endTime": "00:00",
            "frequencies": "all", "comments": "S2S welcome"
        }
    ])";

    auto alerts = PotaAlertsClient::parseAlertsForTest(json);
    QCOMPARE(alerts.size(), 2);
    QCOMPARE(alerts[0].activator, QStringLiteral("K2AYE"));
    QCOMPARE(alerts[0].reference, QStringLiteral("US-3812"));
    QCOMPARE(alerts[0].locationDesc, QStringLiteral("US-KY"));
    QCOMPARE(alerts[0].startDate, QStringLiteral("2026-02-15"));
    QCOMPARE(alerts[0].startTime, QStringLiteral("20:25"));
    QCOMPARE(alerts[0].frequencies, QStringLiteral("20 meter"));
    QCOMPARE(alerts[1].comments, QStringLiteral("S2S welcome"));
}

void TestPotaAlertsClient::rejectsMalformedJson() {
    QCOMPARE(PotaAlertsClient::parseAlertsForTest(QByteArray("not json")).size(), 0);
    QCOMPARE(PotaAlertsClient::parseAlertsForTest(QByteArray("")).size(), 0);
    QCOMPARE(PotaAlertsClient::parseAlertsForTest(QByteArray("{\"not\":\"array\"}")).size(), 0);
}

void TestPotaAlertsClient::skipsRowsMissingActivatorOrReference() {
    QByteArray json = R"([
        {"activator": "", "reference": "US-1234", "name": "x", "locationDesc": "",
         "startDate": "", "endDate": "", "startTime": "", "endTime": "",
         "frequencies": "", "comments": ""},
        {"activator": "K2AYE", "reference": "", "name": "x", "locationDesc": "",
         "startDate": "", "endDate": "", "startTime": "", "endTime": "",
         "frequencies": "", "comments": ""}
    ])";
    QCOMPARE(PotaAlertsClient::parseAlertsForTest(json).size(), 0);
}

QTEST_GUILESS_MAIN(TestPotaAlertsClient)
#include "tst_pota_alerts_client.moc"
