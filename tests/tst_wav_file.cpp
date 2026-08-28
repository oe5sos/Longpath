// =================================================================
// tests/tst_wav_file.cpp  (NereusSDR)
// =================================================================
//
// WAV lesen und schreiben.
//
// Fuer den Sprachspeicher (10 Ansagen aufnehmen, mit Tasten abrufen,
// als CQ senden — Ansage des Betreibers 2026-08-19) und spaeter die
// QSO-Aufnahme.
//
// Der RUNDGANG ist die wichtigste Pruefung: schreiben, lesen, dieselben
// Zahlen erwarten. Ein Vorzeichenfehler oder ein vertauschtes
// Byte-Paar faellt so sofort auf, und zwar ohne dass jemand zuhoeren
// muss.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QFileInfo>
#include <QTemporaryDir>

#include "core/audio/WavFile.h"

using namespace Longpath;

namespace {

QVector<float> ramp(int n)
{
    QVector<float> v(n);
    for (int i = 0; i < n; ++i) {
        // -0,9 bis +0,9: Vollausschlag vermeiden, damit ein
        // Rundungsfehler nicht am Anschlag verschwindet.
        v[i] = -0.9f + 1.8f * static_cast<float>(i) / static_cast<float>(n - 1);
    }
    return v;
}

} // namespace

class TestWavFile : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString pathFor(const QString& name) const
    {
        return m_dir.path() + QLatin1Char('/') + name;
    }

