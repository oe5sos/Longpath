// The ring the equaliser's picture is drawn from.
//
// Small, but it sits on the transmit thread and hands data to the GUI
// without a lock, so the two things worth pinning are the ones that
// would be invisible if wrong: the snapshot must be in time order, and
// it must never claim more than it has.
// no-port-check: NereusSDR-original.

#include <QtTest/QtTest>

#include "core/strip/MicSpectrum.h"

#include <vector>

using namespace NereusSDR;

class TstMicSpectrum : public QObject {
    Q_OBJECT
private slots:
    void an_empty_ring_gives_nothing_rather_than_zeros();
    void a_snapshot_is_oldest_first();
    void it_keeps_the_newest_audio_when_it_wraps();
    void it_never_returns_more_than_it_has();
    void it_holds_enough_for_the_fifteen_second_average();
};

void TstMicSpectrum::an_empty_ring_gives_nothing_rather_than_zeros()
{
    // Zeros would draw as a flat line at -120 dB, which looks like a
    // measurement. Nothing draws as nothing.
    MicSpectrum s(48000);
    std::vector<float> out(1024, 7.0f);
    QCOMPARE(s.snapshot(out.data(), 1024), 0);
    QCOMPARE(out[0], 7.0f);            // untouched
    QCOMPARE(s.framesSeen(), 0ULL);
}

void TstMicSpectrum::a_snapshot_is_oldest_first()
{
    // An FFT of a time-reversed buffer has the same magnitude spectrum,
    // so getting this backwards would be invisible in the picture and
    // wrong in every phase-dependent thing built on it later.
    MicSpectrum s(48000);
    std::vector<float> in(8);
    for (int i = 0; i < 8; ++i) { in[static_cast<size_t>(i)] = float(i); }
    s.feed(in.data(), 8);

    std::vector<float> out(8, -1.0f);
    QCOMPARE(s.snapshot(out.data(), 8), 8);
    for (int i = 0; i < 8; ++i) {
        QCOMPARE(out[static_cast<size_t>(i)], float(i));
    }
}

void TstMicSpectrum::it_keeps_the_newest_audio_when_it_wraps()
{
    // Opposite policy to TxAudioRecorder, which drops the tail. A live
    // view that stopped updating once its ring filled would be a
    // picture of two seconds ago, forever.
    const int cap = 8 * MicSpectrum::kSeconds;
    MicSpectrum s(8);
    std::vector<float> a(static_cast<size_t>(cap), 1.0f);
    std::vector<float> b(static_cast<size_t>(cap), 2.0f);
    s.feed(a.data(), cap);
    s.feed(b.data(), cap);

    std::vector<float> out(static_cast<size_t>(cap), 0.0f);
    QCOMPARE(s.snapshot(out.data(), cap), cap);
    for (float v : out) { QCOMPARE(v, 2.0f); }
    QCOMPARE(s.framesSeen(), static_cast<unsigned long long>(cap) * 2);
}

void TstMicSpectrum::it_never_returns_more_than_it_has()
{
    // The caller asks for a full FFT window and must be told when it
    // cannot have one — otherwise the first picture is an FFT of
    // whatever the buffer was initialised to.
    MicSpectrum s(48000);
    std::vector<float> in(100, 0.5f);
    s.feed(in.data(), 100);

    std::vector<float> out(4096, 9.0f);
    QCOMPARE(s.snapshot(out.data(), 4096), 100);
    QCOMPARE(out[99], 0.5f);
    QCOMPARE(out[100], 9.0f);          // beyond what was written: untouched
}

void TstMicSpectrum::it_holds_enough_for_the_fifteen_second_average()
{
    // Hold averages fifteen seconds. The ring must be able to give
    // that, with room to spare so a full fifteen is there even when the
    // newest block landed a moment ago — otherwise the average is
    // silently taken over whatever happened to fit.
    QVERIFY2(MicSpectrum::kSeconds > MicSpectrum::kHoldSeconds,
             "the ring must be longer than the window it is asked for");

    MicSpectrum s(8000);
    const int want = 8000 * MicSpectrum::kHoldSeconds;
    std::vector<float> in(static_cast<size_t>(want), 0.25f);
    s.feed(in.data(), want);

    std::vector<float> out(static_cast<size_t>(want), 0.0f);
    QCOMPARE(s.snapshot(out.data(), want), want);
}

QTEST_APPLESS_MAIN(TstMicSpectrum)
#include "tst_mic_spectrum.moc"
