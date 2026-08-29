// =================================================================
// tests/tst_wav_recorder_controller.cpp  (NereusSDR)
// =================================================================
//
// Was den Audio-Abgriff mit der "off the air"-Aufnahme verbindet.
//
// Geprueft wird OHNE Funkgeraet und ohne Audiogeraet — wie
// tst_qso_recorder_controller: der Test schreibt selbst in den
// Zwischenspeicher, genau wie es der Audio-Faden tut.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-25 — Original fuer NereusSDR/Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <vector>

#include "core/audio/WavRecorderController.h"

using namespace Longpath;

namespace {

WavRecordingInfo someRecording()
{
    WavRecordingInfo i;
    i.utcStart  = QDateTime(QDate(2026, 8, 25), QTime(20, 0), Qt::UTC);
    i.frequency = QStringLiteral("14.230.000");
    i.mode      = QStringLiteral("USB");
    i.band      = QStringLiteral("20m");
    return i;
}

} // namespace

class TestWavRecorderController : public QObject
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

    void nothingIsCollectedBeforeStart()
    {
        WavRecorderController c;
        c.setSampleRate(1000);

        const std::vector<float> rx(200, 0.5f);   // 100 Rahmen stereo
        c.ring().write(rx.data(), static_cast<int>(rx.size()));
        c.drainNow();

        QCOMPARE(c.recorder().framesWritten(), 0);
        QVERIFY(!c.isRecording());
    }

    void whatTheAudioThreadWritesEndsUpInTheRecording()
    {
        WavRecorderController c;
        c.setSampleRate(1000);
        QVERIFY(c.start(path(QStringLiteral("a.wav")), someRecording()));

        const std::vector<float> rx(2000, 0.25f);  // 1000 Rahmen = 1 s
        QCOMPARE(c.ring().write(rx.data(), 2000), 2000);
        c.drainNow();

        QCOMPARE(c.recorder().framesWritten(), 1000);
        QVERIFY(qAbs(c.recorder().recordedSeconds() - 1.0) < 1e-9);
    }

    void stoppingCollectsWhatIsStillWaiting()
    {
        WavRecorderController c;
        c.setSampleRate(1000);
        QVERIFY(c.start(path(QStringLiteral("b.wav")), someRecording()));

        const std::vector<float> rx(600, 0.3f);   // 300 Rahmen
        c.ring().write(rx.data(), 600);

        QSignalSpy spy(&c, &WavRecorderController::recordingChanged);
        c.stop();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), false);
        QVERIFY(!c.isRecording());
    }

    // Ein Ueberlauf darf nicht still bleiben.
    void aLossIsAnnouncedOnce()
    {
        WavRecorderController c;
        c.setSampleRate(100);          // winzige Zwischenspeicher
        QVERIFY(c.start(path(QStringLiteral("c.wav")), someRecording()));

        QSignalSpy spy(&c, &WavRecorderController::samplesLost);

        const std::vector<float> flood(5000, 0.1f);
        c.ring().write(flood.data(), 5000);
        QVERIFY2(c.droppedSamples() > 0, "so viel kann nicht passen");

        c.drainNow();
        QCOMPARE(spy.count(), 1);

        c.ring().write(flood.data(), 5000);
        c.drainNow();
        QVERIFY2(spy.count() == 1,
                 "einmal melden reicht — sonst steht die Warnung "
                 "fuenfmal je Sekunde da");
    }

    void startingAgainNeedsAFreshFile()
    {
        WavRecorderController c;
        c.setSampleRate(1000);

        QVERIFY(c.start(path(QStringLiteral("first.wav")), someRecording()));
        const std::vector<float> rx(400, 0.4f);
        c.ring().write(rx.data(), 400);
        c.drainNow();
        c.stop();
        QCOMPARE(c.recorder().framesWritten(), 200);

        QVERIFY2(c.start(path(QStringLiteral("second.wav")), someRecording()),
                 "nach stop() muss eine neue Aufnahme moeglich sein");
    }

    // Der Spitzenwert: bewegt sich nur dann, wenn wirklich etwas
    // in der Aufnahme landet.
    void peakReflectsWhatWasDrained()
    {
        WavRecorderController c;
        c.setSampleRate(1000);
        QVERIFY(c.start(path(QStringLiteral("d.wav")), someRecording()));

        const std::vector<float> rx = {0.1f, -0.1f, 0.7f, -0.2f};
        c.ring().write(rx.data(), static_cast<int>(rx.size()));
        c.drainNow();

        QVERIFY2(qAbs(c.lastPeak() - 0.7f) < 1e-6f,
                 "der groesste Betrag im letzten Abholvorgang");
    }
};

QTEST_MAIN(TestWavRecorderController)
#include "tst_wav_recorder_controller.moc"
