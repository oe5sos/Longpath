// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from AetherSDR src/asr/AsrSegmenter.cpp [@31b29583].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//
// Modification history (Longpath):
//   2026-08-23 — Portiert. Namensraum angepasst, sonst zeichengetreu.
//   2026-11-11 — Schwelle am Rauschboden (trackNoiseFloor,
//                effectiveThreshold, Anlauf mit replayStartup),
//                Longpath-eigen. Die Zustandsmaschine je Rahmen ist
//                unveraendert nach processFrame() gezogen. Martin
//                Fischer, KI-gestuetzt ueber Anthropic Claude.
//
#include "asr/AsrSegmenter.h"

#include "asr/IVad.h"

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {
// Rauschboden = dieses Perzentil der Rahmenpegel im Fenster. Gemessen
// am 2026-11-11 (siehe Config::noiseFactor): das leiseste Fuenftel ist
// auf einem belebten Band das Rauschen zwischen den Durchgaengen.
constexpr float kNoisePercentile = 0.20f;
// Neu schaetzen alle 50 Rahmen (0,5 s bei 10 ms) — das Perzentil ueber
// 3000 Werte je Rahmen zu ziehen waere Verschwendung, der Boden aendert
// sich nicht im Takt der Silben.
constexpr int kEstimateEveryFrames = 50;
// Erst nach einer Sekunde Ton schaetzen; bis dahin werden die Rahmen
// aufgehoben und danach nachgespielt (siehe feed()).
constexpr int kWarmupFrames = 100;
// Die ERSTE Schaetzung sieht nur diese eine Sekunde, und die ist oft
// fast ganz Sprache (wer einschaltet, tut das, wenn jemand spricht).
// Dort ist der Boden das leiseste Zwanzigstel, nicht Fuenftel; danach
// gilt kNoisePercentile ueber das ganze Fenster.
constexpr float kStartupPercentile = 0.05f;
} // namespace

AsrSegmenter::AsrSegmenter(const Config& config)
    : m_config(config)
{
    m_frameSamples = framesToSamples(m_config.frameMs);
    if (m_frameSamples < 1) {
        m_frameSamples = 1;
    }
    m_minSpeechSamples = framesToSamples(m_config.minSpeechMs);
    m_hangoverSamples = framesToSamples(m_config.hangoverMs);
    m_maxSegmentSamples = framesToSamples(m_config.maxSegmentMs);
    m_frame.reserve(m_frameSamples);
}

int AsrSegmenter::framesToSamples(int ms) const
{
    return static_cast<int>(static_cast<long long>(ms) * m_config.sampleRate / 1000);
}

void AsrSegmenter::reset()
{
    m_frame.clear();
    m_segment.clear();
    m_inSpeech = false;
    m_trailingSilence = 0;
    m_speechSamples = 0;
    // Ein neuer Lauf (anderes Band, andere Scheibe) misst seinen Boden
    // neu; der alte wuerde sonst bis zu einem Fenster lang nachwirken.
    m_rmsHistory.clear();
    m_rmsHead = 0;
    m_rmsCount = 0;
    m_framesSinceEstimate = 0;
    m_floorEstimated = false;
    m_noiseFloor = 0.0f;
    m_startup.clear();
    if (m_vad != nullptr) {
        m_vad->reset();
    }
}

float AsrSegmenter::effectiveThreshold() const
{
    if (m_config.noiseFactor <= 0.0f) {
        return m_config.speechRms;
    }
    return std::max(m_config.speechRms, m_config.noiseFactor * m_noiseFloor);
}

void AsrSegmenter::trackNoiseFloor(float frameRms)
{
    if (m_config.noiseFactor <= 0.0f) {
        return;
    }
    const int frameMs = std::max(1, m_config.frameMs);
    const int capacity = std::max(kWarmupFrames, m_config.noiseWindowMs / frameMs);
    if (static_cast<int>(m_rmsHistory.size()) != capacity) {
        m_rmsHistory.assign(static_cast<size_t>(capacity), 0.0f);
        m_rmsHead = 0;
        m_rmsCount = 0;
    }
    m_rmsHistory[static_cast<size_t>(m_rmsHead)] = frameRms;
    m_rmsHead = (m_rmsHead + 1) % capacity;
    m_rmsCount = std::min(m_rmsCount + 1, capacity);

    if (m_rmsCount < kWarmupFrames) {
        return;
    }
    // Die erste Schaetzung sofort nach dem Anlauf, danach im Abstand.
    if (m_floorEstimated && ++m_framesSinceEstimate < kEstimateEveryFrames) {
        return;
    }
    const float percentile = m_floorEstimated ? kNoisePercentile : kStartupPercentile;
    m_framesSinceEstimate = 0;
    m_floorEstimated = true;
    std::vector<float> window(m_rmsHistory.begin(), m_rmsHistory.begin() + m_rmsCount);
    const auto k = static_cast<std::ptrdiff_t>(percentile * static_cast<float>(m_rmsCount - 1));
    std::nth_element(window.begin(), window.begin() + k, window.end());
    m_noiseFloor = window[static_cast<size_t>(k)];
}

