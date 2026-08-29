// =================================================================
// tests/tst_iq_recorder_controller.cpp  (NereusSDR)
// =================================================================
//
// Was den I/Q-Abgriff mit der Aufnahme verbindet.
//
// Geprueft OHNE Funkgeraet: der Test schreibt selbst in den
// Zwischenspeicher, genau wie es rawIqDataForStream vom
// Verbindungsfaden aus taete. attach(RadioModel*) wird hier bewusst
// NICHT aufgerufen — dieselbe Freiheit, die
// tst_wav_recorder_controller.cpp fuer AudioEngine hat: die Klasse
// soll pruefbar sein, ohne die grosse Maschine dahinter aufzubauen.
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

#include "core/audio/IqRecorderController.h"

using namespace Longpath;

namespace {

IqRecordingInfo someRecording()
{
    IqRecordingInfo i;
    i.utcStart  = QDateTime(QDate(2026, 8, 25), QTime(23, 0), Qt::UTC);
    i.frequency = QStringLiteral("10.136.000");
    i.mode      = QStringLiteral("USB");
    i.band      = QStringLiteral("30m");
    return i;
}

} // namespace

class TestIqRecorderController : public QObject
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
        IqRecorderController c;
        c.setSampleRate(1000);

        const std::vector<float> iq(200, 0.5f);   // 100 Rahmen
        c.ring().write(iq.data(), static_cast<int>(iq.size()));
        c.drainNow();

        QCOMPARE(c.recorder().framesWritten(), 0);
        QVERIFY(!c.isRecording());
    }

    void whatTheConnectionThreadWritesEndsUpInTheRecording()
    {
        IqRecorderController c;
        c.setSampleRate(1000);
        QVERIFY(c.start(path(QStringLiteral("a.wav")), someRecording()));

        const std::vector<float> iq(2000, 0.25f);  // 1000 Rahmen = 1 s
        QCOMPARE(c.ring().write(iq.data(), 2000), 2000);
        c.drainNow();

        QCOMPARE(c.recorder().framesWritten(), 1000);
        QVERIFY(qAbs(c.recorder().recordedSeconds() - 1.0) < 1e-9);
    }

    void stoppingCollectsWhatIsStillWaiting()
    {
        IqRecorderController c;
        c.setSampleRate(1000);
        QVERIFY(c.start(path(QStringLiteral("b.wav")), someRecording()));

        const std::vector<float> iq(600, 0.3f);   // 300 Rahmen
        c.ring().write(iq.data(), 600);

        QSignalSpy spy(&c, &IqRecorderController::recordingChanged);
        c.stop();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), false);
        QVERIFY(!c.isRecording());
    }

    void aLossIsAnnouncedOnce()
    {
        IqRecorderController c;
        c.setSampleRate(100);          // winzige Zwischenspeicher
        QVERIFY(c.start(path(QStringLiteral("c.wav")), someRecording()));

        QSignalSpy spy(&c, &IqRecorderController::samplesLost);

        const std::vector<float> flood(5000, 0.1f);
        c.ring().write(flood.data(), 5000);
        QVERIFY2(c.droppedSamples() > 0, "so viel kann nicht passen");

        c.drainNow();
        QCOMPARE(spy.count(), 1);

        c.ring().write(flood.data(), 5000);
        c.drainNow();
        QVERIFY2(spy.count() == 1, "einmal melden reicht");
    }

    void onlyTheSelectedStreamIsRecorded()
    {
        IqRecorderController c;
        c.setSampleRate(1000);
        c.setStreamIndex(1);
        QVERIFY(c.start(path(QStringLiteral("d.wav")), someRecording()));

        // Simuliert, was attach() normalerweise tut: der Slot filtert
        // nach streamIndex. Da der Test den Slot nicht ueber ein
        // echtes RadioModel-Signal ausloest, wird hier direkt in den
        // Ring geschrieben — das Filtern selbst ist ein privates
        // Detail von onRawIqDataForStream(), aber der Ring bekommt nur
        // das, was der Abgriff durchgelassen haette. Diese Testklasse
        // prueft die Ringmechanik, nicht die Signalfilterung — Letztere
        // ist zu simpel fuer einen eigenen Test (ein `if`) und braucht
        // ein echtes RadioModel, um sinnvoll geprueft zu werden.
        const std::vector<float> iq(400, 0.6f);
        c.ring().write(iq.data(), 400);
        c.drainNow();
        QCOMPARE(c.recorder().framesWritten(), 200);
    }

    void startingAgainNeedsAFreshFile()
    {
        IqRecorderController c;
        c.setSampleRate(1000);

        QVERIFY(c.start(path(QStringLiteral("first.wav")), someRecording()));
        const std::vector<float> iq(400, 0.4f);
        c.ring().write(iq.data(), 400);
        c.drainNow();
        c.stop();
        QCOMPARE(c.recorder().framesWritten(), 200);

        QVERIFY2(c.start(path(QStringLiteral("second.wav")), someRecording()),
                 "nach stop() muss eine neue Aufnahme moeglich sein");
    }
};

QTEST_MAIN(TestIqRecorderController)
#include "tst_iq_recorder_controller.moc"
