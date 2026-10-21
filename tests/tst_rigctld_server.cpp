// no-port-check: Longpath-original test for the rigctld bridge
// (phase 3K, first step, 2026-10-11).
//
// Two halves:
//   1. The protocol, driven through RigctldServer::handleLine against
//      TestMockRadioModel -- every command a real client sends on
//      connect (read off the wire from Hamlib 4.7.2, transcript in
//      docs/architecture/2026-10-11-rigctld-verification/README.md),
//      plus the refusals.
//   2. The socket, driven through a real QTcpSocket against a listening
//      server, so the line framing and the `q` teardown are proven and
//      not just assumed.
//
// The transmit gate gets the most attention here on purpose: a program
// that can tune the radio must not be able to key it unless that was
// turned on, and turning the server off must close the gate again.

#include <QtTest/QtTest>
#include <QtNetwork/QTcpSocket>

#include "core/RigctldServer.h"
#include "core/WdspTypes.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "TestMockRadioModel.h"

using namespace Longpath;

class TestRigctldServer : public QObject {
    Q_OBJECT

private slots:
    // ── The connect sequence a real client runs ─────────────────────────────

    void connect_sequence_answers_like_rigctld()
    {
        TestMockRadioModel mock;
        RigctldServer s(&mock);

        QCOMPARE(s.handleLine(QStringLiteral("\\get_powerstat")), QStringLiteral("1\n"));
        QCOMPARE(s.handleLine(QStringLiteral("\\chk_vfo")), QStringLiteral("0\n"));

        const QString state = s.handleLine(QStringLiteral("\\dump_state"));
        QVERIFY(state.endsWith(QStringLiteral("done\n")));
        QVERIFY(state.startsWith(QStringLiteral("1\n")));
        QVERIFY(state.contains(QStringLiteral("has_set_freq=1")));
        QVERIFY(state.contains(QStringLiteral("has_get_freq=1")));
        QVERIFY(state.contains(QStringLiteral("ptt_type=0x1")));
        // The mode mask must name the modes we really have.
        QVERIFY(state.contains(QStringLiteral("0xebf")));

        QCOMPARE(s.handleLine(QStringLiteral("v")), QStringLiteral("VFOA\n"));
        QCOMPARE(s.handleLine(QStringLiteral("l KEYSPD")), QStringLiteral("0\n"));
    }

    void verbose_prefix_is_accepted()
    {
        // A client may ask for the verbose answer form with a leading '+'.
        // We answer the plain form either way; what matters is that the
        // command is still recognised.
        TestMockRadioModel mock;
        RigctldServer s(&mock);
        QCOMPARE(s.handleLine(QStringLiteral("+\\chk_vfo")), QStringLiteral("0\n"));
        QCOMPARE(s.handleLine(QStringLiteral("+f")), s.handleLine(QStringLiteral("f")));
    }

    // ── Frequency ───────────────────────────────────────────────────────────

    void frequency_roundtrips()
    {
        TestMockRadioModel mock;
        RigctldServer s(&mock);

        QCOMPARE(s.handleLine(QStringLiteral("F 14250000")), QStringLiteral("RPRT 0\n"));
        QCOMPARE(s.handleLine(QStringLiteral("f")), QStringLiteral("14250000\n"));

        // Clients also send a decimal point.
        QCOMPARE(s.handleLine(QStringLiteral("F 7100000.000000")), QStringLiteral("RPRT 0\n"));
        QCOMPARE(s.handleLine(QStringLiteral("f")), QStringLiteral("7100000\n"));
    }

    void nonsense_frequency_is_refused()
    {
        TestMockRadioModel mock;
        RigctldServer s(&mock);
        s.handleLine(QStringLiteral("F 14250000"));

        QCOMPARE(s.handleLine(QStringLiteral("F abc")), QStringLiteral("RPRT -1\n"));
        QCOMPARE(s.handleLine(QStringLiteral("F -1")), QStringLiteral("RPRT -1\n"));
        QCOMPARE(s.handleLine(QStringLiteral("F")), QStringLiteral("RPRT -1\n"));
        // and the radio stayed where it was
        QCOMPARE(s.handleLine(QStringLiteral("f")), QStringLiteral("14250000\n"));
    }

    // ── Mode ────────────────────────────────────────────────────────────────

