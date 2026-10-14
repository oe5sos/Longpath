// =================================================================
// src/core/strip/LoudnessMatch.cpp  (Longpath)
// =================================================================
// Siehe LoudnessMatch.h für die Begründung.
// =================================================================
// Modification history (Longpath):
//   2026-10-14 — Original für Longpath von Martin Fischer,
//                 KI-gestützt über Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original.

#include "core/strip/LoudnessMatch.h"

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {
// Ein Einpolfilter, in Zeitkonstanten gedacht statt in Beiwerten.
double alphaFuer(double sekunden, double rate)
{
    if (sekunden <= 0.0 || rate <= 0.0) { return 1.0; }
    return 1.0 - std::exp(-1.0 / (sekunden * rate));
}
} // namespace

void LoudnessMatch::prepare(double sampleRate) noexcept
{
    m_rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    m_alphaAuf = alphaFuer(0.02, m_rate);
    m_alphaAb  = alphaFuer(0.4,  m_rate);
    m_alphaFolg    = alphaFuer(1.5, m_rate);
    m_alphaSchnell = alphaFuer(0.05, m_rate);
    m_kIn1.k  = Loudness::shelfAt(m_rate);
    m_kIn2.k  = Loudness::rlbAt(m_rate);
    m_kOut1.k = m_kIn1.k;
    m_kOut2.k = m_kIn2.k;
    reset();
}

void LoudnessMatch::reset() noexcept
{
    m_msIn  = 0.0;
    m_msOut = 0.0;
    m_gain  = 1.0f;
    m_kIn1.leeren();  m_kIn2.leeren();
    m_kOut1.leeren(); m_kOut2.leeren();
    m_gainDb.store(0.0f, std::memory_order_relaxed);
}

void LoudnessMatch::feedInput(const float* samples, int frames) noexcept
{
    if (samples == nullptr || frames <= 0) { return; }
    for (int i = 0; i < frames; ++i) {
        // Gewichtet messen, aber NICHT das Signal anfassen: gefiltert
        // wird eine Kopie, hoerbar bleibt das Original.
        const double v = m_kIn2.schritt(m_kIn1.schritt(double(samples[i])));
        const double q = v * v;
        m_msIn += (q > m_msIn ? m_alphaAuf : m_alphaAb) * (q - m_msIn);
    }
}

void LoudnessMatch::applyOutput(float* samples, int frames) noexcept
{
    if (samples == nullptr || frames <= 0) { return; }

    for (int i = 0; i < frames; ++i) {
        const double v = m_kOut2.schritt(m_kOut1.schritt(double(samples[i])));
        const double q = v * v;
        m_msOut += (q > m_msOut ? m_alphaAuf : m_alphaAb) * (q - m_msOut);
    }

    // Das Ziel: so viel Daempfung, dass der Effektivwert nach der
    // Kette dem davor entspricht.
    //
    // Nicht geregelt wird in der Stille (siehe kStilleDbFs) und
    // natuerlich nicht, wenn die Kette gar nichts getan hat -- dann
    // steht das Verhaeltnis ohnehin bei eins.
    constexpr double kStilleMs =
        1.0e-6;  // (10^(-60/20))^2 = 1e-6
    float zielDb = 0.0f;
    if (isEnabled() && m_msIn > kStilleMs && m_msOut > kStilleMs) {
        const double verhaeltnis = std::sqrt(m_msIn / m_msOut);
        zielDb = std::clamp(float(20.0 * std::log10(verhaeltnis)),
                            kMinDb, kMaxDb);
    }
    // Ist der Ausgleich aus, laeuft die Zahl auf dieselbe Weise nach
    // null zurueck wie sie hingekommen ist: kein Sprung beim
    // Umschalten, in keiner Richtung.
    const float ziel = std::pow(10.0f, zielDb / 20.0f);

    // ── Weit zu laut heisst sofort ──────────────────────────────
    //
    // Fuer Sprunge von mehr als 6 dB wird nicht geglitten, sondern
    // gesetzt. Eine Absenkung in einem Augenblick hoert man als
    // Ducken, nicht als Verzerrung -- und die Alternative ist, dass
    // eine neu gewaehlte Voreinstellung mit vollem Pegel im Ohr
    // steht, bis die Regelung nachgezogen hat.
    //
    // Schneller als die MESSUNG geht es trotzdem nicht: die braucht
    // ihre 20 ms, um den neuen Pegel ueberhaupt zu sehen. Lautheit
    // ohne Zeit gibt es nicht. Was bleibt, ist ein kurzer Anriss,
    // kein Dauerpegel.
    if (ziel < m_gain * 0.5f) { m_gain = ziel; }

    for (int i = 0; i < frames; ++i) {
        const double a = (ziel < m_gain) ? m_alphaSchnell : m_alphaFolg;
        m_gain += float(a) * (ziel - m_gain);
        samples[i] *= m_gain;
    }
    m_gainDb.store(m_gain > 1e-6f ? 20.0f * std::log10(m_gain) : kMinDb,
                   std::memory_order_relaxed);
}

} // namespace Longpath
