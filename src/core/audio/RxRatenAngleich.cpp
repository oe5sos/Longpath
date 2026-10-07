
// =================================================================
// src/core/audio/RxRatenAngleich.cpp  (Longpath)
// =================================================================
//
// Ported from WDSP source (zwei Quellen, darum zwei Koepfe unten):
//   third_party/wdsp/src/rmatch.c  — die Parameterwahl, mit der der
//     Umtaster aufgesetzt wird (rmatch.c:142-143 und create_rmatchV,
//     :500-526 [v2.10.3.13]): fc_high 0,0 (automatisch), fc_low -1,0
//     (keine untere Grenze), gain 1,0, R 1024, varmode 1 (Verhaeltnis
//     je Probe linear gefuehrt).
//   third_party/wdsp/src/varsamp.c — der Umtaster selbst, ueber seine
//     C-Schnittstelle aufgerufen (create_varsamp / xvarsamp /
//     flush_varsamp / destroy_varsamp, varsamp.h:64-80). Der Code
//     wurde NICHT abgeschrieben; er wird benutzt.
//
//   Beide Original-Lizenzkoepfe folgen woertlich.
//
// ABWEICHUNG: Thetis fuehrt das Regelgesetz von BEIDEN Seiten aus und
// nimmt dabei `cs_var` im Geraeterueckruf. Longpath darf im Tonrueckruf
// keine Sperre nehmen (CLAUDE.md), und sein Ring ist absichtlich
// sperrfrei — hier laeuft alles auf dem Erzeugerfaden, und die
// verbrauchten Rahmen werden am Fuellstand abgelesen statt gezaehlt.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-06 — Created in C++20/Qt6 for Longpath, operator Martin
//                 Fischer (OE5SOS), AI-assisted via Anthropic Claude
//                 Code. Zielverzoegerung 50 ms (halber Ring, wie
//                 Thetis) und nur der Lautsprecherweg — beides
//                 Entscheidungen des Betreibers vom 2026-10-06.
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
//
// /*  varsamp.c
//
// This file is part of a program that implements a Software-Defined Radio.
//
// Copyright (C) 2017 Warren Pratt, NR0V
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
#include "core/audio/RxRatenAngleich.h"

#include <algorithm>
#include <cstdint>
#include <cmath>

extern "C" {
// Aus third_party/wdsp/src/varsamp.h. Nur die vier Aufrufe, die hier
// gebraucht werden -- den ganzen WDSP-Kopf einzubinden zoege `comm.h`
// und damit halb WDSP in jede Uebersetzungseinheit, die diesen Kopf
// liest.
void* create_varsamp(int run, int size, double* in, double* out,
                     int in_rate, int out_rate, double fc, double fc_low,
                     int R, double gain, double var, int varmode);
void  destroy_varsamp(void* a);
void  flush_varsamp(void* a);
int   xvarsamp(void* a, double var);
}

