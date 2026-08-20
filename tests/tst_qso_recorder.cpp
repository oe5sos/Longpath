// =================================================================
// tests/tst_qso_recorder.cpp  (NereusSDR)
// =================================================================
//
// Ein QSO aufnehmen: was ankommt UND was man selbst sagt.
//
// Ansage des Betreibers (2026-08-19): „wichtig ist, dass man sich
// selbst aber auch die andere station hört!"
//
// DER WICHTIGSTE FALL DIESER DATEI ist die AUSRICHTUNG. Der Empfang
// laeuft dauernd, das Mikrofon nur beim Senden. Wer beide Spuren
// einfach anhaengt und am Ende auffuellt, bekommt eine Aufnahme, in der
// die eigene Stimme GANZ AM ANFANG steht — egal wann sie gesprochen
// wurde. Das faellt beim Hoeren sofort auf und beim Programmieren nie.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QTemporaryDir>

#include "core/audio/QsoRecorder.h"
#include "core/audio/WavFile.h"

using namespace Longpath;

namespace {

// Verschachtelter Stereoblock mit festem Wert auf beiden Kanaelen.
QVector<float> rxBlock(int frames, float value)
{
    return QVector<float>(frames * 2, value);
}

QVector<float> txBlock(int frames, float value)
{
    return QVector<float>(frames, value);
}

QsoRecordingInfo someQso()
{
    QsoRecordingInfo i;
    i.utcStart  = QDateTime(QDate(2026, 8, 19), QTime(18, 30), Qt::UTC);
    i.frequency = QStringLiteral("14.205.000");
    i.mode      = QStringLiteral("LSB");
    i.band      = QStringLiteral("20m");
    i.callsign  = QStringLiteral("DL1ABC");
    return i;
}

} // namespace

