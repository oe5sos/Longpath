// =================================================================
// tests/tst_cw_decoder.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. CwDecoder (the ggmorse wrapper) against a
// synthetic CW signal:
//   * a 700 Hz, 20 WPM "CQ TEST DE OE5SOS" keyed with 5 ms edges at
//     48 kHz decodes to that text (a decoder that never locks decodes
//     nothing; the worker thread and the ring are exercised for real)
//   * the pitch and speed estimates land near 700 Hz / 20 WPM
//   * silence decodes nothing
//   * the ring drops the oldest audio instead of growing without bound
//   * stop() joins the worker and clears the unlocked estimates
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/CwDecoder.h"

#include <cmath>
#include <map>
#include <vector>

using namespace Longpath;

namespace {

constexpr int   kRate = 48000;
constexpr float kPi = 3.14159265358979f;

// ITU-R M.1677 Morse code, the letters and figures this test needs.
const std::map<QChar, QString>& morseTable()
{
    static const std::map<QChar, QString> t = {
        {'A', "._"}, {'B', "_..."}, {'C', "_._."}, {'D', "_.."}, {'E', "."},
        {'F', ".._."}, {'G', "__."}, {'H', "...."}, {'I', ".."}, {'J', ".___"},
        {'K', "_._"}, {'L', "._.."}, {'M', "__"}, {'N', "_."}, {'O', "___"},
        {'P', ".__."}, {'Q', "__._"}, {'R', "._."}, {'S', "..."}, {'T', "_"},
        {'U', ".._"}, {'V', "..._"}, {'W', ".__"}, {'X', "_.._"}, {'Y', "_.__"},
        {'Z', "__.."}, {'0', "_____"}, {'1', ".____"}, {'2', "..___"},
        {'3', "...__"}, {'4', "...._"}, {'5', "....."}, {'6', "_...."},
        {'7', "__..."}, {'8', "___.."}, {'9', "____."},
    };
    return t;
}

// Keyed tone as interleaved stereo float32: PARIS timing (dit = 1.2 s / wpm),
// 5 ms raised-cosine edges, `leadSilenceS` / `tailSilenceS` of nothing.
std::vector<float> keyedCw(const QString& text, float toneHz, float wpm,
                           float leadSilenceS = 0.5f, float tailSilenceS = 1.5f)
{
    const float dit = 1.2f / wpm;
    std::vector<float> key;   // mono key envelope, 0/1 per sample
    auto add = [&](bool on, float seconds) {
        const int n = static_cast<int>(std::lround(seconds * kRate));
        key.insert(key.end(), static_cast<size_t>(n), on ? 1.0f : 0.0f);
    };
    add(false, leadSilenceS);
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i).toUpper();
        if (c == ' ') { add(false, 7 * dit - 3 * dit); continue; }   // word gap on top of the letter gap
        const auto it = morseTable().find(c);
        if (it == morseTable().end()) { continue; }
        const QString& code = it->second;
        for (int k = 0; k < code.size(); ++k) {
            add(true, code.at(k) == '.' ? dit : 3 * dit);
            add(false, dit);
        }
        add(false, 2 * dit);   // letter gap: 3 dits total
    }
    add(false, tailSilenceS);

    // Edges: 5 ms raised cosine so the envelope has no clicks.
    const int edge = kRate * 5 / 1000;
    std::vector<float> env = key;
    for (size_t i = 1; i < key.size(); ++i) {
        if (key[i] != key[i - 1]) {
            for (int j = 0; j < edge && i + j < key.size(); ++j) {
                const float w = 0.5f - 0.5f * std::cos(kPi * float(j) / float(edge));
                env[i + j] = key[i] > 0.5f ? w : 1.0f - w;
            }
        }
    }
    std::vector<float> out(env.size() * 2);
    for (size_t i = 0; i < env.size(); ++i) {
        const float s = 0.5f * env[i] * std::sin(2.0f * kPi * toneHz * float(i) / float(kRate));
        out[2 * i] = s;
        out[2 * i + 1] = s;
    }
    return out;
}

QString collect(const QSignalSpy& spy)
{
    QString s;
    for (const QList<QVariant>& args : spy) { s += args.at(0).toString(); }
    return s;
}

