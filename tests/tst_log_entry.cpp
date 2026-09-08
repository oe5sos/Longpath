// Verify LogEntry ADIF serialisation and the QRZ logbook response
// parsing — in particular that a duplicate counts as success and that
// UTC conversion happens on the way out.
// no-port-check: NereusSDR-original — Thetis has no logbook.

#include <QtTest/QtTest>
#include <QTimeZone>
#include "models/LogEntry.h"
#include "core/QrzLogbookUploader.h"

using namespace Longpath;

class TstLogEntry : public QObject {
    Q_OBJECT
private slots:
    void adif_fields_are_length_prefixed();
    void empty_fields_are_omitted_entirely();
    void timestamps_are_converted_to_utc();
    void record_ends_with_eor();
    void sota_ref_is_written_directly();
    void pota_ref_is_written_via_sig_pair();
    void pota_sig_is_omitted_when_ref_is_empty();
    void upload_response_ok();
    void upload_response_duplicate_counts_as_success();
    void upload_response_failure_carries_the_reason();
    void upload_response_garbage_is_safe();
};

void TstLogEntry::adif_fields_are_length_prefixed()
{
    LogEntry e;
    e.call = QStringLiteral("OE1W");
    e.mode = QStringLiteral("SSB");
    e.submode = QStringLiteral("LSB");
    e.band = QStringLiteral("40m");

    const QString adif = e.toAdifRecord();
    QVERIFY(adif.contains(QStringLiteral("<CALL:4>OE1W")));
    QVERIFY(adif.contains(QStringLiteral("<MODE:3>SSB")));
    QVERIFY(adif.contains(QStringLiteral("<SUBMODE:3>LSB")));
    QVERIFY(adif.contains(QStringLiteral("<BAND:3>40m")));
}

void TstLogEntry::empty_fields_are_omitted_entirely()
{
    LogEntry e;
    e.call = QStringLiteral("AA1AA");

    const QString adif = e.toAdifRecord();
    // A zero-length field is not "empty", it is malformed — several
    // importers reject the whole record over one <NAME:0>.
    QVERIFY(!adif.contains(QStringLiteral(":0>")));
    QVERIFY(!adif.contains(QStringLiteral("NAME")));
    QVERIFY(!adif.contains(QStringLiteral("FREQ")));
    QVERIFY(!adif.contains(QStringLiteral("TX_PWR")));
}

void TstLogEntry::timestamps_are_converted_to_utc()
{
    LogEntry e;
    e.call = QStringLiteral("AA1AA");
    // 14:30 in a +02:00 zone is 12:30 UTC. Logging local time is
    // silently wrong by hours and only surfaces when the other station
    // cannot match the QSO.
    e.timeOn = QDateTime(QDate(2026, 8, 7), QTime(14, 30),
                         QTimeZone::fromSecondsAheadOfUtc(2 * 3600));

    const QString adif = e.toAdifRecord();
    QVERIFY2(adif.contains(QStringLiteral("<TIME_ON:6>123000")),
             qPrintable(adif));
    QVERIFY(adif.contains(QStringLiteral("<QSO_DATE:8>20260807")));
}

void TstLogEntry::record_ends_with_eor()
{
    LogEntry e;
    e.call = QStringLiteral("AA1AA");
    QVERIFY(e.toAdifRecord().endsWith(QStringLiteral("<EOR>")));
}

void TstLogEntry::sota_ref_is_written_directly()
{
    // SOTA has its own ADIF-registered fields, unlike POTA — no
    // wrapping tag needed.
    LogEntry e;
    e.call = QStringLiteral("OE1W");
    e.mySotaRef = QStringLiteral("OE/OO-001");
    e.sotaRef = QStringLiteral("G/LD-003");

    const QString adif = e.toAdifRecord();
    QVERIFY(adif.contains(QStringLiteral("<MY_SOTA_REF:9>OE/OO-001")));
    QVERIFY(adif.contains(QStringLiteral("<SOTA_REF:8>G/LD-003")));
}

void TstLogEntry::pota_ref_is_written_via_sig_pair()
{
    // POTA has no dedicated field — it rides ADIF's generic Special
    // Interest Activity pair, with MY_SIG/SIG carrying the literal
    // text "POTA".
    LogEntry e;
    e.call = QStringLiteral("OE1W");
    e.myPotaRef = QStringLiteral("OE-1234");
    e.potaRef = QStringLiteral("OE-5678");

    const QString adif = e.toAdifRecord();
    QVERIFY(adif.contains(QStringLiteral("<MY_SIG:4>POTA")));
    QVERIFY(adif.contains(QStringLiteral("<MY_SIG_INFO:7>OE-1234")));
    QVERIFY(adif.contains(QStringLiteral("<SIG:4>POTA")));
    QVERIFY(adif.contains(QStringLiteral("<SIG_INFO:7>OE-5678")));
}

void TstLogEntry::pota_sig_is_omitted_when_ref_is_empty()
{
    // An empty MY_SIG=POTA with no MY_SIG_INFO would be a claim with
    // nothing behind it — the pair only appears when there is a park
    // reference to name.
    LogEntry e;
    e.call = QStringLiteral("OE1W");
    const QString adif = e.toAdifRecord();
    QVERIFY(!adif.contains(QStringLiteral("SIG")));
}

void TstLogEntry::upload_response_ok()
{
    const auto r = QrzLogbookUploader::parseResponse(
        QByteArray("RESULT=OK&LOGID=987654&COUNT=1"));
    QVERIFY(r.ok);
    QVERIFY(!r.duplicate);
    QCOMPARE(r.logId, QStringLiteral("987654"));
}

void TstLogEntry::upload_response_duplicate_counts_as_success()
{
    // Re-uploading a contact the service already holds is the normal
    // outcome of a retry. The QSO is in the logbook, which is what was
    // asked for — surfacing it as an error trains the operator to
    // ignore upload messages.
    const auto r = QrzLogbookUploader::parseResponse(
        QByteArray("RESULT=FAIL&REASON=duplicate%20record"));
    QVERIFY(r.ok);
    QVERIFY(r.duplicate);
}

void TstLogEntry::upload_response_failure_carries_the_reason()
{
    // REASON is percent-encoded; a parser that skips decoding shows the
    // operator "invalid%20api%20key" and helps nobody.
    const auto r = QrzLogbookUploader::parseResponse(
        QByteArray("RESULT=AUTH&REASON=invalid%20api%20key"));
    QVERIFY(!r.ok);
    QCOMPARE(r.message, QStringLiteral("invalid api key"));
}

void TstLogEntry::upload_response_garbage_is_safe()
{
    for (const QByteArray& bad : {QByteArray(), QByteArray("<html>502</html>"),
                                  QByteArray("RESULT=")}) {
        const auto r = QrzLogbookUploader::parseResponse(bad);
        QVERIFY(!r.ok);
        QVERIFY(!r.message.isEmpty());
    }
}

QTEST_APPLESS_MAIN(TstLogEntry)
#include "tst_log_entry.moc"
