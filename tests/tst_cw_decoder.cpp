// SPDX-License-Identifier: GPL-2.0-or-later
// tests/tst_cw_decoder.cpp (Longpath)
//
// The native CW decoder, driven with synthetic Morse: a tone keyed by a
// PARIS-timed envelope with soft edges, plus white noise. Tests the ported
// Zeus pipeline (core/CwDecoderCore.h) through its public API and the Qt
// wrapper (core/CwDecoder.h) through its signals.
//
// What is pinned: clean copy decodes letter for letter at 12, 20 and
// 35 WPM and the speed estimate follows the sender; a tone 80 Hz off the
// configured pitch is tracked, not lost; noise alone does not write on
// the screen; a weak signal still decodes; reset() forgets the sender's
// timing; the wrapper decodes the left channel of interleaved stereo.
// no-port-check: tests a Zeus station-engine port (see core/CwDecoderCore.h);
// the Morse generator here is Longpath-original test scaffolding.

#include <QtTest>
#include <QSignalSpy>

#include "core/CwDecoder.h"
#include "core/CwDecoderCore.h"

#include <cmath>
#include <map>
#include <random>
#include <string>
#include <vector>

using namespace Longpath;

namespace {

constexpr int    kRate = 48000;
constexpr double kPi   = 3.14159265358979323846;

const std::map<char, std::string>& morseTable()
{
    static const std::map<char, std::string> t = {
        {'A', ".-"}, {'B', "-..."}, {'C', "-.-."}, {'D', "-.."}, {'E', "."},
        {'F', "..-."}, {'G', "--."}, {'H', "...."}, {'I', ".."}, {'J', ".---"},
        {'K', "-.-"}, {'L', ".-.."}, {'M', "--"}, {'N', "-."}, {'O', "---"},
        {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."}, {'S', "..."}, {'T', "-"},
        {'U', "..-"}, {'V', "...-"}, {'W', ".--"}, {'X', "-..-"}, {'Y', "-.--"}, {'Z', "--.."},
        {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"}, {'4', "....-"},
        {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."},
        {'?', "..--.."}, {'/', "-..-."}, {'=', "-...-"},
    };
    return t;
}

// The keying envelope for `text` at `wpm`, one bool per sample: PARIS
// timing (dit = 1200 / wpm ms, dah = 3, element gap 1, letter gap 3, word
// gap 7), with a second of silence before and after so the decoder's
// noise-floor warm-up and the final letter gap both happen.
std::vector<bool> keyEnvelope(const std::string& text, double wpm)
{
    const int dit = static_cast<int>(std::lround(kRate * 1.2 / wpm));
    std::vector<bool> env(static_cast<size_t>(kRate), false);   // 1 s lead-in
    const auto push = [&](bool on, int units) {
        env.insert(env.end(), static_cast<size_t>(units * dit), on);
    };
    for (size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == ' ') { push(false, 4); continue; }   // 3 (letter) + 4 = 7
        const auto it = morseTable().find(ch);
        if (it == morseTable().end()) { continue; }
        const std::string& pattern = it->second;
        for (size_t e = 0; e < pattern.size(); ++e) {
            if (e > 0) { push(false, 1); }
            push(true, pattern[e] == '-' ? 3 : 1);
        }
        push(false, 3);                                   // letter gap
    }
    env.insert(env.end(), static_cast<size_t>(kRate), false);   // 1 s tail
    return env;
}

// Tone at `toneHz` under the envelope, 5 ms raised-cosine edges, plus
// white noise so the SNR (tone RMS to full-band noise RMS, in dB) is
// `snrDb`. Amplitude a touch below full scale, as a receiver would.
std::vector<float> synthesize(const std::vector<bool>& env, double toneHz,
                              double snrDb, unsigned seed = 7)
{
    const double toneAmp = 0.5;
    const double toneRms = toneAmp / std::sqrt(2.0);
    const double noiseRms = toneRms / std::pow(10.0, snrDb / 20.0);
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, noiseRms);

    const int edge = kRate * 5 / 1000;
    std::vector<float> out(env.size());
    double phase = 0.0;
    const double inc = 2.0 * kPi * toneHz / kRate;
    double gain = 0.0;
    for (size_t i = 0; i < env.size(); ++i) {
        // Slew the gain over `edge` samples so key clicks stay out of the
        // Goertzel bins.
        const double target = env[i] ? 1.0 : 0.0;
        gain += (target - gain) / edge * 2.0;
        if (gain < 0.0) { gain = 0.0; }
        if (gain > 1.0) { gain = 1.0; }
        out[i] = static_cast<float>(toneAmp * gain * std::sin(phase) + noise(rng));
        phase += inc;
        if (phase > 2.0 * kPi) { phase -= 2.0 * kPi; }
    }
    return out;
}

struct Decoded {
    std::string text;
    int symbols{0};
    int confident{0};
    // The tone the detector was locked to when the last letter came out
    // -- read there, not after the trailing second of noise, during which
    // the bank drifts to whichever bin the noise favours (upstream
    // behaviour; the applet shows the same jitter in silence).
    double trackedHzAtLastLetter{0.0};
};

Decoded runCore(CwDecoderCore& core, const std::vector<float>& audio,
                float minConfidence = 0.15f)
{
    Decoded d;
    core.process(audio.data(), static_cast<int>(audio.size()),
                 [&](const CwDecodedSymbol& s) {
                     ++d.symbols;
                     if (s.confidence >= minConfidence || s.text == " ") {
                         d.text += s.text;
                         if (s.text != " ") {
                             ++d.confident;
                             d.trackedHzAtLastLetter = core.trackedToneHz();
                         }
                     }
                 });
    return d;
}

std::string trimmed(std::string s)
{
    while (!s.empty() && s.front() == ' ') { s.erase(s.begin()); }
    while (!s.empty() && s.back() == ' ') { s.pop_back(); }
    return s;
}

} // namespace

class TstCwDecoder : public QObject {
    Q_OBJECT
private slots:
    void clean_copy_at_20_wpm_decodes_letter_for_letter();
    void the_speed_estimate_follows_the_sender();
    void a_tone_off_the_configured_pitch_is_tracked();
    void noise_alone_does_not_write_on_the_screen();
    void a_weak_signal_still_decodes_the_call();
    void reset_forgets_the_senders_timing();
    void the_wrapper_decodes_the_left_channel_and_reports_stats();
};

void TstCwDecoder::clean_copy_at_20_wpm_decodes_letter_for_letter()
{
    const std::string message = "CQ CQ DE OE5SOS OE5SOS K";
    CwDecoderCore core(kRate, 600.0);
    const auto d = runCore(core, synthesize(keyEnvelope(message, 20.0), 600.0, 30.0));
    QCOMPARE(QString::fromStdString(trimmed(d.text)), QString::fromStdString(message));
    QVERIFY2(std::fabs(core.wpm() - 20.0) < 2.0,
             qPrintable(QStringLiteral("wpm %1").arg(core.wpm())));
    QVERIFY2(core.snrDb() > 15.0, qPrintable(QStringLiteral("snr %1").arg(core.snrDb())));
}

void TstCwDecoder::the_speed_estimate_follows_the_sender()
{
    // The decoder starts assuming 20 WPM. A much slower sender's first
    // dits look like dahs until the periodic re-clustering (every six
    // elements) has seen enough of them -- upstream behaviour, so the
    // first word is allowed to be garbled; everything after it must be
    // clean, and the speed must settle on the sender's.
    const std::string message = "VVV THE QUICK BROWN FOX 73";
    const std::string tail    = "THE QUICK BROWN FOX 73";
    for (double wpm : {12.0, 35.0}) {
        CwDecoderCore core(kRate, 600.0);
        const auto d = runCore(core, synthesize(keyEnvelope(message, wpm), 600.0, 30.0));
        const std::string got = trimmed(d.text);
        QVERIFY2(got.size() >= tail.size() && got.compare(got.size() - tail.size(), tail.size(), tail) == 0,
                 qPrintable(QStringLiteral("sent %1 wpm, decoded '%2'")
                                .arg(wpm).arg(QString::fromStdString(got))));
        QVERIFY2(std::fabs(core.wpm() - wpm) / wpm < 0.15,
                 qPrintable(QStringLiteral("sent %1 wpm, estimated %2").arg(wpm).arg(core.wpm())));
    }
}

void TstCwDecoder::a_tone_off_the_configured_pitch_is_tracked()
{
    // The operator set 600 Hz; the station is at 680 Hz. The bank spans
    // +-125 Hz in 25 Hz steps, so 675 or 700 should win and the copy
    // must come through unchanged.
    const std::string message = "CQ TEST DE OE5SOS";
    CwDecoderCore core(kRate, 600.0);
    const auto d = runCore(core, synthesize(keyEnvelope(message, 20.0), 680.0, 30.0));
    QCOMPARE(QString::fromStdString(trimmed(d.text)), QString::fromStdString(message));
    QVERIFY2(std::fabs(d.trackedHzAtLastLetter - 680.0) <= 30.0,
             qPrintable(QStringLiteral("tracked %1 Hz").arg(d.trackedHzAtLastLetter)));
}

void TstCwDecoder::noise_alone_does_not_write_on_the_screen()
{
    // Five seconds of receiver noise with no tone: the adaptive
    // threshold needs twelve times the floor to call a key-down, so at
    // most a stray character may slip through, never a line of them.
    std::vector<bool> silence(static_cast<size_t>(kRate) * 5, false);
    CwDecoderCore core(kRate, 600.0);
    const auto d = runCore(core, synthesize(silence, 600.0, -40.0, 11));
    QVERIFY2(d.confident <= 2,
             qPrintable(QStringLiteral("%1 confident characters from noise: '%2'")
                            .arg(d.confident).arg(QString::fromStdString(d.text))));
}

void TstCwDecoder::a_weak_signal_still_decodes_the_call()
{
    // -3 dB tone-to-noise over the whole 24 kHz: the 256-sample Goertzel
    // bin keeps ~19 dB of processing gain, which is what makes CW copy at
    // levels where the ear works hard. The call must survive.
    const std::string message = "CQ CQ DE OE5SOS OE5SOS K";
    CwDecoderCore core(kRate, 600.0);
    const auto d = runCore(core, synthesize(keyEnvelope(message, 20.0), 600.0, -3.0, 3));
    QVERIFY2(d.text.find("OE5SOS") != std::string::npos,
             qPrintable(QStringLiteral("decoded '%1'").arg(QString::fromStdString(d.text))));
}

void TstCwDecoder::reset_forgets_the_senders_timing()
{
    CwDecoderCore core(kRate, 600.0);
    runCore(core, synthesize(keyEnvelope("VVV VVV VVV", 35.0), 600.0, 30.0));
    QVERIFY(core.wpm() > 28.0);
    core.reset();
    QVERIFY2(std::fabs(core.wpm() - 20.0) < 0.01, "reset() did not return to the 20 WPM prior");
    QVERIFY(!core.tonePresent());
}

void TstCwDecoder::the_wrapper_decodes_the_left_channel_and_reports_stats()
{
    const std::string message = "OE5SOS";
    const auto mono = synthesize(keyEnvelope(message, 20.0), 600.0, 30.0);
    // Interleave: the tap delivers stereo; the right channel carries
    // something else entirely to prove only the left is read.
    std::vector<float> stereo(mono.size() * 2);
    for (size_t i = 0; i < mono.size(); ++i) {
        stereo[2 * i] = mono[i];
        stereo[2 * i + 1] = 0.3f;
    }

    CwDecoder dec;
    QSignalSpy text(&dec, &CwDecoder::textDecoded);
    QSignalSpy stats(&dec, &CwDecoder::statsUpdated);
    dec.setPitchHz(600);
    dec.start();
    // Feed in pump-sized chunks, as the applet does.
    const int chunk = 2400;
    for (int at = 0; at + chunk <= static_cast<int>(mono.size()); at += chunk) {
        dec.feedAudio(stereo.data() + 2 * at, chunk);
    }

    QString joined;
    for (const auto& args : text) { joined += args.at(0).toString(); }
    QCOMPARE(joined.trimmed(), QString::fromStdString(message));
    QVERIFY(stats.count() >= 1);
    QVERIFY2(std::fabs(dec.wpm() - 20.0) < 2.0, qPrintable(QStringLiteral("wpm %1").arg(dec.wpm())));

    // Out of range pitches are clamped, not thrown.
    dec.setPitchHz(50);
    QCOMPARE(dec.pitchHz(), CwDecoder::kMinPitchHz);
    dec.setPitchHz(9000);
    QCOMPARE(dec.pitchHz(), CwDecoder::kMaxPitchHz);
    dec.stop();
}

QTEST_APPLESS_MAIN(TstCwDecoder)
#include "tst_cw_decoder.moc"