private slots:

    void initTestCase()
    {
        QVERIFY2(m_dir.isValid(), "ohne Arbeitsverzeichnis kein Test");
    }

    // Der Rundgang. Float32 verliert nichts, also duerfen die Zahlen
    // exakt zurueckkommen.
    void writeThenReadGivesTheSameSamples()
    {
        const QVector<float> in = ramp(1000);
        const QString p = pathFor(QStringLiteral("round.wav"));

        QString err;
        QVERIFY2(writeWavMono(p, in, 48000, &err), qPrintable(err));

        const WavData out = readWavMono(p, &err);
        QVERIFY2(out.ok, qPrintable(err));
        QCOMPARE(out.sampleRate, 48000);
        QCOMPARE(out.samples.size(), in.size());

        for (int i = 0; i < in.size(); ++i) {
            QCOMPARE(out.samples[i], in[i]);
        }
    }

    void theSampleRateSurvives()
    {
        const QString p = pathFor(QStringLiteral("rate.wav"));
        QVERIFY(writeWavMono(p, ramp(64), 8000));
        QCOMPARE(readWavMono(p).sampleRate, 8000);
    }

    void durationIsSamplesOverRate()
    {
        const QString p = pathFor(QStringLiteral("dur.wav"));
        QVERIFY(writeWavMono(p, QVector<float>(24000, 0.0f), 48000));
        QVERIFY2(qAbs(wavDurationSeconds(p) - 0.5) < 1e-9,
                 "24000 Werte bei 48 kHz sind eine halbe Sekunde");
    }

    void anEmptyRecordingIsStillAValidFile()
    {
        const QString p = pathFor(QStringLiteral("empty.wav"));
        QVERIFY(writeWavMono(p, {}, 48000));

        const WavData out = readWavMono(p);
        QVERIFY2(out.ok, "eine leere Aufnahme ist kein Fehler");
        QCOMPARE(out.samples.size(), 0);
    }

    // ── Was schiefgehen kann ─────────────────────────────────────────

    void aMissingFileIsReported()
    {
        QString err;
        const WavData out = readWavMono(pathFor(QStringLiteral("nope.wav")), &err);
        QVERIFY(!out.ok);
        QVERIFY2(!err.isEmpty(), "und der Grund steht drin");
    }

    void somethingElseIsNotAWav()
    {
        const QString p = pathFor(QStringLiteral("text.wav"));
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not a wav file, it is a sentence");
        f.close();

        QString err;
        QVERIFY(!readWavMono(p, &err).ok);
        QVERIFY(err.contains(QStringLiteral("not a WAV")));
    }

    void aNegativeSampleRateIsRefused()
    {
        QString err;
        QVERIFY(!writeWavMono(pathFor(QStringLiteral("bad.wav")), ramp(8), 0, &err));
        QVERIFY(!err.isEmpty());
    }

    // ── Fremde Dateien ───────────────────────────────────────────────
    //
    // Wer eine Ansage von anderswo einspielt, bringt selten float32 mit.
    // 16-Bit-PCM ist der Normalfall, und diese Faelle bauen so eine
    // Datei von Hand — sonst prueft der Rundgang nur den eigenen
    // Schreiber gegen den eigenen Leser.

    void sixteenBitPcmIsRead()
    {
        const QString p = pathFor(QStringLiteral("pcm16.wav"));
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));

        const QVector<qint16> pcm = {0, 16384, -16384, 32767, -32768};
        const quint32 dataBytes = static_cast<quint32>(pcm.size()) * 2u;

        auto u32 = [&f](quint32 v) {
            uchar b[4]; qToLittleEndian(v, b); f.write(reinterpret_cast<char*>(b), 4);
        };
        auto u16 = [&f](quint16 v) {
            uchar b[2]; qToLittleEndian(v, b); f.write(reinterpret_cast<char*>(b), 2);
        };

        f.write("RIFF", 4); u32(36 + dataBytes); f.write("WAVE", 4);
        f.write("fmt ", 4); u32(16); u16(1); u16(1); u32(44100);
        u32(44100 * 2); u16(2); u16(16);
        f.write("data", 4); u32(dataBytes);
        for (qint16 v : pcm) { u16(static_cast<quint16>(v)); }
        f.close();

        QString err;
        const WavData out = readWavMono(p, &err);
        QVERIFY2(out.ok, qPrintable(err));
        QCOMPARE(out.sampleRate, 44100);
        QCOMPARE(out.samples.size(), 5);
        QCOMPARE(out.samples[0], 0.0f);
        QVERIFY(qAbs(out.samples[1] - 0.5f) < 1e-4f);
        QVERIFY(qAbs(out.samples[2] + 0.5f) < 1e-4f);
        QVERIFY2(qAbs(out.samples[4] + 1.0f) < 1e-6f,
                 "der negative Vollausschlag ist genau -1");
    }

    // Zwei Kanaele werden gemittelt. Wer nur den linken naehme, verlore
    // alles, was jemand rechts eingesprochen hat.
    void twoChannelsAreAveraged()
    {
        const QString p = pathFor(QStringLiteral("stereo.wav"));
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));

        // Links stumm, rechts halber Ausschlag -> Mittel 0,25.
        const QVector<qint16> pcm = {0, 16384, 0, 16384};
        const quint32 dataBytes = static_cast<quint32>(pcm.size()) * 2u;

        auto u32 = [&f](quint32 v) {
            uchar b[4]; qToLittleEndian(v, b); f.write(reinterpret_cast<char*>(b), 4);
        };
        auto u16 = [&f](quint16 v) {
            uchar b[2]; qToLittleEndian(v, b); f.write(reinterpret_cast<char*>(b), 2);
        };

        f.write("RIFF", 4); u32(36 + dataBytes); f.write("WAVE", 4);
        f.write("fmt ", 4); u32(16); u16(1); u16(2); u32(48000);
        u32(48000 * 4); u16(4); u16(16);
        f.write("data", 4); u32(dataBytes);
        for (qint16 v : pcm) { u16(static_cast<quint16>(v)); }
        f.close();

        const WavData out = readWavMono(p);
        QVERIFY(out.ok);
        QCOMPARE(out.samples.size(), 2);
        QVERIFY2(qAbs(out.samples[0] - 0.25f) < 1e-4f,
                 "stumm und halb ergibt ein Viertel");
    }

    // Ein LIST-Block zwischen fmt und data ist haeufig (jedes zweite
    // Aufnahmeprogramm schreibt seinen Namen hinein). Ein Leser, der
    // Byte 44 fuer den Anfang der Daten haelt, liest dann Text als Ton.
    void aChunkBetweenFmtAndDataIsSkipped()
    {
        const QString p = pathFor(QStringLiteral("list.wav"));
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly));

        const QByteArray note("INFOsoftware written by somebody");
        const QVector<qint16> pcm = {8192, -8192};
        const quint32 dataBytes = 4;
        const quint32 listBytes = static_cast<quint32>(note.size());

        auto u32 = [&f](quint32 v) {
            uchar b[4]; qToLittleEndian(v, b); f.write(reinterpret_cast<char*>(b), 4);
        };
        auto u16 = [&f](quint16 v) {
            uchar b[2]; qToLittleEndian(v, b); f.write(reinterpret_cast<char*>(b), 2);
        };

        f.write("RIFF", 4); u32(36 + listBytes + 8 + dataBytes); f.write("WAVE", 4);
        f.write("fmt ", 4); u32(16); u16(1); u16(1); u32(48000);
        u32(48000 * 2); u16(2); u16(16);
        f.write("LIST", 4); u32(listBytes); f.write(note);
        f.write("data", 4); u32(dataBytes);
        for (qint16 v : pcm) { u16(static_cast<quint16>(v)); }
        f.close();

        const WavData out = readWavMono(p);
        QVERIFY2(out.ok, "der LIST-Block darf den Leser nicht aus dem Tritt bringen");
        QCOMPARE(out.samples.size(), 2);
        QVERIFY(qAbs(out.samples[0] - 0.25f) < 1e-3f);
    }
    // ── 16 Bit mit Dither ────────────────────────────────────────────
    //
    // Aus der Thetis-Durchsicht vom 2026-08-19 (AudioBitDepthMode +
    // DitherEnabled). Eine halbe Stunde Stereo sind in float32 690 MB,
    // in PCM16 noch 173 MB.
    void sixteenBitStereoRoundTripsWithinOneBit()
    {
        const QString p = pathFor(QStringLiteral("pcm16.wav"));
        QVector<float> stereo;
        for (int i = 0; i < 500; ++i) {
            stereo << 0.5f << -0.25f;
        }
        QString err;
        QVERIFY2(writeWavStereo16(p, stereo, 48000, true, &err),
                 qPrintable(err));

        const WavData back = readWavMono(p, &err);
        QVERIFY2(back.ok, qPrintable(err));
        QCOMPARE(back.sampleRate, 48000);
        QCOMPARE(back.samples.size(), 500);

        // readWavMono mittelt: (0,5 + -0,25) / 2 = 0,125. Ein Bit bei
        // 16 Bit sind rund 3e-5, der Dither traegt bis zu einem Bit.
        for (int i = 0; i < 500; ++i) {
            QVERIFY2(qAbs(back.samples[i] - 0.125f) < 1e-4f,
                     "16-Bit-Rundung darf hoechstens ein Bit kosten");
        }
    }

    // Ohne Dither muss dieselbe Eingabe zweimal dieselbe Datei ergeben —
    // und MIT Dither auch, weil der Startwert fest ist. Zufall, der
    // sich nicht wiederholen laesst, macht jeden Vergleich unmoeglich.
    void savingTwiceGivesTheSameBytes()
    {
        QVector<float> stereo;
        for (int i = 0; i < 200; ++i) { stereo << 0.3f << 0.1f; }

        const QString a = pathFor(QStringLiteral("twice-a.wav"));
        const QString b = pathFor(QStringLiteral("twice-b.wav"));
        QVERIFY(writeWavStereo16(a, stereo, 8000, true));
        QVERIFY(writeWavStereo16(b, stereo, 8000, true));

        QFile fa(a), fb(b);
        QVERIFY(fa.open(QIODevice::ReadOnly));
        QVERIFY(fb.open(QIODevice::ReadOnly));
        QCOMPARE(fa.readAll(), fb.readAll());
    }

    // Was ueber den Rand geht, wird gekappt und laeuft nicht um. Ein
    // Ueberlauf, der umlaeuft, klingt wie ein Schuss.
    void loudInputIsClippedNotWrapped()
    {
        const QString p = pathFor(QStringLiteral("loud.wav"));
        QVector<float> stereo;
        for (int i = 0; i < 100; ++i) { stereo << 2.0f << -2.0f; }
        QVERIFY(writeWavStereo16(p, stereo, 8000, false));

        const WavData back = readWavMono(p);
        QVERIFY(back.ok);
        for (float v : back.samples) {
            QVERIFY2(qAbs(v) < 1e-3f,
                     "+1 und -1 mitteln sich zu null; ein Umlauf haette "
                     "das Vorzeichen gedreht");
        }
    }

    // Die Datei ist halb so gross wie float32 — der ganze Grund.
    void itIsHalfTheSizeOfFloat32()
    {
        QVector<float> stereo(2000, 0.2f);
        const QString big   = pathFor(QStringLiteral("size-f32.wav"));
        const QString small = pathFor(QStringLiteral("size-16.wav"));
        QVERIFY(writeWavStereo(big, stereo, 8000));
        QVERIFY(writeWavStereo16(small, stereo, 8000, true));

        const qint64 b = QFileInfo(big).size();
        const qint64 s = QFileInfo(small).size();
        QVERIFY2(s < b / 2 + 64,
                 qPrintable(QStringLiteral("float32 %1 B, PCM16 %2 B")
                                .arg(b).arg(s)));
    }

    // ── WavStreamWriter (Phase 3M Recording) ─────────────────────────
    //
    // Anders als writeWavStereo*: die Datei wird beim Schreiben schon
    // angelegt, der Kopf traegt erst 0 als Groesse und wird bei
    // close() nachgetragen. Der Rundgang muss trotzdem stimmen — sonst
    // hat eine lange Aufnahme am Ende einen falschen Kopf und jedes
    // Abspielprogramm haelt sie fuer leer oder abgeschnitten.

    void streamWriterRoundTripsFloat32()
    {
        const QString p = pathFor(QStringLiteral("stream-f32.wav"));
        WavStreamWriter w;
        QString err;
        QVERIFY2(w.open(p, 48000, WavStreamWriter::Format::Float32Stereo,
                        /*dither=*/true, &err),
                 qPrintable(err));

        // In mehreren Bloecken schreiben, wie es der Zeitgeber-Abholer
        // tut — nicht alles auf einmal.
        const QVector<float> block1 = {0.5f, -0.5f, 0.25f, -0.25f};
        const QVector<float> block2 = {0.1f, -0.1f};
        QVERIFY(w.writeInterleaved(block1.constData(), 2));
        QVERIFY(w.writeInterleaved(block2.constData(), 1));
        QCOMPARE(w.framesWritten(), 3);
        w.close();
        QVERIFY(!w.isOpen());

        const WavData back = readWavMono(p);
        QVERIFY(back.ok);
        QCOMPARE(back.sampleRate, 48000);
        QCOMPARE(back.samples.size(), 3);   // readWavMono mittelt L/R
        QCOMPARE(back.samples[0], 0.0f);    // (0,5 + -0,5) / 2
        QVERIFY(qAbs(back.samples[1] - 0.0f) < 1e-6f);
        QVERIFY(qAbs(back.samples[2] - 0.0f) < 1e-6f);
    }

    void streamWriterRoundTripsPcm16WithinOneBit()
    {
        const QString p = pathFor(QStringLiteral("stream-pcm16.wav"));
        WavStreamWriter w;
        QVERIFY(w.open(p, 8000, WavStreamWriter::Format::Pcm16Stereo, true));

        for (int i = 0; i < 10; ++i) {
            const QVector<float> block = {0.5f, -0.25f};
            QVERIFY(w.writeInterleaved(block.constData(), 1));
        }
        w.close();

        const WavData back = readWavMono(p);
        QVERIFY(back.ok);
        QCOMPARE(back.samples.size(), 10);
        for (float v : back.samples) {
            QVERIFY2(qAbs(v - 0.125f) < 1e-4f,
                     "16-Bit-Rundung darf hoechstens ein Bit kosten");
        }
    }

    // Die Groesse im RIFF- und im data-Blockkopf muss die WAHRE
    // Laenge tragen, nicht die 0 vom Anlegen — sonst liest z. B. ein
    // Werkzeug, das die Groesse statt bis zum Dateiende glaubt, gar
    // nichts.
    void streamWriterPatchesTheHeaderSizesOnClose()
    {
        const QString p = pathFor(QStringLiteral("stream-sizes.wav"));
        WavStreamWriter w;
        QVERIFY(w.open(p, 44100, WavStreamWriter::Format::Pcm16Stereo, false));

        const QVector<float> block(2 * 500, 0.1f);   // 500 Rahmen
        QVERIFY(w.writeInterleaved(block.constData(), 500));
        w.close();

        QFile f(p);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray blob = f.readAll();
        f.close();

        // data-Groesse bei Offset 40: 500 Rahmen * 2 Kanaele * 2 Byte.
        const quint32 dataBytes = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar*>(blob.constData() + 40));
        QCOMPARE(dataBytes, static_cast<quint32>(500 * 2 * 2));

        // RIFF-Groesse bei Offset 4: alles nach den ersten 8 Byte.
        const quint32 riffBytes = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar*>(blob.constData() + 4));
        QCOMPARE(riffBytes, static_cast<quint32>(36 + dataBytes));

        // Und die Datei ist wirklich so lang, wie der Kopf behauptet —
        // sonst waere das Nachtragen nur Kosmetik am Kopf gewesen.
        QCOMPARE(static_cast<quint32>(blob.size()), 44u + dataBytes);
    }

    void streamWriterClosingTwiceIsHarmless()
    {
        const QString p = pathFor(QStringLiteral("stream-double-close.wav"));
        WavStreamWriter w;
        QVERIFY(w.open(p, 8000, WavStreamWriter::Format::Pcm16Stereo));
        const QVector<float> block = {0.2f, 0.2f};
        QVERIFY(w.writeInterleaved(block.constData(), 1));
        w.close();
        w.close();   // darf nicht abstuerzen und nichts mehr aendern
        QVERIFY(!w.isOpen());
    }

    void writingWithoutOpeningIsRefused()
    {
        WavStreamWriter w;
        const QVector<float> block = {0.1f, 0.1f};
        QVERIFY2(!w.writeInterleaved(block.constData(), 1),
                 "ohne open() gibt es nichts zu schreiben in");
    }

};

QTEST_MAIN(TestWavFile)
#include "tst_wav_file.moc"