void AsrSegmenter::setMaxSegmentMs(int ms)
{
    m_config.maxSegmentMs = ms;
    m_maxSegmentSamples = framesToSamples(ms);
    if (m_maxSegmentSamples < m_frameSamples) {
        m_maxSegmentSamples = m_frameSamples;
    }
}

void AsrSegmenter::setSpeechRms(float rms)
{
    // Read live in feed(); takes effect on the next frame.
    m_config.speechRms = rms;
}

void AsrSegmenter::setHangoverMs(int ms)
{
    m_config.hangoverMs = ms;
    m_hangoverSamples = framesToSamples(ms);
}

void AsrSegmenter::closeSegment(std::vector<std::vector<float>>& out)
{
    // Gate on speech content, not total length: the segment also carries the
    // hangover silence, which must not count toward the minimum-speech floor.
    if (m_speechSamples >= m_minSpeechSamples) {
        out.push_back(std::move(m_segment));
    }
    m_segment.clear();
    m_inSpeech = false;
    m_trailingSilence = 0;
    m_speechSamples = 0;
}

void AsrSegmenter::processFrame(const float* frame, int n, bool speechFrame,
                                std::vector<std::vector<float>>& out)
{
    if (speechFrame) {
        if (!m_inSpeech) {
            m_inSpeech = true;
            m_segment.clear();
        }
        m_trailingSilence = 0;
        m_segment.insert(m_segment.end(), frame, frame + n);
        m_speechSamples += n;
    } else if (m_inSpeech) {
        // Keep the silence inside the segment until hangover closes it, so
        // brief inter-word gaps don't split an utterance.
        m_segment.insert(m_segment.end(), frame, frame + n);
        m_trailingSilence += n;
        if (m_trailingSilence >= m_hangoverSamples) {
            closeSegment(out);
        }
    }
    // else: silence outside speech — ignored.

    // Hard cap on a single utterance.
    if (m_inSpeech && static_cast<int>(m_segment.size()) >= m_maxSegmentSamples) {
        closeSegment(out);
    }
}

namespace {
float frameRms(const float* frame, int n)
{
    double sumSq = 0.0;
    for (int i = 0; i < n; ++i) {
        sumSq += static_cast<double>(frame[i]) * frame[i];
    }
    return n > 0 ? static_cast<float>(std::sqrt(sumSq / n)) : 0.0f;
}
} // namespace

void AsrSegmenter::replayStartup(std::vector<std::vector<float>>& out)
{
    // Die aufgehobene erste Sekunde, jetzt mit bekannter Schwelle.
    const float threshold = effectiveThreshold();
    const int n = static_cast<int>(m_startup.size());
    for (int i = 0; i + m_frameSamples <= n; i += m_frameSamples) {
        const float* f = m_startup.data() + i;
        processFrame(f, m_frameSamples, frameRms(f, m_frameSamples) >= threshold, out);
    }
    m_startup.clear();
}

std::vector<std::vector<float>> AsrSegmenter::feed(const float* samples, int count)
{
    std::vector<std::vector<float>> out;

    for (int i = 0; i < count; ++i) {
        m_frame.push_back(samples[i]);
        if (static_cast<int>(m_frame.size()) < m_frameSamples) {
            continue;
        }
        const int n = static_cast<int>(m_frame.size());

        // Decide speech/silence for this frame: a plugged-in VAD (e.g. Silero)
        // if present, else the built-in RMS energy threshold.
        if (m_vad != nullptr) {
            processFrame(m_frame.data(), n, m_vad->isSpeech(m_frame.data(), n), out);
        } else {
            const float rms = frameRms(m_frame.data(), n);
            trackNoiseFloor(rms);
            if (m_config.noiseFactor > 0.0f && !m_floorEstimated) {
                // ── Anlauf (Longpath, 2026-11-11) ───────────────────
                //
                // Den Boden kennt der Schneider erst nach einer Sekunde.
                // Bis dahin hebt er die Rahmen auf, statt zu raten: mit
                // der festen Schwelle ginge auf einem lauten Band die
                // erste Sekunde Rauschen als "Sprache" an Whisper (der
                // darauf gern Text erfindet), und taub zu sein verloere
                // den ersten Durchgang nach jedem Einschalten.
                m_startup.insert(m_startup.end(), m_frame.begin(), m_frame.end());
            } else {
                if (!m_startup.empty()) {
                    replayStartup(out);
                }
                processFrame(m_frame.data(), n, rms >= effectiveThreshold(), out);
            }
        }

        m_frame.clear();
    }

    return out;
}

std::vector<std::vector<float>> AsrSegmenter::flush()
{
    std::vector<std::vector<float>> out;
    // Ein Strom, kuerzer als der Anlauf: Boden unbekannt, es gilt
    // speechRms wie ohne Rauschboden.
    if (!m_startup.empty()) {
        replayStartup(out);
    }
    // Fold any partial frame into the segment before closing.
    if (m_inSpeech && !m_frame.empty()) {
        m_segment.insert(m_segment.end(), m_frame.begin(), m_frame.end());
    }
    m_frame.clear();
    if (m_inSpeech) {
        closeSegment(out);
    }
    return out;
}

} // namespace Longpath
