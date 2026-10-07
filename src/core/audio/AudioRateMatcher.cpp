
// =================================================================
// src/core/audio/AudioRateMatcher.cpp  (Longpath)
// =================================================================
//
// Ported from WDSP source:
//   third_party/wdsp/src/rmatch.c (mirrors Thetis Project Files/
//   Source/wdsp/rmatch.c at v2.10.3.13). Portiert sind das
//   Regelgesetz control() (rmatch.c:256-272), die beiden gleitenden
//   Mittelwerte xmav (:56-69) und xaamav (:101-126), die Parameter
//   aus create_rmatchV (:500-526) und die Umrechnung der
//   Verstaerkung auf die Nennrate (:147).
//
//   NICHT portiert: der Resampler varsamp, der Ringpuffer, das
//   Ausblenden bei Ueber-/Unterlauf und die beiden kritischen
//   Abschnitte. Longpath fuehrt das Regelgesetz nur auf der
//   Erzeugerseite aus, weil im Tonrueckruf keine Sperre genommen
//   werden darf (CLAUDE.md); Thetis nimmt dort cs_var.
//
//   Original WDSP source license preserved verbatim below.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-05 — Created in C++20 for Longpath, operator Martin
//                 Fischer (OE5SOS), AI-assisted via Anthropic Claude
//                 Code. Abweichung von der Vorlage: die proportionale
//                 Verstaerkung steht auf 4,0e-7 statt Thetis' 4,0e-6
//                 (rmatch.c:521) — mit 4,0e-6 divergiert der
//                 Regelkreis bei Longpaths Aufrufrate, gemessen ueber
//                 5,5 simulierte Stunden. Begruendung und Messreihe
//                 stehen bei der Einstellung selbst.
// =================================================================

// /*  rmatch.c
//
// This file is part of a program that implements a Software-Defined Radio.
//
// Copyright (C) 2017, 2018, 2022 Warren Pratt, NR0V
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// The author can be reached by email at
//
// warren@wpratt.com
//
// */
#include "core/audio/AudioRateMatcher.h"

#include <algorithm>
#include <cstdint>
#include <cmath>

