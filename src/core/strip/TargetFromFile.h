#pragma once

// =================================================================
// src/core/strip/TargetFromFile.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// "I want to sound like that."
//
// Every other way of choosing a target asks the operator to translate
// a sound into words — warm, present, punchy — and then asks the
// software to translate those words back into decibels. Both
// translations lose, and the second one loses in whichever direction
// the author of the built-in curve happened to prefer.
//
// A recording skips both. Load a WAV of a signal that sounded right —
// your own audio from a good day, or somebody else's off the air — and
// its long-term average spectrum becomes the target directly. That is
// the professional match-EQ workflow, and it is the only method here
// where nobody's opinion sits between the operator's ear and the
// number.
//
// What it cannot do, and says so rather than pretending: a recording
// made through a receiver carries that receiver's filter, and a
// recording of a different voice carries a different larynx. The
// result is a target, not a transplant. It is still worth far more
// than an adjective.
//
// Reading is deliberately plain: uncompressed PCM and IEEE float WAV,
// parsed here, no decoder dependency. A file that is not that is
// refused by name rather than misread — a wrong target derived
// silently from a misparsed header is worse than no target at all.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QString>
#include <QVector>

#include <cstdint>
#include <vector>

namespace Longpath::TargetFromFile {

// What the header said. `ok` false means `error` is worth showing.
struct WavInfo {
    bool     ok{false};
    QString  error;
    int      channels{0};
    int      sampleRate{0};
    int      bitsPerSample{0};
    bool     isFloat{false};
    // Byte offset and length of the `data` chunk within the file.
    int64_t  dataOffset{0};
    int64_t  dataBytes{0};
};

// Parse a RIFF/WAVE header out of the first bytes of a file. Chunks
// are walked rather than assumed to be in a fixed order, because a
// great many real files put `LIST` or `fact` between `fmt ` and
// `data`, and a reader that assumes offset 44 gets a burst of noise at
// the start of every one of them.
WavInfo parseWavHeader(const char* bytes, int64_t size);

// Decode the data chunk to mono float in -1..1. Channels are averaged,
// because a stereo recording of a voice is one voice and the target is
// about its spectrum, not its image.
std::vector<float> decodeMono(const char* bytes, int64_t size,
                              const WavInfo& info);

// The long-term average spectrum of `mono`, sampled at the target's
// twelve frequencies and expressed in dB relative to 1 kHz.
//
// Averaged in the power domain over overlapping Hann windows, the same
// way the live curve is: averaging decibels gives a silent window the
// same weight as a loud one, so leading silence and the gaps between
// words would flatten exactly the peaks the target is meant to
// capture. Windows below `kSilenceDbFs` are skipped outright for the
// same reason.
//
// Empty on failure, with `error` set.
QVector<double> ltasAtTargetPoints(const std::vector<float>& mono,
                                   int sampleRate, QString* error);

// Anything quieter than this in a window is not the recording, it is
// the pause before it. -55 dBFS RMS is comfortably below speech and
// comfortably above the noise floor of any recording worth matching.
inline constexpr double kSilenceDbFs = -55.0;

// How far below the loudest part of the recording the curve is allowed
// to go before it stops being information.
//
// Found by checking the maths against synthetic signals rather than by
// reasoning about it, which is the only way this class of fault ever
// gets found. Without the floor, a band with almost nothing in it
// reports whatever the FFT's leakage and the arithmetic's rounding
// happen to produce — a number with no physical meaning that swings by
// tens of decibels depending on where the windows landed. Two things
// then go wrong: the target asks for a 90 dB boost somewhere nobody
// speaks, and if the empty band happens to be the 1 kHz reference the
// ENTIRE curve is offset by that nonsense.
//
// So: nothing is reported more than 60 dB below the peak, and a
// recording whose 1 kHz reference is itself down there is refused
// rather than measured. Sixty decibels is already far more range than
// any equaliser should be asked to chase.
inline constexpr double kDynamicRangeDb = 60.0;

// The whole job: path in, twelve dB values out. Empty with `error` set
// if the file cannot be read, is not PCM/float WAV, or holds less than
// about a second of sound to average.
QVector<double> fromWavFile(const QString& path, QString* error);

} // namespace Longpath::TargetFromFile
