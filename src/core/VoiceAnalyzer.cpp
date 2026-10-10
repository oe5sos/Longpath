// =================================================================
// src/core/VoiceAnalyzer.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see VoiceAnalyzer.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "VoiceAnalyzer.h"

#include "core/Loudness.h"

#include "models/TransmitModel.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>
#include <vector>

namespace Longpath {

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

    // ── Der Raum: wie lange es nach dem Sprechen weiterklingt ────────
    //
    // Keine Messung nach Norm -- dafür bräuchte es einen Impuls und
    // einen stillen Raum. Hier wird das genommen, was die Aufnahme
    // ohnehin hergibt: die Stellen, an denen die Stimme aufhört. Der
    // Pegel fällt danach in den Raumhall hinein, und die Steigung
    // dieses Falls ist die Abklingzeit.
    //
    // Mehrere Abklingvorgänge, und genommen wird der MITTLERE (der
    // Median), nicht der Durchschnitt: ein einziges Türknallen im
    // falschen Moment würde einen Durchschnitt verderben.
    {
        // Gesucht wird nicht die Stelle, an der die Hüllkurve unter
        // eine Schwelle rutscht -- im halligen Raum kommt sie dort
        // zwischen zwei Silben nie an, und im trockenen ist sie
        // schon beim ersten Fenster am Grundgeräusch. Beides hat der
        // erste Entwurf übersehen und für BEIDE Räume „nichts
        // gefunden" gemeldet.
        //
        // Gesucht wird stattdessen jeder Gipfel, dem ein
        // ununterbrochenes Fallen folgt. Was danach zählt, ist die
        // STEIGUNG dieses Falls: ein trockener Raum fällt in zwei
        // Fenstern ins Grundgeräusch (sehr steil, kurze Abklingzeit),
        // ein halliger braucht zwanzig.
        std::vector<double> perSecond;
        const double floorE = std::max(quiet, kEps) * 4.0;   // 6 dB drüber
        for (int w = 1; w + 2 < windows; ++w) {
            const double here = energy[static_cast<size_t>(w)];
            // Kein „örtlicher Gipfel" als Bedingung: in einem
            // halligen Raum ist die Hüllkurve während der Silbe ein
            // Plateau mit Kräuselung, und der eine Punkt, an dem das
            // Abklingen beginnt, ist dann zufällig kein Gipfel. Der
            // zweite Entwurf hat daran genau das Signal nicht
            // gefunden, für das die Messung gedacht ist.
            //
            // Es reicht: laut genug, und das nächste Fenster ist
            // leiser. Mehrfachfunde auf derselben Flanke verhindert
            // das Weiterspringen am Ende.
            if (here <= gate) { continue; }
            if (energy[static_cast<size_t>(w + 1)] >= here) { continue; }

            const double first = 10.0 * std::log10(std::max(here, kEps));
            double last = first;
            int n = 0;
            for (int k = w + 1; k < windows && k < w + 60; ++k) {
                const double e = energy[static_cast<size_t>(k)];
                if (e >= energy[static_cast<size_t>(k - 1)]) { break; }
                last = 10.0 * std::log10(std::max(e, kEps));
                n = k - w;
                if (e < floorE) { break; }
            }
            const double fall = first - last;
            if (n >= 2 && fall >= 10.0) {
                const double seconds = double(n) * win / sampleRate;
                perSecond.push_back(fall / seconds);
                w += n;                      // nicht zweimal dieselbe Flanke
            }
        }
        if (perSecond.size() >= 3) {
            std::sort(perSecond.begin(), perSecond.end());
            const double mid = perSecond[perSecond.size() / 2];
            if (mid > 1.0) {
                r.reverbMs = 60.0 / mid * 1000.0;         // auf 60 dB
                r.reverbValid = true;
            }
        }
    }

