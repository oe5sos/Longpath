#pragma once

// =================================================================
// src/core/VoiceAnalyzer.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// Listen to a recording of the operator's voice and say what to change.
//
// This was asked for as "AI help with the settings", and it is worth
// being plain about what it is not. There is no model here. An EQ
// recommendation is a measurement — the long-term average spectrum of
// the speech, compared with a target curve — and a measurement beats a
// guess from a network in every way that matters: it is the same answer
// every time, every number can be explained, it needs no service, and
// it can be tested. A language model could put the result into a
// sentence. It cannot make the result better.
//
// What is measured, and why each one earns its place:
//
//   Band levels     the ten WDSP graphic-EQ bands, so the answer can be
//                   applied directly instead of translated.
//   Noise floor     measured in the pauses between words, which is the
//                   only place it is visible. Sets the gate threshold.
//   Crest factor    how much louder the peaks are than the average, and
//                   therefore how much compression is left to give.
//   Hum             50 or 60 Hz and harmonics. The operator is the last
//                   person to hear their own mains hum; usually it is a
//                   contact on the air who mentions it.
//   Sibilance       5-8 kHz against 1-3 kHz. Sets the de-esser.
//   Clipping        counted, because a clipped recording invalidates
//                   every other number here and the analysis has to say
//                   so rather than quietly reporting nonsense.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include <QString>
#include <QStringList>

#include <array>

namespace Longpath {

// WDSP's graphic EQ has fixed band centres; the analysis uses the same
// ones so its output needs no conversion.
// From TxChannel.h:1611 — 32 / 63 / 125 / 250 / 500 / 1k / 2k / 4k / 8k / 16k.
inline constexpr int kVoiceBandCount = 10;
inline constexpr std::array<double, kVoiceBandCount> kVoiceBandHz = {
    32.0, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0
};

struct VoiceAnalysis {
    bool valid{false};
    QString problem;            // why it is not valid, in plain words

    double analysedSeconds{0.0};   // speech only, pauses excluded
    double speechFraction{0.0};    // how much of the recording was speech

    // Measured band levels in dB, relative to the 1 kHz band. Relative
    // because the absolute level depends on mic gain, which is a
    // different control and a different problem.
    std::array<double, kVoiceBandCount> bandDb{};

    // What to set the ten EQ bands to. Only ever cuts — see the note on
    // targetDb() for why.
    std::array<double, kVoiceBandCount> suggestedEqDb{};
    // Make-up gain to put back what the cuts took away.
    double suggestedPreampDb{0.0};

    double noiseFloorDbFs{-120.0};  // in the pauses
    double speechDbFs{-120.0};      // average during speech
    double crestFactorDb{0.0};
    double humDb{-120.0};           // hum against speech, dB
    int    humBaseHz{0};            // 50 or 60, whichever was found
    double sibilanceDb{0.0};        // 5-8k against 1-3k
    int    clippedSamples{0};

    // One line each, ordered worst first. Written to be read out loud.
    QStringList findings;
};

class VoiceAnalyzer {
public:
    // Needs a few seconds of speech. Shorter than this and the average
    // is of one sentence's worth of vowels rather than of a voice.
    static constexpr double kMinSpeechSeconds = 3.0;

    static VoiceAnalysis analyse(const float* samples, int frames,
                                 int sampleRate);

    // The target curve, in dB relative to 1 kHz, at a band centre.
    //
    // A considered default, not a law. Below 200 Hz is proximity-effect
    // bass that the SSB filter removes anyway, after it has already
    // eaten compressor headroom. 1-3 kHz is where intelligibility
    // lives. Above 3 kHz the transmit filter is closing, so energy
    // there costs ALC action and buys nothing.
    static double targetDb(double hz);

    // Turn a measured spectrum into EQ settings.
    //
    // Separated out so the rule can be tested on numbers rather than on
    // audio, and exposed because the live overlay draws the same curve.
    //
    // The result only ever cuts. Boosting a band to reach a target is
    // how an analyser amplifies hiss in a band the microphone barely
    // produces — the classic way an automatic EQ makes things worse.
    // Cuts plus make-up gain reach the same shape without adding
    // anything that was not there.
    static void suggestEq(const std::array<double, kVoiceBandCount>& measuredDb,
                          std::array<double, kVoiceBandCount>& outEqDb,
                          double& outPreampDb);
};

} // namespace Longpath
