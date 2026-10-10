// =================================================================
// src/core/strip/ClientClipper.cpp  (Longpath)
// =================================================================
// Siehe ClientClipper.h.
//
// Modification history (Longpath):
//   2026-10-10 — Created in C++20 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include "core/strip/ClientClipper.h"

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Die Kennlinie. Bei knee = 0 eine harte Kante bei ±1, bei knee = 1
// ein weicher Bogen, der sich ±1 nur nähert.
//
// Dazwischen wird der gerade Teil bis (1 - knee) durchgelassen und
// der Rest auf den verbleibenden Platz gebogen -- eine Kurve, die am
// Knick stetig ist und keine Kante hat, an der die Oberwellen
// explodieren.
inline float shape(float x, float knee) noexcept
{
    const float a = std::fabs(x);
    const float s = (x < 0.0f) ? -1.0f : 1.0f;
    if (knee < 1e-4f) {
        return s * std::min(a, 1.0f);
    }
    const float lin = 1.0f - knee;
    if (a <= lin) { return x; }
    const float over = (a - lin) / knee;        // 0 .. inf
    return s * (lin + knee * std::tanh(over));
}

} // namespace

ClientClipper::ClientClipper()
{
    // Halbbandtiefpass, Fenstersinus mit Hann-Fenster. Grenze bei
    // einem Viertel der hohen Rate, also 24 kHz bei 96 kHz -- genau
    // die Spiegelfrequenz, die beim Verdoppeln entsteht.
    const int   n = kTaps;
    const double c = (n - 1) / 2.0;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const double t = double(i) - c;
        const double sinc = (std::fabs(t) < 1e-9)
                                ? 0.5
                                : std::sin(kPi * 0.5 * t) / (kPi * t);
        const double win = 0.5 - 0.5 * std::cos(2.0 * kPi * i / (n - 1));
        const double v = sinc * win;
        m_h[static_cast<size_t>(i)] = float(v);
        sum += v;
    }
    // Auf Gleichspannungsverstärkung 1 normieren. Ohne das wandert
    // der Pegel beim Ein- und Ausschalten, und man vergleicht wieder
    // laut gegen leise.
    for (float& v : m_h) { v = float(double(v) / sum); }
}

void ClientClipper::prepare(double sampleRate)
{
    m_rate = sampleRate > 1000.0 ? sampleRate : 48000.0;
    reset();
}

void ClientClipper::setEnabled(bool on) noexcept
{
    m_on.store(on, std::memory_order_release);
}

bool ClientClipper::isEnabled() const noexcept
{
    return m_on.load(std::memory_order_acquire);
}

void ClientClipper::setDriveDb(float db) noexcept
{
    m_driveDb.store(std::clamp(db, 0.0f, 18.0f), std::memory_order_release);
}

float ClientClipper::driveDb() const noexcept
{
    return m_driveDb.load(std::memory_order_acquire);
}

void ClientClipper::setKnee(float k) noexcept
{
    m_knee.store(std::clamp(k, 0.0f, 1.0f), std::memory_order_release);
}

float ClientClipper::knee() const noexcept
{
    return m_knee.load(std::memory_order_acquire);
}

void ClientClipper::setLevelMatch(bool on) noexcept
{
    m_levelMatch.store(on, std::memory_order_release);
}

bool ClientClipper::levelMatch() const noexcept
{
    return m_levelMatch.load(std::memory_order_acquire);
}

float ClientClipper::clippedDb() const noexcept
{
    return m_clippedDb.load(std::memory_order_relaxed);
}

void ClientClipper::reset() noexcept
{
    m_up.fill(0.0f);
    m_down.fill(0.0f);
    m_upPos = 0;
    m_downPos = 0;
    m_matchGain = 1.0f;
    m_msIn = 0.0;
    m_msRaw = 0.0;
    m_clippedDb.store(0.0f, std::memory_order_relaxed);
}

