// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/applets/PanelAutomatik.cpp  (Longpath)
// =================================================================
// Siehe PanelAutomatik.h fuer die drei Regeln und die Begruendung
// der Nachlaufzeit.
// =================================================================

#include "PanelAutomatik.h"

namespace Longpath {

PanelAutomatik::Tat PanelAutomatik::pruefe(bool hatInhalt, bool sichtbar,
                                           qint64 jetztMs)
{
    // Regel 3 zuerst: wer selbst gegriffen hat, bestimmt.
    if (m_betreiberEntschied) {
        m_warLeer = !hatInhalt;
        return Tat::Nichts;
    }

    if (hatInhalt) {
        m_warLeer = false;
        if (!sichtbar) {
            m_vonUnsAuf = true;
            return Tat::Aufmachen;
        }
        return Tat::Nichts;
    }

    // Leer. Den Augenblick merken, ab dem es leer IST -- nicht jeden
    // leeren Durchgang neu, sonst liefe die Nachlaufzeit nie ab.
    if (!m_warLeer) {
        m_warLeer  = true;
        m_leerSeit = jetztMs;
        return Tat::Nichts;
    }

    // Nur zumachen, was wir selbst aufgemacht haben.
    if (!m_vonUnsAuf || !sichtbar) { return Tat::Nichts; }
    if ((jetztMs - m_leerSeit) < kNachlaufMs) { return Tat::Nichts; }

    m_vonUnsAuf = false;
    return Tat::Zumachen;
}

void PanelAutomatik::zuruecksetzen()
{
    m_vonUnsAuf = false;
    m_betreiberEntschied = false;
    m_leerSeit = 0;
    m_warLeer = true;
}

}  // namespace Longpath
