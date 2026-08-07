// =================================================================
// src/core/VoiceAnalyzer.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see VoiceAnalyzer.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "VoiceAnalyzer.h"

#include "models/TransmitModel.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>
#include <vector>

namespace NereusSDR {

namespace {

constexpr double kEps = 1e-20;

double toDb(double power) { return 10.0 * std::log10(std::max(power, kEps)); }

// Radix-2 in place. Small and self-contained on purpose: the project
// has FFTW, but pulling the spectrum path into a unit test would drag
// in the whole DSP build for something that runs once on a button
// press and takes microseconds either way.
void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    if (n < 2) { return; }

    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) { j ^= bit; }
        j ^= bit;
        if (i < j) { std::swap(a[i], a[j]); }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * M_PI / static_cast<double>(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<double> u = a[i + k];
                const std::complex<double> v = a[i + k + len / 2] * w;
                a[i + k]             = u + v;
                a[i + k + len / 2]   = u - v;
                w *= wl;
            }
        }
    }
}

// Which EQ band a frequency belongs to. Bands are an octave apart, so
// the boundary is the geometric mean of two neighbours — the halfway
// point on a logarithmic axis, which is where the ear puts it too.
int bandForHz(double hz)
{
    if (hz <= 0.0) { return -1; }
    for (int i = 0; i < kVoiceBandCount; ++i) {
        const double lo = (i == 0)
            ? 0.0
            : std::sqrt(kVoiceBandHz[i - 1] * kVoiceBandHz[i]);
        const double hi = (i == kVoiceBandCount - 1)
            ? 1e9
            : std::sqrt(kVoiceBandHz[i] * kVoiceBandHz[i + 1]);
        if (hz >= lo && hz < hi) { return i; }
    }
    return -1;
}

} // namespace

double VoiceAnalyzer::targetDb(double hz)
{
    // Anchored at 1 kHz = 0 dB. Interpolated logarithmically between
    // the points, because that is how the axis and the ear both work.
    struct Point { double hz; double db; };
    static const Point kCurve[] = {
        {  20.0, -24.0},   // nothing here but rumble and handling noise
        {  63.0, -20.0},
        { 125.0, -12.0},   // proximity-effect bass: costs headroom, adds nothing
        { 250.0,  -6.0},
        { 500.0,  -2.0},
        {1000.0,   0.0},
        {2000.0,  +3.0},   // where intelligibility lives
        {2700.0,  +2.0},   // the SSB filter starts closing
        {4000.0,  -3.0},
        {8000.0, -12.0},   // outside the passband; only feeds the limiter
        {20000.0,-20.0},
    };
    constexpr int n = int(sizeof(kCurve) / sizeof(kCurve[0]));

    if (hz <= kCurve[0].hz)     { return kCurve[0].db; }
    if (hz >= kCurve[n - 1].hz) { return kCurve[n - 1].db; }

    for (int i = 1; i < n; ++i) {
        if (hz <= kCurve[i].hz) {
            const double t = std::log(hz / kCurve[i - 1].hz)
                           / std::log(kCurve[i].hz / kCurve[i - 1].hz);
            return kCurve[i - 1].db + t * (kCurve[i].db - kCurve[i - 1].db);
        }
    }
    return 0.0;
}

