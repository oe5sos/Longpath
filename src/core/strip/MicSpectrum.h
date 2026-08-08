#pragma once

// =================================================================
// src/core/strip/MicSpectrum.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// The last two seconds of microphone, so the equaliser can be drawn
// over the voice it is shaping.
//
// The design decision worth stating: the audio thread does nothing but
// copy samples into a ring. No FFT, no averaging, no allocation, no
// atomics beyond one write index. Everything expensive happens on the
// GUI thread at whatever rate it feels like painting.
//
// This is the opposite of the usual arrangement, where the DSP thread
// computes a spectrum and hands it over. That arrangement is faster in
// principle and worse in practice: the transmit path is the one thread
// in this program that must never be late, and an FFT is exactly the
// kind of work that grows when someone later wants finer resolution.
// A memcpy cannot grow.
//
// Reading is a snapshot, not a lock. The GUI copies the ring and
// carries on; if a block lands mid-copy the snapshot contains a seam,
// which is one wrong pixel column in a picture that repaints ten times
// a second. Paying for a lock on the transmit path to prevent that
// would be the wrong trade, and taking a lock ON the transmit path
// would be a fault.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <atomic>
#include <cstddef>
#include <vector>

namespace NereusSDR {

class MicSpectrum {
public:
    // Sixteen seconds at 48 kHz — six megabytes, and worth it.
    //
    // The live view only needs a fraction of a second. The long window
    // is for Hold, which averages the last fifteen seconds into one
    // curve: a spectrum of two seconds of speech is a spectrum of two
    // or three vowels, and shaping an equaliser against that shapes it
    // against whichever sounds you happened to make.
    //
    // Fifteen seconds is the same figure the voice check asks for, and
    // for the same reason. Sixteen is allocated so a full fifteen is
    // always there even if the last block landed a moment ago.
    static constexpr int kSeconds = 16;
    static constexpr int kHoldSeconds = 15;

    explicit MicSpectrum(int sampleRate = 48000);

    void setSampleRate(int hz);
    int  sampleRate() const noexcept { return m_sampleRate; }

    // Audio thread. Mono. Never allocates, never blocks.
    void feed(const float* samples, int frames) noexcept;

    // GUI thread. Copies the most recent `count` samples into `out`,
    // oldest first. Returns how many were written — fewer than asked
    // for until the ring has filled, so a caller must not assume a full
    // buffer just because it asked for one.
    int snapshot(float* out, int count) const;

    // Total frames ever written. The GUI uses this to tell "nothing is
    // arriving" from "arriving and silent", which are different faults
    // with different remedies.
    unsigned long long framesSeen() const noexcept
    { return m_seen.load(std::memory_order_acquire); }

private:
    int m_sampleRate{48000};
    std::vector<float> m_ring;
    std::atomic<size_t> m_write{0};
    std::atomic<unsigned long long> m_seen{0};
};

} // namespace NereusSDR
