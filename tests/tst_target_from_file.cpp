// =================================================================
// tests/tst_target_from_file.cpp  (NereusSDR)
// =================================================================
//
// A target taken from a recording is only worth having if the reading
// is right. A misparsed header does not produce an error, it produces
// a plausible-looking curve derived from metadata — which is the worst
// possible failure here, because it looks like a measurement.
//
// So these tests build WAV files byte by byte, including the awkward
// ones: a LIST chunk between fmt and data, 24-bit, 8-bit unsigned,
// float, and stereo. Then they check that a known shape comes back as
// that shape — at the right frequency, at any recording level, and with
// or without the silence every real recording begins and ends with.
//
// The test signal is deliberately not a sine. See the note on buzz()
// below: a sine was tried first, and the forty-decibel disagreements it
// produced are what found the missing dynamic-range floor in
// TargetFromFile. Verification is supposed to find things.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/StripTargets.h"
#include "core/strip/TargetFromFile.h"

#include <QByteArray>
#include <QTemporaryDir>
#include <QFile>
#include <QtTest>

#include <cmath>
#include <vector>

using namespace NereusSDR;

namespace {

void put32(QByteArray& b, uint32_t v)
{
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
    b.append(char((v >> 16) & 0xFF));
    b.append(char((v >> 24) & 0xFF));
}

void put16(QByteArray& b, uint16_t v)
{
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
}

// A WAV built the long way round, so the parser is tested against real
// bytes rather than against a struct that happens to match it.
QByteArray makeWav(const std::vector<float>& mono, int rate, int channels,
                   int bits, bool isFloat, bool withListChunk)
{
    QByteArray fmt;
    put16(fmt, uint16_t(isFloat ? 3 : 1));
    put16(fmt, uint16_t(channels));
    put32(fmt, uint32_t(rate));
    const int block = channels * bits / 8;
    put32(fmt, uint32_t(rate * block));
    put16(fmt, uint16_t(block));
    put16(fmt, uint16_t(bits));

    QByteArray data;
    for (float s : mono) {
        for (int ch = 0; ch < channels; ++ch) {
            if (isFloat) {
                float f = s;
                data.append(reinterpret_cast<const char*>(&f), sizeof(f));
            } else if (bits == 8) {
                data.append(char(uint8_t(std::lround(s * 127.0f) + 128)));
            } else if (bits == 16) {
                put16(data, uint16_t(int16_t(std::lround(s * 32767.0f))));
            } else if (bits == 24) {
                const int32_t v = int32_t(std::lround(double(s) * 8388607.0));
                data.append(char(v & 0xFF));
                data.append(char((v >> 8) & 0xFF));
                data.append(char((v >> 16) & 0xFF));
            } else {
                put32(data, uint32_t(int32_t(std::llround(double(s)
                                                          * 2147483000.0))));
            }
        }
    }

    QByteArray body;
    body.append("WAVE");
    body.append("fmt ");
    put32(body, uint32_t(fmt.size()));
    body.append(fmt);
    if (withListChunk) {
        // The chunk that breaks every reader which assumes byte 44.
        const QByteArray junk("INFOISFTLavf58.29.100\0", 22);
        body.append("LIST");
        put32(body, uint32_t(junk.size()));
        body.append(junk);
    }
    body.append("data");
    put32(body, uint32_t(data.size()));
    body.append(data);

    QByteArray out;
    out.append("RIFF");
    put32(out, uint32_t(body.size()));
    out.append(body);
    return out;
}

std::vector<float> tone(double hz, int rate, double seconds, float amp)
{
    const auto n = size_t(rate * seconds);
    std::vector<float> v(n);
    for (size_t i = 0; i < n; ++i) {
        v[i] = amp * float(std::sin(2.0 * M_PI * hz * double(i) / rate));
    }
    return v;
}

// ── Something to measure ─────────────────────────────────────────────
//
// Four hundred-odd sinusoids 17 Hz apart with pseudo-random phases,
// shaped by two formant-like humps. It sounds like a buzz and it is not
// speech, but it has the two properties these tests need and a pure
// tone does not: energy everywhere across the voice range, so the
// spectrum is defined at every point rather than being leakage; and no
// periodicity that lines up with the analysis window, so the average
// converges to the same answer wherever the windows happen to land.
//
// A sine was the obvious test signal and the wrong one. Its "spectrum"
// away from the tone is numerical noise, and measuring that produced
// swings of forty decibels between two recordings of the same sound —
// which is how the dynamic-range floor in TargetFromFile came to exist.
//
// The generator is an LCG rather than anything from <random>, so the
// samples are identical on every platform and a failure is reproducible.
std::vector<float> buzz(int rate, double seconds, float amp = 0.3f,
                        double boostHz = 0.0, double boostDb = 0.0)
{
    const auto n = size_t(double(rate) * seconds);
    std::vector<double> x(n, 0.0);
    uint32_t st = 12345;
    const double top = std::min(7000.0, rate * 0.45);
    for (double f = 40.0; f < top; f += 17.0) {
        st = uint32_t((uint64_t(st) * 1103515245ull + 12345ull) & 0x7FFFFFFFull);
        const double ph = (double(st) / 2147483648.0) * 2.0 * M_PI;
        double g = 1.0 / (1.0 + std::pow((f - 500.0) / 400.0, 2.0))
                 + 0.7 / (1.0 + std::pow((f - 1800.0) / 700.0, 2.0))
                 + 0.15 / (1.0 + std::pow((f - 3200.0) / 1200.0, 2.0));
        if (boostHz > 0.0) {
            const double d = std::log2(f / boostHz) * 2.0;
            g *= std::pow(10.0, (boostDb / (1.0 + d * d)) / 20.0);
        }
        for (size_t i = 0; i < n; ++i) {
            x[i] += g * std::sin(2.0 * M_PI * f * double(i) / rate + ph);
        }
    }
    double peak = 0.0;
    for (double v : x) { peak = std::max(peak, std::abs(v)); }
    std::vector<float> out(n);
    for (size_t i = 0; i < n; ++i) {
        out[i] = float(amp * x[i] / (peak > 0.0 ? peak : 1.0));
    }
    return out;
}

// Where the twelve target frequencies sit, so a test can name a point
// by frequency rather than by index and stay right if the list moves.
int pointAt(double hz)
{
    const double* f = StripTargets::userPointFreqs();
    int best = 0;
    for (int i = 1; i < StripTargets::kUserPointCount; ++i) {
        if (std::abs(f[i] - hz) < std::abs(f[best] - hz)) { best = i; }
    }
    return best;
}

} // namespace

