#pragma once

// =================================================================
// src/core/strip/BandLimit.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Der Kanal, durch den wirklich gesendet wird -- als Filter im
// Mithoerweg.
//
// ── Wozu ─────────────────────────────────────────────────────────────
//
// Beim Mithoeren lief bisher die volle Bandbreite der Soundkarte in
// den Kopfhoerer: 20 Hz bis 20 kHz. Der Sender macht daraus 200 bis
// 2500 Hz, oder 50 bis 4050, je nach Kanal. Wer also „Waerme bei
// 300 Hz" und „Schaerfe bei 3,8 kHz" nach Gehoer einstellt, stellt
// zum Teil Energie ein, die der Sender anschliessend wegwirft -- und
// hoert einen Ton, den auf der Gegenseite nie jemand bekommt.
//
// Das ist die stillste Art, eine Einstellung falsch zu machen: jedes
// einzelne Urteil war richtig, nur ueber das falsche Signal.
//
// ── Was es ist ───────────────────────────────────────────────────────
//
// Butterworth achter Ordnung, hoch und tief, als Kaskade von je vier
// Biquads. 48 dB je Oktave -- steil genug, um sich wie eine
// Kanalflanke anzuhoeren, und weit entfernt von dem, was ein
// Filter zweiter Ordnung vorgaukelt.
//
// Es ist ausdruecklich KEINE Nachbildung eines bestimmten Geraets.
// Ein ANAN filtert anders als eine FLEX, und beide anders als diese
// Kaskade. Es ist die ehrliche Antwort auf „ungefaehr was faellt
// weg" -- und die ist unendlich viel naeher an der Wahrheit als gar
// kein Filter.
//
// Die vier Kanaele sind die Zahlen des Betreibers (siehe
// BroadcastOptimizer.h): 200-2500, 150-2700, 100-3200, 50-4050.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-10 — Created in C++20 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include <array>
#include <atomic>

namespace Longpath {

class BandLimit {
public:
    // Ein benannter Kanal. kCount ist „roh" -- gar kein Filter.
    struct Channel {
        const char* name;
        int lowHz;
        int highHz;
    };
    static constexpr int kChannelCount = 4;
    static const std::array<Channel, kChannelCount>& channels();

    BandLimit() = default;

    void prepare(double sampleRate);

    // -1 = roh, sonst Index in channels(). Hauptfaden, sperrfrei.
    void setChannel(int index) noexcept;
    int  channel() const noexcept;

    // Audiofaden. Mono.
    void process(float* samples, int frames) noexcept;

    void reset() noexcept;

private:
    // Ein Biquad in Direktform II, transponiert.
    struct Biquad {
        double b0{1}, b1{0}, b2{0}, a1{0}, a2{0};
        double z1{0}, z2{0};
        inline double step(double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void clear() noexcept { z1 = z2 = 0.0; }
    };

    static constexpr int kSections = 4;     // achte Ordnung
    void design(int lowHz, int highHz);

    std::array<Biquad, kSections> m_hp{};
    std::array<Biquad, kSections> m_lp{};
    double m_rate{48000.0};
    std::atomic<int> m_channel{-1};
    int m_designed{-2};        // welcher Kanal gerade gerechnet ist
};

} // namespace Longpath