    void mode_maps_both_ways()
    {
        TestMockRadioModel mock;
        RigctldServer s(&mock);

        QCOMPARE(s.handleLine(QStringLiteral("M CW 500")), QStringLiteral("RPRT 0\n"));
        // CW on the wire is CWU for us; the answer comes back as CW.
        QVERIFY(s.handleLine(QStringLiteral("m")).startsWith(QStringLiteral("CW\n")));

        QCOMPARE(s.handleLine(QStringLiteral("M CWR 0")), QStringLiteral("RPRT 0\n"));
        QVERIFY(s.handleLine(QStringLiteral("m")).startsWith(QStringLiteral("CWR\n")));

        QCOMPARE(s.handleLine(QStringLiteral("M PKTUSB 3000")), QStringLiteral("RPRT 0\n"));
        QVERIFY(s.handleLine(QStringLiteral("m")).startsWith(QStringLiteral("PKTUSB\n")));

        QCOMPARE(s.handleLine(QStringLiteral("M USB 2400")), QStringLiteral("RPRT 0\n"));
        const QString answer = s.handleLine(QStringLiteral("m"));
        QCOMPARE(answer, QStringLiteral("USB\n2400\n"));
    }

    void unknown_mode_is_refused()
    {
        TestMockRadioModel mock;
        RigctldServer s(&mock);
        s.handleLine(QStringLiteral("M USB 2400"));

        QCOMPARE(s.handleLine(QStringLiteral("M WFM 150000")), QStringLiteral("RPRT -1\n"));
        QCOMPARE(s.handleLine(QStringLiteral("M")), QStringLiteral("RPRT -1\n"));
        // unchanged
        QVERIFY(s.handleLine(QStringLiteral("m")).startsWith(QStringLiteral("USB\n")));
    }

    void width_zero_keeps_the_passband()
    {
        TestMockRadioModel mock;
        RigctldServer s(&mock);
        s.handleLine(QStringLiteral("M USB 2400"));
        const QString before = s.handleLine(QStringLiteral("m"));
        s.handleLine(QStringLiteral("M USB 0"));
        QCOMPARE(s.handleLine(QStringLiteral("m")), before);
    }

    // ── VFO and split ───────────────────────────────────────────────────────

    void vfo_a_is_accepted_others_refused()
    {
        TestMockRadioModel mock;
        RigctldServer s(&mock);
        QCOMPARE(s.handleLine(QStringLiteral("V VFOA")), QStringLiteral("RPRT 0\n"));
        QCOMPARE(s.handleLine(QStringLiteral("V currVFO")), QStringLiteral("RPRT 0\n"));
        QCOMPARE(s.handleLine(QStringLiteral("V VFOB")), QStringLiteral("RPRT -1\n"));
    }

    void split_can_be_read_but_not_set()
    {
        // Longpath deleted the split setter on purpose (RadioModel.h on
        // split()); the supported client setting is "Fake It".
        TestMockRadioModel mock;
        RigctldServer s(&mock);
        QCOMPARE(s.handleLine(QStringLiteral("s")), QStringLiteral("0\nVFOB\n"));
        QCOMPARE(s.handleLine(QStringLiteral("S 1 VFOB")), QStringLiteral("RPRT -11\n"));
    }

    // ── The transmit gate ───────────────────────────────────────────────────

    void ptt_is_refused_until_it_is_allowed()
    {
        TestMockRadioModel mock;
        RigctldServer s(&mock);
        QVERIFY(!s.pttAllowed());             // closed by default

        QSignalSpy refused(&s, &RigctldServer::pttRefused);
        QCOMPARE(s.handleLine(QStringLiteral("T 1")), QStringLiteral("RPRT -1\n"));
        QCOMPARE(refused.count(), 1);
        QVERIFY(!mock.mox());                 // nothing reached the MOX path
        QCOMPARE(s.handleLine(QStringLiteral("t")), QStringLiteral("0\n"));

        s.setPttAllowed(true);
        QCOMPARE(s.handleLine(QStringLiteral("T 1")), QStringLiteral("RPRT 0\n"));
        QVERIFY(mock.mox());
        QCOMPARE(s.handleLine(QStringLiteral("t")), QStringLiteral("1\n"));
        QCOMPARE(refused.count(), 1);         // no new refusal
    }

    void unkeying_works_even_with_the_gate_closed()
    {
        // A client must always be able to stop transmitting, whatever the
        // gate says -- otherwise closing it mid-transmission would strand
        // the radio keyed.
        TestMockRadioModel mock;
        RigctldServer s(&mock);
        s.setPttAllowed(true);
        s.handleLine(QStringLiteral("T 1"));
        QVERIFY(mock.mox());

        s.setPttAllowed(false);
        QCOMPARE(s.handleLine(QStringLiteral("T 0")), QStringLiteral("RPRT 0\n"));
        QVERIFY(!mock.mox());
    }

