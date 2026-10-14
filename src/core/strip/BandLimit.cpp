// =================================================================
// src/core/strip/BandLimit.cpp  (Longpath)
// =================================================================
// Siehe BandLimit.h.
//
// Modification history (Longpath):
//   2026-10-10 — Created in C++20 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include "core/strip/BandLimit.h"

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Die Guete der k-ten Stufe einer Butterworth-Kaskade der Ordnung
// `order`. Fuer die achte Ordnung sind das 2,563 / 0,900 / 0,601 /
// 0,510 -- die erste Stufe ueberhoeht kraeftig, die letzte kaum, und
// zusammen ergeben sie genau den flachen Durchlass, der Butterworth
// ausmacht.
double butterworthQ(int k, int order)
{
    return 1.0 / (2.0 * std::sin((2.0 * k + 1.0) * kPi / (2.0 * order)));
}

} // namespace

const std::array<BandLimit::Channel, BandLimit::kChannelCount>&
BandLimit::channels()
{
    // Die Zahlen des Betreibers, nicht erfundene Rundungen:
    // „dx oder contest profil ist im bereich 200-2500(2700), ein 4 k
    //  profil ist von ca 50-4050." Dazwischen gestuft.
    static const std::array<Channel, kChannelCount> kList = {{
        { "200 – 2500 Hz · DX / Contest", 200, 2500 },
        { "150 – 2700 Hz · schmal",       150, 2700 },
        { "100 – 3200 Hz · mittel",       100, 3200 },
        { "50 – 4050 Hz · ESSB",           50, 4050 },
        // ── Eine Stufe darüber, seit 2026-10-14 ──────────────────
        //
        // „ich bin immer verwundert, wie groß der unterschied
        //  zwischen 4k und volle bandbreite ist."
        //
        // Der Sprung von 4 kHz auf die volle Bandbreite der
        // Soundkarte ist kein Vergleich, sondern ein Abgrund: oben
        // liegen noch 16 kHz, die kein Sender je überträgt. Diese
        // Stufe steht dazwischen und beantwortet die Frage, die
        // dahintersteckt -- wieviel davon käme zurück, wenn der
        // Sender breiter dürfte.
        //
        // Auf der Luft ist das eine Frage des Geräts und der
        // Rücksicht auf die Nachbarn, nicht unsere; hier ist es eine
        // Frage ans Ohr.
        { "50 – 6000 Hz · breit",          50, 6000 },
    }};
    return kList;
}

void BandLimit::prepare(double sampleRate)
{
    m_rate = sampleRate > 1000.0 ? sampleRate : 48000.0;
    m_designed = -2;          // erzwingt Neurechnung im naechsten Block
    reset();
}

void BandLimit::setChannel(int index) noexcept
{
    const int v = (index >= 0 && index < kChannelCount) ? index : -1;
    m_channel.store(v, std::memory_order_release);
}

int BandLimit::channel() const noexcept
{
    return m_channel.load(std::memory_order_acquire);
}

void BandLimit::reset() noexcept
{
    for (Biquad& b : m_hp) { b.clear(); }
    for (Biquad& b : m_lp) { b.clear(); }
}

void BandLimit::setOrders(int hochpass, int tiefpass)
{
    const auto gerade = [](int n) {
        if (n <= 0) { return 0; }        // 0 = nach der Regel
        return std::clamp((n / 2) * 2, 2, kMaxSections * 2);
    };
    m_hpOrder = gerade(hochpass);
    m_lpOrder = gerade(tiefpass);
    m_designed = -2;            // erzwingt Neurechnung
}

void BandLimit::design(int lowHz, int highHz)
{
    const double nyq = m_rate * 0.5;
    const int hpSek = (m_hpOrder > 0 ? m_hpOrder
                                     : hochpassOrdnungFuer(lowHz)) / 2;
    const int lpSek = m_lpOrder / 2;

    for (Biquad& b : m_hp) { b = Biquad{}; }
    for (Biquad& b : m_lp) { b = Biquad{}; }

    for (int k = 0; k < kMaxSections; ++k) {
        // Hochpass
        if (k < hpSek) {
            const double q = butterworthQ(k, hpSek * 2);
            const double f = std::min(double(lowHz), nyq * 0.45);
            const double w = 2.0 * kPi * f / m_rate;
            const double cw = std::cos(w);
            const double alpha = std::sin(w) / (2.0 * q);
            const double a0 = 1.0 + alpha;
            Biquad& b = m_hp[static_cast<size_t>(k)];
            b.b0 =  (1.0 + cw) * 0.5 / a0;
            b.b1 = -(1.0 + cw)       / a0;
            b.b2 =  (1.0 + cw) * 0.5 / a0;
            b.a1 = (-2.0 * cw)       / a0;
            b.a2 = (1.0 - alpha)     / a0;
        }
        // Tiefpass
        if (k < lpSek) {
            const double q = butterworthQ(k, lpSek * 2);
            const double f = std::min(double(highHz), nyq * 0.90);
            const double w = 2.0 * kPi * f / m_rate;
            const double cw = std::cos(w);
            const double alpha = std::sin(w) / (2.0 * q);
            const double a0 = 1.0 + alpha;
            Biquad& b = m_lp[static_cast<size_t>(k)];
            b.b0 = (1.0 - cw) * 0.5 / a0;
            b.b1 = (1.0 - cw)       / a0;
            b.b2 = (1.0 - cw) * 0.5 / a0;
            b.a1 = (-2.0 * cw)      / a0;
            b.a2 = (1.0 - alpha)    / a0;
        }
    }
}

void BandLimit::process(float* samples, int frames) noexcept
{
    if (samples == nullptr || frames <= 0) { return; }

    const int ch = m_channel.load(std::memory_order_acquire);
    if (ch < 0) {
        // Roh. Die Zustaende werden geleert, damit ein Einschalten
        // mitten im Satz nicht den Rest eines alten Signals ausspuckt.
        if (m_designed != -1) { reset(); m_designed = -1; }
        return;
    }

    if (ch != m_designed) {
        design(channels()[static_cast<size_t>(ch)].lowHz,
               channels()[static_cast<size_t>(ch)].highHz);
        reset();
        m_designed = ch;
    }

    for (int i = 0; i < frames; ++i) {
        double x = double(samples[i]);
        for (Biquad& b : m_hp) { x = b.step(x); }
        for (Biquad& b : m_lp) { x = b.step(x); }
        // Ein Filter, das NaN ausspuckt, darf nicht in den Kopfhoerer.
        // Die Kette hat ihren eigenen Waechter, dieser hier steht
        // hinter ihr und braucht seinen eigenen.
        if (!std::isfinite(x)) { reset(); x = 0.0; }
        samples[i] = float(x);
    }
}

} // namespace Longpath