// Feed in 20 ms slices from the calling thread, letting the event loop
// deliver the worker's queued signals in between, as the applet's pump does.
void feedAll(CwDecoder& d, const std::vector<float>& stereo)
{
    const int framesPerSlice = kRate / 50;
    const int frames = static_cast<int>(stereo.size() / 2);
    for (int at = 0; at < frames; at += framesPerSlice) {
        const int n = std::min(framesPerSlice, frames - at);
        d.feedAudio(stereo.data() + static_cast<size_t>(at) * 2, n);
        QTest::qWait(2);
    }
}

} // namespace

class TestCwDecoder : public QObject {
    Q_OBJECT

private slots:
    void decodesKeyedText()
    {
        CwDecoder d;
        QSignalSpy text(&d, &CwDecoder::textDecoded);
        QSignalSpy stats(&d, &CwDecoder::statsUpdated);
        d.start();
        QVERIFY(d.isRunning());

        const QString sent = QStringLiteral("CQ TEST DE OE5SOS");
        feedAll(d, keyedCw(sent, 700.0f, 20.0f));
        // Let the worker drain the ring (the signal is 12 s of audio; the
        // decoder works faster than real time).
        QTRY_VERIFY_WITH_TIMEOUT(d.queuedSamplesForTest() < kRate / 10, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(collect(text).contains(QStringLiteral("TEST")), 5000);
        const QString got = collect(text).simplified();
        QVERIFY2(got.contains(QStringLiteral("CQ TEST DE OE5SOS")),
                 qPrintable(QStringLiteral("decoded: '%1'").arg(got)));

        QVERIFY(stats.count() > 0);
        QVERIFY2(std::abs(d.estimatedPitch() - 700.0f) < 40.0f,
                 qPrintable(QStringLiteral("pitch %1").arg(d.estimatedPitch())));
        QVERIFY2(std::abs(d.estimatedSpeed() - 20.0f) < 4.0f,
                 qPrintable(QStringLiteral("speed %1").arg(d.estimatedSpeed())));
        d.stop();
        QVERIFY(!d.isRunning());
    }

    void silenceDecodesNothing()
    {
        CwDecoder d;
        QSignalSpy text(&d, &CwDecoder::textDecoded);
        d.start();
        std::vector<float> silence(kRate * 2 * 3, 0.0f);   // 3 s stereo
        feedAll(d, silence);
        QTRY_VERIFY_WITH_TIMEOUT(d.queuedSamplesForTest() < kRate / 10, 10000);
        QTest::qWait(200);
        QCOMPARE(collect(text).trimmed(), QString());
        d.stop();
    }

    void ringDropsTheOldestBeyondCapacity()
    {
        CwDecoder d;
        // Not started: feedAudio refuses, the ring stays empty.
        std::vector<float> stereo(kRate * 2, 0.1f);
        d.feedAudio(stereo.data(), kRate);
        QCOMPARE(d.queuedSamplesForTest(), 0);
        // Started with a worker that is starved of frames? It drains as
        // fast as it can, so bound the test to the trim itself: a single
        // oversized feed (6 s) must leave at most 4 s.
        d.start();
        std::vector<float> big(kRate * 2 * 6, 0.0f);
        d.feedAudio(big.data(), kRate * 6);
        QVERIFY(d.queuedSamplesForTest() <= kRate * 4);
        d.stop();
    }

    void stopClearsUnlockedEstimates()
    {
        CwDecoder d;
        QSignalSpy stats(&d, &CwDecoder::statsUpdated);
        d.start();
        feedAll(d, keyedCw(QStringLiteral("VVV"), 600.0f, 25.0f, 0.3f, 1.0f));
        QTRY_VERIFY_WITH_TIMEOUT(d.estimatedPitch() > 0.0f, 15000);
        d.lockPitch(true);
        QVERIFY(d.isPitchLocked());
        const float locked = d.estimatedPitch();
        d.stop();
        // The locked pitch survives, the unlocked speed is cleared; the
        // clearing emission arrives through the queue.
        QCOMPARE(d.estimatedPitch(), locked);
        QCOMPARE(d.estimatedSpeed(), 0.0f);
        QTRY_VERIFY(stats.count() > 0 && stats.last().at(1).toFloat() == 0.0f);
        d.lockPitch(false);
        QVERIFY(!d.isPitchLocked());
    }
};

QTEST_GUILESS_MAIN(TestCwDecoder)
#include "tst_cw_decoder.moc"