namespace Longpath {

namespace {

// Dieselben Werte, die rmatch.c:142-143 dem Umtaster gibt
// (create_rmatchV, rmatch.c:500-526 [v2.10.3.13]):
constexpr double kFcHigh  =  0.0;   // 0 = automatisch
constexpr double kFcLow   = -1.0;   // -1 = keine untere Grenze
constexpr double kGain    =  1.0;
constexpr int    kR       = 1024;   // Koeffizientendichte
constexpr int    kVarmode = 1;      // Verhaeltnis je Probe linear fuehren

// Wieviel mehr Rahmen hoechstens herauskommen koennen. Das Verhaeltnis ist
// auf 1,04 gedeckelt (rmatch.c:269); 1,1 ist also reichlich und faengt
// zusaetzlich den Rest auf, den der Umtaster aus seinem inneren Zustand
// nachliefert.
constexpr double kPlatzFaktor = 1.1;

}  // namespace

RxRatenAngleich::RxRatenAngleich(int rate, int kanaele)
    : m_rate(rate > 0 ? rate : 48000)
    , m_kanaele(std::clamp(kanaele, 1, 2))
{
    AudioRateMatcher::Einstellungen e;
    e.nennRateEin = m_rate;
    e.nennRateAus = m_rate;
    m_regler = AudioRateMatcher(e);
}

RxRatenAngleich::~RxRatenAngleich()
{
    if (m_varsamp) { destroy_varsamp(m_varsamp); m_varsamp = nullptr; }
}

void RxRatenAngleich::baueUmtaster(int blockRahmen)
{
    if (m_varsamp) { destroy_varsamp(m_varsamp); m_varsamp = nullptr; }
    m_blockRahmen = blockRahmen;
    m_neubauten.fetch_add(1, std::memory_order_relaxed);

    // varsamp rechnet KOMPLEX: zwei doubles je Probe. Fuer Stereo faellt
    // das zusammen (links/rechts auf die beiden Teile); bei Mono wird der
    // zweite Teil mitgefuehrt und am Ende verworfen -- das kostet nichts
    // und erspart einen zweiten Rechenweg.
    m_ein.assign(static_cast<size_t>(blockRahmen) * 2, 0.0);
    const int platz = static_cast<int>(std::ceil(blockRahmen * kPlatzFaktor)) + 8;
    m_aus.assign(static_cast<size_t>(platz) * 2, 0.0);
    m_ausFloat.assign(static_cast<size_t>(platz) * 2, 0.0f);

    m_varsamp = create_varsamp(1, blockRahmen, m_ein.data(), m_aus.data(),
                               m_rate, m_rate, kFcHigh, kFcLow, kR, kGain,
                               1.0, kVarmode);
}

RxRatenAngleich::Ausgabe RxRatenAngleich::verarbeite(
    const float* ein, int rahmen, std::int64_t fuellungRahmen, std::int64_t ringRahmen)
{
    if (!ein || rahmen <= 0) { return { ein, 0 }; }

    // Unbekannter Fuellstand -> unveraendert durchreichen. Ein Regler ohne
    // Messgroesse ist keiner, und ein Umtaster, der auf Verdacht laeuft,
    // kostet Rechenzeit und Tonqualitaet fuer nichts.
    if (fuellungRahmen < 0 || ringRahmen <= 0) { return { ein, rahmen }; }

    // Dem Regler beide Richtungen melden: erzeugte Rahmen positiv, die vom
    // Geraet verbrauchten negativ. Die verbrauchten liest der Erzeuger am
    // Fuellstand ab -- so muss der Rueckruf nichts mitfuehren und keine
    // Sperre nehmen (ebendas unterscheidet diesen Weg von Thetis).
    const std::int64_t letzteFuellung =
        m_letzteFuellung.load(std::memory_order_relaxed);
    if (letzteFuellung >= 0) {
        const std::int64_t verbraucht = letzteFuellung + m_letzteAusgabe - fuellungRahmen;

        // ── Ein geleerter Ring ist kein Verbrauch (2026-10-06) ───────────
        //
        // `AudioEngine::setMasterMuted(true)` ruft `IAudioBus::flush()`,
        // und PortAudioBus setzt dabei den Lese- auf den Schreibzeiger:
        // der Fuellstand faellt in einem Schritt auf null. Aus dem
        // Unterschied gelesen sieht das aus wie ein Verbrauch von 2880
        // statt 480 Rahmen -- das Sechsfache, und es stimmt nicht: das
        // Geraet hat nichts davon gehoert, die Rahmen wurden weggeworfen.
        //
        // So eine Zahl in die Regelung zu geben verschiebt das gemittelte
        // Verhaeltnis, und zwar bei JEDEM Stummschalten. Der Fehler
        // waere leise: kein Knacken, kein Aussetzer, nur ein Regler, der
        // mit der Zeit auf eine falsche Rate zieht.
        //
        // Darum eine Plausibilitaetsgrenze. Mehr als das Vierfache des
        // Blocks kann in der Zeit zwischen zwei Bloecken nicht verbraucht
        // worden sein; was darueber liegt, ist ein Sprung und keine
        // Messung. Er wird AUSGELASSEN, nicht gekappt -- ein gekappter
        // Sprung waere immer noch falsch, nur weniger.
        const std::int64_t plausibel = std::int64_t(rahmen) * m_plausibelFaktor;
        if (verbraucht > 0 && verbraucht <= plausibel) {
            m_regler.melde(-static_cast<int>(verbraucht),
                           fuellungRahmen, ringRahmen);
        } else if (verbraucht > plausibel) {
            m_spruenge.fetch_add(1, std::memory_order_relaxed);
        }
    }
    m_regler.melde(rahmen, fuellungRahmen, ringRahmen);

    if (!m_regler.regeltSchon()) {
        m_letzteFuellung.store(fuellungRahmen, std::memory_order_relaxed);
        m_letzteAusgabe = rahmen;
        return { ein, rahmen };
    }

    if (!m_varsamp || m_blockRahmen != rahmen) { baueUmtaster(rahmen); }

    for (int i = 0; i < rahmen; ++i) {
        m_ein[static_cast<size_t>(i) * 2 + 0] = ein[static_cast<size_t>(i) * m_kanaele];
        m_ein[static_cast<size_t>(i) * 2 + 1] =
            (m_kanaele == 2) ? ein[static_cast<size_t>(i) * 2 + 1]
                             : ein[static_cast<size_t>(i)];
    }

    const int heraus = xvarsamp(m_varsamp, m_regler.verhaeltnis());
    const int sicher = std::clamp(heraus, 0,
        static_cast<int>(m_ausFloat.size() / 2));
    for (int i = 0; i < sicher; ++i) {
        const float l = static_cast<float>(m_aus[static_cast<size_t>(i) * 2 + 0]);
        if (m_kanaele == 2) {
            m_ausFloat[static_cast<size_t>(i) * 2 + 0] = l;
            m_ausFloat[static_cast<size_t>(i) * 2 + 1] =
                static_cast<float>(m_aus[static_cast<size_t>(i) * 2 + 1]);
        } else {
            m_ausFloat[static_cast<size_t>(i)] = l;
        }
    }

    m_versatz.fetch_add(static_cast<std::int64_t>(sicher) - rahmen,
                        std::memory_order_relaxed);
    m_letzteFuellung.store(fuellungRahmen, std::memory_order_relaxed);
    m_letzteAusgabe = sicher;
    return { m_ausFloat.data(), sicher };
}

QString RxRatenAngleich::protokollZeile(std::int64_t jetztMs, int abstandSek)
{
    // Vor der Anlaufzeit gibt es nichts zu melden -- da regelt noch
    // niemand, und eine Zeile "var = 1,0" waere eine Aussage ueber nichts.
    if (!m_regler.regeltSchon()) { return {}; }
    if (m_letzteMeldungMs != 0
        && (jetztMs - m_letzteMeldungMs) < std::int64_t(abstandSek) * 1000) {
        return {};
    }
    m_letzteMeldungMs = jetztMs;

    // Der Versatz ist die eigentliche Zahl: so viele Rahmen hat der
    // Ausgleich bis jetzt zugelegt oder weggenommen. Laeuft er richtig,
    // waechst er stetig und langsam; springt er, stimmt etwas nicht.
    const double ppm = (m_regler.verhaeltnis() - 1.0) * 1e6;
    return QStringLiteral(
        "RX-Driftausgleich: Verhaeltnis %1 (%2 ppm), Fuellstand %3 Rahmen, "
        "Versatz %4 Rahmen seit dem Start, %5 Spruenge ausgelassen, "
        "%6 Umtaster-Neubauten")
        .arg(m_regler.verhaeltnis(), 0, 'f', 9)
        .arg(ppm, 0, 'f', 2)
        .arg(m_letzteFuellung.load(std::memory_order_relaxed))
        .arg(versatz())
        .arg(spruenge())
        .arg(umtasterNeubauten());
}

void RxRatenAngleich::zuruecksetzen()
{
    m_regler.zuruecksetzen();
    if (m_varsamp) { flush_varsamp(m_varsamp); }
    m_versatz.store(0, std::memory_order_relaxed);
    m_spruenge.store(0, std::memory_order_relaxed);
    m_neubauten.store(0, std::memory_order_relaxed);
    m_letzteMeldungMs = 0;
    m_letzteFuellung.store(-1, std::memory_order_relaxed);
    m_letzteAusgabe = 0;
}

}  // namespace Longpath
