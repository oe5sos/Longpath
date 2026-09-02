// =================================================================
// tests/tst_qso_recorder_controller.cpp  (NereusSDR)
// =================================================================
//
// Was die Abgriffe mit der Aufnahme verbindet.
//
// Geprueft wird OHNE Funkgeraet und ohne Audiogeraet: der Test
// schreibt selbst in die Zwischenspeicher, genau wie es der Audio-Faden
// tut, und sieht nach, was hinten in der Aufnahme steht.
//
// DER FALL, DER STILL FALSCH WIRD, ist die Reihenfolge beim Abholen.
// Der Empfang ist die Uhr: QsoRecorder fuellt die Sprechspur bis zum
// aktuellen Empfangsstand mit Stille auf, sobald Mikrofonton kommt.
// Wer zuerst das Mikrofon abholt, legt die eigene Stimme um einen
// Zeitgeber-Takt zu frueh — 200 ms, hoerbar, und im Quelltext
// unsichtbar.
//
// Seit 2026-09-02 (zeitlich unbegrenzt, siehe QsoRecorder.h) braucht
// start() einen echten Zielpfad — die Aufnahme geht streamend auf die
// Platte statt im Speicher zu sammeln. Wo die Ausrichtung frueher am
// txFrames()-Zaehler ablesbar war (der die Auffuellung mitzaehlte),
// steht sie jetzt direkt in der geschriebenen Datei, weil der Zaehler
// seither nur noch das echt hereingekommene zaehlt.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
//   2026-09-02 — An das streamende QsoRecorder angepasst (zeitlich
//                 unbegrenzt), von Martin Fischer, KI-gestuetzt ueber
//                 Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <vector>

#include "core/audio/QsoRecorderController.h"
#include "core/audio/WavFile.h"

using namespace Longpath;

namespace {

QsoRecordingInfo someQso()
{
    QsoRecordingInfo i;
    i.utcStart  = QDateTime(QDate(2026, 8, 19), QTime(21, 0), Qt::UTC);
    i.frequency = QStringLiteral("7.131.300");
    i.mode      = QStringLiteral("LSB");
    i.band      = QStringLiteral("40m");
    i.callsign  = QStringLiteral("DL1ABC");
    return i;
}

} // namespace