void VoiceAnalyzer::suggestEq(
    const std::array<double, kVoiceBandCount>& measuredDb,
    std::array<double, kVoiceBandCount>& outEqDb,
    double& outPreampDb)
{
    // How far each band is from where it should be.
    std::array<double, kVoiceBandCount> want{};
    for (int i = 0; i < kVoiceBandCount; ++i) {
        want[i] = targetDb(kVoiceBandHz[i]) - measuredDb[i];
    }

    // A band that contains nothing cannot be shaped, and must not be
    // used to decide how much to cut everything else.
    //
    // This is where the obvious version of this function falls apart,
    // and it took a run against a realistic spectrum to see it. Shift
    // the whole shape down by its own maximum and that maximum comes
    // from the emptiest band — 16 kHz sits 45 dB below the voice, so it
    // "wants" +27 dB, and every real band then gets cut by 27 and lands
    // on the floor. The suggestion for an ordinary voice was every
    // slider at minimum.
    const double loudest = *std::max_element(measuredDb.begin(),
                                             measuredDb.end());
    constexpr double kPresentWithinDb = 30.0;
    std::array<bool, kVoiceBandCount> present{};
    for (int i = 0; i < kVoiceBandCount; ++i) {
        present[i] = measuredDb[i] > loudest - kPresentWithinDb;
    }

    // Anchor on the speech bands, 125 Hz to 2 kHz. Those are what
    // actually reaches the far end; letting 4 kHz set the reference
    // means cutting the whole voice to lift something the transmit
    // filter is about to remove.
    constexpr int kSpeechFirst = 2;   // 125 Hz
    constexpr int kSpeechLast  = 6;   // 2 kHz
    double anchor = -1e9;
    for (int i = kSpeechFirst; i <= kSpeechLast; ++i) {
        if (present[i]) { anchor = std::max(anchor, want[i]); }
    }
    if (anchor < -1e8) {
        anchor = *std::max_element(want.begin(), want.end());
    }

    // Cuts only. Reaching a target by boosting is how an automatic EQ
    // adds 20 dB of hiss to a band the microphone barely produces;
    // cutting to the same shape and making the level up with the preamp
    // lands in the same place without inventing anything.
    for (int i = 0; i < kVoiceBandCount; ++i) {
        outEqDb[i] = present[i]
            ? std::clamp(want[i] - anchor,
                         double(TransmitModel::kTxEqBandDbMin), 0.0)
            : 0.0;   // nothing there: leave the slider alone
    }
    const double highest = anchor;

    // Put back what the cuts took, within what the preamp can give.
    outPreampDb = std::clamp(highest,
                             double(TransmitModel::kTxEqPreampDbMin),
                             double(TransmitModel::kTxEqPreampDbMax));
}