class TargetFromFileTest : public QObject {
    Q_OBJECT

private slots:

    // ── The header ───────────────────────────────────────────────────

    void rejectsNonWav()
    {
        const QByteArray junk("this is a text file, not a recording");
        const auto info = TargetFromFile::parseWavHeader(junk.constData(),
                                                         junk.size());
        QVERIFY(!info.ok);
        QVERIFY(!info.error.isEmpty());
    }

    void rejectsTruncated()
    {
        const QByteArray tiny("RIFF");
        const auto info = TargetFromFile::parseWavHeader(tiny.constData(),
                                                         tiny.size());
        QVERIFY(!info.ok);
    }

    void readsPlainHeader()
    {
        const QByteArray w = makeWav(tone(1000, 48000, 0.2, 0.5f),
                                     48000, 1, 16, false, false);
        const auto info = TargetFromFile::parseWavHeader(w.constData(),
                                                         w.size());
        QVERIFY2(info.ok, qPrintable(info.error));
        QCOMPARE(info.sampleRate, 48000);
        QCOMPARE(info.channels, 1);
        QCOMPARE(info.bitsPerSample, 16);
        QVERIFY(!info.isFloat);
        QCOMPARE(info.dataBytes, int64_t(48000 * 0.2) * 2);
    }

    // The one that matters. A reader that assumes the data chunk starts
    // at byte 44 plays several kilobytes of metadata as audio, and the
    // resulting "target" is noise shaped like a text string.
    void findsDataAfterAnUnknownChunk()
    {
        const QByteArray w = makeWav(tone(1000, 48000, 0.2, 0.5f),
                                     48000, 1, 16, false, true);
        const auto info = TargetFromFile::parseWavHeader(w.constData(),
                                                         w.size());
        QVERIFY2(info.ok, qPrintable(info.error));
        QVERIFY(info.dataOffset > 44);
        QCOMPARE(info.dataBytes, int64_t(48000 * 0.2) * 2);
    }

    void rejectsCompressed()
    {
        QByteArray w = makeWav(tone(1000, 48000, 0.2, 0.5f),
                               48000, 1, 16, false, false);
        // Turn the format tag into something that is not PCM or float.
        const int tagAt = 20;
        w[tagAt] = char(0x11);          // IMA ADPCM
        const auto info = TargetFromFile::parseWavHeader(w.constData(),
                                                         w.size());
        QVERIFY(!info.ok);
        QVERIFY(info.error.contains(QStringLiteral("compressed")));
    }

    // ── Decoding ─────────────────────────────────────────────────────