    // ── Poppen: kurze Stösse unter 80 Hz ─────────────────────────────
    //
    // Ein P oder B direkt ins Mikrofon setzt einen Luftstoss, der im
    // Spektrum ganz unten als kurzer, kräftiger Berg erscheint. Ein
    // Hochpass nimmt ihn weg -- aber nur, wenn jemand weiss, dass er
    // da ist. Deshalb gezählt statt stillschweigend gefiltert.
    {
        // Einpoliger Tiefpass bei 80 Hz, zweimal durchlaufen: steil
        // genug, um den Grundton einer tiefen Stimme (um 100 Hz)
        // schon deutlich zu dämpfen.
        const double a = std::exp(-2.0 * M_PI * 80.0 / sampleRate);
        double z1 = 0.0;
        double z2 = 0.0;
        std::vector<double> lf(static_cast<size_t>(windows), 0.0);
        for (int w = 0; w < windows; ++w) {
            double sum = 0.0;
            for (int i = 0; i < win; ++i) {
                z1 = (1.0 - a) * samples[w * win + i] + a * z1;
                z2 = (1.0 - a) * z1 + a * z2;
                sum += z2 * z2;
            }
            lf[static_cast<size_t>(w)] = sum / win;
        }

        std::vector<double> during;
        for (int w = 0; w < windows; ++w) {
            if (energy[static_cast<size_t>(w)] > gate) {
                during.push_back(lf[static_cast<size_t>(w)]);
            }
        }
        if (during.size() >= 10) {
            std::sort(during.begin(), during.end());
            const double usual = std::max(during[during.size() / 2], kEps);
            double worst = 0.0;
            for (int w = 0; w < windows; ++w) {
                if (energy[static_cast<size_t>(w)] <= gate) { continue; }
                const double over = 10.0 * std::log10(
                    std::max(lf[static_cast<size_t>(w)], kEps) / usual);
                if (over >= 12.0) { ++r.plosiveCount; }
                worst = std::max(worst, over);
            }
            r.plosiveDb = worst;
        }
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

    // Und wo es sitzt: die lauteste Stelle zwischen 4 und 9 kHz,
    // ueber Drittel-Oktaven geglaettet, damit nicht eine einzelne
    // Spitze die Frequenz bestimmt.
    {
        double best = 0.0;
        double bestHz = 6000.0;
        const double step = std::pow(2.0, 1.0 / 3.0);
        for (double lo = 4000.0; lo < 9000.0; lo *= step) {
            const double hi = std::min(9000.0, lo * step);
            double acc = 0.0;
            int    cnt = 0;
            for (int k = 1; k <= kFft / 2; ++k) {
                const double hz = k * binHz;
                if (hz >= lo && hz < hi) {
                    acc += power[static_cast<size_t>(k)];
                    ++cnt;
                }
            }
            if (cnt > 0 && acc / cnt > best) {
                best   = acc / cnt;
                bestHz = std::sqrt(lo * hi);
            }
        }
        r.sibilanceHz = bestHz;
    }

    // ── Hum ──────────────────────────────────────────────────────────
    // Both mains frequencies are tried and the stronger wins, rather
    // than assuming the operator's country. Harmonics are included
    // because a transformer hum is rarely a clean sine.
    // ── Der Wachhund vor dem Brummfenster (2026-10-09) ───────────────
    //
    // Die Fenster sind plus/minus zwei Faecher breit, das sind bei
    // dieser Laenge rund 23 Hz. Eine Maennerstimme mit 110 Hz
    // Grundton legt damit IHREN Grundton in das 100-Hz-Fenster (77 bis
    // 123 Hz) und ihre zweite Oberwelle in das 200-Hz-Fenster. Das
    // Ergebnis war am Geraet gemessen: 110 Hz bei -12,5 dB, 150 Hz bei
    // -17,6 dB -- „Netzbrumm", der in Wahrheit die Stimme selbst war.
    // Und weil StripTuner daran haengt, kerbte die Automatik den
    // Grundton mit -18 dB aus.
    //
    // Also: ein Brummfenster, in dem eine Oberwelle der STIMME liegt,
    // wird uebersprungen. Trennen laesst sich beides bei dieser
    // Aufloesung nicht -- ein 100-Hz-Brumm und eine 110-Hz-Stimme
    // teilen sich dasselbe Fach. Was sich machen laesst, ist nicht so
    // zu tun, als ginge es: was uebersprungen wurde, wird gezaehlt,
    // und bleibt nichts uebrig, gibt es keine Zahl (siehe
    // VoiceAnalysis::humMeasurable).
    const auto masksAVoiceHarmonic = [&](double hz) {
        const double voiceF0 = r.pitchHz;   // beim AUFRUF gelesen
        if (voiceF0 < 40.0) { return false; }   // keine stabile Tonhoehe

        // Zwei verschieden weite Abstandsregeln, und der Unterschied
        // ist gemessen, nicht geschaetzt.
        //
        // Fuer die OBERWELLEN der Stimme reicht die Fensterbreite
        // selbst: liegt eine darin, ist das Fenster hin.
        //
        // Fuer den GRUNDTON nicht. Er ist das Lauteste im ganzen
        // Spektrum, und die Silbenflanken verschmieren seine Linie
        // weit nach unten -- bei einer 110-Hz-Stimme liegt das
        // 63-Hz-Band nur rund 23 dB unter ihm, und das 50-Hz-Fenster
        // faengt den Ausläufer. Mit der engen Regel allein meldete
        // der Detektor dort weiter -9,5 dB „Brumm", wo keiner ist.
        // Acht Faecher sind rund 93 Hz: weit genug, dass eine
        // 110-Hz-Stimme das 50-Hz-Fenster sperrt, und eng genug, dass
        // eine 210-Hz-Stimme es freigibt (160 Hz entfernt).
        const double nearHarmonic = 2.5 * binHz;
        const double nearF0       = 8.0 * binHz;
        if (std::abs(voiceF0 - hz) <= nearF0) { return true; }
        for (int n = 2; n <= 12; ++n) {
            const double v = voiceF0 * n;
            if (v > hz + nearHarmonic + binHz) { break; }
            if (std::abs(v - hz) <= nearHarmonic) { return true; }
        }
        return false;
    };

    int  humWindowsUsed = 0;
    int  humWindowsMasked = 0;
    bool humBaseMasked = false;
    auto humAt = [&](double base) {
        double total = 0.0;
        humWindowsUsed = 0;
        humWindowsMasked = 0;
        humBaseMasked = false;
        for (int h = 1; h <= 4; ++h) {
            const double hz = base * h;
            if (masksAVoiceHarmonic(hz)) {
                ++humWindowsMasked;
                // Das erste Fenster ist das, in dem Netzbrumm lebt.
                // Ist es hin, gibt es keine Messung -- die restlichen
                // Oberwellen sind beim Brumm ohnehin die schwaecheren.
                if (h == 1) { humBaseMasked = true; }
                continue;
            }
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
                ++humWindowsUsed;
            }
        }
        return total;
    };
    // Which mains? Decided on the FUNDAMENTAL alone, from the
    // interpolated peak frequency. The ±2-bin harmonic windows above
    // are ±23 Hz wide at this FFT size, which makes the 50- and
    // 60-family windows overlap almost completely — a clean 60 Hz hum
    // scored a dead heat and lost the tie to 50 (caught by
    // tst_voice_analyzer on the 2026-08-11 full-suite run). humAt()
    // still measures the STRENGTH, harmonics and all; it just no
    // longer votes on the base.
    int    kPeak = -1;
    double pPeak = 0.0;
    const int kLo = std::max(1, int(std::floor(40.0 / binHz)));
    const int kHi = std::min(kFft / 2, int(std::ceil(70.0 / binHz)));
    for (int k = kLo; k <= kHi; ++k) {
        if (power[static_cast<size_t>(k)] > pPeak) {
            pPeak = power[static_cast<size_t>(k)];
            kPeak = k;
        }
    }
    double fPeak = (kPeak > 0) ? kPeak * binHz : 50.0;
    if (kPeak > 1 && kPeak < kFft / 2) {
        // Parabolic interpolation over log power — the standard
        // fractional-bin refinement; ±23 Hz of bin quantisation
        // collapses to a fraction of a hertz for a clean tone.
        const auto lp = [&](int k) {
            return std::log(std::max(1e-30,
                power[static_cast<size_t>(k)]));
        };
        const double a = lp(kPeak - 1);
        const double b = lp(kPeak);
        const double c = lp(kPeak + 1);
        const double denom = a - 2.0 * b + c;
        if (std::abs(denom) > 1e-12) {
            fPeak = (kPeak + 0.5 * (a - c) / denom) * binHz;
        }
    }
    r.humBaseHz =
        (std::abs(fPeak - 60.0) < std::abs(fPeak - 50.0)) ? 60 : 50;
    // Vorlaeufig: der Wachhund braucht die Tonhoehe, und die wird
    // weiter unten gemessen. Nach ihr wird das hier noch einmal
    // gerechnet -- siehe „Brumm, zweiter Durchgang".
    r.humDb = toDb(humAt(double(r.humBaseHz))) - toDb(mid);

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

