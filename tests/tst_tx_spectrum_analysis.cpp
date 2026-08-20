// =================================================================
// tests/tst_tx_spectrum_analysis.cpp  (NereusSDR)
// =================================================================
//
// Occupied bandwidth is the one number in the audio tool that is about
// somebody else. Everything else in the channel strip is taste; this is
// whether the neighbours can use the band. A wrong figure here does not
// merely mislead the operator, it misleads them in public.
//
// So the tests are built on signals whose bandwidth is known by
// construction — brick-wall filtered noise — and check the measurement
// against the filter that made them.
//
// The important one is dip_in_the_middle_does_not_end_the_measurement.
// See the note there: the naive implementation is wrong by a factor of
// two, and wrong in the direction that tells a splattering station it is
// clean.
//
// no-port-check: NereusSDR-original.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/TxSpectrumAnalysis.h"

#include <QtTest/QtTest>

#include <cmath>
#include <complex>
#include <vector>

using namespace Longpath;

namespace {

constexpr int kRate = 48000;
constexpr int kFft  = 4096;
constexpr double kBinHz = double(kRate) / kFft;

// Deterministic white noise. An LCG rather than <random> so the samples
// are byte-identical on every platform and a failure is reproducible.
std::vector<double> whiteNoise(int n)
{
    std::vector<double> x(static_cast<size_t>(n));
    uint32_t st = 987654321u;
    for (int i = 0; i < n; ++i) {
        st = uint32_t((uint64_t(st) * 1103515245ull + 12345ull)
                      & 0x7FFFFFFFull);
        x[static_cast<size_t>(i)] = (double(st) / 1073741824.0) - 1.0;
    }
    return x;
}

void dft(std::vector<std::complex<double>>& a, bool inverse)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) { j ^= bit; }
        j ^= bit;
        if (i < j) { std::swap(a[i], a[j]); }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = (inverse ? 2.0 : -2.0) * M_PI / double(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<double> u = a[i + k];
                const std::complex<double> v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
    if (inverse) {
        for (auto& v : a) { v /= double(n); }
    }
}

// Noise with a brick wall at known edges, so the measurement has
// something true to be compared against. `notchLo/notchHi` optionally
// carve a deep hole in the middle.
std::vector<float> bandLimited(double loHz, double hiHz, int n,
                               double notchLo = 0.0, double notchHi = 0.0,
                               double notchDb = -40.0)
{
    // Round up to a power of two for the transform, then trim.
    size_t m = 1;
    while (m < size_t(n)) { m <<= 1; }
    const std::vector<double> x = whiteNoise(int(m));

    std::vector<std::complex<double>> a(m);
    for (size_t i = 0; i < m; ++i) { a[i] = {x[i], 0.0}; }
    dft(a, false);

    const double df = double(kRate) / double(m);
    const double notchGain = std::pow(10.0, notchDb / 20.0);
    for (size_t i = 0; i <= m / 2; ++i) {
        const double f = double(i) * df;
        double g = (f < loHz || f > hiHz) ? 0.0 : 1.0;
        if (notchHi > notchLo && f > notchLo && f < notchHi) {
            g *= notchGain;
        }
        a[i] *= g;
        if (i > 0 && i < m / 2) { a[m - i] *= g; }
    }
    dft(a, true);

    std::vector<float> out(static_cast<size_t>(n));
    double peak = 0.0;
    for (int i = 0; i < n; ++i) {
        peak = std::max(peak, std::abs(a[static_cast<size_t>(i)].real()));
    }
    if (peak <= 0.0) { peak = 1.0; }
    for (int i = 0; i < n; ++i) {
        out[static_cast<size_t>(i)] =
            float(0.5 * a[static_cast<size_t>(i)].real() / peak);
    }
    return out;
}

TxSpectrumAnalysis::Occupancy measure(const std::vector<float>& sig)
{
    const std::vector<double> mag = TxSpectrumAnalysis::ltasDb(sig, kFft);
    return TxSpectrumAnalysis::occupiedBandwidth(mag, kBinHz);
}

} // namespace

class TstTxSpectrumAnalysis : public QObject {
    Q_OBJECT
private slots:

    void ltas_refuses_a_block_too_short_to_transform()
    {
        QVERIFY(TxSpectrumAnalysis::ltasDb(std::vector<float>(100, 0.1f),
                                           kFft).empty());
    }

    void ltas_refuses_silence()
    {
        QVERIFY(TxSpectrumAnalysis::ltasDb(std::vector<float>(kRate, 0.0f),
                                           kFft).empty());
    }

    void ltas_refuses_a_bad_fft_size()
    {
        const std::vector<float> sig = bandLimited(300, 2700, kRate);
        QVERIFY(TxSpectrumAnalysis::ltasDb(sig, 1000).empty());   // not 2^n
        QVERIFY(TxSpectrumAnalysis::ltasDb(sig, 32).empty());     // too small
    }

    void an_empty_spectrum_is_reported_not_guessed()
    {
        const auto occ =
            TxSpectrumAnalysis::occupiedBandwidth({}, kBinHz);
        QVERIFY(!occ.valid);
        QVERIFY(!occ.note.isEmpty());
    }