    void decodesEveryWidthToTheSameSignal()
    {
        const std::vector<float> src = tone(1000, 48000, 0.05, 0.5f);
        struct Case { int bits; bool isFloat; double tol; };
        for (const Case cs : {Case{8, false, 0.02}, Case{16, false, 0.001},
                              Case{24, false, 0.0005},
                              Case{32, false, 0.0005},
                              Case{32, true, 1e-6}}) {
            const QByteArray w = makeWav(src, 48000, 1, cs.bits, cs.isFloat,
                                         false);
            const auto info = TargetFromFile::parseWavHeader(w.constData(),
                                                             w.size());
            QVERIFY2(info.ok, qPrintable(info.error));
            const auto got = TargetFromFile::decodeMono(w.constData(),
                                                        w.size(), info);
            QCOMPARE(got.size(), src.size());
            double worst = 0.0;
            for (size_t i = 0; i < src.size(); ++i) {
                worst = std::max(worst, std::abs(double(got[i] - src[i])));
            }
            QVERIFY2(worst < cs.tol,
                     qPrintable(QStringLiteral("%1-bit%2 worst error %3")
                                    .arg(cs.bits)
                                    .arg(cs.isFloat ? " float" : "")
                                    .arg(worst)));
        }
    }

    void averagesStereoToMono()
    {
        const std::vector<float> src = tone(1000, 48000, 0.05, 0.5f);
        const QByteArray w = makeWav(src, 48000, 2, 16, false, false);
        const auto info = TargetFromFile::parseWavHeader(w.constData(),
                                                         w.size());
        QVERIFY(info.ok);
        QCOMPARE(info.channels, 2);
        const auto got = TargetFromFile::decodeMono(w.constData(), w.size(),
                                                    info);
        // Both channels hold the same signal here, so the average is it.
        QCOMPARE(got.size(), src.size());
        QVERIFY(std::abs(double(got[100] - src[100])) < 0.001);
    }

    // ── The spectrum ─────────────────────────────────────────────────

    void refusesSomethingTooShortToAverage()
    {
        QString err;
        const auto v = TargetFromFile::ltasAtTargetPoints(
            tone(1000, 48000, 0.05, 0.5f), 48000, &err);
        QVERIFY(v.isEmpty());
        QVERIFY(err.contains(QStringLiteral("too short")));
    }

    void refusesSilence()
    {
        QString err;
        const auto v = TargetFromFile::ltasAtTargetPoints(
            std::vector<float>(48000 * 2, 0.0f), 48000, &err);
        QVERIFY(v.isEmpty());
        QVERIFY(!err.isEmpty());
    }

    // A recording with nothing at the 1 kHz reference cannot be
    // measured against it. Before the dynamic-range floor existed this
    // returned a curve anyway — offset by whatever the FFT's leakage
    // happened to be, which was tens of decibels and different every
    // time. Refusing is the only honest answer.
    void refusesAPureToneRatherThanMeasuringItsLeakage()
    {
        QString err;
        const auto v = TargetFromFile::ltasAtTargetPoints(
            tone(500, 48000, 2.0, 0.5f), 48000, &err);
        QVERIFY(v.isEmpty());
        QVERIFY2(err.contains(QStringLiteral("1 kHz")), qPrintable(err));
    }

    void measuresSomethingWithEnergyEverywhere()
    {
        QString err;
        const auto v = TargetFromFile::ltasAtTargetPoints(
            buzz(48000, 3.0), 48000, &err);
        QVERIFY2(!v.isEmpty(), qPrintable(err));
        QCOMPARE(v.size(), StripTargets::kUserPointCount);
        // The shape is two humps around 500 and 1800 Hz falling away
        // above 3 kHz — 6 kHz must be well below 500 Hz.
        QVERIFY(v[pointAt(500.0)] > v[pointAt(6000.0)] + 15.0);
    }

    // A target is a shape, not a level. Recording the same sound ten
    // decibels quieter must give the same curve — otherwise the target
    // depends on how far the operator was from the microphone, which is
    // the one thing it must not depend on.
    void isTheSameCurveAtAnyRecordingLevel()
    {
        QString e1, e2;
        const auto loud  = TargetFromFile::ltasAtTargetPoints(
            buzz(48000, 3.0, 0.3f), 48000, &e1);
        const auto quiet = TargetFromFile::ltasAtTargetPoints(
            buzz(48000, 3.0, 0.03f), 48000, &e2);
        QVERIFY2(!loud.isEmpty(), qPrintable(e1));
        QVERIFY2(!quiet.isEmpty(), qPrintable(e2));
        for (int i = 0; i < loud.size(); ++i) {
            QVERIFY2(std::abs(loud[i] - quiet[i]) < 0.5,
                     qPrintable(QStringLiteral("point %1: %2 vs %3")
                                    .arg(i).arg(loud[i]).arg(quiet[i])));
        }
    }