class TestQsoRecorderController : public QObject
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
        QsoRecorderController c;
        c.setSampleRate(1000);

        const std::vector<float> rx(200, 0.5f);   // 100 Rahmen stereo
        c.rxRing().write(rx.data(), static_cast<int>(rx.size()));
        c.drainNow();

        // Abgeholt wird, angenommen nicht: QsoRecorder::feedRx tut vor
        // start() nichts. Wichtig, weil der Abgriff im Leerlauf
        // weiterlaufen kann — was dabei anfaellt, gehoert nicht in die
        // naechste Aufnahme.
        QCOMPARE(c.recorder().rxFrames(), qint64(0));
        QVERIFY(!c.isRecording());
    }

    void whatTheAudioThreadWritesEndsUpInTheRecording()
    {
        QsoRecorderController c;
        c.setSampleRate(1000);
        QVERIFY(c.start(path(QStringLiteral("basic.wav")), someQso()));

        const std::vector<float> rx(2000, 0.25f);  // 1000 Rahmen = 1 s
        QCOMPARE(c.rxRing().write(rx.data(), 2000), 2000);
        c.drainNow();

        QCOMPARE(c.recorder().rxFrames(), qint64(1000));
        QVERIFY(qAbs(c.recorder().recordedSeconds() - 1.0) < 1e-9);
    }

    // DIE REIHENFOLGE. Zehn — hier der Einfachheit halber eine —
    // Sekunde Empfang liegt bereit, dann eine halbe Sekunde eigene
    // Stimme, beide im selben Abholvorgang. Gestoppt wird SOFORT
    // danach, ohne dass weiterer Empfang nachkommt: die Stimme muss
    // trotzdem vollstaendig in der Datei landen, ab Sekunde eins, nicht
    // ab dem Anfang und nicht verworfen, weil der Empfang genau in dem
    // Moment schwieg.
    void theReceiverIsTheClockEvenWhenBothArriveTogether()
    {
        QsoRecorderController c;
        c.setSampleRate(1000);
        const QString p = path(QStringLiteral("clock.wav"));
        QVERIFY(c.start(p, someQso()));

        const std::vector<float> rx(2000, 0.2f);   // 1 s Empfang stereo
        const std::vector<float> tx(500, 0.9f);    // 0,5 s Stimme mono

        c.rxRing().write(rx.data(), 2000);
        c.txRing().write(tx.data(), 500);
        c.drainNow();

        QCOMPARE(c.recorder().rxFrames(), qint64(1000));
        QCOMPARE(c.recorder().txFrames(), qint64(500));  // echt hereingekommen

        c.stop();

        // Die eigentliche Behauptung steht in der Datei: die Stimme
        // faengt bei Rahmen 1000 an (dem Empfangsstand zum Zeitpunkt
        // des Abholens) und geht nicht verloren, obwohl kein weiterer
        // Empfang mehr nachkam.
        const WavData back = readWavMono(p);
        QVERIFY(back.ok);
        QCOMPARE(back.samples.size(), 1500);
        // vor der Stimme: nur der Empfang (0,2), rechts noch Stille —
        // gemittelt 0,1.
        QVERIFY2(qAbs(back.samples[100] - 0.1f) < 1e-3f,
                 "vor der Stimme traegt nur der Empfang");
        // ab Rahmen 1000: der Empfang ist zu Ende (finalizeTail fuellt
        // ihn mit Stille auf), nur die eigene Stimme traegt — 0,9
        // gemittelt mit Stille auf der Gegenseite ist 0,45.
        QVERIFY2(qAbs(back.samples[1200] - 0.45f) < 1e-3f,
                 "die Stimme steht ab Sekunde eins und geht nicht verloren");
    }

    void stoppingCollectsWhatIsStillWaiting()
    {
        QsoRecorderController c;
        c.setSampleRate(1000);
        QVERIFY(c.start(path(QStringLiteral("waiting.wav")), someQso()));

        const std::vector<float> rx(600, 0.3f);   // 300 Rahmen
        c.rxRing().write(rx.data(), 600);

        QSignalSpy spy(&c, &QsoRecorderController::recordingChanged);
        c.stop();

        QCOMPARE(c.recorder().rxFrames(), qint64(300));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), false);
        QVERIFY(!c.isRecording());
    }

    // Ein Ueberlauf darf nicht still bleiben. Eine Luecke in einer
    // Aufnahme faellt sonst erst auf, wenn man sie braucht.
    void aLossIsAnnouncedOnce()
    {
        QsoRecorderController c;
        c.setSampleRate(100);          // winzige Zwischenspeicher
        QVERIFY(c.start(path(QStringLiteral("loss.wav")), someQso()));

        QSignalSpy spy(&c, &QsoRecorderController::samplesLost);

        const std::vector<float> flood(5000, 0.1f);
        c.rxRing().write(flood.data(), 5000);
        QVERIFY2(c.droppedSamples() > 0, "so viel kann nicht passen");

        c.drainNow();
        QCOMPARE(spy.count(), 1);

        c.rxRing().write(flood.data(), 5000);
        c.drainNow();
        QVERIFY2(spy.count() == 1,
                 "einmal melden reicht — sonst steht die Warnung "
                 "fuenfmal je Sekunde da");
    }

    void startingAgainBeginsWithAnEmptyRecording()
    {
        QsoRecorderController c;
        c.setSampleRate(1000);

        QVERIFY(c.start(path(QStringLiteral("first.wav")), someQso()));
        const std::vector<float> rx(400, 0.4f);
        c.rxRing().write(rx.data(), 400);
        c.drainNow();
        c.stop();
        QCOMPARE(c.recorder().rxFrames(), qint64(200));

        QVERIFY(c.start(path(QStringLiteral("second.wav")), someQso()));
        QCOMPARE(c.recorder().rxFrames(), qint64(0));
    }
};

QTEST_MAIN(TestQsoRecorderController)
#include "tst_qso_recorder_controller.moc"