VoiceAnalysis VoiceAnalyzer::analyse(const float* samples, int frames,
                                     int sampleRate)
{
    VoiceAnalysis r;
    if (!samples || frames <= 0 || sampleRate <= 0) {
        r.problem = QStringLiteral("Nothing to analyse");
        return r;
    }

    // ── Clipping ─────────────────────────────────────────────────────
    // Checked first and reported loudly. A clipped recording makes
    // every spectrum below it wrong — the flat tops add harmonics that
    // look like a bright voice — so the answer must be "fix this first"
    // rather than a confident and false EQ curve.
    for (int i = 0; i < frames; ++i) {
        if (std::abs(samples[i]) >= 0.999f) { ++r.clippedSamples; }
    }

    // ── Speech versus pause ──────────────────────────────────────────
    // A short-term energy envelope, then a threshold partway between
    // the quietest and loudest frames. Averaging the pauses into the
    // spectrum would measure the room, not the voice.
    const int win = std::max(64, sampleRate / 100);   // 10 ms
    const int windows = frames / win;
    if (windows < 4) {
        r.problem = QStringLiteral("Recording is too short to analyse");
        return r;
    }

    std::vector<double> energy(static_cast<size_t>(windows), 0.0);
    for (int w = 0; w < windows; ++w) {
        double sum = 0.0;
        for (int i = 0; i < win; ++i) {
            const double v = samples[w * win + i];
            sum += v * v;
        }
        energy[static_cast<size_t>(w)] = sum / win;
    }

    std::vector<double> sorted = energy;
    std::sort(sorted.begin(), sorted.end());
    const double quiet = sorted[static_cast<size_t>(windows * 0.10)];
    const double loud  = sorted[static_cast<size_t>(windows * 0.90)];
    // Geometric midpoint: a threshold set arithmetically sits far too
    // close to the loud end when the two are decades apart, which they
    // are for speech.
    const double gate = std::sqrt(std::max(quiet, kEps) * std::max(loud, kEps));

    r.noiseFloorDbFs = 10.0 * std::log10(std::max(quiet, kEps));
    r.speechDbFs     = 10.0 * std::log10(std::max(loud, kEps));

    int speechWindows = 0;
    for (double e : energy) { if (e > gate) { ++speechWindows; } }
    r.speechFraction  = double(speechWindows) / windows;
    r.analysedSeconds = double(speechWindows) * win / sampleRate;

    if (r.analysedSeconds < kMinSpeechSeconds) {
        r.problem = QStringLiteral(
            "Only %1 seconds of speech — say a little more")
            .arg(r.analysedSeconds, 0, 'f', 1);
        return r;
    }

    // ── Spectrum of the speech windows only ──────────────────────────
    constexpr int kFft = 4096;
    std::vector<double> power(kFft / 2 + 1, 0.0);
    int blocks = 0;

    // Hann window: without one, every block boundary is a step, and the
    // spectral leakage from those steps would swamp the differences
    // between bands that this whole analysis is about.
    std::vector<double> hann(kFft);
    for (int i = 0; i < kFft; ++i) {
        hann[static_cast<size_t>(i)] =
            0.5 * (1.0 - std::cos(2.0 * M_PI * i / (kFft - 1)));
    }

    for (int start = 0; start + kFft <= frames; start += kFft / 2) {
        const int w = start / win;
        if (w >= windows || energy[static_cast<size_t>(w)] <= gate) { continue; }

        std::vector<std::complex<double>> buf(kFft);
        for (int i = 0; i < kFft; ++i) {
            buf[static_cast<size_t>(i)] =
                samples[start + i] * hann[static_cast<size_t>(i)];
        }
        fft(buf);
        for (int k = 0; k <= kFft / 2; ++k) {
            power[static_cast<size_t>(k)] += std::norm(buf[static_cast<size_t>(k)]);
        }
        ++blocks;
    }

    if (blocks == 0) {
        r.problem = QStringLiteral("Could not find a clear stretch of speech");
        return r;
    }
    for (double& p : power) { p /= blocks; }

    // ── Band levels ──────────────────────────────────────────────────
    std::array<double, kVoiceBandCount> bandPower{};
    std::array<int, kVoiceBandCount> binCount{};
    const double binHz = double(sampleRate) / kFft;

    double sib = 0.0, mid = 0.0;
    for (int k = 1; k <= kFft / 2; ++k) {
        const double hz = k * binHz;
        const int b = bandForHz(hz);
        if (b >= 0) {
            bandPower[static_cast<size_t>(b)] += power[static_cast<size_t>(k)];
            ++binCount[static_cast<size_t>(b)];
        }
        if (hz >= 5000.0 && hz < 8000.0) { sib += power[static_cast<size_t>(k)]; }
        if (hz >= 1000.0 && hz < 3000.0) { mid += power[static_cast<size_t>(k)]; }
    }
    for (int i = 0; i < kVoiceBandCount; ++i) {
        if (binCount[static_cast<size_t>(i)] > 0) {
            bandPower[static_cast<size_t>(i)] /= binCount[static_cast<size_t>(i)];
        }
    }

    const double ref = bandPower[5];   // the 1 kHz band
    for (int i = 0; i < kVoiceBandCount; ++i) {
        r.bandDb[static_cast<size_t>(i)] =
            toDb(bandPower[static_cast<size_t>(i)]) - toDb(ref);
    }
    r.sibilanceDb = toDb(sib) - toDb(mid);

    // ── Hum ──────────────────────────────────────────────────────────
    // Both mains frequencies are tried and the stronger wins, rather
    // than assuming the operator's country. Harmonics are included
    // because a transformer hum is rarely a clean sine.
    auto humAt = [&](double base) {
        double total = 0.0;
        for (int h = 1; h <= 4; ++h) {
            const double hz = base * h;
            const int k = int(std::lround(hz / binHz));
            if (k > 0 && k <= kFft / 2) {
                // Two bins either side, so a mains frequency that is
                // not exactly 50.000 Hz is still caught.
                for (int d = -2; d <= 2; ++d) {
                    const int kk = k + d;
                    if (kk > 0 && kk <= kFft / 2) {
                        total += power[static_cast<size_t>(kk)];
                    }
                }
            }
        }
        return total;
    };
    const double hum50 = humAt(50.0);
    const double hum60 = humAt(60.0);
    r.humBaseHz = hum50 >= hum60 ? 50 : 60;
    r.humDb = toDb(std::max(hum50, hum60)) - toDb(mid);

    // ── Crest factor ─────────────────────────────────────────────────
    double peak = 0.0, sumSq = 0.0;
    int n = 0;
    for (int w = 0; w < windows; ++w) {
        if (energy[static_cast<size_t>(w)] <= gate) { continue; }
        for (int i = 0; i < win; ++i) {
            const double v = samples[w * win + i];
            peak = std::max(peak, std::abs(v));
            sumSq += v * v;
            ++n;
        }
    }
    const double rms = n > 0 ? std::sqrt(sumSq / n) : 0.0;
    r.crestFactorDb = 20.0 * std::log10(std::max(peak, kEps)
                                        / std::max(rms, kEps));

    suggestEq(r.bandDb, r.suggestedEqDb, r.suggestedPreampDb);

    // ── Findings, worst first ────────────────────────────────────────
    if (r.clippedSamples > 0) {
        r.findings << QStringLiteral(
            "Clipping — %1 samples hit the rail. Turn the microphone "
            "gain down and record again; nothing below this can be "
            "trusted until it is fixed.").arg(r.clippedSamples);
    }
    if (r.humDb > -35.0) {
        r.findings << QStringLiteral(
            "Mains hum at %1 Hz, %2 dB under your voice. Check the "
            "microphone cable and the earth before anything else — "
            "no EQ setting hides this.")
            .arg(r.humBaseHz).arg(-r.humDb, 0, 'f', 0);
    }
    if (r.noiseFloorDbFs > r.speechDbFs - 25.0) {
        r.findings << QStringLiteral(
            "Background noise is only %1 dB below your voice. A noise "
            "gate will help; a quieter room will help more.")
            .arg(r.speechDbFs - r.noiseFloorDbFs, 0, 'f', 0);
    }
    if (r.bandDb[2] > -4.0 || r.bandDb[3] > -2.0) {
        r.findings << QStringLiteral(
            "Bass-heavy — close to the microphone, or the microphone "
            "likes it. That energy is removed by the transmit filter "
            "after it has already used up compressor headroom.");
    }
    if (r.sibilanceDb > -6.0) {
        r.findings << QStringLiteral(
            "Strong sibilance. Worth a de-esser, or turning the "
            "microphone a few degrees off-axis.");
    }
    if (r.crestFactorDb > 18.0) {
        r.findings << QStringLiteral(
            "Peaks are %1 dB above the average — plenty of room for "
            "compression to raise your average power.")
            .arg(r.crestFactorDb, 0, 'f', 0);
    } else if (r.crestFactorDb < 8.0) {
        r.findings << QStringLiteral(
            "Peaks are only %1 dB above the average. This is already "
            "heavily processed; more compression will cost quality and "
            "buy very little.").arg(r.crestFactorDb, 0, 'f', 0);
    }
    if (r.findings.isEmpty()) {
        r.findings << QStringLiteral(
            "Nothing wrong found. The suggested curve is a shaping "
            "preference, not a correction.");
    }

    r.valid = true;
    return r;
}

} // namespace NereusSDR
