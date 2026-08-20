// =================================================================
// tests/tst_wav_player.cpp  (NereusSDR)
// =================================================================
//
// Der Abspieler zum Nachhoeren.
//
// Was hier geprueft werden KANN, ohne ein Audiogeraet: dass er eine
// unlesbare Datei nicht annimmt, dass er sie mit einem Grund
// zurueckweist statt still zu scheitern, und dass zweimaliges
// Anhalten nicht knallt.
//
// Was hier NICHT geprueft werden kann: ob wirklich Ton herauskommt.
// Auf einer Bauplattform gibt es kein Ausgabegeraet, und ein Test, der
// so tut als ob, prueft nur seine eigene Attrappe. Der Fall steht auf
// der Bank.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QTemporaryDir>

#include "core/audio/WavFile.h"
#include "core/audio/WavPlayer.h"

using namespace Longpath;

class TestWavPlayer : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString pathFor(const QString& n) const
    { return m_dir.path() + QLatin1Char('/') + n; }

private slots:

    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void aMissingFileIsRefusedWithAReason()
    {
        WavPlayer p;
        QString err;
        QVERIFY(!p.play(pathFor(QStringLiteral("nope.wav")), &err));
        QVERIFY2(!err.isEmpty(),
                 "ein Abspieler, der still scheitert, laesst einen an der "
                 "Lautstaerke drehen");
        QVERIFY(!p.isPlaying());
    }

    void somethingThatIsNotAWavIsRefused()
    {
        const QString p = pathFor(QStringLiteral("text.wav"));
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not a wav file at all");
        f.close();

        WavPlayer player;
        QString err;
        QVERIFY(!player.play(p, &err));
        QVERIFY(!err.isEmpty());
    }

    // Eine gueltige, aber leere Datei ist ein eigener Fall: sie liest
    // sich sauber und hat trotzdem nichts zu sagen.
    void anEmptyRecordingIsRefused()
    {
        const QString p = pathFor(QStringLiteral("empty.wav"));
        QVERIFY(writeWavMono(p, QVector<float>{}, 48000));

        WavPlayer player;
        QString err;
        QVERIFY(!player.play(p, &err));
        QVERIFY2(err.contains(QStringLiteral("no audio")),
                 qPrintable(QStringLiteral("Grund war: %1").arg(err)));
    }

    // Anhalten, ohne dass etwas laeuft, muss folgenlos sein — das ist
    // der Weg, den jeder Aufraeumpfad nimmt.
    void stoppingWhenNothingPlaysIsHarmless()
    {
        WavPlayer p;
        p.stop();
        p.stop();
        QVERIFY(!p.isPlaying());
        QVERIFY(p.currentPath().isEmpty());
    }
};

QTEST_MAIN(TestWavPlayer)
#include "tst_wav_player.moc"
