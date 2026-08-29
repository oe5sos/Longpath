// =================================================================
// tests/tst_wav_recorder.cpp  (NereusSDR)
// =================================================================
//
// "Off the air"-Aufnahme: nur was ankommt, keine eigene Stimme.
//
// Anders als tst_qso_recorder gibt es hier keine Ausrichtungsfrage —
// nur eine Quelle. Der wichtigste Fall hier ist STREAMEND: die Datei
// muss waehrend der Aufnahme schon auf der Platte stehen (siehe
// WavRecorder.h), nicht erst bei stop().
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-25 — Original fuer NereusSDR/Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QTemporaryDir>

#include "core/audio/WavFile.h"
#include "core/audio/WavRecorder.h"

using namespace Longpath;

namespace {

QVector<float> rxBlock(int frames, float value)
{
    return QVector<float>(frames * 2, value);
}

WavRecordingInfo someRecording()
{
    WavRecordingInfo i;
    i.utcStart  = QDateTime(QDate(2026, 8, 25), QTime(19, 0), Qt::UTC);
    i.frequency = QStringLiteral("7.165.000");
    i.mode      = QStringLiteral("LSB");
    i.band      = QStringLiteral("40m");
    return i;
}

} // namespace

class TestWavRecorder : public QObject
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
        WavRecorder r;
        const auto rx = rxBlock(100, 0.5f);
        r.feed(rx.constData(), 100);
        QCOMPARE(r.framesWritten(), 0);
        QVERIFY(!r.isRecording());
    }

    // DIE HAUPTSACHE: die Datei steht schon auf der Platte, waehrend
    // aufgenommen wird — nicht erst nach stop(). Eine abgestuerzte
    // Aufnahme darf nicht komplett verloren sein.
    void theFileExistsWhileStillRecording()
    {
        WavRecorder r;
        r.setSampleRate(1000);
        const QString p = path(QStringLiteral("live.wav"));
        QString err;
        QVERIFY2(r.start(p, someRecording(), &err), qPrintable(err));

        const auto rx = rxBlock(500, 0.3f);
        r.feed(rx.constData(), 500);

        QVERIFY2(QFile::exists(p),
                 "streamend heisst: die Datei existiert schon jetzt");
        QVERIFY(r.isRecording());
    }

    void recordedAudioRoundTrips()
    {
        WavRecorder r;
        r.setSampleRate(8000);
        const QString p = path(QStringLiteral("audio.wav"));
        QVERIFY(r.start(p, someRecording()));

        const auto rx = rxBlock(4000, 0.4f);   // 0,5 s
        r.feed(rx.constData(), 4000);
        r.stop();

        QCOMPARE(r.framesWritten(), 4000);
        QVERIFY2(qAbs(r.recordedSeconds() - 0.5) < 1e-9,
                 "4000 Rahmen bei 8000 Hz sind eine halbe Sekunde");

        const WavData back = readWavMono(p);
        QVERIFY(back.ok);
        QCOMPARE(back.sampleRate, 8000);
        QCOMPARE(back.samples.size(), 4000);
        // readWavMono mittelt L/R; beide Kanaele tragen 0,4, der
        // Dither traegt bis zu einem Bit bei PCM16 (Vorgabe).
        QVERIFY(qAbs(back.samples[2000] - 0.4f) < 1e-3f);
    }

    void float32IsUsedWhenAsked()
    {
        WavRecorder r;
        r.setSaveFloat32(true);
        r.setSampleRate(8000);
        const QString p = path(QStringLiteral("f32.wav"));
        QVERIFY(r.start(p, someRecording()));
        const auto rx = rxBlock(100, 0.5f);
        r.feed(rx.constData(), 100);
        r.stop();

        // Float32 rundet nicht: exakt 0,5, kein Dither-Rest.
        const WavData back = readWavMono(p);
        QVERIFY(back.ok);
        QCOMPARE(back.samples[0], 0.5f);
    }

    // Die Beschreibung daneben, wie bei der QSO-Aufnahme.
    void aDescriptionIsWrittenBeside()
    {
        WavRecorder r;
        r.setSampleRate(48000);
        const QString wav = path(QStringLiteral("described.wav"));
        QVERIFY(r.start(wav, someRecording()));
        const auto rx = rxBlock(48000, 0.1f);
        r.feed(rx.constData(), 48000);
        r.stop();

        QFile j(path(QStringLiteral("described.json")));
        QVERIFY2(j.exists(), "neben der WAV liegt die Beschreibung");
        QVERIFY(j.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(j.readAll());
        QVERIFY(text.contains(QStringLiteral("7.165.000")));
        QVERIFY(text.contains(QStringLiteral("LSB")));
        QVERIFY(text.contains(QStringLiteral("40m")));
        QVERIFY2(text.contains(QStringLiteral("off the air")),
                 "und sie sagt, dass es keine QSO-Aufnahme ist");
    }

    void theDescriptionReadsBack()
    {
        WavRecorder r;
        r.setSampleRate(8000);
        const QString wav = path(QStringLiteral("roundtrip.wav"));
        QVERIFY(r.start(wav, someRecording()));
        const auto rx = rxBlock(8000, 0.2f);
        r.feed(rx.constData(), 8000);
        r.stop();

        const WavRecordingInfo back = readWavRecordingDescription(wav);
        QCOMPARE(back.frequency, QStringLiteral("7.165.000"));
        QCOMPARE(back.mode, QStringLiteral("LSB"));
        QCOMPARE(back.band, QStringLiteral("40m"));
        QCOMPARE(back.sampleRate, 8000);
        QVERIFY(qAbs(back.seconds - 1.0) < 1e-9);
    }

    void startingTwiceWithoutStoppingIsRefused()
    {
        WavRecorder r;
        r.setSampleRate(8000);
        QVERIFY(r.start(path(QStringLiteral("a.wav")), someRecording()));
        QVERIFY2(!r.start(path(QStringLiteral("b.wav")), someRecording()),
                 "eine laufende Aufnahme muss erst gestoppt werden");
    }

    void startingAgainAfterStopBeginsFresh()
    {
        WavRecorder r;
        r.setSampleRate(1000);
        QVERIFY(r.start(path(QStringLiteral("first.wav")), someRecording()));
        const auto rx = rxBlock(300, 0.3f);
        r.feed(rx.constData(), 300);
        r.stop();
        QCOMPARE(r.framesWritten(), 300);

        QVERIFY(r.start(path(QStringLiteral("second.wav")), someRecording()));
        QCOMPARE(r.framesWritten(), 0);
    }
};

QTEST_MAIN(TestWavRecorder)
#include "tst_wav_recorder.moc"
