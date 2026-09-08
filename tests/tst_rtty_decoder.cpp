// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: tests an AetherSDR port (see src/core/RttyDecoder.h);
// the inline Thetis mention below is a single default-value citation,
// not a Thetis derivation of this test.
//
// core/RttyDecoder.{h,cpp}: feeds a synthetic AFSK RTTY signal (real sine
// tones, Baudot/ITA2-encoded start-stop framing) through the actual
// biquad-filter/Schmitt-trigger/clock-recovery decode path and checks the
// decoded text comes back correctly -- not just that the code compiles.

#include <QtTest>
#include <QSignalSpy>

#include "core/RttyDecoder.h"

#include <cmath>

using namespace Longpath;

namespace {

// Same ITA2/Baudot LTRS table as RttyDecoder.cpp (the international
// standard, not AetherSDR-specific -- see that file's own header comment).
// Reversed here (char -> 5-bit code) to encode a test message.
int ltrsCodeFor(QChar c)
{
    static const char kTable[32] = {
        '\0', 'E', '\n', 'A', ' ', 'S', 'I', 'U',
        '\r', 'D',  'R', 'J', 'N', 'F', 'C', 'K',
         'T', 'Z',  'L', 'W', 'H', 'Y', 'P', 'Q',
         'O', 'B',  'G', '\0', 'M', 'X', 'V', '\0'
    };
    const char ascii = c.toLatin1();
    for (int i = 0; i < 32; ++i) {
        if (kTable[i] == ascii) { return i; }
    }
    return -1;
}

// Generates a real AFSK RTTY waveform (continuous-phase sine tones, genuine
// start-stop Baudot framing: 1 start bit (space) + 5 data bits LSB-first +
// 1.5 stop bits (mark)) for `text` (LTRS-only characters). This exercises
// RttyDecoder's actual bandpass/envelope/Schmitt-trigger/clock-recovery
// pipeline end to end, not just the lookup table.
QVector<float> generateRttyAudio(const QString& text, int markHz, int shiftHz,
                                  double baud, double sampleRate)
{
    const double spaceHz = markHz - shiftHz;  // non-reversed, matches RttyDecoder's default
    struct Seg { bool mark; double bits; };
    QVector<Seg> segs;
    segs.append({true, 6.0});  // settle: let the envelope filters lock onto idle mark

    for (const QChar c : text) {
        const int code = ltrsCodeFor(c);
        Q_ASSERT(code >= 0);
        segs.append({false, 1.0});  // start bit = space
        for (int b = 0; b < 5; ++b) {
            segs.append({static_cast<bool>((code >> b) & 1), 1.0});
        }
        segs.append({true, 1.5});  // stop bits = mark
    }
    segs.append({true, 3.0});  // trailing settle

    QVector<float> out;
    double phase = 0.0;
    const double samplesPerBit = sampleRate / baud;
    for (const auto& seg : segs) {
        const double freq = seg.mark ? markHz : spaceHz;
        const int n = static_cast<int>(std::lround(seg.bits * samplesPerBit));
        const double phaseInc = 2.0 * M_PI * freq / sampleRate;
        for (int i = 0; i < n; ++i) {
            out.append(static_cast<float>(0.6 * std::sin(phase)));
            phase += phaseInc;
            if (phase > 2.0 * M_PI) { phase -= 2.0 * M_PI; }
        }
    }
    return out;
}

// RttyDecoder::feedAudio expects interleaved-stereo (L, R, L, R, ...), same
// shape as the live audio tap (AudioEngine writes dual-mono blocks). Turns
// a mono buffer into that shape.
QVector<float> toInterleavedStereo(const QVector<float>& mono)
{
    QVector<float> stereo(mono.size() * 2);
    for (int i = 0; i < mono.size(); ++i) {
        stereo[2 * i] = mono[i];
        stereo[2 * i + 1] = mono[i];
    }
    return stereo;
}

} // namespace

