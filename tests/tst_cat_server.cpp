// no-port-check: Longpath-original test for the Kenwood CAT bridge
// (phase 3K, second step, 2026-10-11).
//
// CatParser against TestMockRadioModel for the protocol -- the IF frame's
// field widths above all, because a logger reads that one frame and a
// single byte out of place shifts everything after it -- plus CatServer
// over a real socket for the ';'-framing, which is where a stream
// protocol usually goes wrong.
//
// Field widths come from Thetis IF() at CATCommands.cs:317-390
// [v2.10.3.15-5-g852bf0e]: 11 frequency, 4 step, 6 incremental tuning,
// 1 RIT, 1 XIT, 3 memory dummy, 1 TX, 1 mode, 1 FR/FT dummy, 1 scan
// dummy, 1 split, 4 balance dummy = 35.

#include <QtTest/QtTest>
#include <QtNetwork/QTcpSocket>

#include "core/CatParser.h"
#include "core/CatServer.h"
#include "models/RadioModel.h"
#include "TestMockRadioModel.h"

using namespace Longpath;

class TestCatServer : public QObject {
    Q_OBJECT

private slots:
    // ── Identity and status ─────────────────────────────────────────────────

    void id_reports_ts2000()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        // Thetis ID() default branch returns "019" (TS-2000).
        QCOMPARE(p.handleCommand(QStringLiteral("ID;")), QStringLiteral("ID019;"));
        // ID takes no argument.
        QCOMPARE(p.handleCommand(QStringLiteral("ID1;")), QStringLiteral("?;"));
    }

    void if_frame_has_thetis_field_widths()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        p.handleCommand(QStringLiteral("FA00014250000;"));
        mock.setTuneStepHzForTest(0, 100);
        p.handleCommand(QStringLiteral("MD2;"));

        const QString answer = p.handleCommand(QStringLiteral("IF;"));
        QCOMPARE(answer.size(), 2 + 35 + 1);
        QVERIFY(answer.startsWith(QStringLiteral("IF")));
        QVERIFY(answer.endsWith(QLatin1Char(';')));

        const QString payload = answer.mid(2, 35);
        QCOMPARE(payload.left(11), QStringLiteral("00014250000"));   // frequency
        QCOMPARE(payload.mid(11, 4), QStringLiteral("0010"));        // 100 Hz step
        QCOMPARE(payload.mid(15, 6), QStringLiteral("+00000"));      // no RIT/XIT
        QCOMPARE(payload.mid(21, 1), QStringLiteral("0"));           // RIT off
        QCOMPARE(payload.mid(22, 1), QStringLiteral("0"));           // XIT off
        QCOMPARE(payload.mid(23, 3), QStringLiteral("000"));         // memory dummy
        QCOMPARE(payload.mid(26, 1), QStringLiteral("0"));           // not keyed
        QCOMPARE(payload.mid(27, 1), QStringLiteral("2"));           // USB
        QCOMPARE(payload.mid(28, 1), QStringLiteral("0"));           // FR/FT dummy
        QCOMPARE(payload.mid(29, 1), QStringLiteral("0"));           // scan dummy
        QCOMPARE(payload.mid(30, 1), QStringLiteral("0"));           // split
        QCOMPARE(payload.mid(31, 4), QStringLiteral("0000"));        // balance dummy
    }

    void if_frame_carries_rit_and_tx_state()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        p.setPttAllowed(true);

        QMetaObject::invokeMethod(&mock, "setRitEnable", Q_ARG(bool, true));
        QMetaObject::invokeMethod(&mock, "setRitOffset", Q_ARG(int, -1200));
        p.handleCommand(QStringLiteral("TX;"));

        const QString payload = p.handleCommand(QStringLiteral("IF;")).mid(2, 35);
        QCOMPARE(payload.mid(15, 6), QStringLiteral("-01200"));
        QCOMPARE(payload.mid(21, 1), QStringLiteral("1"));   // RIT on
        QCOMPARE(payload.mid(22, 1), QStringLiteral("0"));   // XIT stays off
        QCOMPARE(payload.mid(26, 1), QStringLiteral("1"));   // keyed

        p.handleCommand(QStringLiteral("RX;"));
    }

    void if_reports_only_one_of_rit_and_xit()
    {
        // Thetis takes RIT first and only ever sets one flag
        // (CATCommands.cs:333-338, the else-if is upstream behaviour).
        TestMockRadioModel mock;
        CatParser p(&mock);
        QMetaObject::invokeMethod(&mock, "setRitEnable", Q_ARG(bool, true));
        QMetaObject::invokeMethod(&mock, "setXitEnable", Q_ARG(bool, true));
        QMetaObject::invokeMethod(&mock, "setRitOffset", Q_ARG(int, 300));
        QMetaObject::invokeMethod(&mock, "setXitOffset", Q_ARG(int, 900));

        const QString payload = p.handleCommand(QStringLiteral("IF;")).mid(2, 35);
        QCOMPARE(payload.mid(15, 6), QStringLiteral("+00300"));   // RIT wins
        QCOMPARE(payload.mid(21, 1), QStringLiteral("1"));
        QCOMPARE(payload.mid(22, 1), QStringLiteral("0"));
    }

    void step_code_follows_thetis_table()
    {
        // The tweaked cases from Thetis Step2String keep their own codes.
        QCOMPARE(CatParser::stepCodeFromHz(1),       QStringLiteral("0000"));
        QCOMPARE(CatParser::stepCodeFromHz(10),      QStringLiteral("0001"));
        QCOMPARE(CatParser::stepCodeFromHz(50),      QStringLiteral("1000"));
        QCOMPARE(CatParser::stepCodeFromHz(100),     QStringLiteral("0010"));
        QCOMPARE(CatParser::stepCodeFromHz(250),     QStringLiteral("1001"));
        QCOMPARE(CatParser::stepCodeFromHz(500),     QStringLiteral("1010"));
        QCOMPARE(CatParser::stepCodeFromHz(1000),    QStringLiteral("0011"));
        QCOMPARE(CatParser::stepCodeFromHz(5000),    QStringLiteral("1011"));
        QCOMPARE(CatParser::stepCodeFromHz(9000),    QStringLiteral("1100"));
        QCOMPARE(CatParser::stepCodeFromHz(10000),   QStringLiteral("0100"));
        QCOMPARE(CatParser::stepCodeFromHz(100000),  QStringLiteral("0101"));
        QCOMPARE(CatParser::stepCodeFromHz(250000),  QStringLiteral("1101"));
        QCOMPARE(CatParser::stepCodeFromHz(500000),  QStringLiteral("1110"));
        QCOMPARE(CatParser::stepCodeFromHz(1000000), QStringLiteral("0110"));
        QCOMPARE(CatParser::stepCodeFromHz(10000000),QStringLiteral("0111"));
    }

    // ── Frequency ───────────────────────────────────────────────────────────

    void fa_roundtrips_with_eleven_digits()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        QCOMPARE(p.handleCommand(QStringLiteral("FA00014250000;")), QString());
        QCOMPARE(p.handleCommand(QStringLiteral("FA;")),
                 QStringLiteral("FA00014250000;"));
    }

    void fa_refuses_wrong_width_or_nonsense()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        p.handleCommand(QStringLiteral("FA00014250000;"));

        QCOMPARE(p.handleCommand(QStringLiteral("FA0001425000;")), QStringLiteral("?;"));
        QCOMPARE(p.handleCommand(QStringLiteral("FA000142500000;")), QStringLiteral("?;"));
        QCOMPARE(p.handleCommand(QStringLiteral("FAabcdefghijk;")), QStringLiteral("?;"));
        QCOMPARE(p.handleCommand(QStringLiteral("FA00000000000;")), QStringLiteral("?;"));
        // and nothing moved
        QCOMPARE(p.handleCommand(QStringLiteral("FA;")),
                 QStringLiteral("FA00014250000;"));
    }

    void fb_addresses_the_second_slice()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        QCOMPARE(p.handleCommand(QStringLiteral("FB00007100000;")), QString());
        QCOMPARE(p.handleCommand(QStringLiteral("FB;")),
                 QStringLiteral("FB00007100000;"));
        // VFO A untouched by a VFO B write
        QCOMPARE(p.handleCommand(QStringLiteral("FA;")),
                 QStringLiteral("FA00000000000;"));
    }

    // ── Mode ────────────────────────────────────────────────────────────────

    void mode_digits_map_both_ways()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        QCOMPARE(p.handleCommand(QStringLiteral("MD3;")), QString());
        QCOMPARE(p.handleCommand(QStringLiteral("MD;")), QStringLiteral("MD3;"));
        QCOMPARE(p.handleCommand(QStringLiteral("MD1;")), QString());
        QCOMPARE(p.handleCommand(QStringLiteral("MD;")), QStringLiteral("MD1;"));
        QCOMPARE(p.handleCommand(QStringLiteral("MD9;")), QString());
        QCOMPARE(p.handleCommand(QStringLiteral("MD;")), QStringLiteral("MD9;"));
    }

    void mode_eight_is_refused()
    {
        // 8 is FSK on a TS-2000 and has no Longpath counterpart.
        TestMockRadioModel mock;
        CatParser p(&mock);
        p.handleCommand(QStringLiteral("MD2;"));
        QCOMPARE(p.handleCommand(QStringLiteral("MD8;")), QStringLiteral("?;"));
        QCOMPARE(p.handleCommand(QStringLiteral("MD;")), QStringLiteral("MD2;"));
    }

    // ── The commands we answer but do not obey ──────────────────────────────

    void power_status_can_be_read_but_not_switched_off()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        QCOMPARE(p.handleCommand(QStringLiteral("PS;")), QStringLiteral("PS1;"));
        QCOMPARE(p.handleCommand(QStringLiteral("PS1;")), QString());
        // A logging program does not get to power the station down.
        QCOMPARE(p.handleCommand(QStringLiteral("PS0;")), QStringLiteral("?;"));
    }

    void auto_information_is_refused_so_clients_poll()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        QCOMPARE(p.handleCommand(QStringLiteral("AI;")), QStringLiteral("AI0;"));
        QCOMPARE(p.handleCommand(QStringLiteral("AI0;")), QString());
        QCOMPARE(p.handleCommand(QStringLiteral("AI1;")), QStringLiteral("?;"));
    }

    void vfo_selection_accepts_only_vfo_a()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        QCOMPARE(p.handleCommand(QStringLiteral("FR;")), QStringLiteral("FR0;"));
        QCOMPARE(p.handleCommand(QStringLiteral("FR0;")), QString());
        QCOMPARE(p.handleCommand(QStringLiteral("FR1;")), QStringLiteral("?;"));

        QCOMPARE(p.handleCommand(QStringLiteral("FT;")), QStringLiteral("FT0;"));
        QCOMPARE(p.handleCommand(QStringLiteral("FT0;")), QString());
        // FT1 would be split, which Longpath does not have.
        QCOMPARE(p.handleCommand(QStringLiteral("FT1;")), QStringLiteral("?;"));
    }

    void unknown_commands_answer_question_mark()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        QCOMPARE(p.handleCommand(QStringLiteral("ZZSW;")), QStringLiteral("?;"));
        QCOMPARE(p.handleCommand(QStringLiteral("XY;")), QStringLiteral("?;"));
        QCOMPARE(p.handleCommand(QStringLiteral("K;")), QStringLiteral("?;"));
        QCOMPARE(p.handleCommand(QStringLiteral(";")), QStringLiteral("?;"));
    }

    // ── S meter ─────────────────────────────────────────────────────────────

    void s_meter_follows_the_thetis_scale()
    {
        // Spot values through Thetis's own two-piece scale: below S9 it is
        // (dBm+127)/6 * 1.6667, above S9 it is 15 + (dBm+73).
        QCOMPARE(CatParser::kenwoodSMeterFromDbm(-140.0), 0);    // clamped floor
        QCOMPARE(CatParser::kenwoodSMeterFromDbm(-127.0), 0);    // S0
        QCOMPARE(CatParser::kenwoodSMeterFromDbm(-73.0),  15);   // S9
        QCOMPARE(CatParser::kenwoodSMeterFromDbm(-63.0),  25);   // S9+10
        QCOMPARE(CatParser::kenwoodSMeterFromDbm(-10.0),  30);   // clamped ceiling
        QCOMPARE(CatParser::kenwoodSMeterFromDbm(-200.0), 0);    // no reading
    }

    void sm_answers_five_digits_for_the_main_receiver()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        mock.setRxSignalDbmForTest(0, -73.0);

        // Thetis pads to five digits -- wider than a TS-2000 sends, but it
        // is what programs set up for Thetis expect (CATCommands.cs:925).
        QCOMPARE(p.handleCommand(QStringLiteral("SM0;")), QStringLiteral("SM00015;"));
        QCOMPARE(p.handleCommand(QStringLiteral("SM2;")), QStringLiteral("SM00015;"));
        // Any other argument is an error, same as upstream.
        QCOMPARE(p.handleCommand(QStringLiteral("SM1;")), QStringLiteral("?;"));
        QCOMPARE(p.handleCommand(QStringLiteral("SM;")),  QStringLiteral("?;"));
    }

    // ── RIT and XIT: the documented stand-in for split ──────────────────────

    void rit_and_xit_can_be_switched_and_read()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);

        QCOMPARE(p.handleCommand(QStringLiteral("RT;")), QStringLiteral("RT0;"));
        QCOMPARE(p.handleCommand(QStringLiteral("RT1;")), QString());
        QCOMPARE(p.handleCommand(QStringLiteral("RT;")), QStringLiteral("RT1;"));
        QCOMPARE(p.handleCommand(QStringLiteral("RT2;")), QStringLiteral("?;"));

        QCOMPARE(p.handleCommand(QStringLiteral("XT;")), QStringLiteral("XT0;"));
        QCOMPARE(p.handleCommand(QStringLiteral("XT1;")), QString());
        QCOMPARE(p.handleCommand(QStringLiteral("XT;")), QStringLiteral("XT1;"));
    }

    void rit_switch_that_cannot_take_is_refused()
    {
        // Against a real RadioModel with no slice: RT1 must not answer
        // silently, or a contest program believes its RIT is on. Found at
        // the wire on 2026-10-11.
        RadioModel radio;
        CatParser p(&radio);
        QCOMPARE(p.handleCommand(QStringLiteral("RT1;")), QStringLiteral("?;"));
        QCOMPARE(p.handleCommand(QStringLiteral("RT;")), QStringLiteral("RT0;"));
        QCOMPARE(p.handleCommand(QStringLiteral("XT1;")), QStringLiteral("?;"));

        // With a slice it takes.
        radio.addSlice();
        QCOMPARE(p.handleCommand(QStringLiteral("RT1;")), QString());
        QCOMPARE(p.handleCommand(QStringLiteral("RT;")), QStringLiteral("RT1;"));
    }

    void ru_and_rd_step_the_active_offset()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        mock.setTuneStepHzForTest(0, 100);

        // Nothing on yet: there is no offset to step.
        QCOMPARE(p.handleCommand(QStringLiteral("RU;")), QStringLiteral("?;"));

        p.handleCommand(QStringLiteral("RT1;"));
        QCOMPARE(p.handleCommand(QStringLiteral("RU;")), QString());
        QCOMPARE(p.handleCommand(QStringLiteral("RU;")), QString());
        QCOMPARE(mock.ritOffset(), 200);
        QCOMPARE(p.handleCommand(QStringLiteral("RD;")), QString());
        QCOMPARE(mock.ritOffset(), 100);

        // The IF frame must show it.
        const QString payload = p.handleCommand(QStringLiteral("IF;")).mid(2, 35);
        QCOMPARE(payload.mid(15, 6), QStringLiteral("+00100"));
        QCOMPARE(payload.mid(21, 1), QStringLiteral("1"));
    }

    void rc_clears_whichever_offset_is_active()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);

        // Neither on: nothing to clear, and saying "done" would be a lie.
        QCOMPARE(p.handleCommand(QStringLiteral("RC;")), QStringLiteral("?;"));

        p.handleCommand(QStringLiteral("XT1;"));
        QMetaObject::invokeMethod(&mock, "setXitOffset", Q_ARG(int, 1500));
        QCOMPARE(p.handleCommand(QStringLiteral("RC;")), QString());
        QCOMPARE(mock.xitOffset(), 0);

        // RIT takes precedence when both are on, same as in the IF frame.
        p.handleCommand(QStringLiteral("RT1;"));
        QMetaObject::invokeMethod(&mock, "setRitOffset", Q_ARG(int, 800));
        QMetaObject::invokeMethod(&mock, "setXitOffset", Q_ARG(int, 900));
        QCOMPARE(p.handleCommand(QStringLiteral("RC;")), QString());
        QCOMPARE(mock.ritOffset(), 0);
        QCOMPARE(mock.xitOffset(), 900);
    }

    // ── The transmit gate ───────────────────────────────────────────────────

    void tx_is_refused_until_it_is_allowed()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        QVERIFY(!p.pttAllowed());

        QCOMPARE(p.handleCommand(QStringLiteral("TX;")), QStringLiteral("?;"));
        QVERIFY(!mock.mox());
        QVERIFY(p.takePttRefused());
        QVERIFY(!p.takePttRefused());   // the flag is taken, not latched

        p.setPttAllowed(true);
        QCOMPARE(p.handleCommand(QStringLiteral("TX;")), QString());
        QVERIFY(mock.mox());
    }

    void rx_unkeys_even_with_the_gate_closed()
    {
        TestMockRadioModel mock;
        CatParser p(&mock);
        p.setPttAllowed(true);
        p.handleCommand(QStringLiteral("TX;"));
        QVERIFY(mock.mox());

        p.setPttAllowed(false);
        QCOMPARE(p.handleCommand(QStringLiteral("RX;")), QString());
        QVERIFY(!mock.mox());
    }

    void thetis_ignores_the_tx_argument_and_so_do_we()
    {
        // Thetis TX(string s) / RX(string s) never look at s
        // (CATCommands.cs:970, :822).
        TestMockRadioModel mock;
        CatParser p(&mock);
        p.setPttAllowed(true);
        QCOMPARE(p.handleCommand(QStringLiteral("TX1;")), QString());
        QVERIFY(mock.mox());
        QCOMPARE(p.handleCommand(QStringLiteral("RX0;")), QString());
        QVERIFY(!mock.mox());
    }

    // ── The socket and the ';' framing ──────────────────────────────────────

    void several_commands_in_one_packet_are_all_answered()
    {
        TestMockRadioModel mock;
        CatServer s(&mock);
        QVERIFY(s.start(QHostAddress::LocalHost, 0));

        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, s.port());
        QVERIFY(client.waitForConnected(3000));
        QTRY_COMPARE(s.clientCount(), 1);

        client.write("FA00021074000;ID;FA;");
        QTRY_VERIFY(client.bytesAvailable() > 0);
        QTRY_COMPARE(QString::fromLatin1(client.peek(64)),
                     QStringLiteral("ID019;FA00021074000;"));

        s.stop();
    }

    void a_command_split_across_packets_is_reassembled()
    {
        TestMockRadioModel mock;
        CatServer s(&mock);
        QVERIFY(s.start(QHostAddress::LocalHost, 0));

        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, s.port());
        QVERIFY(client.waitForConnected(3000));
        QTRY_COMPARE(s.clientCount(), 1);

        // Half a command, then the rest -- a real serial-over-TCP client
        // does this all the time.
        client.write("FA000142");
        QVERIFY(client.waitForBytesWritten(3000));
        QTest::qWait(50);
        client.write("50000;FA;");
        QTRY_VERIFY(client.bytesAvailable() > 0);
        QTRY_COMPARE(QString::fromLatin1(client.peek(32)),
                     QStringLiteral("FA00014250000;"));

        s.stop();
    }

    void stopping_the_server_closes_the_transmit_gate()
    {
        TestMockRadioModel mock;
        CatServer s(&mock);
        QVERIFY(s.start(QHostAddress::LocalHost, 0));
        s.setPttAllowed(true);
        QVERIFY(s.pttAllowed());

        s.stop();
        QVERIFY(!s.pttAllowed());
        QCOMPARE(s.handleCommand(QStringLiteral("TX;")), QStringLiteral("?;"));
    }

    void server_reports_refused_tx()
    {
        TestMockRadioModel mock;
        CatServer s(&mock);
        QSignalSpy refused(&s, &CatServer::txRefused);
        QCOMPARE(s.handleCommand(QStringLiteral("TX;")), QStringLiteral("?;"));
        QCOMPARE(refused.count(), 1);
    }
};

QTEST_MAIN(TestCatServer)
#include "tst_cat_server.moc"
