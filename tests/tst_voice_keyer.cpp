// =================================================================
// tests/tst_voice_keyer.cpp  (NereusSDR)
// =================================================================
//
// Der Sprachspeicher: zehn Ansagen, Tasten, CQ-Wiederholung.
//
// Auf Ansage des Betreibers (2026-08-19): „sinn macht auch, dort 10
// audio files aufnehmen zu koennen und diese dann mit shortcuts zu
// verbinden und als CQ CALL usw. abspielen zu lassen."
//
// Zwei Teile, getrennt geprueft — genau deshalb sind sie getrennt
// gebaut: der Speicher kennt kein Audio, die Quelle keine Plaetze.
// Zusammengeschweisst braeuchte jeder dieser Faelle ein Funkgeraet.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QTemporaryDir>

#include "core/audio/VoiceKeyer.h"
#include "core/audio/WavFile.h"

using namespace Longpath;

namespace {

// Ein hoerbares Muster statt Stille: so faellt auf, wenn beim Umtasten
// Werte verlorengehen.
QVector<float> tone(int n, float amp = 0.5f)
{
    QVector<float> v(n);
    for (int i = 0; i < n; ++i) {
        v[i] = amp * std::sin(2.0 * M_PI * 700.0 * i / 48000.0);
    }
    return v;
}

} // namespace

