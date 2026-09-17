// =================================================================
// src/core/CwDecoder.cpp  (Longpath)
// =================================================================
//
// Longpath-original Qt wrapper around CwDecoderCore; see CwDecoder.h.
// no-port-check: Longpath-original; ported logic is in CwDecoderCore.cpp.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-17 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/CwDecoder.h"

#include <algorithm>

namespace Longpath {

namespace {
// The Zeus service broadcasts decoder status at 10 Hz
// (CwBroadcastCadence.cs); the applet's labels need no more.
constexpr qint64 kStatsPeriodMs = 100;
// One pump call is ~2400 frames; reserve for a generous multiple so the
// mono scratch never grows on the pump timer after the first block.
constexpr int kMonoReserve = 48000;
} // namespace

CwDecoder::CwDecoder(QObject* parent)
    : QObject(parent)
    , m_core(kSampleRateHz, 600.0)
{
    m_mono.reserve(kMonoReserve);
}

void CwDecoder::start()
{
    if (m_running) { return; }
    m_core.reset();
    m_statsEverSent = false;
    m_statsClock.invalidate();
    m_running = true;
}

void CwDecoder::stop()
{
    m_running = false;
}

void CwDecoder::setPitchHz(int hz)
{
    hz = std::clamp(hz, kMinPitchHz, kMaxPitchHz);
    if (hz == m_pitchHz) { return; }
    m_pitchHz = hz;
    m_core.retune(hz);
}

void CwDecoder::reset()
{
    m_core.reset();
}

void CwDecoder::feedAudio(const float* interleavedStereo, int frames)
{
    if (!m_running || interleavedStereo == nullptr || frames <= 0) { return; }

    if (static_cast<int>(m_mono.size()) < frames) { m_mono.resize(static_cast<size_t>(frames)); }
    for (int i = 0; i < frames; ++i) {
        m_mono[static_cast<size_t>(i)] = interleavedStereo[2 * i];
    }

    m_core.process(m_mono.data(), frames, [this](const CwDecodedSymbol& s) {
        emit textDecoded(QString::fromStdString(s.text), s.confidence);
    });

    emitStatsIfDue();
}

void CwDecoder::emitStatsIfDue()
{
    if (m_statsEverSent && m_statsClock.isValid()
        && m_statsClock.elapsed() < kStatsPeriodMs) {
        return;
    }
    m_statsEverSent = true;
    m_statsClock.restart();
    emit statsUpdated(static_cast<float>(m_core.wpm()),
                      static_cast<float>(m_core.snrDb()),
                      m_core.tonePresent(),
                      static_cast<float>(m_core.trackedToneHz()));
}

} // namespace Longpath
