// =================================================================
// tests/tst_iq_recorder.cpp  (NereusSDR)
// =================================================================
//
// Rohes I/Q aufzeichnen — vor WDSP, keine Demodulation.
//
// Fast baugleich zu tst_wav_recorder.cpp; der wichtigste Unterschied
// hier ist die VORGABE: float32 statt PCM16-mit-Dither, weil eine
// I/Q-Aufnahme fuer Auswertung gedacht ist, nicht zum Anhoeren (siehe
// IqRecorder.h).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-25 — Original fuer NereusSDR/Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QTemporaryDir>

#include "core/audio/IqRecorder.h"
#include "core/audio/WavFile.h"

using namespace Longpath;

namespace {

// I,Q,I,Q… verschachtelt, fester Wert auf beiden Kanaelen.
QVector<float> iqBlock(int frames, float value)
{
    return QVector<float>(frames * 2, value);
}

IqRecordingInfo someRecording()
{
    IqRecordingInfo i;
    i.utcStart  = QDateTime(QDate(2026, 8, 25), QTime(22, 0), Qt::UTC);
    i.frequency = QStringLiteral("14.074.000");
    i.mode      = QStringLiteral("USB");
    i.band      = QStringLiteral("20m");
    return i;
}

} // namespace

class TestIqRecorder : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString path(const QString& n) const
    {
        return m_dir.path() + QLatin1Char('/') + n;
    }

private slots:

    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void feedingBeforeStartDoesNothing()
    {
        IqRecorder r;
        const auto iq = iqBlock(100, 0.5f);
        r.feed(iq.constData(), 100);
        QCOMPARE(r.framesWritten(), 0);
        QVERIFY(!r.isRecording());
    }

    void defaultsToFloat32()
    {
        IqRecorder r;
        QVERIFY2(r.saveFloat32(),
                 "I/Q-Aufnahmen sollen ohne Nachfrage keine Bit-Tiefe "
                 "verlieren — anders als WavRecorder");
    }

    void theFileExistsWhileStillRecording()
    {
        IqRecorder r;
        r.setSampleRate(1000);
        const QString p = path(QStringLiteral("live.wav"));
        QString err;
        QVERIFY2(r.start(p, someRecording(), &err), qPrintable(err));

        const auto iq = iqBlock(500, 0.3f);
        r.feed(iq.constData(), 500);

        QVERIFY2(QFile::exists(p),
                 "streamend heisst: die Datei existiert schon jetzt");
        QVERIFY(r.isRecording());
    }

    void recordedIqRoundTripsAtFullPrecision()
    {
        IqRecorder r;
        r.setSampleRate(8000);
        const QString p = path(QStringLiteral("iq.wav"));
        QVERIFY(r.start(p, someRecording()));

        const auto iq = iqBlock(4000, 0.4f);   // 0,5 s
        r.feed(iq.constData(), 4000);
        r.stop();

        QCOMPARE(r.framesWritten(), 4000);
        QVERIFY2(qAbs(r.recordedSeconds() - 0.5) < 1e-9,
                 "4000 Rahmen bei 8000 Hz sind eine halbe Sekunde");

        const WavData back = readWavMono(p);
        QVERIFY(back.ok);
        QCOMPARE(back.sampleRate, 8000);
        QCOMPARE(back.samples.size(), 4000);
        // Float32: kein Rundungsfehler, exakt 0,4 (readWavMono mittelt
        // I und Q, beide tragen denselben Wert).
        QCOMPARE(back.samples[2000], 0.4f);
    }

    void pcm16IsAvailableOnRequest()
    {
        IqRecorder r;
        r.setSaveFloat32(false);
        r.setSampleRate(8000);
        const QString p = path(QStringLiteral("pcm16.wav"));
        QVERIFY(r.start(p, someRecording()));
        const auto iq = iqBlock(100, 0.5f);
        r.feed(iq.constData(), 100);
        r.stop();

        const WavData back = readWavMono(p);
        QVERIFY(back.ok);
        QVERIFY2(qAbs(back.samples[0] - 0.5f) < 1e-4f,
                 "PCM16-Rundung darf hoechstens ein Bit kosten");
    }

    void aDescriptionIsWrittenBeside()
    {
        IqRecorder r;
        r.setSampleRate(48000);
        const QString wav = path(QStringLiteral("described.wav"));
        QVERIFY(r.start(wav, someRecording()));
        const auto iq = iqBlock(48000, 0.1f);
        r.feed(iq.constData(), 48000);
        r.stop();

        QFile j(path(QStringLiteral("described.json")));
        QVERIFY2(j.exists(), "neben der WAV liegt die Beschreibung");
        QVERIFY(j.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(j.readAll());
        QVERIFY(text.contains(QStringLiteral("14.074.000")));
        QVERIFY(text.contains(QStringLiteral("USB")));
        QVERIFY(text.contains(QStringLiteral("20m")));
        QVERIFY2(text.contains(QStringLiteral("pre-demodulation")),
                 "und sie sagt, dass es kein demodulierter Ton ist");
    }

    void theDescriptionReadsBack()
    {
        IqRecorder r;
        r.setSampleRate(8000);
        const QString wav = path(QStringLiteral("roundtrip.wav"));
        QVERIFY(r.start(wav, someRecording()));
        const auto iq = iqBlock(8000, 0.2f);
        r.feed(iq.constData(), 8000);
        r.stop();

        const IqRecordingInfo back = readIqRecordingDescription(wav);
        QCOMPARE(back.frequency, QStringLiteral("14.074.000"));
        QCOMPARE(back.mode, QStringLiteral("USB"));
        QCOMPARE(back.band, QStringLiteral("20m"));
        QCOMPARE(back.sampleRate, 8000);
        QVERIFY(qAbs(back.seconds - 1.0) < 1e-9);
    }

    void startingTwiceWithoutStoppingIsRefused()
    {
        IqRecorder r;
        r.setSampleRate(8000);
        QVERIFY(r.start(path(QStringLiteral("a.wav")), someRecording()));
        QVERIFY2(!r.start(path(QStringLiteral("b.wav")), someRecording()),
                 "eine laufende Aufnahme muss erst gestoppt werden");
    }
};

QTEST_MAIN(TestIqRecorder)
#include "tst_iq_recorder.moc"
