// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/safety/SwrRiegel.cpp  (Longpath)
// =================================================================
// Siehe SwrRiegel.h fuer die beiden Riegel und die drei Faelle, in
// denen eine Messung nichts bedeutet.
// =================================================================

#include "core/safety/SwrRiegel.h"

#include <algorithm>

namespace Longpath::safety {

void SwrRiegel::setzeGrenze(float g)
{
    // Unter 1,0 gibt es keine Stehwelle; eine Grenze darunter waere
    // eine Dauersperre. Nach oben bleibt es dem Betreiber ueberlassen,
    // aber nicht ins Sinnlose: ab 10 misst ohnehin keine Brueckenart
    // mehr brauchbar.
    m_grenze = std::clamp(g, 1.0f, 10.0f);
}

bool SwrRiegel::messung(float swr, float vorwaertsW, qint64 jetztMs)
{
    if (!m_an) { return false; }

    // Unterhalb weniger Watt sagt die Zahl nichts. Sie wird auch NICHT
    // gemerkt: sonst stuende nach jeder Sendung die Rauschmessung des
    // Ausklingens als „letzte Messung" da.
    if (vorwaertsW < kMinLeistungW) { return false; }

    m_letzteSwr   = swr;
    m_letzteMs    = jetztMs;
    m_hatGemessen = true;

    if (swr <= m_grenze) { return false; }

    // Nur die ERSTE Ueberschreitung meldet den Bruch. Danach kommen
    // noch Messungen herein, waehrend der Sender ausklingt; jede davon
    // als neuen Bruch zu melden hiesze, dieselbe Sache mehrfach zu
    // tun — und das Protokoll zuzuschuetten.
    if (m_gebrochen) { return false; }
    m_gebrochen = true;
    return true;
}

bool SwrRiegel::darfTasten(qint64 jetztMs) const
{
    if (!m_an)          { return true; }
    if (!m_hatGemessen) { return true; }   // Abwesenheit ist keine Aussage
    if ((jetztMs - m_letzteMs) >= kHaltbarkeitMs) { return true; }
    return m_letzteSwr <= m_grenze;
}

QString SwrRiegel::grund(qint64 jetztMs) const
{
    if (darfTasten(jetztMs)) { return {}; }
    return QStringLiteral("Stehwelle %1 ueber der Grenze %2")
        .arg(m_letzteSwr, 0, 'f', 2)
        .arg(m_grenze, 0, 'f', 2);
}

void SwrRiegel::zuruecksetzen()
{
    m_gebrochen = false;
}

}  // namespace Longpath::safety
