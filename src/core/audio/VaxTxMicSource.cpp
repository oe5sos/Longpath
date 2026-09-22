// =================================================================
// src/core/audio/VaxTxMicSource.cpp  (Longpath)
// =================================================================
//
// Longpath-original file. See VaxTxMicSource.h for design rationale
// and license block.
// =================================================================

// no-port-check: Longpath-original file; no Thetis logic ported here.

#include "core/audio/VaxTxMicSource.h"
#include "core/AudioEngine.h"

namespace Longpath {

VaxTxMicSource::VaxTxMicSource(AudioEngine* engine)
    : m_engine(engine)
{
}

int VaxTxMicSource::pullSamples(float* dst, int n)
{
    if (m_engine == nullptr) {
        return 0;
    }
    return m_engine->pullVaxTxMic(dst, n);
}

} // namespace Longpath