    // ── Pitch ────────────────────────────────────────────────────────
    //
    // Normalised autocorrelation per speech frame, median over the
    // frames that had a clear peak. The median and not the mean: one
    // octave error on one frame — the classic autocorrelation failure
    // — would drag a mean by tens of hertz and leaves a median alone.
    //
    // Only frames whose best lag scores above kPitchConfidence are
    // counted. An unvoiced frame has no periodicity to find, and a
    // number produced from one is noise wearing a unit.
    {
        constexpr double kPitchLowHz  = 60.0;    // below any speaking voice
        constexpr double kPitchHighHz = 350.0;   // above any speaking voice
        constexpr double kPitchConfidence = 0.30;

        const int frame = std::min(frames, std::max(1024, sampleRate / 20));
        const int lagMin = std::max(1, int(sampleRate / kPitchHighHz));
        const int lagMax = std::min(frame / 2, int(sampleRate / kPitchLowHz));

        std::vector<double> pitches;
        for (int start = 0; start + frame <= frames; start += frame / 2) {
            const int w = start / win;
            if (w >= windows || energy[static_cast<size_t>(w)] <= gate) {
                continue;
            }
            double zero = 0.0;
            for (int i = 0; i < frame; ++i) {
                const double v = samples[start + i];
                zero += v * v;
            }
            if (zero <= kEps) { continue; }

            double bestScore = 0.0;
            int    bestLag   = 0;
            for (int lag = lagMin; lag <= lagMax; ++lag) {
                double acc = 0.0;
                for (int i = 0; i + lag < frame; ++i) {
                    acc += double(samples[start + i])
                         * double(samples[start + i + lag]);
                }
                const double score = acc / zero;
                if (score > bestScore) {
                    bestScore = score;
                    bestLag   = lag;
                }
            }
            if (bestLag > 0 && bestScore >= kPitchConfidence) {
                pitches.push_back(double(sampleRate) / bestLag);
            }
        }
        if (!pitches.empty()) {
            std::sort(pitches.begin(), pitches.end());
            r.pitchHz = pitches[pitches.size() / 2];
        }
    }

