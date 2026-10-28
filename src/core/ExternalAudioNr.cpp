// =================================================================
// src/core/ExternalAudioNr.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Begruendung und Bauform stehen im Kopf.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-28 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include "ExternalAudioNr.h"

#include <algorithm>

#include "strip/NoiseReduction.h"
#ifdef HAVE_MNR
#include "MacNRFilter.h"
#endif

namespace Longpath {

ExternalAudioNr::ExternalAudioNr() = default;

// Nicht `= default` im Kopf: dort sind NoiseReduction und MacNRFilter
// nur vorausdeklariert, und ein unique_ptr braucht den vollstaendigen
// Typ genau hier, wo er zerstoert wird.
ExternalAudioNr::~ExternalAudioNr() = default;

void ExternalAudioNr::setSlot(NrSlot slot)
{
    if (slot == m_slot) { return; }
    m_slot = slot;
    baueAuf();
}

void ExternalAudioNr::setAmount(float amount)
{
    m_amount = std::clamp(amount, 0.0f, 1.0f);
    if (m_rnnoise) { m_rnnoise->setAmount(m_amount); }
#ifdef HAVE_MNR
    if (m_mnr) { m_mnr->setStrength(m_amount); }
#endif
}

bool ExternalAudioNr::isActive() const
{
    switch (m_slot) {
    case NrSlot::NR3:
        return m_rnnoise && m_rnnoise->isUsable();
#ifdef HAVE_MNR
    case NrSlot::MNR:
        return m_mnr && m_mnr->isValid();
#endif
    default:
        return false;
    }
}

void ExternalAudioNr::baueAuf()
{
    // Erst bauen, wenn jemand das Verfahren waehlt: rnnoise laedt ein
    // Modell, der Apple-Weg legt FFT-Plaene an. Eine Scheibe, die nie
    // NR benutzt, soll beides nicht bezahlen.
    if (m_slot == NrSlot::NR3 && !m_rnnoise) {
        m_rnnoise = std::make_unique<NoiseReduction>();
        m_rnnoise->prepare(48000.0);
        m_rnnoise->setAmount(m_amount);
    }
#ifdef HAVE_MNR
    if (m_slot == NrSlot::MNR && !m_mnr) {
        m_mnr = std::make_unique<MacNRFilter>();
        m_mnr->setStrength(m_amount);
    }
#endif
    if (m_rnnoise) {
        m_rnnoise->setEnabled(m_slot == NrSlot::NR3);
        if (m_slot != NrSlot::NR3) { m_rnnoise->reset(); }
    }
#ifdef HAVE_MNR
    if (m_mnr && m_slot != NrSlot::MNR) { m_mnr->reset(); }
#endif
}

void ExternalAudioNr::process(float* interleavedStereo, int frames)
{
    if (!interleavedStereo || frames <= 0) { return; }

    if (m_slot == NrSlot::NR3) {
        if (!m_rnnoise || !m_rnnoise->isUsable()) { return; }
        // Verschraenkt, zwei Kanaele — genau die Form, die der
        // Sendezug ihm auch gibt.
        m_rnnoise->process(interleavedStereo, frames, 2);
        return;
    }

#ifdef HAVE_MNR
    if (m_slot == NrSlot::MNR) {
        if (!m_mnr || !m_mnr->isValid()) { return; }
        // Der Apple-Weg will getrennte Kanaele. Die Puffer wachsen
        // einmal auf die groesste gesehene Blocklaenge und bleiben
        // dann stehen — im Tonweg wird nicht je Block belegt.
        if (static_cast<int>(m_links.size()) < frames) {
            m_links.resize(static_cast<size_t>(frames));
            m_rechts.resize(static_cast<size_t>(frames));
        }
        for (int i = 0; i < frames; ++i) {
            m_links[static_cast<size_t>(i)]  = interleavedStereo[2 * i];
            m_rechts[static_cast<size_t>(i)] = interleavedStereo[2 * i + 1];
        }
        m_mnr->process(m_links.data(), m_rechts.data(), frames);
        for (int i = 0; i < frames; ++i) {
            interleavedStereo[2 * i]     = m_links[static_cast<size_t>(i)];
            interleavedStereo[2 * i + 1] = m_rechts[static_cast<size_t>(i)];
        }
        return;
    }
#endif
    // Alles andere: Durchlauf. Siehe Kopf — NR1/NR2/NR4 sind
    // WDSP-Einheiten und haben hier kein Gegenstueck.
}

void ExternalAudioNr::reset()
{
    if (m_rnnoise) { m_rnnoise->reset(); }
#ifdef HAVE_MNR
    if (m_mnr) { m_mnr->reset(); }
#endif
}

} // namespace Longpath
