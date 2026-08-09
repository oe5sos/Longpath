#pragma once

// =================================================================
// src/gui/applets/eq/EqHost.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. The seam between AetherSDR's ported equaliser
// widgets and NereusSDR's channel strip.
//
// The ported widgets talk to an `AudioEngine`, and they use exactly
// five things from it:
//
//     clientEqTx()                    the ClientEq to edit
//     clientEqRx()                    the receive-side one
//     copyRecentClientEqTxSamples()   audio for the FFT behind the curve
//     copyRecentClientEqRxSamples()
//     saveClientEqSettings()          persistence
//
// NereusSDR has no AudioEngine of that shape, and its channel strip is
// transmit-only. Rather than editing five ported files to reach into
// StripChain — which would turn a verbatim port into a modified one and
// make the next upstream comparison useless — this presents the same
// five names over NereusSDR's own parts.
//
// An adapter is the honest way to port a widget: the borrowed code
// stays byte-comparable against upstream, and every adaptation lives in
// one file that is ours and can be read on its own.
//
// ── Receive side ─────────────────────────────────────────────────────
//
// There isn't one. NereusSDR's channel strip processes the microphone
// on the way out; the receive audio goes through WDSP and has its own
// equaliser applet. clientEqRx() therefore returns null, and the panel
// is only ever shown on the transmit path. Returning null rather than
// silently handing back the transmit equaliser is deliberate: an
// operator who somehow reached the RX view should get an empty panel,
// not a working one that edits the wrong thing.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QPointer>

namespace NereusSDR {

class ClientEq;
class StripChain;

class EqHost {
public:
    explicit EqHost(StripChain* chain) : m_chain(chain) {}

    void setChain(StripChain* chain) { m_chain = chain; }

    // The equaliser the widgets edit. Null when no radio is connected,
    // which every ported widget already handles — they were written
    // against an engine that can be between devices.
    ClientEq* clientEqTx();
    ClientEq* clientEqRx() { return nullptr; }   // see the header note

    // Recent microphone audio for the analyser behind the curve, oldest
    // first. Returns how many samples were written, which may be fewer
    // than asked for until the ring has filled.
    int copyRecentClientEqTxSamples(float* dst, int count);
    int copyRecentClientEqRxSamples(float*, int) { return 0; }

    void saveClientEqSettings();

private:
    StripChain* m_chain{nullptr};
};

} // namespace NereusSDR
