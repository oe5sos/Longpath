// =================================================================
// src/core/Ft8Uhrversatz.cpp  (Longpath)
// =================================================================
//
// Siehe Ft8Uhrversatz.h fuer das Warum. Hier steht nur das Rechnen.
//
// Modification history (Longpath):
//   2026-11-13 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "Ft8Uhrversatz.h"

#include <algorithm>
#include <cmath>

namespace Longpath {

void Ft8Uhrversatz::setzeFt4(bool ft4)
{
    if (ft4 == m_ft4) { return; }
    m_ft4 = ft4;
    // Der Spielraum aendert sich, also taugen die alten Perioden
    // nicht mehr als Beleg fuer das neue Urteil.
    leere();
}

void Ft8Uhrversatz::merke(double dtSek)
{
    // Was ausserhalb des Suchfensters liegt, kann der Dekoder gar
    // nicht gefunden haben; so ein Wert waere ein Rechenfehler und
    // kein Messwert. Grosszuegig begrenzt, damit ein Grenzfall nicht
    // still verschwindet.
    if (!std::isfinite(dtSek) || std::abs(dtSek) > 10.0) { return; }
    m_laufend.push_back(dtSek);
}

void Ft8Uhrversatz::periodeFertig()
{
    if (m_laufend.size() >= kMindestFunde) {
        m_perioden.push_back(median(m_laufend));
        while (m_perioden.size() > kPerioden) { m_perioden.pop_front(); }
    }
    m_laufend.clear();
}

void Ft8Uhrversatz::leere()
{
    m_laufend.clear();
    m_perioden.clear();
}

double Ft8Uhrversatz::versatzSek() const
{
    if (!hatUrteil()) { return 0.0; }
    return median(std::vector<double>(m_perioden.begin(), m_perioden.end()));
}

Ft8Uhrversatz::Lage Ft8Uhrversatz::lage() const
{
    if (!hatUrteil()) { return Lage::Unbekannt; }
    const double v = std::abs(versatzSek());
    const double schlecht   = m_ft4 ? kSchlechtFt4   : kSchlechtFt8;
    const double auffaellig = m_ft4 ? kAuffaelligFt4 : kAuffaelligFt8;
    if (v >= schlecht)   { return Lage::Schlecht; }
    if (v >= auffaellig) { return Lage::Auffaellig; }
    return Lage::Ok;
}

double Ft8Uhrversatz::median(std::vector<double> werte)
{
    if (werte.empty()) { return 0.0; }
    const std::size_t mitte = werte.size() / 2;
    std::nth_element(werte.begin(), werte.begin() + long(mitte), werte.end());
    const double oben = werte[mitte];
    if (werte.size() % 2 == 1) { return oben; }
    // Bei gerader Anzahl das Mittel der beiden mittleren. `nth_element`
    // hat die linke Haelfte nur partitioniert, nicht sortiert -- das
    // groesste Element LINKS der Mitte ist das zweite der beiden.
    const double unten = *std::max_element(werte.begin(),
                                           werte.begin() + long(mitte));
    return (unten + oben) / 2.0;
}

}  // namespace Longpath