class TestVoiceKeyer : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private slots:

    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    // ── Der Speicher ─────────────────────────────────────────────────

    void thereAreTenSlots()
    {
        VoiceKeyerStore s;
        QCOMPARE(s.slotCount(), 10);
        QVERIFY2(s.slot(0).isEmpty(), "am Anfang ist nichts aufgenommen");
    }

    // F1 bis F10 — die Reihe, die Funkprogramme seit je fuer Ansagen
    // benutzen. Wer von woanders kommt, greift damit richtig.
    void theKeysDefaultToTheFunctionRow()
    {
        VoiceKeyerStore s;
        QCOMPARE(s.slot(0).shortcut, QStringLiteral("F1"));
        QCOMPARE(s.slot(9).shortcut, QStringLiteral("F10"));
    }

    void aRecordingFillsTheSlot()
    {
        VoiceKeyerStore s;
        s.setFolder(m_dir.path() + QStringLiteral("/keyer"));

        QString err;
        QVERIFY2(s.setRecording(0, tone(48000), 48000, &err), qPrintable(err));

        QVERIFY(!s.slot(0).isEmpty());
        QVERIFY2(qAbs(s.slot(0).seconds - 1.0) < 1e-6,
                 "48000 Werte bei 48 kHz sind eine Sekunde");
        QVERIFY2(QFileInfo::exists(s.slot(0).wavPath),
                 "und die Datei liegt wirklich da");
    }

    // Eine leere Aufnahme ist fast immer ein Bedienfehler. Sie
    // stillschweigend zu speichern hiesse, im Betrieb eine STUMME
    // Ansage zu senden — und niemand merkt es, weil man sich selbst
    // nicht hoert.
    void anEmptyRecordingIsRefused()
    {
        VoiceKeyerStore s;
        s.setFolder(m_dir.path() + QStringLiteral("/keyer2"));

        QString err;
        QVERIFY(!s.setRecording(1, {}, 48000, &err));
        QVERIFY(!err.isEmpty());
        QVERIFY(s.slot(1).isEmpty());
    }

    void importingChecksTheFileNow()
    {
        VoiceKeyerStore s;
        s.setFolder(m_dir.path() + QStringLiteral("/keyer3"));

        // Etwas, das keine WAV ist: der Fehler muss BEIM IMPORT kommen,
        // nicht mitten im CQ-Ruf.
        const QString bogus = m_dir.path() + QStringLiteral("/notes.txt");
        QFile f(bogus);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("just a note");
        f.close();

        QString err;
        QVERIFY(!s.importFile(2, bogus, &err));
        QVERIFY(!err.isEmpty());
        QVERIFY(s.slot(2).isEmpty());
    }

    void importingARealFileWorks()
    {
        VoiceKeyerStore s;
        s.setFolder(m_dir.path() + QStringLiteral("/keyer4"));

        const QString wav = m_dir.path() + QStringLiteral("/outside.wav");
        QVERIFY(writeWavMono(wav, tone(24000), 48000));

        QString err;
        QVERIFY2(s.importFile(3, wav, &err), qPrintable(err));
        QVERIFY(!s.slot(3).isEmpty());
        QVERIFY(qAbs(s.slot(3).seconds - 0.5) < 1e-6);
    }

    // Leeren loescht die DATEI nicht: wer sich verklickt, soll seine
    // Ansage nicht verlieren.
    void clearingKeepsTheFile()
    {
        VoiceKeyerStore s;
        s.setFolder(m_dir.path() + QStringLiteral("/keyer5"));
        QVERIFY(s.setRecording(4, tone(4800), 48000));

        const QString path = s.slot(4).wavPath;
        s.clearSlot(4);

        QVERIFY(s.slot(4).isEmpty());
        QVERIFY2(QFileInfo::exists(path),
                 "die Aufnahme bleibt auf der Platte");
    }

    void labelsAndKeysStick()
    {
        VoiceKeyerStore s;
        s.setLabel(0, QStringLiteral("CQ"));
        s.setShortcut(0, QStringLiteral("Ctrl+1"));
        QCOMPARE(s.slot(0).label, QStringLiteral("CQ"));
        QCOMPARE(s.slot(0).shortcut, QStringLiteral("Ctrl+1"));
    }

    void anImpossibleSlotIsHarmless()
    {
        VoiceKeyerStore s;
        s.setFolder(m_dir.path());
        QVERIFY(!s.setRecording(-1, tone(100), 48000));
        QVERIFY(!s.setRecording(99, tone(100), 48000));
        QVERIFY(s.slot(-1).isEmpty());
        QVERIFY(s.slot(99).isEmpty());
    }

    // ── Die Wiedergabe ───────────────────────────────────────────────

    void nothingLoadedMeansSilence()
    {
        WavTxSource src;
        float buf[64];
        std::fill_n(buf, 64, 1.0f);

        QCOMPARE(src.pullSamples(buf, 64), 64);
        for (float v : buf) {
            QCOMPARE(v, 0.0f);   // Stille, nicht Muell
        }
        QVERIFY(!src.isPlaying());
    }

    void aLoadedAnnouncementPlaysAndEnds()
    {
        const QString wav = m_dir.path() + QStringLiteral("/cq.wav");
        QVERIFY(writeWavMono(wav, tone(480), 48000));   // 10 ms

        WavTxSource src;
        QString err;
        QVERIFY2(src.load(wav, 48000, &err), qPrintable(err));
        QCOMPARE(src.sampleCount(), 480);

        src.play();
        QVERIFY(src.isPlaying());

        QVector<float> buf(480);
        QCOMPARE(src.pullSamples(buf.data(), 480), 480);

        bool anySound = false;
        for (float v : buf) { if (qAbs(v) > 0.01f) { anySound = true; break; } }
        QVERIFY2(anySound, "es muss auch wirklich etwas herauskommen");

        // Eine weitere Runde beendet sie.
        src.pullSamples(buf.data(), 480);
        QVERIFY2(!src.isPlaying(), "nach dem Ende hoert sie von selbst auf");
    }

    // Der CQ-Ruf im Contest: wiederholen mit Pause. Ohne Pause klebte
    // ein Ruf am naechsten.
    void repeatKeepsGoing()
    {
        const QString wav = m_dir.path() + QStringLiteral("/cq2.wav");
        QVERIFY(writeWavMono(wav, tone(480), 48000));

        WavTxSource src;
        QVERIFY(src.load(wav, 48000));
        src.setRepeat(true, 0.01);   // 10 ms Pause
        src.play();

        QVector<float> buf(4800);    // 100 ms, also mehrere Durchgaenge
        src.pullSamples(buf.data(), 4800);
        QVERIFY2(src.isPlaying(), "im Wiederholbetrieb hoert sie nicht auf");

        src.stop();
        QVERIFY(!src.isPlaying());
    }

    // Fremde Abtastrate: die Ansage kommt mit 44,1 kHz, der Sender
    // laeuft mit 48. Umgetastet wird beim LADEN, nicht im Audio-Faden.
    void aForeignSampleRateIsResampledOnLoad()
    {
        const QString wav = m_dir.path() + QStringLiteral("/cq441.wav");
        QVERIFY(writeWavMono(wav, tone(44100), 44100));   // 1 s

        WavTxSource src;
        QVERIFY(src.load(wav, 48000));
        QVERIFY2(qAbs(src.seconds() - 1.0) < 0.01,
                 "eine Sekunde bleibt eine Sekunde");
        QVERIFY2(qAbs(src.sampleCount() - 48000) < 100,
                 "aber sie besteht jetzt aus 48000 Werten");
    }

    void stoppingRewinds()
    {
        const QString wav = m_dir.path() + QStringLiteral("/cq3.wav");
        QVERIFY(writeWavMono(wav, tone(4800), 48000));

        WavTxSource src;
        QVERIFY(src.load(wav, 48000));
        src.play();

        QVector<float> buf(1000);
        src.pullSamples(buf.data(), 1000);
        src.stop();
        src.play();

        // Nach dem Neustart kommt wieder der Anfang.
        QVector<float> again(1000);
        src.pullSamples(again.data(), 1000);
        QCOMPARE(again.first(), buf.first());
    }
};

QTEST_MAIN(TestVoiceKeyer)
#include "tst_voice_keyer.moc"