void ClientClipper::process(float* samples, int frames, int channels) noexcept
{
    if (samples == nullptr || frames <= 0) { return; }
    if (channels != 1) { return; }
    if (!isEnabled()) { return; }

    const float driveDb = m_driveDb.load(std::memory_order_acquire);
    if (driveDb < 0.01f) { return; }            // nichts zu tun

    const float drive = std::pow(10.0f, driveDb / 20.0f);
    const float kneeV = m_knee.load(std::memory_order_acquire);
    const bool  match = m_levelMatch.load(std::memory_order_acquire);

    float peakBefore = 0.0f;
    float peakAfter  = 0.0f;
    double sumBefore = 0.0;
    double sumAfter  = 0.0;

    for (int i = 0; i < frames; ++i) {
        const float x = samples[i] * drive;
        peakBefore = std::max(peakBefore, std::fabs(samples[i]));
        sumBefore += double(samples[i]) * double(samples[i]);

        // ── Hinauf auf die doppelte Rate ─────────────────────────
        //
        // Polyphase: die geraden Koeffizienten machen den ersten
        // Ausgabewert, die ungeraden den zweiten. Das ist dasselbe
        // wie „Null einschieben und filtern", nur ohne die Haelfte
        // der Multiplikationen mit Null.
        m_up[static_cast<size_t>(m_upPos)] = x;
        float a = 0.0f;
        float b = 0.0f;
        for (int k = 0; k < kTaps / 2; ++k) {
            const int at = (m_upPos - k + kTaps) % kTaps;
            a += m_h[static_cast<size_t>(2 * k)]
               * m_up[static_cast<size_t>(at)];
            b += m_h[static_cast<size_t>(2 * k + 1)]
               * m_up[static_cast<size_t>(at)];
        }
        m_upPos = (m_upPos + 1) % kTaps;
        // Faktor zwei, weil beim Einschieben von Nullen die halbe
        // Energie verlorengeht.
        a *= 2.0f;
        b *= 2.0f;

        // ── Dort abschneiden ─────────────────────────────────────
        a = shape(a, kneeV);
        b = shape(b, kneeV);

        // ── Und zurück auf die halbe Rate ────────────────────────
        m_down[static_cast<size_t>(m_downPos)] = a;
        m_downPos = (m_downPos + 1) % kTaps;
        m_down[static_cast<size_t>(m_downPos)] = b;
        m_downPos = (m_downPos + 1) % kTaps;
        float y = 0.0f;
        for (int k = 0; k < kTaps; ++k) {
            const int at = (m_downPos - 1 - k + 2 * kTaps) % kTaps;
            y += m_h[static_cast<size_t>(k)] * m_down[static_cast<size_t>(at)];
        }

        // Vor dem Abgleich messen, nach dem Abgleich ausgeben.
        sumAfter += double(y) * double(y);
        y *= m_matchGain;
        if (!std::isfinite(y)) { reset(); y = 0.0f; }
        samples[i] = y;
        peakAfter = std::max(peakAfter, std::fabs(y));
    }

    // Der Abgleich wirkt auf den NAECHSTEN Block, aus dem gemessenen
    // Verhaeltnis des laufenden. Geglaettet, sonst pumpt er; und nur
    // oberhalb von Stille, sonst regelt er das Rauschen hoch.
    if (match) {
        const int n = std::max(1, frames);
        const double a = std::min(1.0, double(n) / (0.2 * m_rate));
        m_msIn  += a * (sumBefore / n - m_msIn);
        m_msRaw += a * (sumAfter  / n - m_msRaw);
        if (m_msRaw > 1e-12 && m_msIn > 1e-12) {
            m_matchGain = float(std::clamp(std::sqrt(m_msIn / m_msRaw),
                                           0.05, 4.0));
        }
    } else if (m_matchGain != 1.0f) {
        m_matchGain += 0.05f * (1.0f - m_matchGain);
        if (std::fabs(m_matchGain - 1.0f) < 1e-4f) { m_matchGain = 1.0f; }
    }

    // Wieviel Spitze weg ist. Bei eingeschalteter Nachregelung ist
    // das direkt der Gewinn an Scheitelfaktor.
    if (peakBefore > 1e-5f && peakAfter > 1e-9f) {
        const float db = 20.0f * std::log10(peakBefore / peakAfter);
        m_clippedDb.store(std::max(0.0f, db), std::memory_order_relaxed);
    }
}

} // namespace Longpath