class TstRttyDecoder : public QObject
{
    Q_OBJECT

private slots:
    void decodesASyntheticMessage()
    {
        // Thetis-sourced default mark/shift (SliceModel::m_rttyMarkHz/
        // m_rttyShiftHz, From Thetis setup.designer.cs:40635-40665
        // [v2.10.3.13]) at the standard ham HF RTTY baud rate.
        constexpr int kMarkHz  = 2295;
        constexpr int kShiftHz = 170;
        constexpr double kBaud = 45.45;
        constexpr double kSampleRate = 48000.0;

        // A leading throwaway word primes the clock-recovery loop (the
        // "gentle clock correction on transitions" in decodeLoop() takes a
        // few characters to converge from cold start -- real RTTY copy has
        // the same warm-up behavior on any live decoder, this one included).
        // The actual assertion is on the word AFTER that, once locked.
        const QString warmup  = QStringLiteral("TEST");
        const QString message = QStringLiteral("RTTY");
        const QVector<float> mono = generateRttyAudio(warmup + message, kMarkHz, kShiftHz,
                                                        kBaud, kSampleRate);
        const QVector<float> stereo = toInterleavedStereo(mono);

        RttyDecoder decoder;
        decoder.setMarkFreqHz(kMarkHz);
        decoder.setShiftHz(kShiftHz);
        decoder.setBaudRate(static_cast<float>(kBaud));
        decoder.start();

        QSignalSpy textSpy(&decoder, &RttyDecoder::textDecoded);
        decoder.feedAudio(stereo.constData(), mono.size());

        // The worker thread decodes in real 10ms chunks regardless of how
        // fast this test feeds it, so this genuinely needs wall-clock time
        // to pass -- QTRY_* polls instead of a single fixed sleep.
        QTRY_VERIFY_WITH_TIMEOUT(textSpy.count() >= warmup.size(), 5000);
        QTest::qWait(500);  // let any trailing characters land

        QString decoded;
        for (const QList<QVariant>& call : textSpy) {
            decoded += call.at(0).toString();
        }
        QVERIFY2(decoded.endsWith(message),
                 qPrintable(QStringLiteral("expected decoded text to end with '%1', got '%2'")
                                .arg(message, decoded)));

        decoder.stop();
    }

    void statsReportLockOnCleanSignal()
    {
        constexpr int kMarkHz  = 2295;
        constexpr int kShiftHz = 170;
        constexpr double kBaud = 45.45;
        constexpr double kSampleRate = 48000.0;

        // A long run of alternating characters gives statsUpdated (fired
        // ~2x/second) something to lock onto.
        const QVector<float> mono = generateRttyAudio(QStringLiteral("RTTYRTTYRTTYRTTY"),
                                                        kMarkHz, kShiftHz, kBaud, kSampleRate);
        const QVector<float> stereo = toInterleavedStereo(mono);

        RttyDecoder decoder;
        decoder.setMarkFreqHz(kMarkHz);
        decoder.setShiftHz(kShiftHz);
        decoder.setBaudRate(static_cast<float>(kBaud));
        decoder.start();

        QSignalSpy statsSpy(&decoder, &RttyDecoder::statsUpdated);
        decoder.feedAudio(stereo.constData(), mono.size());

        QTRY_VERIFY_WITH_TIMEOUT(statsSpy.count() >= 1, 5000);

        bool sawLock = false;
        for (const QList<QVariant>& call : statsSpy) {
            if (call.at(3).toBool()) { sawLock = true; break; }
        }
        QVERIFY2(sawLock, "expected at least one statsUpdated(locked=true) on a clean signal");

        decoder.stop();
    }

    void silenceProducesNoText()
    {
        RttyDecoder decoder;
        decoder.start();

        QSignalSpy textSpy(&decoder, &RttyDecoder::textDecoded);
        QVector<float> silence(48000 * 2, 0.0f);  // 0.5s stereo silence
        decoder.feedAudio(silence.constData(), 48000);

        QTest::qWait(300);
        QCOMPARE(textSpy.count(), 0);

        decoder.stop();
    }
};

QTEST_MAIN(TstRttyDecoder)
#include "tst_rtty_decoder.moc"