    // ── Brumm, zweiter Durchgang ─────────────────────────────────────
    //
    // Jetzt erst, weil der Wachhund die Tonhoehe braucht und die
    // Tonhoehe aus der Zeitreihe kommt, nicht aus dem Spektrum. Beim
    // ersten Durchgang oben war r.pitchHz noch null, der Wachhund
    // also wirkungslos -- genau der Fehler, den dieser zweite
    // Durchgang behebt (gefunden vom eigenen Prueftand, der sonst
    // gruen geworden waere, ohne etwas zu pruefen).
    {
        const double humPower = humAt(double(r.humBaseHz));
        r.humMaskedHarmonics = humWindowsMasked;

        // Entscheidend ist das ERSTE Fenster, nicht die Zahl der
        // belegten.
        //
        // Das war der zweite Anlauf. Der erste rechnete mit den
        // uebriggebliebenen Fenstern weiter und meldete bei einer
        // 110-Hz-Stimme immer noch -9,5 dB „Brumm", wo keiner ist.
        // Der Grund steht am Messwert: das 63-Hz-Band liegt bei so
        // einer Stimme nur rund 23 dB unter dem Grundton -- die
        // Silbenflanken verschmieren die Grundtonlinie weit nach
        // unten, und das 50-Hz-Fenster faengt den Ausläufer. Sauber
        // trennen laesst sich das bei 11,7 Hz je Fach nicht.
        //
        // Also wird nicht geschaetzt. Liegt auch nur eine Oberwelle
        // der Stimme in einem der Fenster, ist das Ergebnis keine
        // Messung -- Hausstil Regel 7: Unbekannt ist ein Strich,
        // keine Null. Eine Zahl, die man nicht verteidigen kann, ist
        // schlimmer als keine: StripTuner hat auf genau so eine Zahl
        // hin den Grundton mit -18 dB ausgekerbt.
        if (humWindowsUsed == 0 || humBaseMasked) {
            r.humMeasurable = false;
            r.humDb = -120.0;
        } else {
            r.humMeasurable = true;
            r.humDb = toDb(humPower) - toDb(mid);
        }
    }