    // Brick-wall filtered noise, 300 to 2700 Hz. The measurement should
    // land on the filter that made it, within a bin or two.
    void measures_the_bandwidth_it_was_given()
    {
        const auto occ = measure(bandLimited(300.0, 2700.0, kRate * 2));
        QVERIFY2(occ.valid, qPrintable(occ.note));

        QVERIFY2(std::abs(occ.lowHz26 - 300.0) < 40.0,
                 qPrintable(QStringLiteral("low edge %1").arg(occ.lowHz26)));
        QVERIFY2(std::abs(occ.highHz26 - 2700.0) < 40.0,
                 qPrintable(QStringLiteral("high edge %1").arg(occ.highHz26)));
        QVERIFY2(std::abs(occ.bandwidth26Hz - 2400.0) < 80.0,
                 qPrintable(QStringLiteral("bandwidth %1")
                                .arg(occ.bandwidth26Hz)));

        // -60 dB is measured further down the skirts, so it is wider.
        QVERIFY(occ.bandwidth60Hz >= occ.bandwidth26Hz);
    }

    // ── The test this file exists for ────────────────────────────────
    //
    // A real spectrum has dips in it. Walking OUTWARD from the peak
    // stops at the first one; on this signal — the same 300-2700 Hz
    // noise with a 40 dB notch at 1400-1500 — the outward walk reports
    // 1242 Hz against a true 2414 Hz. Wrong by a factor of two, and
    // wrong in the flattering direction: it would tell an operator who
    // is splattering that they are clean.
    //
    // Scanning inward from the ends cannot do that. Verified
    // independently in Python before this was written; both numbers
    // above are from that run.
    void a_dip_in_the_middle_does_not_end_the_measurement()
    {
        const auto plain = measure(bandLimited(300.0, 2700.0, kRate * 2));
        const auto notched =
            measure(bandLimited(300.0, 2700.0, kRate * 2, 1400.0, 1500.0));
        QVERIFY2(plain.valid, qPrintable(plain.note));
        QVERIFY2(notched.valid, qPrintable(notched.note));

        QVERIFY2(std::abs(notched.bandwidth26Hz - plain.bandwidth26Hz) < 80.0,
                 qPrintable(QStringLiteral("notch changed the bandwidth from "
                                           "%1 to %2 — the scan is stopping "
                                           "at the dip")
                                .arg(plain.bandwidth26Hz)
                                .arg(notched.bandwidth26Hz)));
        QVERIFY(notched.bandwidth26Hz > 2200.0);
    }

    // Driving the signal into a nonlinearity generates products outside
    // the original band. That is what splatter IS, and the measurement
    // has to see it — otherwise the number is decorative.
    void distortion_widens_the_measured_bandwidth()
    {
        const std::vector<float> clean = bandLimited(300.0, 2700.0, kRate * 2);
        std::vector<float> driven(clean.size());
        double peak = 0.0;
        for (size_t i = 0; i < clean.size(); ++i) {
            driven[i] = float(std::tanh(double(clean[i]) * 4.0));
            peak = std::max(peak, std::abs(double(driven[i])));
        }
        for (auto& v : driven) { v = float(0.5 * double(v) / peak); }

        const auto a = measure(clean);
        const auto b = measure(driven);
        QVERIFY2(a.valid && b.valid, "measurement failed");
        QVERIFY2(b.bandwidth26Hz > a.bandwidth26Hz + 1000.0,
                 qPrintable(QStringLiteral("clean %1 Hz, driven %2 Hz — the "
                                           "distortion products are not "
                                           "being seen")
                                .arg(a.bandwidth26Hz)
                                .arg(b.bandwidth26Hz)));
    }

    void the_peak_is_inside_the_band()
    {
        const auto occ = measure(bandLimited(300.0, 2700.0, kRate * 2));
        QVERIFY(occ.valid);
        QVERIFY(occ.peakHz >= 300.0 - kBinHz && occ.peakHz <= 2700.0 + kBinHz);
    }

    // ── Advice ───────────────────────────────────────────────────────

    void advice_is_silent_about_a_clean_signal()
    {
        const auto occ = measure(bandLimited(300.0, 2700.0, kRate * 2));
        QVERIFY(occ.valid);
        // A 2.4 kHz signal in a 2.7 kHz filter is exactly what it should
        // be. Saying anything here would train the operator to ignore it.
        QVERIFY(TxSpectrumAnalysis::advice(occ, 2700.0).isEmpty());
    }

    void advice_speaks_when_energy_escapes_the_filter()
    {
        // A signal far wider than the filter it is supposed to fit.
        const auto occ = measure(bandLimited(300.0, 5500.0, kRate * 2));
        QVERIFY2(occ.valid, qPrintable(occ.note));
        const QString said = TxSpectrumAnalysis::advice(occ, 2700.0);
        QVERIFY2(!said.isEmpty(), "nothing said about a signal twice the "
                                  "width of its filter");
        // Checking for a word, not for a typographic minus sign: a test
        // that fails when someone changes U+2212 to a hyphen is a test
        // about punctuation.
        QVERIFY(said.contains(QStringLiteral("filter")));
    }

    void advice_says_nothing_about_an_invalid_measurement()
    {
        TxSpectrumAnalysis::Occupancy occ;    // valid == false
        QVERIFY(TxSpectrumAnalysis::advice(occ, 2700.0).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TstTxSpectrumAnalysis)
#include "tst_tx_spectrum_analysis.moc"
