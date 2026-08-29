// no-port-check: NereusSDR/Longpath-original test file.

// =================================================================
// tests/tst_playback_radio_connection.cpp  (NereusSDR/Longpath)
// =================================================================
//
// Phase 3M-C (docs/architecture/phase3m-recording-plan.md).
// PlaybackRadioConnection reads a real IqRecorder-written WAV+JSON
// pair (round-tripped through the actual recorder, not hand-built
// bytes — the same fixture IqRecorder's own tests use) and confirms
// it re-emits the exact samples via the normal iqDataReceived path,
// with the same ConnectionState discipline every other RadioConnection
// in this project follows.
//
// Covers plan doc C.1-C.4:
//   C.1 skeleton — every TX-shaped pure virtual is a genuine no-op.
//   C.2 file reader + timer-paced emission — round-trip sample values.
//   C.3 non-discovery construction — openRecording() before
//       connectToRadio(), no RadioInfo/discovery involved.
//   C.4 end-of-file behaviour — Stop (default) vs. Loop.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/PlaybackRadioConnection.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/audio/IqRecorder.h"

using namespace Longpath;

namespace {

QVector<float> iqBlock(int frames, float iValue, float qValue)
{
    QVector<float> out(frames * 2);
    for (int i = 0; i < frames; ++i) {
        out[i * 2]     = iValue;
        out[i * 2 + 1] = qValue;
    }
    return out;
}

IqRecordingInfo someRecordingInfo()
{
    IqRecordingInfo i;
    i.utcStart  = QDateTime(QDate(2026, 8, 26), QTime(9, 0), Qt::UTC);
    i.frequency = QStringLiteral("14.074.000");
    i.mode      = QStringLiteral("USB");
    i.band      = QStringLiteral("20m");
    return i;
}

} // namespace

class TestPlaybackRadioConnection : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString path(const QString& n) const
    {
        return m_dir.path() + QLatin1Char('/') + n;
    }

    // Writes a real WAV+JSON pair via IqRecorder itself (the same
    // fixture that class's own tests use) — a genuine round-trip
    // fixture, not hand-built bytes. blockCount blocks of blockFrames
    // frames each, each block carrying a distinct (I,Q) value pair so
    // block boundaries are individually verifiable.
    QString writeFixture(const QString& name, int sampleRate,
                         int blockFrames, int blockCount)
    {
        IqRecorder r;
        r.setSampleRate(sampleRate);
        r.setSaveFloat32(true);
        const QString p = path(name);
        const bool started = r.start(p, someRecordingInfo());
        Q_ASSERT(started);
        for (int b = 0; b < blockCount; ++b) {
            const float iv = 0.1f * static_cast<float>(b + 1);
            const float qv = -0.1f * static_cast<float>(b + 1);
            const auto block = iqBlock(blockFrames, iv, qv);
            r.feed(block.constData(), blockFrames);
        }
        r.stop();
        return p;
    }

