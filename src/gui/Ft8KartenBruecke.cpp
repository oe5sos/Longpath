// no-port-check: Longpath-original, see Ft8KartenBruecke.h.
//
// =================================================================
// src/gui/Ft8KartenBruecke.cpp  (Longpath)
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Created in C++20/Qt6 for Longpath by Martin Fischer
//                 (OE5SOS), AI-assisted via Anthropic Claude (Claude Code).
// =================================================================
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gui/Ft8KartenBruecke.h"

#include "gui/applets/Ft8Applet.h"

namespace Longpath {

Ft8KartenBruecke& Ft8KartenBruecke::instanz()
{
    static Ft8KartenBruecke b;
    return b;
}

void Ft8KartenBruecke::verbinde(Ft8Applet* applet)
{
    if (m_verbindung) { disconnect(m_verbindung); }
    m_applet = applet;
    if (applet) {
        m_verbindung = connect(applet, &Ft8Applet::gehoert,
                               this, &Ft8KartenBruecke::gehoert);
    }
}

QString Ft8KartenBruecke::rufe(const Ft8Gehoert& g)
{
    if (m_applet.isNull()) {
        return QStringLiteral("the FT8 decoder is not running");
    }
    if (g.zeile.trimmed().isEmpty()) {
        return QStringLiteral("nothing heard from %1 to answer").arg(g.rufzeichen);
    }
    return m_applet->rufeVonDerKarte(g.zeile);
}

} // namespace Longpath
