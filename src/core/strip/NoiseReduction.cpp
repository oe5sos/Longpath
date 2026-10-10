// =================================================================
// src/core/strip/NoiseReduction.cpp  (Longpath)
// =================================================================
// Siehe NoiseReduction.h.
//
// Modification history (Longpath):
//   2026-10-10 — Created in C++20 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include "core/strip/NoiseReduction.h"

#include "core/LogCategories.h"
#include "core/ModelPaths.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

extern "C" {
#include "rnnoise.h"
}

namespace Longpath {

namespace {

// rnnoise rechnet in der Grössenordnung von int16, nicht in ±1.
constexpr float kScale    = 32768.0f;
constexpr float kInvScale = 1.0f / kScale;

// Das Modell wird EINMAL je Programmlauf geladen und von allen
// Instanzen geteilt. Es sind 3,5 MB, und ein zweiter Kanalzug -- der
// Mithörer hat seinen eigenen -- soll sie nicht noch einmal lesen.
RNNModel* sharedModel()
{
    static RNNModel* model = []() -> RNNModel* {
        const QString path = ModelPaths::rnnoiseDefaultLargeBin();
        if (path.isEmpty()) {
            qCWarning(lcDsp) << "strip denoise: kein rnnoise-Modell gefunden"
                             << "-- die Stufe bleibt wirkungslos";
            return nullptr;
        }
        RNNModel* m = rnnoise_model_from_filename(
            path.toLocal8Bit().constData());
        if (m == nullptr) {
            qCWarning(lcDsp) << "strip denoise: rnnoise-Modell nicht lesbar:"
                             << path;
        } else {
            qCInfo(lcDsp) << "strip denoise: rnnoise-Modell geladen aus" << path;
        }
        return m;
    }();
    return model;
}

} // namespace

NoiseReduction::NoiseReduction() = default;

NoiseReduction::~NoiseReduction()
{
    if (m_state != nullptr) { rnnoise_destroy(m_state); }
}

void NoiseReduction::prepare(double sampleRate)
{
    // Keine stille Umtastung. rnnoise ist auf 48 kHz trainiert; es mit
    // etwas anderem zu füttern ergibt ein Ergebnis, das plausibel
    // aussieht und falsch ist.
    const bool rateOk = std::fabs(sampleRate - 48000.0) < 1.0;
    if (!rateOk) {
        if (m_state != nullptr) { rnnoise_destroy(m_state); m_state = nullptr; }
        m_usable = false;
        qCWarning(lcDsp) << "strip denoise: braucht 48 kHz, bekommen"
                         << sampleRate << "-- Stufe reicht durch";
        return;
    }

    RNNModel* model = sharedModel();
    if (model == nullptr) { m_usable = false; return; }

    if (m_state == nullptr) { m_state = rnnoise_create(model); }
    if (m_state == nullptr) { m_usable = false; return; }

    m_frame = rnnoise_get_frame_size();
    if (m_frame <= 0) { m_frame = 480; }
    m_in.assign(static_cast<size_t>(m_frame), 0.0f);
    m_wet.assign(static_cast<size_t>(m_frame), 0.0f);
    m_dry.assign(static_cast<size_t>(m_frame), 0.0f);
    m_pos = 0;
    m_usable = true;
}

void NoiseReduction::setEnabled(bool on) noexcept
{
    m_on.store(on, std::memory_order_release);
}

bool NoiseReduction::isEnabled() const noexcept
{
    return m_on.load(std::memory_order_acquire);
}

void NoiseReduction::setAmount(float a) noexcept
{
    m_amount.store(std::clamp(a, 0.0f, 1.0f), std::memory_order_release);
}

float NoiseReduction::amount() const noexcept
{
    return m_amount.load(std::memory_order_acquire);
}

int NoiseReduction::latencySamples() const noexcept
{
    return (m_usable && isEnabled()) ? m_frame : 0;
}

float NoiseReduction::voiceProbability() const noexcept
{
    return m_voiceProb.load(std::memory_order_relaxed);
}

float NoiseReduction::reductionDb() const noexcept
{
    return m_reductionDb.load(std::memory_order_relaxed);
}

void NoiseReduction::reset() noexcept
{
    std::fill(m_in.begin(),  m_in.end(),  0.0f);
    std::fill(m_wet.begin(), m_wet.end(), 0.0f);
    std::fill(m_dry.begin(), m_dry.end(), 0.0f);
    m_pos = 0;
    m_voiceProb.store(0.0f, std::memory_order_relaxed);
    m_reductionDb.store(0.0f, std::memory_order_relaxed);
}

void NoiseReduction::runFrame() noexcept
{
    // Der trockene Block ist der, der gerade hereingekommen ist --
    // nicht der, der gerade ausgegeben wird. Beide verlassen die
    // Stufe im selben Durchgang, sonst ist die Mischung eine
    // Mischung aus zwei verschiedenen Zeitpunkten und löscht sich
    // stellenweise aus.
    std::copy(m_in.begin(), m_in.end(), m_dry.begin());

    const float p = rnnoise_process_frame(m_state, m_wet.data(), m_in.data());
    m_voiceProb.store(p, std::memory_order_relaxed);

    // Was weggenommen wurde, gemessen: Effektivwert vorher gegen
    // nachher. Eine gerechnete Zahl waere eine Behauptung über ein
    // neuronales Netz, und die hat hier niemand.
    double sumIn = 0.0;
    double sumOut = 0.0;
    for (int i = 0; i < m_frame; ++i) {
        sumIn  += double(m_dry[size_t(i)]) * double(m_dry[size_t(i)]);
        sumOut += double(m_wet[size_t(i)]) * double(m_wet[size_t(i)]);
    }
    if (sumIn > 1e-9) {
        const double r = std::sqrt(sumOut / sumIn);
        m_reductionDb.store(float(20.0 * std::log10(std::max(r, 1e-6))),
                            std::memory_order_relaxed);
    }
}

void NoiseReduction::process(float* samples, int frames, int channels) noexcept
{
    if (samples == nullptr || frames <= 0) { return; }
    if (channels != 1) { return; }          // der Kanalzug ist mono
    if (!m_usable || !isEnabled())  { return; }
    if (m_state == nullptr)         { return; }

    const float wet = m_amount.load(std::memory_order_acquire);
    const float dry = 1.0f - wet;

    for (int i = 0; i < frames; ++i) {
        const size_t k = static_cast<size_t>(m_pos);
        const float x = samples[i];

        // Erst ausgeben, was im vorigen Block berechnet wurde, dann
        // den neuen Wert an dieselbe Stelle legen. Das sind genau
        // m_frame Abtastwerte Verzögerung -- die 10 ms aus dem Kopf
        // des Headers. Der trockene Anteil kommt aus demselben Block
        // wie der entrauschte, also sind beide gleich alt.
        samples[i] = (m_dry[k] * dry + m_wet[k] * wet) * kInvScale;

        m_in[k] = x * kScale;

        if (++m_pos >= m_frame) {
            m_pos = 0;
            runFrame();
        }
    }
}

} // namespace Longpath