    void stopping_the_server_closes_the_gate()
    {
        TestMockRadioModel mock;
        RigctldServer s(&mock);
        QVERIFY(s.start(QHostAddress::LocalHost, 0));
        s.setPttAllowed(true);
        QVERIFY(s.pttAllowed());

        s.stop();
        QVERIFY(!s.pttAllowed());

        // and it stays closed after a restart
        QVERIFY(s.start(QHostAddress::LocalHost, 0));
        QVERIFY(!s.pttAllowed());
        QCOMPARE(s.handleLine(QStringLiteral("T 1")), QStringLiteral("RPRT -1\n"));
        s.stop();
    }

    // ── Unknown commands ────────────────────────────────────────────────────

    void unknown_command_says_not_implemented()
    {
        TestMockRadioModel mock;
        RigctldServer s(&mock);
        QCOMPARE(s.handleLine(QStringLiteral("\\dump_caps")), QStringLiteral("RPRT -11\n"));
        QCOMPARE(s.handleLine(QStringLiteral("w ZZZZ")), QStringLiteral("RPRT -11\n"));
        QCOMPARE(s.handleLine(QStringLiteral("l RFPOWER")), QStringLiteral("RPRT -11\n"));
    }

    void strength_is_db_relative_to_s9()
    {
        TestMockRadioModel mock;
        RigctldServer s(&mock);

        // Hamlib's STRENGTH is dB relative to S9, and S9 is -73 dBm.
        mock.setRxSignalDbmForTest(0, -73.0);
        QCOMPARE(s.handleLine(QStringLiteral("l STRENGTH")), QStringLiteral("0\n"));
        mock.setRxSignalDbmForTest(0, -53.0);
        QCOMPARE(s.handleLine(QStringLiteral("l STRENGTH")), QStringLiteral("20\n"));
        mock.setRxSignalDbmForTest(0, -93.0);
        QCOMPARE(s.handleLine(QStringLiteral("l STRENGTH")), QStringLiteral("-20\n"));

        // No WDSP channel: no reading, rather than a made-up weak signal.
        mock.setRxSignalDbmForTest(0, -200.0);
        QCOMPARE(s.handleLine(QStringLiteral("l STRENGTH")), QStringLiteral("RPRT -11\n"));
    }

    void quit_returns_a_null_reply()
    {
        TestMockRadioModel mock;
        RigctldServer s(&mock);
        QVERIFY(s.handleLine(QStringLiteral("q")).isNull());
        QVERIFY(s.handleLine(QStringLiteral("quit")).isNull());
    }

    // ── The socket ──────────────────────────────────────────────────────────

    void a_real_client_can_read_the_frequency()
    {
        TestMockRadioModel mock;
        RigctldServer s(&mock);
        QVERIFY(s.start(QHostAddress::LocalHost, 0));
        const quint16 port = s.port();
        QVERIFY(port != 0);

        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, port);
        QVERIFY(client.waitForConnected(3000));
        QTRY_COMPARE(s.clientCount(), 1);

        s.handleLine(QStringLiteral("F 21074000"));
        s.handleLine(QStringLiteral("M USB 2400"));   // pin mode and passband

        // waitForReadyRead would deadlock here: client and server share this
        // thread, so blocking in the client starves the server's readyRead.
        // QTRY_* keeps the event loop turning, which is what both sides need.
        client.write("f\n");
        QTRY_VERIFY(client.canReadLine());
        QCOMPARE(QString::fromLatin1(client.readLine()).trimmed(),
                 QStringLiteral("21074000"));

        // Two commands in one write must both be answered, in order.
        client.write("m\nv\n");
        QTRY_VERIFY(client.canReadLine());
        QCOMPARE(QString::fromLatin1(client.readLine()).trimmed(),
                 QStringLiteral("USB"));            // m, first line
        QTRY_VERIFY(client.canReadLine());
        QCOMPARE(QString::fromLatin1(client.readLine()).trimmed(),
                 QStringLiteral("2400"));           // m, passband
        QTRY_VERIFY(client.canReadLine());
        QCOMPARE(QString::fromLatin1(client.readLine()).trimmed(),
                 QStringLiteral("VFOA"));           // v