    // Leading and trailing silence must not change the answer. Every
    // real recording has both, and averaging them in would flatten
    // exactly the peaks the target exists to capture.
    void ignoresTheSilenceAroundTheSound()
    {
        const std::vector<float> sound = buzz(48000, 3.0);
        std::vector<float> padded(48000, 0.0f);
        padded.insert(padded.end(), sound.begin(), sound.end());
        padded.insert(padded.end(), 48000, 0.0f);

        QString e1, e2;
        const auto bare = TargetFromFile::ltasAtTargetPoints(sound, 48000,
                                                             &e1);
        const auto pad  = TargetFromFile::ltasAtTargetPoints(padded, 48000,
                                                             &e2);
        QVERIFY2(!bare.isEmpty(), qPrintable(e1));
        QVERIFY2(!pad.isEmpty(), qPrintable(e2));
        for (int i = 0; i < bare.size(); ++i) {
            QVERIFY2(std::abs(bare[i] - pad[i]) < 1.0,
                     qPrintable(QStringLiteral("point %1: %2 vs %3")
                                    .arg(i).arg(bare[i]).arg(pad[i])));
        }
    }

    // The whole point of the feature: a recording with more 3 kHz in it
    // must produce a target with more 3 kHz in it, and must not move
    // anything else. This is the test that would catch a smoothing
    // window applied on the wrong axis — an error that puts the answer
    // within a factor of two of right, which on a log axis is a whole
    // octave and a target that makes the voice worse.
    void followsTheShapeOfTheRecording()
    {
        QString e1, e2;
        const auto plain = TargetFromFile::ltasAtTargetPoints(
            buzz(48000, 3.0), 48000, &e1);
        const auto lifted = TargetFromFile::ltasAtTargetPoints(
            buzz(48000, 3.0, 0.3f, 3000.0, 12.0), 48000, &e2);
        QVERIFY2(!plain.isEmpty(), qPrintable(e1));
        QVERIFY2(!lifted.isEmpty(), qPrintable(e2));

        const int at3k = pointAt(3000.0);
        QVERIFY2(lifted[at3k] - plain[at3k] > 8.0,
                 qPrintable(QStringLiteral("3 kHz moved only %1 dB")
                                .arg(lifted[at3k] - plain[at3k])));
        for (double hz : {125.0, 200.0, 500.0, 800.0}) {
            const int i = pointAt(hz);
            QVERIFY2(std::abs(lifted[i] - plain[i]) < 2.0,
                     qPrintable(QStringLiteral("%1 Hz moved %2 dB and "
                                               "should not have")
                                    .arg(hz).arg(lifted[i] - plain[i])));
        }
    }

    // A rate of 8 kHz says nothing above 4 kHz. Reporting a deep cut
    // there would tell the operator to remove a band the recording
    // never covered.
    void doesNotInventAnythingAboveNyquist()
    {
        QString err;
        const auto v = TargetFromFile::ltasAtTargetPoints(
            buzz(8000, 3.0), 8000, &err);
        QVERIFY2(!v.isEmpty(), qPrintable(err));
        QCOMPARE(v[pointAt(6000.0)], v[pointAt(4500.0)]);
    }

    // ── End to end ───────────────────────────────────────────────────

    void readsAFileFromDisk()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("match.wav"));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            // Stereo, 24-bit, with a LIST chunk in the way: every
            // awkward part of the reader exercised in one file.
            const QByteArray w = makeWav(buzz(48000, 3.0), 48000, 2, 24,
                                         false, true);
            QCOMPARE(f.write(w), qint64(w.size()));
        }
        QString err;
        const auto v = TargetFromFile::fromWavFile(path, &err);
        QVERIFY2(!v.isEmpty(), qPrintable(err));
        QCOMPARE(v.size(), StripTargets::kUserPointCount);
        QVERIFY(v[pointAt(500.0)] > v[pointAt(6000.0)] + 15.0);
    }

    void namesTheProblemWhenTheFileIsMissing()
    {
        QString err;
        const auto v = TargetFromFile::fromWavFile(
            QStringLiteral("/nonexistent/nothing.wav"), &err);
        QVERIFY(v.isEmpty());
        QVERIFY(!err.isEmpty());
    }
};

QTEST_MAIN(TargetFromFileTest)
#include "tst_target_from_file.moc"