private slots:

    void initTestCase() { QVERIFY(m_dir.isValid()); }

    // ── C.1: skeleton ─────────────────────────────────────────────

    void everyTxShapedSetterIsANoOp()
    {
        PlaybackRadioConnection conn;
        conn.init();

        conn.setTxFrequency(14074000);
        conn.setAttenuator(10);
        conn.setPreamp(true);
        conn.setTxDrive(50);
        conn.setMox(true);
        conn.setAntennaRouting(AntennaRouting{});
        const float iq[4] = {0.1f, 0.1f, 0.1f, 0.1f};
        conn.sendTxIq(iq, 2);
        conn.setTrxRelay(true);
        conn.setMicBoost(true);
        conn.setLineIn(true);
        conn.setMicTipRing(true);
        conn.setMicBias(true);
        conn.setLineInGain(10);
        conn.setUserDigOut(0x0F);
        conn.setPuresignalRun(true);
        conn.setMicPTTDisabled(true);
        conn.setMicXlr(false);
        conn.setWatchdogEnabled(false);
        conn.setReceiverFrequency(0, 14074000);
        conn.setActiveReceiverCount(2);
        conn.setSampleRate(192000);

        // Reaching here without a crash/assert IS the test, plus: none
        // of the above should have changed connection state.
        QCOMPARE(conn.state(), ConnectionState::Disconnected);
    }

    void protocolVersionIsDistinctFromEveryRealProtocol()
    {
        PlaybackRadioConnection conn;
        QCOMPARE(conn.protocolVersion(), 0);
        QVERIFY(conn.protocolVersion() != static_cast<int>(ProtocolVersion::Protocol1));
        QVERIFY(conn.protocolVersion() != static_cast<int>(ProtocolVersion::Protocol2));
    }

    // ── C.3: non-discovery construction ──────────────────────────────

    void connectWithoutOpenRecordingFails()
    {
        PlaybackRadioConnection conn;
        conn.init();

        QSignalSpy failSpy(&conn, &RadioConnection::connectFailed);
        conn.connectToRadio(RadioInfo{});

        QCOMPARE(failSpy.count(), 1);
        QCOMPARE(conn.state(), ConnectionState::Disconnected);
        QVERIFY(!conn.isRecordingLoaded());
    }

    void openRecordingDoesNotRequireAnyDiscoveredRadioInfo()
    {
        const QString p = writeFixture(QStringLiteral("construct.wav"), 4000, 40, 2);

        PlaybackRadioConnection conn;
        conn.init();
        QString error;
        QVERIFY2(conn.openRecording(p, &error), qPrintable(error));
        QVERIFY(conn.isRecordingLoaded());

        // A default-constructed RadioInfo — no discovery, no MAC, no
        // address — is enough, because it's ignored entirely.
        conn.connectToRadio(RadioInfo{});
        QCOMPARE(conn.state(), ConnectionState::Connected);
    }

    void recordingInfoRoundTripsTheSidecarMetadata()
    {
        const QString p = writeFixture(QStringLiteral("meta.wav"), 4000, 40, 1);

        PlaybackRadioConnection conn;
        conn.init();
        QVERIFY(conn.openRecording(p));

        QCOMPARE(conn.recordingInfo().frequency, QStringLiteral("14.074.000"));
        QCOMPARE(conn.recordingInfo().mode, QStringLiteral("USB"));
        QCOMPARE(conn.recordingInfo().band, QStringLiteral("20m"));
        QCOMPARE(conn.recordingInfo().sampleRate, 4000);
    }

    // ── C.2: file reader + timer-paced emission ──────────────────────

    void playbackEmitsTheRecordedSamplesInOrder()
    {
        // 4000 Hz, 10 ms tick -> 40 frames/block. Two blocks of 40
        // frames each, distinct (I,Q) per source block, so a listener
        // sees exactly two emitted blocks with the right values.
        const QString p = writeFixture(QStringLiteral("replay.wav"), 4000, 40, 2);

        PlaybackRadioConnection conn;
        conn.init();
        QVERIFY(conn.openRecording(p));

        QSignalSpy iqSpy(&conn, &RadioConnection::iqDataReceived);
        conn.connectToRadio(RadioInfo{});

        // Two ticks (20ms) plus slack for the recording to fully play
        // and stop (default EOF behaviour).
        QTRY_VERIFY_WITH_TIMEOUT(iqSpy.count() >= 2, 500);

        const int index0 = iqSpy.at(0).at(0).toInt();
        const auto samples0 = iqSpy.at(0).at(1).value<QVector<float>>();
        QCOMPARE(index0, 0);
        QCOMPARE(samples0.size(), 80);  // 40 frames * 2
        QCOMPARE(samples0.first(), 0.1f);
        QCOMPARE(samples0.at(1), -0.1f);

        const auto samples1 = iqSpy.at(1).at(1).value<QVector<float>>();
        QCOMPARE(samples1.first(), 0.2f);
        QCOMPARE(samples1.at(1), -0.2f);
    }

    void monoRecordingIsDoubledToInterleavedIq()
    {
        // readWavStereo() must hand back a usable (L=R) pair for a
        // mono source rather than refusing it — exercised here through
        // the real playback path, not just WavFile's own unit tests.
        QString error;
        const bool wrote = writeWavMono(path(QStringLiteral("mono.wav")),
                                        QVector<float>(40, 0.42f), 4000, &error);
        QVERIFY2(wrote, qPrintable(error));

        PlaybackRadioConnection conn;
        conn.init();
        QVERIFY2(conn.openRecording(path(QStringLiteral("mono.wav")), &error),
                 qPrintable(error));

        QSignalSpy iqSpy(&conn, &RadioConnection::iqDataReceived);
        conn.connectToRadio(RadioInfo{});
        QTRY_VERIFY_WITH_TIMEOUT(iqSpy.count() >= 1, 500);

        const auto samples = iqSpy.at(0).at(1).value<QVector<float>>();
        QCOMPARE(samples.first(), 0.42f);
        QCOMPARE(samples.at(1), 0.42f);
    }

    // ── C.4: end-of-file behaviour ────────────────────────────────────

    void endOfFileStopsByDefault()
    {
        const QString p = writeFixture(QStringLiteral("eof-stop.wav"), 4000, 40, 1);

        PlaybackRadioConnection conn;
        conn.init();
        QVERIFY(conn.openRecording(p));
        QCOMPARE(conn.endOfFileBehavior(),
                 PlaybackRadioConnection::EndOfFileBehavior::Stop);

        QSignalSpy stateSpy(&conn, &RadioConnection::connectionStateChanged);
        conn.connectToRadio(RadioInfo{});

        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Disconnected, 500);
        QCOMPARE(conn.emittedBlockCountForTest(), 1);
        QCOMPARE(conn.frameCursorForTest(), qint64(0));

        // Sequence must have actually passed through Connected, not
        // just started and ended Disconnected by coincidence.
        bool sawConnected = false;
        for (int i = 0; i < stateSpy.count(); ++i) {
            if (stateSpy.at(i).at(0).value<ConnectionState>() == ConnectionState::Connected) {
                sawConnected = true;
            }
        }
        QVERIFY(sawConnected);
    }

    void endOfFileLoopsWhenConfigured()
    {
        const QString p = writeFixture(QStringLiteral("eof-loop.wav"), 4000, 40, 1);

        PlaybackRadioConnection conn;
        conn.init();
        QVERIFY(conn.openRecording(p));
        conn.setEndOfFileBehavior(PlaybackRadioConnection::EndOfFileBehavior::Loop);

        conn.connectToRadio(RadioInfo{});

        // One block per 10ms tick; wait long enough for several wraps.
        QTRY_VERIFY_WITH_TIMEOUT(conn.emittedBlockCountForTest() >= 5, 500);
        QCOMPARE(conn.state(), ConnectionState::Connected);  // never dropped
    }

    void disconnectStopsEmissionAndResetsCursor()
    {
        const QString p = writeFixture(QStringLiteral("disc.wav"), 4000, 40, 5);

        PlaybackRadioConnection conn;
        conn.init();
        QVERIFY(conn.openRecording(p));
        conn.connectToRadio(RadioInfo{});

        QTRY_VERIFY_WITH_TIMEOUT(conn.frameCursorForTest() > 0, 500);
        conn.disconnect();

        QCOMPARE(conn.state(), ConnectionState::Disconnected);
        QCOMPARE(conn.frameCursorForTest(), qint64(0));

        const int blocksAtDisconnect = conn.emittedBlockCountForTest();
        QTest::qWait(50);
        QCOMPARE(conn.emittedBlockCountForTest(), blocksAtDisconnect);  // truly stopped
    }
};

QTEST_MAIN(TestPlaybackRadioConnection)
#include "tst_playback_radio_connection.moc"
