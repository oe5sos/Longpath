// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/Ft8Warteschlange.cpp  (Longpath)
// =================================================================
// Siehe Ft8Warteschlange.h fuer die beiden Listen und die Messung,
// die sie noetig gemacht hat.
// =================================================================

#include "core/Ft8Warteschlange.h"

namespace Longpath {

bool Ft8Warteschlange::merkeAnruf(const QString& partner, int snr,
                                  const QString& locator, qint64 jetztMs)
{
    const QString k = schluessel(partner);
    if (k.isEmpty()) { return false; }

    // Wer gerade gearbeitet wurde, stellt sich nicht wieder an.
    if (inKarenz(k, jetztMs)) { return false; }

    for (Wartend& w : m_schlange) {
        if (schluessel(w.partner) == k) {
            // Schon in der Schlange: Rapport und Zeit auffrischen,
            // aber die Reihenfolge nicht anfassen. Wer dreimal ruft,
            // draengt sich nicht vor -- sonst gewaenne der
            // Hartnaeckigste, nicht der Erste.
            w.snr    = snr;
            w.seitMs = jetztMs;
            if (!locator.isEmpty()) { w.locator = locator; }
            return false;
        }
    }

    Wartend neu;
    neu.partner = partner;
    neu.snr     = snr;
    neu.locator = locator;
    neu.seitMs  = jetztMs;
    m_schlange.append(neu);

    while (m_schlange.size() > kMaxWartende) { m_schlange.removeFirst(); }
    return true;
}

void Ft8Warteschlange::merkeGearbeitet(const QString& partner, qint64 jetztMs)
{
    const QString k = schluessel(partner);
    if (k.isEmpty()) { return; }
    m_gearbeitet.insert(k, jetztMs);
    vergiss(partner);
}

bool Ft8Warteschlange::inKarenz(const QString& partner, qint64 jetztMs) const
{
    const auto it = m_gearbeitet.constFind(schluessel(partner));
    if (it == m_gearbeitet.constEnd()) { return false; }
    return (jetztMs - it.value()) < kKarenzMs;
}

Ft8Warteschlange::Wartend Ft8Warteschlange::nimmNaechsten(qint64 jetztMs)
{
    while (!m_schlange.isEmpty()) {
        const Wartend w = m_schlange.first();
        m_schlange.removeFirst();
        if ((jetztMs - w.seitMs) >= kHaltbarkeitMs) { continue; }
        if (inKarenz(w.partner, jetztMs))           { continue; }
        return w;
    }
    return Wartend{};
}

int Ft8Warteschlange::wartende(qint64 jetztMs) const
{
    int n = 0;
    for (const Wartend& w : m_schlange) {
        if ((jetztMs - w.seitMs) >= kHaltbarkeitMs) { continue; }
        if (inKarenz(w.partner, jetztMs))           { continue; }
        ++n;
    }
    return n;
}

void Ft8Warteschlange::vergiss(const QString& partner)
{
    const QString k = schluessel(partner);
    for (int i = m_schlange.size() - 1; i >= 0; --i) {
        if (schluessel(m_schlange.at(i).partner) == k) {
            m_schlange.removeAt(i);
        }
    }
}

void Ft8Warteschlange::leeren()
{
    m_schlange.clear();
    m_gearbeitet.clear();
}

}  // namespace Longpath