        client.write("q\n");
        QTRY_COMPARE(s.clientCount(), 0);
        s.stop();
    }

    // ── Long-form getters: value AND status ─────────────────────────────────

    void lock_mode_answers_value_then_rprt()
    {
        // This is the shape that cost a 20-second stall at the wire on
        // 2026-10-11: a long-form getter answered with a bare RPRT left
        // the client waiting for a value, and the NEXT command (M CW 500)
        // looked like it had been ignored. Value first, status second.
        TestMockRadioModel mock;
        RigctldServer s(&mock);

        QCOMPARE(s.handleLine(QStringLiteral("\\get_lock_mode")),
                 QStringLiteral("0\nRPRT 0\n"));

        QCOMPARE(s.handleLine(QStringLiteral("\\set_lock_mode 1")),
                 QStringLiteral("RPRT 0\n"));
        QVERIFY(mock.lock(0));
        QCOMPARE(s.handleLine(QStringLiteral("\\get_lock_mode")),
                 QStringLiteral("1\nRPRT 0\n"));

        QCOMPARE(s.handleLine(QStringLiteral("\\set_lock_mode 0")),
                 QStringLiteral("RPRT 0\n"));
        QVERIFY(!mock.lock(0));
        QCOMPARE(s.handleLine(QStringLiteral("\\set_lock_mode")),
                 QStringLiteral("RPRT -1\n"));
    }

    void dump_state_declares_mode_as_targetable()
    {
        // targetable_vfo must carry RIG_TARGETABLE_MODE (0x2) as well as
        // FREQ (0x1): with 0x1 alone the client refuses set_mode on its
        // own side and nothing reaches us. Also found at the wire.
        TestMockRadioModel mock;
        RigctldServer s(&mock);
        const QString state = s.handleLine(QStringLiteral("\\dump_state"));
        QVERIFY(state.contains(QStringLiteral("targetable_vfo=0x3")));
        // The range lists' vfo mask has to match what the client is on,
        // or Hamlib finds no range to validate against and refuses.
        QVERIFY(state.contains(QStringLiteral("0x17e00007")));
    }

    // ── Against the real model, not the mock ────────────────────────────────

    void frequency_and_mode_reach_a_real_slice()
    {
        // The wire run on 2026-10-11 set the frequency fine but came back
        // with the old mode, so this pins both against a real RadioModel
        // with a real slice -- the mock cannot tell us whether
        // SliceModel::setDspMode actually took.
        RadioModel radio;
        radio.addSlice();
        SliceModel* const slice = radio.sliceById(0);
        QVERIFY(slice);

        RigctldServer s(&radio);
        QCOMPARE(s.handleLine(QStringLiteral("F 14250000")), QStringLiteral("RPRT 0\n"));
        QCOMPARE(qint64(slice->frequency()), qint64(14250000));
        QCOMPARE(s.handleLine(QStringLiteral("f")), QStringLiteral("14250000\n"));

        QCOMPARE(s.handleLine(QStringLiteral("M CW 500")), QStringLiteral("RPRT 0\n"));
        QCOMPARE(slice->dspMode(), DSPMode::CWU);
        QVERIFY2(s.handleLine(QStringLiteral("m")).startsWith(QStringLiteral("CW\n")),
                 qPrintable(QStringLiteral("mode query answered: ")
                            + s.handleLine(QStringLiteral("m"))));

        QCOMPARE(s.handleLine(QStringLiteral("M LSB 0")), QStringLiteral("RPRT 0\n"));
        QCOMPARE(slice->dspMode(), DSPMode::LSB);
    }

    void a_set_that_cannot_take_is_refused()
    {
        // No slice at all -- a client that connected before the radio.
        // RPRT 0 here would be invisible to the client: Hamlib answers
        // its own `f` from its frequency cache and would report the value
        // it asked for. Measured against the running app on 2026-10-11:
        // rigctl said 18100000 while the radio still held 0.
        RadioModel radio;
        RigctldServer s(&radio);

        QCOMPARE(s.handleLine(QStringLiteral("F 14250000")), QStringLiteral("RPRT -1\n"));
        QCOMPARE(s.handleLine(QStringLiteral("f")), QStringLiteral("0\n"));
        QCOMPARE(s.handleLine(QStringLiteral("M CW 500")), QStringLiteral("RPRT -1\n"));
    }

    void a_taken_port_fails_cleanly()
    {
        TestMockRadioModel mock;
        RigctldServer first(&mock);
        QVERIFY(first.start(QHostAddress::LocalHost, 0));
        const quint16 port = first.port();

        RigctldServer second(&mock);
        QVERIFY(!second.start(QHostAddress::LocalHost, port));
        QVERIFY(!second.isListening());
        first.stop();
    }
};

QTEST_MAIN(TestRigctldServer)
#include "tst_rigctld_server.moc"
