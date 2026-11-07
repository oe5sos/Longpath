// =================================================================
// src/core/AudioZuIq.cpp  (Longpath)
// =================================================================
//
// no-port-check: Longpath-original, siehe AudioZuIq.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-07 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "AudioZuIq.h"

#include <cmath>

namespace Longpath {

AudioZuIq::AudioZuIq(int abgriffe)
{
    const int laenge = (abgriffe % 2 == 1) ? abgriffe : abgriffe + 1;
    m_koeffizienten.resize(laenge);
    m_verlauf.fill(0.0f, laenge);

    const int mitte = laenge / 2;
    for (int i = 0; i < laenge; ++i) {
        const int k = i - mitte;
        if (k % 2 == 0) {
            m_koeffizienten[i] = 0.0f;
            continue;
        }
        const double ideal = 2.0 / (M_PI * k);
        const double fenster = 0.54 + 0.46 * std::cos(M_PI * k / mitte);
        m_koeffizienten[i] = static_cast<float>(ideal * fenster);
    }
}

QVector<float> AudioZuIq::verarbeite(const float* ton, int n)
{
    QVector<float> aus(n * 2);
    const int laenge = static_cast<int>(m_koeffizienten.size());
    const int mitte = laenge / 2;
    // Siehe Kopf von AudioZuIq.h: USB ist hier die negative Frequenz.
    const float vorzeichen = m_gespiegelt ? 1.0f : -1.0f;

    for (int i = 0; i < n; ++i) {
        m_verlauf[m_kopf] = ton[i];

        float q = 0.0f;
        for (int k = 0; k < laenge; ++k) {
            const float c = m_koeffizienten[k];
            if (c == 0.0f) { continue; }
            q += c * m_verlauf[(m_kopf - k + laenge) % laenge];
        }

        aus[2 * i]     = m_verlauf[(m_kopf - mitte + laenge) % laenge];
        aus[2 * i + 1] = vorzeichen * q;
        m_kopf = (m_kopf + 1) % laenge;
    }
    return aus;
}

} // namespace Longpath
