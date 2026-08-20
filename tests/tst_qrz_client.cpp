// Verify the QRZ XML parser against real response shapes, including
// the two failures the upstream comments call out: the <QRZDatabase>
// root element swallowing every child, and a latitude accepted without
// a longitude leaving bogus coordinates.
//
// no-port-check: ported from AetherSDR src/core/QrzClient.cpp
// [@3a1f59e], attributed in the ported file's header.

#include <QtTest/QtTest>
#include "core/QrzClient.h"

using namespace Longpath;

class TstQrzClient : public QObject {
    Q_OBJECT
private slots:
    void session_key_is_found_inside_the_qrzdatabase_root();
    void callsign_block_is_parsed();
    void session_error_is_surfaced();
    void latitude_without_longitude_is_not_trusted();
    void both_coordinates_present_sets_the_flag();
    void empty_and_garbage_input_is_safe();
    void display_name_prefers_the_formatted_name();
    void callsign_shape_accepts_portable_rejects_garble();
};

void TstQrzClient::session_key_is_found_inside_the_qrzdatabase_root()
{
    // The real server wraps everything in <QRZDatabase>. A parser that
    // calls readElementText on the root reads to ITS end tag and never
    // sees <Session> — that shipped once upstream and broke login
    // end-to-end with no visible error.
    const QByteArray xml =
        "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
        "<QRZDatabase version=\"1.34\" xmlns=\"http://xmldata.qrz.com\">"
        "<Session><Key>abc123def456</Key><Count>42</Count>"
        "<SubExp>Wed Dec 31 00:00:00 2026</SubExp></Session>"
        "</QRZDatabase>";

    const auto r = QrzClient::parseXml(xml);
    QCOMPARE(r.sessionKey, QStringLiteral("abc123def456"));
    QVERIFY(r.sessionMessage.contains(QStringLiteral("Subscription")));
}

void TstQrzClient::callsign_block_is_parsed()
{
    const QByteArray xml =
        "<QRZDatabase><Session><Key>k</Key></Session>"
        "<Callsign><call>OE5SOS</call><fname>Martin</fname>"
        "<name>Fischer</name><name_fmt>Martin Fischer</name_fmt>"
        "<addr2>Linz</addr2><country>Austria</country>"
        "<grid>JN78</grid><class>CEPT</class>"
        "<lotw>1</lotw><eqsl>0</eqsl><mqsl>1</mqsl>"
        "</Callsign></QRZDatabase>";

    const auto r = QrzClient::parseXml(xml);
    QVERIFY(r.info.isValid());
    QCOMPARE(r.info.call, QStringLiteral("OE5SOS"));
    QCOMPARE(r.info.city, QStringLiteral("Linz"));
    QCOMPARE(r.info.country, QStringLiteral("Austria"));
    QCOMPARE(r.info.grid, QStringLiteral("JN78"));
    QVERIFY(r.info.lotw);
    QVERIFY(!r.info.eqsl);
    QVERIFY(r.info.mailQsl);
}

void TstQrzClient::session_error_is_surfaced()
{
    const QByteArray xml =
        "<QRZDatabase><Session>"
        "<Error>Username/password incorrect</Error>"
        "</Session></QRZDatabase>";

    const auto r = QrzClient::parseXml(xml);
    QVERIFY(r.sessionKey.isEmpty());
    QCOMPARE(r.sessionError, QStringLiteral("Username/password incorrect"));
    QVERIFY(!r.info.isValid());
}

void TstQrzClient::latitude_without_longitude_is_not_trusted()
{
    // Half a coordinate pair would leave (lat, 0.0) — a point in the
    // Gulf of Guinea for any European station.
    const QByteArray xml =
        "<QRZDatabase><Callsign><call>AA1AA</call>"
        "<lat>48.30</lat></Callsign></QRZDatabase>";

    const auto r = QrzClient::parseXml(xml);
    QVERIFY(r.info.isValid());
    QVERIFY(!r.info.hasLatLon);
}

void TstQrzClient::both_coordinates_present_sets_the_flag()
{
    const QByteArray xml =
        "<QRZDatabase><Callsign><call>AA1AA</call>"
        "<lat>48.30</lat><lon>14.29</lon></Callsign></QRZDatabase>";

    const auto r = QrzClient::parseXml(xml);
    QVERIFY(r.info.hasLatLon);
    QVERIFY(qAbs(r.info.latitude - 48.30) < 0.001);
    QVERIFY(qAbs(r.info.longitude - 14.29) < 0.001);
}

void TstQrzClient::empty_and_garbage_input_is_safe()
{
    for (const QByteArray& bad : {QByteArray(), QByteArray("not xml at all"),
                                  QByteArray("<QRZDatabase><unclosed>")}) {
        const auto r = QrzClient::parseXml(bad);
        QVERIFY(r.sessionKey.isEmpty());
        QVERIFY(!r.info.isValid());
    }
}

void TstQrzClient::display_name_prefers_the_formatted_name()
{
    CallsignInfo i;
    i.call = QStringLiteral("AA1AA");
    QCOMPARE(i.displayName(), QStringLiteral("AA1AA"));

    i.firstName = QStringLiteral("Ann");
    QCOMPARE(i.displayName(), QStringLiteral("Ann"));

    i.lastName = QStringLiteral("Meier");
    QCOMPARE(i.displayName(), QStringLiteral("Ann Meier"));

    i.nameFmt = QStringLiteral("Dr. Ann Meier");
    QCOMPARE(i.displayName(), QStringLiteral("Dr. Ann Meier"));
}

void TstQrzClient::callsign_shape_accepts_portable_rejects_garble()
{
    QVERIFY(Callsigns::isLikelyCallsign(QStringLiteral("OE5SOS")));
    QVERIFY(Callsigns::isLikelyCallsign(QStringLiteral("oe5sos")));
    QVERIFY(Callsigns::isLikelyCallsign(QStringLiteral("LU1EAF/F")));
    QVERIFY(Callsigns::isLikelyCallsign(QStringLiteral("VP2E/G0ABC")));

    // Common CW garble must not trigger a network lookup.
    QVERIFY(!Callsigns::isLikelyCallsign(QStringLiteral("599")));
    QVERIFY(!Callsigns::isLikelyCallsign(QStringLiteral("73")));
    QVERIFY(!Callsigns::isLikelyCallsign(QStringLiteral("5NN")));
    QVERIFY(!Callsigns::isLikelyCallsign(QString{}));

    QCOMPARE(Callsigns::normalized(QStringLiteral("  oe5sos ")),
             QStringLiteral("OE5SOS"));
}

QTEST_APPLESS_MAIN(TstQrzClient)
#include "tst_qrz_client.moc"