namespace Longpath {

namespace {

// Die Ringgrössen sind in rmatch Zweierpotenzen, und der Index läuft über
// eine Maske statt über Modulo (rmatch.c:125). Hier dieselbe Annahme --
// aber geprüft statt vorausgesetzt: eine krumme Grösse ergäbe eine Maske,
// die stillschweigend einen anderen Ring beschreibt als der, der angelegt
// wurde.
int aufZweierpotenz(int n)
{
    int p = 1;
    while (p < n) { p <<= 1; }
    return p;
}

}  // namespace

// ── GleitenderMittelwert (xmav, rmatch.c:56-69) ────────────────────────────

GleitenderMittelwert::GleitenderMittelwert(int ringMin, int ringMax,
                                           double nennwert)
    : m_ringMin(std::max(1, ringMin))
    , m_ringMax(aufZweierpotenz(std::max(1, ringMax)))
    , m_maske(m_ringMax - 1)
    , m_nennwert(nennwert)
    , m_ring(static_cast<size_t>(m_ringMax), 0)
{
}

double GleitenderMittelwert::schiebe(int wert)
{
    if (m_geladen >= m_ringMax) { m_summe -= m_ring[static_cast<size_t>(m_i)]; }
    if (m_geladen < m_ringMax)  { m_geladen++; }
    m_ring[static_cast<size_t>(m_i)] = wert;
    m_summe += wert;

    const double aus = (m_geladen >= m_ringMin)
        ? static_cast<double>(m_summe) / static_cast<double>(m_geladen)
        : m_nennwert;
    m_i = (m_i + 1) & m_maske;
    return aus;
}

void GleitenderMittelwert::zuruecksetzen()
{
    std::fill(m_ring.begin(), m_ring.end(), 0);
    m_summe = 0; m_i = 0; m_geladen = 0;
}

// ── VerhaeltnisMittelwert (xaamav, rmatch.c:101-126) ───────────────────────

VerhaeltnisMittelwert::VerhaeltnisMittelwert(int ringMin, int ringMax,
                                             double nennVerhaeltnis)
    : m_ringMin(std::max(1, ringMin))
    , m_ringMax(aufZweierpotenz(std::max(1, ringMax)))
    , m_maske(m_ringMax - 1)
    , m_nennVerhaeltnis(nennVerhaeltnis)
    , m_ring(static_cast<size_t>(m_ringMax), 0)
{
}

double VerhaeltnisMittelwert::schiebe(int wert)
{
    // Der hinausfallende Wert wird aus SEINEM Topf genommen -- positive aus
    // `pos`, negative aus `neg`. Genau das macht den Mittelwert
    // vorzeichengetrennt: er mittelt nicht Zahlen, er bildet das
    // Verhältnis zweier Summen.
    if (m_geladen >= m_ringMax) {
        const int alt = m_ring[static_cast<size_t>(m_i)];
        if (alt >= 0) { m_pos -= alt; } else { m_neg += alt; }
    }
    if (m_geladen <= m_ringMax) { m_geladen++; }
    m_ring[static_cast<size_t>(m_i)] = wert;
    if (wert >= 0) { m_pos += wert; } else { m_neg -= wert; }

    double aus;
    if (m_geladen >= m_ringMin) {
        aus = (m_pos != 0)
            ? static_cast<double>(m_neg) / static_cast<double>(m_pos)
            : m_nennVerhaeltnis;
    } else if (m_neg > 0 && m_pos > 0) {
        // Übergang: solange zu wenige Werte da sind, zum Nennverhältnis hin
        // mischen, statt auf einer Handvoll Zahlen zu regeln.
        const double anteil = static_cast<double>(m_geladen)
                            / static_cast<double>(m_ringMin);
        aus = (1.0 - anteil) * m_nennVerhaeltnis
            + anteil * (static_cast<double>(m_neg) / static_cast<double>(m_pos));
    } else {
        aus = m_nennVerhaeltnis;
    }
    m_i = (m_i + 1) & m_maske;
    return aus;
}

void VerhaeltnisMittelwert::zuruecksetzen()
{
    std::fill(m_ring.begin(), m_ring.end(), 0);
    m_pos = m_neg = 0; m_i = 0; m_geladen = 0;
}

// ── Das Regelgesetz (control, rmatch.c:256-272) ────────────────────────────

AudioRateMatcher::AudioRateMatcher()
    : AudioRateMatcher(Einstellungen{})
{
}

AudioRateMatcher::AudioRateMatcher(const Einstellungen& e)
    : m_e(e)
    , m_nennVerhaeltnis(e.nennRateEin > 0
          ? static_cast<double>(e.nennRateAus) / static_cast<double>(e.nennRateEin)
          : 1.0)
    , m_invNennVerhaeltnis(e.nennRateAus > 0
          ? static_cast<double>(e.nennRateEin) / static_cast<double>(e.nennRateAus)
          : 1.0)
    // rmatch.c:147 -- die Verstärkung hängt an der Nennrate, sonst regelt
    // dieselbe Zahl bei 96 kHz doppelt so hart wie bei 48 kHz.
    , m_prVerstaerkung(e.propVerstaerkung * 48000.0
          / (e.nennRateAus > 0 ? static_cast<double>(e.nennRateAus) : 48000.0))
    , m_ffMav(e.ffRingMin, e.ffRingMax, m_nennVerhaeltnis)
    , m_propMav(e.propRingMin, e.propRingMax, 0.0)
    , m_anlaufRahmen(static_cast<std::int64_t>(e.anlaufSekunden
          * (e.nennRateEin > 0 ? e.nennRateEin : 48000)))
{
}

void AudioRateMatcher::melde(int aenderung, std::int64_t ringFuellung,
                             std::int64_t ringGroesse)
{
    if (aenderung > 0) { m_erzeugteRahmen += aenderung; }

    // Anlaufzeit: Thetis setzt `control_flag` erst nach drei Sekunden
    // (rmatch.c, startup delay). Vorher wird GAR NICHT geregelt -- auch die
    // Mittelwerte bleiben leer. Ein Regler, der auf die ersten Blöcke nach
    // dem Start anspringt, regelt auf den Anlauf und nicht auf die Drift.
    if (!m_regelt.load(std::memory_order_relaxed)) {
        if (m_erzeugteRahmen < m_anlaufRahmen) { return; }
        m_regelt.store(true, std::memory_order_relaxed);
    }

    // Vorsteuerung: das gemittelte Verhältnis verbraucht/erzeugt, auf die
    // Nennraten bezogen, exponentiell geglättet (rmatch.c:258-262).
    const double aktuell = m_ffMav.schiebe(aenderung) * m_invNennVerhaeltnis;
    m_vorsteuerung = m_e.ffAlpha * aktuell
                   + (1.0 - m_e.ffAlpha) * m_vorsteuerung;

    // Rückführung: die mittlere Abweichung vom halben Ring (rmatch.c:264-266).
    const std::int64_t ziel = ringGroesse / 2;
    const std::int64_t abweichung = ringFuellung - ziel;
    // Der Mittelwert rechnet in int, wie die Quelle. Ein Füllstand, der
    // nicht in int passt, wäre ein Ring von über zwei Milliarden Rahmen --
    // begrenzen statt überlaufen.
    const int abwInt = static_cast<int>(std::clamp<std::int64_t>(
        abweichung, -2147483647LL, 2147483647LL));
    m_mittlereAbweichung = m_propMav.schiebe(abwInt);

    // rmatch.c:268-271
    const double var = std::clamp(
        m_vorsteuerung - m_prVerstaerkung * m_mittlereAbweichung,
        m_e.untereGrenze, m_e.obereGrenze);
    m_var.store(var, std::memory_order_relaxed);
}

void AudioRateMatcher::zuruecksetzen()
{
    m_ffMav.zuruecksetzen();
    m_propMav.zuruecksetzen();
    m_vorsteuerung = 1.0;
    m_mittlereAbweichung = 0.0;
    m_var.store(1.0, std::memory_order_relaxed);
    m_erzeugteRahmen = 0;
    m_regelt.store(false, std::memory_order_relaxed);
}

}  // namespace Longpath
