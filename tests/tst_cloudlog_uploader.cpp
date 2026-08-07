// The two parts of a Cloudlog upload that go wrong are the address the
// operator pasted and the answer the server gave. Both are testable
// without a server, and neither is the HTTP.
// no-port-check: NereusSDR-original.

#include <QtTest/QtTest>
#include "core/CloudlogUploader.h"

using namespace NereusSDR;

class TstCloudlogUploader : public QObject {
    Q_OBJECT
private slots:
    void endpoint_from_a_bare_host();
    void endpoint_tolerates_what_the_browser_shows();
    void endpoint_defaults_to_https();
    void endpoint_of_nothing_is_nothing();
    void success_is_success();
    void duplicate_counts_as_success();
    void rejected_key_says_so();
    void wrong_address_says_so();
    void not_configured_until_all_three_are_set();
};

void TstCloudlogUploader::endpoint_from_a_bare_host()
{
    QCOMPARE(CloudlogUploader::qsoEndpoint(
                 QStringLiteral("https://log.example.org")),
             QStringLiteral("https://log.example.org/index.php/api/qso"));
}

void TstCloudlogUploader::endpoint_tolerates_what_the_browser_shows()
{
    // Operators paste the address bar. Every one of these is the same
    // instance, and refusing any of them buys a support conversation
    // about which spelling is "correct".
    const QString want =
        QStringLiteral("https://log.example.org/index.php/api/qso");
    for (const QString& in : {
             QStringLiteral("https://log.example.org/"),
             QStringLiteral("https://log.example.org/index.php"),
             QStringLiteral("https://log.example.org/index.php/"),
             QStringLiteral("https://log.example.org/api/qso"),
             QStringLiteral("https://log.example.org/index.php/api/qso"),
             QStringLiteral("  https://log.example.org  "),
         }) {
        QCOMPARE(CloudlogUploader::qsoEndpoint(in), want);
    }
}

void TstCloudlogUploader::endpoint_defaults_to_https()
{
    // An API key sent in clear over http is handed to the network.
    QCOMPARE(CloudlogUploader::qsoEndpoint(QStringLiteral("log.example.org")),
             QStringLiteral("https://log.example.org/index.php/api/qso"));
    // But an explicit http:// is the operator's decision to make — a
    // Cloudlog on the same LAN is a normal setup.
    QCOMPARE(CloudlogUploader::qsoEndpoint(QStringLiteral("http://192.168.1.9")),
             QStringLiteral("http://192.168.1.9/index.php/api/qso"));
}

void TstCloudlogUploader::endpoint_of_nothing_is_nothing()
{
    QVERIFY(CloudlogUploader::qsoEndpoint(QString{}).isEmpty());
    QVERIFY(CloudlogUploader::qsoEndpoint(QStringLiteral("   ")).isEmpty());
    QVERIFY(CloudlogUploader::qsoEndpoint(QStringLiteral("/")).isEmpty());
}

void TstCloudlogUploader::success_is_success()
{
    const auto r = CloudlogUploader::parseResponse(
        201, R"({"status":"created","reason":"QSO added"})");
    QVERIFY(r.ok);
    QVERIFY(!r.duplicate);
}

void TstCloudlogUploader::duplicate_counts_as_success()
{
    // Re-sending a contact the log already has is the normal outcome of
    // a retry. Reporting it as a failure would train the operator to
    // ignore upload errors.
    const auto r = CloudlogUploader::parseResponse(
        200, R"({"status":"duplicate","reason":"Duplicate QSO"})");
    QVERIFY(r.ok);
    QVERIFY(r.duplicate);
}

void TstCloudlogUploader::rejected_key_says_so()
{
    for (int code : {401, 403}) {
        const auto r = CloudlogUploader::parseResponse(code, "Forbidden");
        QVERIFY(!r.ok);
        QVERIFY2(r.message.contains(QStringLiteral("API key")),
                 qPrintable(r.message));
    }
}

void TstCloudlogUploader::wrong_address_says_so()
{
    // A 404 here almost always means the instance URL is wrong, not that
    // the contact was bad — saying which saves the operator checking the
    // QSO first.
    const auto r = CloudlogUploader::parseResponse(404, "<html>Not Found");
    QVERIFY(!r.ok);
    QVERIFY2(r.message.contains(QStringLiteral("instance URL")),
             qPrintable(r.message));
}

void TstCloudlogUploader::not_configured_until_all_three_are_set()
{
    CloudlogUploader u;
    QVERIFY(!u.isConfigured());
    u.setBaseUrl(QStringLiteral("https://log.example.org"));
    QVERIFY(!u.isConfigured());
    u.setApiKey(QStringLiteral("abc"));
    QVERIFY(!u.isConfigured());
    // The station profile is required rather than defaulted to 1: an
    // operator with a home and a portable profile would otherwise have
    // every contact filed under whichever came first.
    u.setStationProfileId(QStringLiteral("2"));
    QVERIFY(u.isConfigured());
}

QTEST_MAIN(TstCloudlogUploader)
#include "tst_cloudlog_uploader.moc"
