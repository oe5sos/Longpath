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
    static constexpr int kChannelCount = 5;
    static const std::array<Channel, kChannelCount>& channels();

    BandLimit() = default;

    void prepare(double sampleRate);

    // ── Die Ordnung der beiden Flanken, getrennt ─────────────────────
    //
    // Ein Filter hat zwei Seiten, und bis zum 2026-10-10 wurde nur
    // eine angesehen. Der Pegelgang ist tadellos; die
    // Gruppenlaufzeit war nie gemessen. Acht Pole bei 50 Hz stehen
    // sehr dicht am Einheitskreis, und das kostet Zeit: am Geraet
    // gemessen kam 60 Hz im ESSB-Kanal 16,8 ms nach 1 kHz heraus.
    // Das hoert man nicht als Klangfarbe, sondern als Verschmieren
    // -- und genau so hat es der Betreiber gemeldet („4k sollte
    // aehnlich sein wie volle bandbreite, ist es aber nicht").
    //
    // Darum ist die Ordnung jetzt eine Zahl und keine Konstante:
    // damit sich Steilheit gegen Laufzeit abwaegen laesst, mit
    // Messwerten statt mit Geschmack. 2, 4, 6 oder 8.
    void setOrders(int hochpass, int tiefpass);
    int  highPassOrder() const { return m_hpOrder; }
    int  lowPassOrder()  const { return m_lpOrder; }

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

    static constexpr int kMaxSections = 4;  // bis zur achten Ordnung
    void design(int lowHz, int highHz);

    std::array<Biquad, kMaxSections> m_hp{};
    std::array<Biquad, kMaxSections> m_lp{};
    // ── 12 dB je Oktave unten, 48 oben ───────────────────────────
    //
    // Nicht symmetrisch, und das ist der Punkt. Gemessen am
    // 2026-10-10 am Kanal 50-4050 (tst_bandbreiten_genau::
    // steepnessAgainstSmear):
    //
    //   Hochpass    60 Hz   100 Hz   Pegel 30 Hz   20 Hz
    //   12 dB/Okt    3,6 ms   1,6 ms     -9,4 dB   -16,0 dB
    //   24 dB/Okt    8,1 ms   2,7 ms    -17,8 dB   -31,8 dB
    //   48 dB/Okt   16,8 ms   4,9 ms    -35,5 dB   -63,7 dB
    //
    // Die untere Flanke kostet Zeit, und zwar viel: acht Pole bei
    // 50 Hz liessen 60 Hz 16,8 ms nach 1 kHz herauskommen. Das
    // hoert man nicht als Klangfarbe, sondern als Verschmieren des
    // Stimmbodens -- „4k sollte aehnlich sein wie volle bandbreite,
    // ist es aber nicht".
    //
    // Dafuer bekommt man unten nichts, was eine Stimme braucht: bei
    // 12 dB je Oktave liegen 30 Hz schon 9,4 dB tief und 20 Hz 16.
    // Was darunter liegt, ist Trittschall und kein Ton.
    //
    // Oben bleibt es bei 48: dort kostet die Steilheit nichts
    // (0,2 bis 0,4 ms gemessen), und genau diese Kante ist der
    // Grund, warum man einen Kanal ueberhaupt waehlt.
    //
    // Ehrlich bleibt dabei: ein Sender filtert mit linearer Phase
    // (FIR), also GAR nicht verschmiert. Jede IIR-Nachbildung ist
    // darin falsch; diese ist nur weniger falsch als die vorige.
    //
    // ── Und darum haengt die Ordnung an der Ecke ──────────────────
    //
    // Eine flache untere Flanke fuer ALLE Kanaele waere auch wieder
    // falsch, und die Pruefstaende haben es sofort gezeigt: im
    // DX-Kanal sitzt die Ecke bei 200 Hz, also MITTEN in der
    // Stimme. Mit 12 dB je Oktave kaeme 100 Hz dort mit -12,3 dB
    // durch -- ein DX-Filter tut das nie, und genau diese Steilheit
    // ist der Grund, warum man den Kanal waehlt.
    //
    // Die Laufzeit an der Ecke waechst mit der Ordnung und faellt
    // mit der Eckfrequenz. Also:
    //
    //   Ecke unter 150 Hz  ->  2. Ordnung. Dort liegt keine Stimme
    //                          mehr, Steilheit bringt nichts und
    //                          kostet am meisten Zeit.
    //   Ecke ab 150 Hz     ->  8. Ordnung. Dort IST die Steilheit
    //                          der Zweck, und die Ecke liegt am
    //                          Rand der Stimme statt in ihrer Mitte.
    //
    // 0 heisst „nach dieser Regel"; setOrders() ueberschreibt sie,
    // damit der Pruefstand beides messen kann.
    static int hochpassOrdnungFuer(int eckeHz)
    {
        return eckeHz < 150 ? 2 : 8;
    }
    int m_hpOrder{0};
    int m_lpOrder{8};
    double m_rate{48000.0};
    std::atomic<int> m_channel{-1};
    int m_designed{-2};        // welcher Kanal gerade gerechnet ist
};

} // namespace Longpath
