// =================================================================
// src/core/ExternalAudioNr.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Begruendung und Bauform stehen im Kopf.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-24 — NR2 (WDSP-emnr) und NR4 (WDSP-sbnr) als freistehende
//                 Einheiten, dieselben wie im Anvelina-Kanal.
//   2026-10-28 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include "ExternalAudioNr.h"

#include <algorithm>

#ifdef HAVE_WDSP
extern "C" {
void* lp_extnr_create_emnr(int bsize, int rate, int gain, int npe, int ae);
void  lp_extnr_set_emnr(void* a, int gain, int npe, int ae);
void* lp_extnr_create_sbnr(int bsize, int rate);
void  lp_extnr_set_sbnr(void* a, float red, float smooth, float white,
                        float rescale, float post, int scaling);
void  lp_extnr_process(void* a, const float* in, float* out);
void  lp_extnr_destroy(void* a);
}
#endif

#include "strip/NoiseReduction.h"
#ifdef HAVE_MNR
#include "MacNRFilter.h"
#endif

namespace Longpath {

ExternalAudioNr::ExternalAudioNr() = default;

// Nicht `= default` im Kopf: dort sind NoiseReduction und MacNRFilter
// nur vorausdeklariert, und ein unique_ptr braucht den vollstaendigen
// Typ genau hier, wo er zerstoert wird.
ExternalAudioNr::~ExternalAudioNr()
{
#ifdef HAVE_WDSP
    if (m_emnr) { lp_extnr_destroy(m_emnr); }
    if (m_sbnr) { lp_extnr_destroy(m_sbnr); }
#endif
}

namespace {
constexpr int kBlock = 512;   // Proben je WDSP-Block, 48 kHz
}

void ExternalAudioNr::setNr2(int gainMethod, int npeMethod, bool aeFilter)
{
    m_nr2Gain = std::clamp(gainMethod, 0, 3);
    m_nr2Npe  = std::clamp(npeMethod, 0, 2);
    m_nr2Ae   = aeFilter;
#ifdef HAVE_WDSP
    if (m_emnr) { lp_extnr_set_emnr(m_emnr, m_nr2Gain, m_nr2Npe, m_nr2Ae); }
#endif
}

void ExternalAudioNr::setNr4(float reductionDb, float smoothing, float whitening,
                             float rescale, float postThreshold, int scalingType)
{
    m_nr4Red = reductionDb; m_nr4Smooth = smoothing; m_nr4White = whitening;
    m_nr4Rescale = rescale; m_nr4Post = postThreshold; m_nr4Scaling = scalingType;
#ifdef HAVE_WDSP
    if (m_sbnr) {
        lp_extnr_set_sbnr(m_sbnr, m_nr4Red, m_nr4Smooth, m_nr4White,
                          m_nr4Rescale, m_nr4Post, m_nr4Scaling);
    }
#endif
}

void ExternalAudioNr::laufeBlockweise(float* io, int frames, void* handle)
{
#ifdef HAVE_WDSP
    // Mono aus (L+R)/2 in die Eingangs-FIFO, in festen Bloecken rechnen,
    // Ergebnis auf beide Kanaele. Ausgangs-FIFO startet mit einem Block
    // Stille: so liegt nach jedem Aufruf mindestens `frames` bereit.
    if (m_block.size() != static_cast<size_t>(kBlock)) {
        m_block.assign(kBlock, 0.f);
    }
    if (m_fifoOut.empty() && m_fifoIn.empty()) {
        m_fifoOut.assign(kBlock, 0.f);
    }
    for (int i = 0; i < frames; ++i) {
        m_fifoIn.push_back(0.5f * (io[2 * i] + io[2 * i + 1]));
    }
    while (static_cast<int>(m_fifoIn.size()) >= kBlock) {
        lp_extnr_process(handle, m_fifoIn.data(), m_block.data());
        m_fifoIn.erase(m_fifoIn.begin(), m_fifoIn.begin() + kBlock);
        m_fifoOut.insert(m_fifoOut.end(), m_block.begin(), m_block.end());
    }
    for (int i = 0; i < frames; ++i) {
        const float v = m_fifoOut[static_cast<size_t>(i)];
        io[2 * i] = v;
        io[2 * i + 1] = v;
    }
    m_fifoOut.erase(m_fifoOut.begin(), m_fifoOut.begin() + frames);
#else
    Q_UNUSED(io) Q_UNUSED(frames) Q_UNUSED(handle)
#endif
}

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
    case NrSlot::NR2:
        return m_emnr != nullptr;
    case NrSlot::NR4:
        return m_sbnr != nullptr;
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
#ifdef HAVE_WDSP
    if (m_slot == NrSlot::NR2 && !m_emnr) {
        m_emnr = lp_extnr_create_emnr(kBlock, 48000, m_nr2Gain, m_nr2Npe,
                                      m_nr2Ae ? 1 : 0);
    }
    if (m_slot == NrSlot::NR4 && !m_sbnr) {
        m_sbnr = lp_extnr_create_sbnr(kBlock, 48000);
        lp_extnr_set_sbnr(m_sbnr, m_nr4Red, m_nr4Smooth, m_nr4White,
                          m_nr4Rescale, m_nr4Post, m_nr4Scaling);
    }
    // Wechsel des Verfahrens: alte Rahmen gehoeren nicht ins neue.
    m_fifoIn.clear();
    m_fifoOut.clear();
#endif
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

    if (m_slot == NrSlot::NR2 && m_emnr) {
        laufeBlockweise(interleavedStereo, frames, m_emnr);
        return;
    }
    if (m_slot == NrSlot::NR4 && m_sbnr) {
        laufeBlockweise(interleavedStereo, frames, m_sbnr);
        return;
    }

    if (m_slot == NrSlot::NR3) {
        if (!m_rnnoise || !m_rnnoise->isUsable()) { return; }
        // rnnoise im Kanalzug ist mono (process() verwirft channels != 1,
        // so lief NR3 am Rechner nie): (L+R)/2 rechnen, auf beide Kanaele.
        if (static_cast<int>(m_mono.size()) < frames) {
            m_mono.resize(static_cast<size_t>(frames));
        }
        for (int i = 0; i < frames; ++i) {
            m_mono[static_cast<size_t>(i)] =
                0.5f * (interleavedStereo[2 * i] + interleavedStereo[2 * i + 1]);
        }
        m_rnnoise->process(m_mono.data(), frames, 1);
        for (int i = 0; i < frames; ++i) {
            interleavedStereo[2 * i] = interleavedStereo[2 * i + 1] =
                m_mono[static_cast<size_t>(i)];
        }
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
    // Alles andere: Durchlauf (NR1 bleibt die Geraete-NR).
}

void ExternalAudioNr::reset()
{
    m_fifoIn.clear();
    m_fifoOut.clear();
    if (m_rnnoise) { m_rnnoise->reset(); }
#ifdef HAVE_MNR
    if (m_mnr) { m_mnr->reset(); }
#endif
}

} // namespace Longpath
