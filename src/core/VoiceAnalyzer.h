#pragma once

// =================================================================
// src/core/VoiceAnalyzer.h  (Longpath)
// =================================================================
//
// Longpath-original.
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
// Modification history (Longpath):
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

    // Ob die Brummmessung ueberhaupt etwas messen konnte.
    //
    // Die Fenster um 50/100/150/200 Hz (bzw. 60/120/...) sind rund
    // 23 Hz breit. Eine Stimme mit 110 Hz Grundton legt ihren eigenen
    // Grundton in das 100-Hz-Fenster und ihre zweite Oberwelle in das
    // 200-Hz-Fenster -- am Geraet gemessen (2026-10-09): 110 Hz bei
    // -12,5 dB als „Netzbrumm", woraufhin StripTuner den Grundton mit
    // -18 dB auskerbte.
    //
    // Fenster, in denen eine Oberwelle der Stimme liegt, werden jetzt
    // uebersprungen. Bleibt keines uebrig, ist humDb KEINE Messung und
    // dieses Feld false. Wer es ignoriert und humDb trotzdem liest,
    // bekommt -120 -- also „kein Brumm", was die sichere Seite ist.
    bool   humMeasurable{true};
    int    humMaskedHarmonics{0};   // wie viele Fenster die Stimme belegt
    double sibilanceDb{0.0};        // 5-8k against 1-3k

    // Where the sibilance actually sits, in Hz — the loudest point
    // between 4 and 9 kHz. A de-esser aimed at a fixed frequency is
    // aimed at somebody else's voice; Zeus measures this and shows
    // it ("Sib 4.0k"), and the de-esser follows it.
    double sibilanceHz{6000.0};
    int    clippedSamples{0};

    // ── Der Raum, und das Mikrofon darin ─────────────────────────────
    //
    // Die drei Zahlen, bei denen Software aufhört zu helfen. Ein
    // Entzerrer kann einen Raum nicht trockenlegen, ein Kompressor
    // kein Poppen reparieren, und beide machen es schlimmer, weil sie
    // anheben.
    //
    // Geschätzt aus dem Abklingen nach Sprachenden, nicht aus einem
    // Impuls -- also eine Hausnummer und keine Messung nach Norm.
    // Darum reverbValid: unter drei brauchbaren Abklingvorgängen wird
    // gar nichts behauptet.
    double reverbMs{0.0};
    bool   reverbValid{false};

    // Poppen: kurze Stösse unter 80 Hz, wie sie ein P oder B direkt
    // ins Mikrofon setzt. Gezählt wird, wie oft, und wie weit der
    // stärkste über dem sonst üblichen Tiefton lag.
    int    plosiveCount{0};
    double plosiveDb{0.0};

    // ── Three more, added for the broadcast optimiser ────────────────
    //
    // Each exists because a decision downstream was being made without
    // it, and was therefore being made on a guess.

    // The voice's own fundamental, in Hz; 0 when no stable pitch was
    // found (a whisper, or a recording that is mostly consonants).
    //
    // The high-pass was previously chosen from how much energy sat
    // below the speech band, which is the right question asked the
    // wrong way round: the same 40 Hz of rumble sits under a 95 Hz
    // voice and a 185 Hz voice, but a filter that is correct for one
    // removes the bottom octave of the other. Pitch is the number
    // that makes that decision safe.
    double pitchHz{0.0};

    // Where the long-term speech spectrum has fallen 20 dB below its
    // own peak. This is the microphone and the room, not the voice,
    // and it is the only honest answer to "is a 4 kHz profile worth
    // building for this station?" — if the capsule stops at 3.1 kHz,
    // the top 900 Hz of a 4 kHz channel carries hiss.
    double rolloffHz{0.0};

    // Loudness, the way everything outside this program measures it:
    // ITU-R BS.1770 integrated loudness and EBU Tech 3342 loudness
    // range. See core/Loudness.h.
    //
    // LRA is here for the same reason a crest factor is, and answers
    // a different question. Crest is peak-against-average inside a
    // syllable and is what a compressor answers; LRA is how much
    // louder one sentence is than the next — turning the head,
    // leaning in — and no compressor setting fixes it. A chain set
    // from crest alone pumps on a voice that wanders.
    //
    // And unlike a home-made spread, these two numbers mean the same
    // thing in every other tool, which is what makes a before-and-
    // after across a profile worth printing.
    double loudnessLufs{-70.0};
    double loudnessRangeLu{0.0};
    bool   loudnessRangeValid{false};
    double samplePeakDbFs{-120.0};

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