class TestQsoRecorder : public QObject
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

    void nothingIsRecordedBeforeStart()
    {
        QsoRecorder r;
        const auto rx = rxBlock(100, 0.5f);
        r.feedRx(rx.constData(), 100);
        QCOMPARE(r.rxFrames(), 0);
        QVERIFY(!r.isRecording());
    }

    void receivedAudioIsMixedToOneChannel()
    {
        QsoRecorder r;
        r.start(someQso());
        const auto rx = rxBlock(50, 0.4f);
        r.feedRx(rx.constData(), 50);
        QCOMPARE(r.rxFrames(), 50);
    }

    // ── Die Ausrichtung ──────────────────────────────────────────────
    //
    // Zehn Sekunden zuhoeren, dann eine Sekunde sprechen. Die eigene
    // Stimme muss BEI SEKUNDE ZEHN liegen, nicht am Anfang.
    void ownVoiceLandsWhereItWasSpoken()
    {
        QsoRecorder r;
        r.setSampleRate(1000);      // rechnet sich leichter
        r.start(someQso());

        const auto rx = rxBlock(10000, 0.2f);   // 10 s Empfang
        r.feedRx(rx.constData(), 10000);

        const auto tx = txBlock(1000, 0.9f);    // 1 s Sprechen
        r.feedTx(tx.constData(), 1000);

        QCOMPARE(r.rxFrames(), 10000);
        QVERIFY2(r.txFrames() == 11000,
                 "die Sprechspur muss bis Sekunde 10 mit Stille aufgefuellt "
                 "sein und dann die Stimme tragen");

        r.stop();
        const QString p = path(QStringLiteral("qso.wav"));
        QString err;
        QVERIFY2(r.save(p, &err), qPrintable(err));

        // Nachsehen, was wirklich in der Datei steht: der rechte Kanal
        // muss bis Sekunde 10 still sein und danach laut.
        const WavData back = readWavMono(p, &err);
        QVERIFY2(back.ok, qPrintable(err));
        // readWavMono mittelt beide Kanaele: vor der Stimme also nur der
        // halbe Empfangswert, danach Empfang und Stimme zusammen.
        QVERIFY2(qAbs(back.samples[5000] - 0.1f) < 1e-4f,
                 "vor dem Sprechen traegt nur der Empfang");
        // Bei 10500 ist der Empfang schon zu Ende (er lief 10 s), es
        // traegt also nur noch die eigene Stimme: 0,9 auf einem Kanal
        // und Stille auf dem anderen mitteln sich zu 0,45.
        QVERIFY2(qAbs(back.samples[10500] - 0.45f) < 1e-3f,
                 "waehrend des Sprechens traegt die eigene Stimme");
    }

    // Der umgekehrte Fall: erst sprechen, dann empfangen. Auch hier darf
    // nichts durcheinandergeraten.
    void speakingFirstAlsoLinesUp()
    {
        QsoRecorder r;
        r.setSampleRate(1000);
        r.start(someQso());

        const auto tx = txBlock(2000, 0.8f);
        r.feedTx(tx.constData(), 2000);
        const auto rx = rxBlock(5000, 0.3f);
        r.feedRx(rx.constData(), 5000);

        r.stop();
        QCOMPARE(r.txFrames(), 5000);   // auf Empfangslaenge gebracht
        QVERIFY2(qAbs(r.recordedSeconds() - 5.0) < 1e-9,
                 "die Aufnahme ist so lang wie das laengere von beiden");
    }

    void bothTracksEndUpInOneStereoFile()
    {
        QsoRecorder r;
        r.setSampleRate(8000);
        r.start(someQso());

        // ERST sprechen, DANN empfangen — nur so decken sich beide
        // Spuren. Andersherum schiebt die Ausrichtung die Sprechspur
        // hinter den Empfang, und dann gibt es keine Stelle, an der
        // beide gleichzeitig etwas tragen. (Genau darauf bin ich beim
        // ersten Anlauf hereingefallen: der Testfall war falsch, nicht
        // der Recorder.)
        const auto tx = txBlock(800, -0.25f);
        r.feedTx(tx.constData(), 800);
        const auto rx = rxBlock(800, 0.25f);
        r.feedRx(rx.constData(), 800);
        r.stop();

        const QString p = path(QStringLiteral("stereo-qso.wav"));
        QVERIFY(r.save(p));

        // Beide Kanaele heben sich beim Mitteln auf — genau daran
        // erkennt man, dass sie GETRENNT geschrieben wurden und nicht
        // vorher zusammengemischt.
        //
        // NICHT auf exakt null pruefen, und nicht an einer einzigen
        // Stelle. Seit die Aufnahme als 16-Bit-PCM mit Dither
        // geschrieben wird (Thetis-Durchsicht 2026-08-19), traegt jeder
        // Abtastwert bis zu einem Bit Zufall, und die beiden Kanaele
        // bekommen ihren eigenen. Die alte Fassung verglich EINEN Wert
        // gegen 1e-6 und blieb gruen, weil sich der Dither an genau
        // dieser Stelle zufaellig aufhob — ein Test, der aus Glueck
        // besteht, prueft nichts.
        //
        // Ein Bit bei 16 Bit sind rund 3e-5; die Schranke liegt bei
        // 1e-4, also deutlich unter den 0,25, die ein
        // ZUSAMMENGEMISCHTER Kanal zeigen wuerde.
        const WavData back = readWavMono(p);
        QVERIFY(back.ok);
        float worst = 0.0f;
        for (int i = 100; i < 700; ++i) {
            worst = qMax(worst, qAbs(back.samples[i]));
        }
        QVERIFY2(worst < 1e-4f,
                 qPrintable(QStringLiteral(
                     "links +0,25 und rechts -0,25 muessen sich zu null "
                     "mitteln; groesster Rest war %1").arg(worst)));
    }

    // Die Beschreibung daneben: eine Aufnahme ohne Frequenz, Modus und
    // Zeit ist in einem halben Jahr nur eine Datei mit Rauschen darauf.
    void aDescriptionIsWrittenBeside()
    {
        QsoRecorder r;
        r.start(someQso());
        const auto rx = rxBlock(48000, 0.1f);
        r.feedRx(rx.constData(), 48000);
        r.stop();

        const QString wav = path(QStringLiteral("described.wav"));
        QVERIFY(r.save(wav));

        QFile j(path(QStringLiteral("described.json")));
        QVERIFY2(j.exists(), "neben der WAV liegt die Beschreibung");
        QVERIFY(j.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(j.readAll());
        QVERIFY(text.contains(QStringLiteral("14.205.000")));
        QVERIFY(text.contains(QStringLiteral("LSB")));
        QVERIFY(text.contains(QStringLiteral("DL1ABC")));
        QVERIFY2(text.contains(QStringLiteral("left = received")),
                 "und sie sagt, welche Spur welche ist");
    }

    void savingNothingIsRefused()
    {
        QsoRecorder r;
        QString err;
        QVERIFY(!r.save(path(QStringLiteral("empty.wav")), &err));
        QVERIFY(!err.isEmpty());
    }

    void clearingStartsOver()
    {
        QsoRecorder r;
        r.start(someQso());
        const auto rx = rxBlock(100, 0.5f);
        r.feedRx(rx.constData(), 100);
        r.clear();
        QCOMPARE(r.rxFrames(), 0);
        QVERIFY(!r.isRecording());
    }

    // Eine vergessene Aufnahme darf die Platte nicht auffressen.
    void thereIsACeiling()
    {
        QsoRecorder r;
        r.setSampleRate(1000);
        r.start(someQso());

        const int cap = QsoRecorder::kMaxMinutes * 60 * 1000;
        const auto rx = rxBlock(cap + 5000, 0.1f);
        r.feedRx(rx.constData(), cap + 5000);

        QCOMPARE(r.rxFrames(), cap);
    }
};

QTEST_MAIN(TestQsoRecorder)
#include "tst_qso_recorder.moc"
