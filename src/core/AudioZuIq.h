#pragma once

// no-port-check: Longpath-original.

// =================================================================
// src/core/AudioZuIq.h  (Longpath)
// =================================================================
//
// Macht aus dem Ton eines klassischen Funkgeraets ein I/Q-Signal.
//
// Der TS-590 gibt Longpath kein Bandsignal, aber seinen Ton ueber USB.
// Der Ton ist bereits die Niederfrequenz hinter dem Filter des Geraets:
// 1 kHz Ton in USB heisst 1 kHz ueber dem Traeger. Als analytisches
// Signal liegt er damit genau dort, wo ein SDR ihn zeigen wuerde, und
// Longpaths ganzer Empfangsweg laeuft ohne Sonderfall: Ton, schmaler
// Wasserfall ueber die Filterbreite, Filter und NR.
//
// Das Spektrum zeigt nur, was durch das Filter des Geraets kommt. Es ist
// kein Bandbild.
//
// Die Umwandlung ist ein FIR-Hilbert-Transformator: Q ist der um 90 Grad
// gedrehte Ton, I derselbe Ton um die Gruppenlaufzeit verzoegert. In LSB
// liegt das Signal unter dem Traeger, also wird Q gespiegelt.
//
// Der Filter kennt keinen Faden. Wer ihn benutzt, ruft ihn aus einem.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-07 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QVector>

namespace Longpath {

class AudioZuIq {
public:
    /// Ungerade Laenge. 255 Abgriffe bei 48 kHz halten die Spiegelfrequenz
    /// ab rund 200 Hz unter -40 dB. Die Gruppenlaufzeit sind 2,6 ms.
    explicit AudioZuIq(int abgriffe = 255);

    void setGespiegelt(bool gespiegelt) { m_gespiegelt = gespiegelt; }
    bool gespiegelt() const { return m_gespiegelt; }

    /// `n` Tonproben hinein, 2*n Werte heraus: I, Q, I, Q, ...
    QVector<float> verarbeite(const float* ton, int n);

private:
    QVector<float> m_koeffizienten;
    QVector<float> m_verlauf;
    int  m_kopf{0};
    bool m_gespiegelt{false};
};

} // namespace Longpath