    // ── Roll-off ─────────────────────────────────────────────────────
    //
    // Smoothed into sixth-octave bins before being read. A single
    // narrow notch — a room mode, a comb from a reflection — is not a
    // roll-off, and reading the raw spectrum would let one bin decide
    // whether a 4 kHz profile is worth building.
    //
    // Scanned upward from the loudest bin. The first sixth-octave that
    // is more than 20 dB down AND is not recovered by the next two is
    // where the microphone has stopped; recovering is what a notch
    // does and a roll-off does not.
    {
        constexpr double kRolloffDownDb = 20.0;
        constexpr double kBinsPerOctave = 6.0;
        constexpr double kScanLowHz  = 200.0;
        constexpr double kScanHighHz = 10000.0;

        std::vector<double> smoothHz, smoothDb;
        const double step = std::pow(2.0, 1.0 / kBinsPerOctave);
        for (double lo = kScanLowHz; lo < kScanHighHz; lo *= step) {
            const double hi = lo * step;
            double acc = 0.0;
            int    n2  = 0;
            for (int k = 1; k <= kFft / 2; ++k) {
                const double hz = k * binHz;
                if (hz >= lo && hz < hi) {
                    acc += power[static_cast<size_t>(k)];
                    ++n2;
                }
            }
            if (n2 > 0) {
                smoothHz.push_back(std::sqrt(lo * hi));
                smoothDb.push_back(toDb(acc / n2));
            }
        }
        if (!smoothDb.empty()) {
            // ── Gemessen gegen 1 kHz, nicht gegen die lauteste
            //    Stelle ─────────────────────────────────────────────
            //
            // Die erste Fassung suchte den Abfall von der lautesten
            // Sechstel-Oktave aus. Die liegt bei jeder Stimme um
            // 200-500 Hz, und von dort sind 20 dB bis 2,4 kHz keine
            // Mikrofongrenze, sondern die natuerliche Neigung einer
            // Stimme. Am Geraet meldete das dem Betreiber bei 2,7 k,
            // 3,2 k UND 4,0 k, sein Mikrofon sei bei 2397 Hz am Ende
            // (2026-10-09) -- und redete ihm damit genau die Profile
            // aus, die in Ordnung waren.
            //
            // Derselbe Fehler wie beim Klangziel: die Neigung einer
            // Stimme ist kein Mangel. Der Bezug ist deshalb 1 kHz,
            // mitten im Sprachband, und gesucht wird ab dort
            // aufwaerts.
            size_t ref = 0;
            for (size_t i = 0; i < smoothHz.size(); ++i) {
                if (std::abs(smoothHz[i] - 1000.0)
                    < std::abs(smoothHz[ref] - 1000.0)) {
                    ref = i;
                }
            }
            const double floorDb = smoothDb[ref] - kRolloffDownDb;
            r.rolloffHz = smoothHz.back();
            for (size_t i = ref + 1; i < smoothDb.size(); ++i) {
                if (smoothDb[i] >= floorDb) { continue; }
                bool recovers = false;
                for (size_t j = i + 1; j <= i + 2 && j < smoothDb.size(); ++j) {
                    if (smoothDb[j] >= floorDb) { recovers = true; }
                }
                if (!recovers) {
                    r.rolloffHz = smoothHz[i];
                    break;
                }
            }
        }
    }

    // ── Loudness and loudness range ──────────────────────────────────
    //
    // Measured over the WHOLE take, pauses included, and that is not
    // an oversight: BS.1770's two-stage gate is what removes the
    // pauses, and it does it the same way in every tool that reports
    // the number. Pre-filtering the take to the speech windows here
    // would quietly produce a figure that no other meter agrees with.
    {
        const Loudness::Result l =
            Loudness::measure(samples, frames, sampleRate);
        if (l.valid) {
            r.loudnessLufs       = l.integratedLufs;
            r.loudnessRangeLu    = l.rangeLu;
            r.loudnessRangeValid = l.rangeValid;
            r.samplePeakDbFs     = l.samplePeakDbFs;
        }
    }

    suggestEq(r.bandDb, r.suggestedEqDb, r.suggestedPreampDb);

    // ── Findings, worst first ────────────────────────────────────────
    if (r.clippedSamples > 0) {
        r.findings << QStringLiteral(
            "Clipping — %1 samples hit the rail. Turn the microphone "
            "gain down and record again; nothing below this can be "
            "trusted until it is fixed.").arg(r.clippedSamples);
    }
    if (r.humMeasurable && r.humDb > -35.0) {
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

} // namespace Longpath
