// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/safety/FlexAbstimmRegler.cpp  (Longpath)
// =================================================================
// Siehe FlexAbstimmRegler.h: warum es waehrend des Traegers nur
// hinuntergeht, und warum ein Prozent die Untergrenze ist.
// =================================================================

#include "core/safety/FlexAbstimmRegler.h"

#include <algorithm>
#include <cmath>

namespace Longpath::safety {

void FlexAbstimmRegler::setzeZielWatt(double w)
{
    // Unter einem Zehntel Watt misst die Bruecke nichts mehr, nach oben
    // ist die Maschine selbst die Grenze (53 dBm = 200 W laut ihrer
    // eigenen Messwerttabelle).
    m_zielW = std::clamp(w, 0.1, 200.0);
}

void FlexAbstimmRegler::setzeDecke(int prozent)
{
    m_decke = std::clamp(prozent, kMinProzent, 100);
}

int FlexAbstimmRegler::startProzent(const QString& band) const
{
    if (!m_an) { return m_decke; }
    const auto it = m_gelernt.constFind(band);
    const int p = (it != m_gelernt.constEnd()) ? it.value() : kMinProzent;
    return std::clamp(p, kMinProzent, m_decke);
}

std::optional<int> FlexAbstimmRegler::messung(const QString& band,
                                              int aktuellProzent,
                                              double fwdWatt)
{
    if (!m_an) { return std::nullopt; }

    // Unterhalb der Messbarkeit sagt die Zahl nichts. Sie wird auch
    // nicht gemerkt -- sonst stuende das Ausklingen als Messung da.
    if (fwdWatt < kMinMessbarW) { return std::nullopt; }

    m_letzteW[band] = fwdWatt;

    const double obereSchranke = m_zielW * (1.0 + kToleranz);
    const double untereSchranke = m_zielW * (1.0 - kToleranz);

    if (fwdWatt > obereSchranke) {
        if (aktuellProzent <= kMinProzent) {
            // Schon am Anschlag und trotzdem zu viel: der Hebel reicht
            // nicht. Das ist ein BEFUND, keine Panne -- und der
            // Aufrufer soll ihn melden, statt weiter zu regeln.
            m_unerreichbar[band] = true;
            m_gelernt[band] = kMinProzent;
            return std::nullopt;
        }
        // Hinunter, und zwar beherzt: die Kurve ist uns unbekannt, und
        // zu viel Leistung ist die gefaehrliche Richtung. Der Faktor
        // kommt aus dem Verhaeltnis, nicht aus einer festen Stufe --
        // bei 23 W statt 1 W soll der naechste Schritt nicht 22 mal
        // dauern.
        const double verhaeltnis = m_zielW / fwdWatt;
        int neu = static_cast<int>(std::floor(aktuellProzent * verhaeltnis));
        neu = std::clamp(neu, kMinProzent, aktuellProzent - 1);
        m_gelernt[band] = neu;
        return neu;
    }

    if (fwdWatt < untereSchranke) {
        // Zu wenig. Waehrend des Traegers wird NICHT aufgedreht --
        // siehe Kopf. Nur fuer das naechste Mal einen Schritt merken,
        // und nie ueber die Decke.
        const int neu = std::clamp(aktuellProzent + 1, kMinProzent, m_decke);
        m_gelernt[band] = neu;
        return std::nullopt;
    }

    // Im Fenster: das ist der Wert, mit dem dieses Band anfangen soll.
    m_gelernt[band] = std::clamp(aktuellProzent, kMinProzent, m_decke);
    m_unerreichbar.remove(band);
    return std::nullopt;
}

std::optional<int> FlexAbstimmRegler::gelernt(const QString& band) const
{
    const auto it = m_gelernt.constFind(band);
    if (it == m_gelernt.constEnd()) { return std::nullopt; }
    return it.value();
}

bool FlexAbstimmRegler::zielUnerreichbar(const QString& band) const
{
    return m_unerreichbar.value(band, false);
}

std::optional<double> FlexAbstimmRegler::letzteLeistung(const QString& band) const
{
    const auto it = m_letzteW.constFind(band);
    if (it == m_letzteW.constEnd()) { return std::nullopt; }
    return it.value();
}

}  // namespace Longpath::safety
