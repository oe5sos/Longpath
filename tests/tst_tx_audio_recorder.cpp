// Recording your own transmit audio.
//
// feed() runs on the audio thread, so the interesting failures are not
// wrong numbers but dropouts and corruption: an allocation mid-callback,
// a buffer that wraps and silently eats the start of a recording, a
// clamp that turns the loudest moment into a click.
// no-port-check: NereusSDR-original. The idea is AetherSDR's
// ClientPuduMonitor; no code is shared, and the attribution is in
// TxAudioRecorder.h.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/TxAudioRecorder.h"

#include <cmath>

using namespace NereusSDR;

namespace {

qint16 sampleAt(const QByteArray& wav, int index)
{
    const int at = 44 + index * 2;
    return static_cast<qint16>(
        static_cast<quint8>(wav.at(at))
        | (static_cast<quint8>(wav.at(at + 1)) << 8));
}

} // namespace

class TstTxAudioRecorder : public QObject {
    Q_OBJECT
private slots:
    void nothing_is_captured_until_started();
    void a_recording_starts_empty_every_time();
    void it_stops_at_thirty_seconds_and_says_so();
    void the_buffer_never_wraps_over_the_beginning();
    void loud_samples_clamp_instead_of_wrapping();
    void the_wav_header_describes_what_follows();
    void changing_the_rate_while_recording_is_refused();
    void saving_with_nothing_recorded_explains_itself();
};

void TstTxAudioRecorder::nothing_is_captured_until_started()
{
    TxAudioRecorder r;
    const std::vector<float> block(480, 0.5f);
    r.feed(block.data(), static_cast<int>(block.size()));
    QCOMPARE(r.recordedFrames(), 0);
    QVERIFY(!r.hasRecording());
    QVERIFY(r.toWav().isEmpty());
}

void TstTxAudioRecorder::a_recording_starts_empty_every_time()
{
    // Record, listen, adjust, record again is the whole workflow. If a
    // second take appended to the first, the comparison would be
    // against a mixture of both.
    TxAudioRecorder r;
    const std::vector<float> block(480, 0.25f);

    r.start();
    r.feed(block.data(), 480);
    r.stop();
    QCOMPARE(r.recordedFrames(), 480);

    r.start();
    QCOMPARE(r.recordedFrames(), 0);
    r.feed(block.data(), 100);
    QCOMPARE(r.recordedFrames(), 100);
}

void TstTxAudioRecorder::it_stops_at_thirty_seconds_and_says_so()
{
    TxAudioRecorder r;
    r.setSampleRate(8000);              // small, so the test is quick
    const int cap = 8000 * TxAudioRecorder::kMaxSeconds;

    QSignalSpy full(&r, &TxAudioRecorder::recordingFull);
    r.start();

    const std::vector<float> block(4000, 0.1f);
    for (int i = 0; i < 100; ++i) { r.feed(block.data(), 4000); }

    QCOMPARE(r.recordedFrames(), cap);
    QCOMPARE(r.recordedSeconds(), double(TxAudioRecorder::kMaxSeconds));

    // Once, not once per block for as long as the operator keeps
    // talking — the queued emit would otherwise flood the event loop.
    QVERIFY(full.wait(1000));
    QCoreApplication::processEvents();
    QCOMPARE(full.count(), 1);
}

void TstTxAudioRecorder::the_buffer_never_wraps_over_the_beginning()
{
    // Overrunning is handled by dropping the tail, not by wrapping.
    // Wrapping would quietly replace the start of a recording the
    // operator is still speaking into, and they would play it back and
    // hear a take that never happened.
    TxAudioRecorder r;
    r.setSampleRate(1000);
    const int cap = 1000 * TxAudioRecorder::kMaxSeconds;

    r.start();
    const std::vector<float> first(cap, 0.5f);
    r.feed(first.data(), cap);
    const std::vector<float> second(1000, -0.9f);
    r.feed(second.data(), 1000);
    r.stop();

    const QByteArray wav = r.toWav();
    QCOMPARE(r.recordedFrames(), cap);
    // The first sample is still from the first block.
    QVERIFY(sampleAt(wav, 0) > 0);
    // And so is the last: the overrun was dropped, not written over
    // the beginning.
    QVERIFY(sampleAt(wav, cap - 1) > 0);
}

void TstTxAudioRecorder::loud_samples_clamp_instead_of_wrapping()
{
    // The transmit chain can produce values above 1.0 before the
    // limiter. Scaled without a clamp, 1.5 wraps to a large negative
    // number and puts a click exactly where the operator was loudest —
    // which sounds like a fault in the microphone.
    TxAudioRecorder r;
    r.setSampleRate(8000);
    r.start();
    const std::vector<float> hot = {1.5f, -1.5f, 1.0f, -1.0f, 0.0f};
    r.feed(hot.data(), static_cast<int>(hot.size()));
    r.stop();

    const QByteArray wav = r.toWav();
    QCOMPARE(sampleAt(wav, 0), qint16(32767));
    QCOMPARE(sampleAt(wav, 1), qint16(-32767));
    QCOMPARE(sampleAt(wav, 2), qint16(32767));
    QCOMPARE(sampleAt(wav, 3), qint16(-32767));
    QCOMPARE(sampleAt(wav, 4), qint16(0));
}

void TstTxAudioRecorder::the_wav_header_describes_what_follows()
{
    TxAudioRecorder r;
    r.setSampleRate(12000);
    r.start();
    const std::vector<float> block(1200, 0.5f);
    r.feed(block.data(), 1200);
    r.stop();

    const QByteArray wav = r.toWav();
    QCOMPARE(wav.left(4), QByteArray("RIFF"));
    QCOMPARE(wav.mid(8, 4), QByteArray("WAVE"));
    QCOMPARE(wav.mid(36, 4), QByteArray("data"));
    // 44-byte header plus two bytes per frame, and the length field has
    // to agree with the file or players truncate or read past the end.
    QCOMPARE(wav.size(), 44 + 1200 * 2);
    const quint32 dataLen =
        static_cast<quint8>(wav.at(40))
        | (static_cast<quint8>(wav.at(41)) << 8)
        | (static_cast<quint8>(wav.at(42)) << 16)
        | (static_cast<quint8>(wav.at(43)) << 24);
    QCOMPARE(dataLen, quint32(1200 * 2));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("check.wav"));
    QVERIFY(r.saveWav(path));
    QCOMPARE(QFileInfo(path).size(), qint64(wav.size()));
}

void TstTxAudioRecorder::changing_the_rate_while_recording_is_refused()
{
    // Resizing the buffer while the audio thread is writing into it is
    // a use-after-free. Refusing is the only safe answer; the caller
    // can stop first.
    TxAudioRecorder r;
    r.setSampleRate(8000);
    r.start();
    r.setSampleRate(48000);
    QCOMPARE(r.sampleRate(), 8000);

    r.stop();
    r.setSampleRate(48000);
    QCOMPARE(r.sampleRate(), 48000);
}

void TstTxAudioRecorder::saving_with_nothing_recorded_explains_itself()
{
    TxAudioRecorder r;
    QTemporaryDir dir;
    QString err;
    QVERIFY(!r.saveWav(dir.filePath(QStringLiteral("empty.wav")), &err));
    QVERIFY(!err.isEmpty());
}

QTEST_MAIN(TstTxAudioRecorder)
#include "tst_tx_audio_recorder.moc"
