#pragma once

// =================================================================
// src/core/audio/RxRatenAngleich.h  (Longpath)
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

// =================================================================
// src/core/audio/RxRatenAngleich.h  (Longpath)
// =================================================================
//
// Der Driftausgleich am Empfangston — die Verdrahtung.
//
// Das Regelgesetz steht in `AudioRateMatcher` (portiert aus WDSP
// `rmatch.c`). Hier wird es an den Tonweg gehängt: Füllstand ablesen,
// Verhältnis holen, Rahmen damit umtasten, und das Ergebnis in den Bus
// schieben.
//
// ── Warum überhaupt ──────────────────────────────────────────────────
//
// Funkgerät und Tonkarte haben zwei Quarze. Gemessen am 2026-09-27
// (SunSDR2 QRP + MacBook Air): der Ausgangsring füllt sich um 0,23 ms je
// Minute, also rund 4 ppm. Mit dem 100-ms-Ring läuft er nach etwa fünf
// Stunden über; läuft das Gerät andersherum, läuft er nach derselben
// Zeit leer. Beides knackt.
//
// ── Die Entscheidungen des Betreibers (2026-10-06) ───────────────────
//
//   * Zielverzögerung: der HALBE Ring, wie bei Thetis. `rmatch` regelt
//     auf `rsize/2`, und das ist dort die eingestellte Latenz.
//   * NUR die Lautsprecher, nicht der Mithörweg. Der hat im
//     `MasterMixer` schon ein eigenes nachgeführtes Polster; zwei
//     Regelungen auf demselben Strom arbeiten gegeneinander. Und er
//     läuft nur beim Senden — 0,23 ms je Minute sammeln sich in einem
//     Überzug nicht an. Drift ist ein Problem von Stunden.
//
// ── Was NICHT passiert ───────────────────────────────────────────────
//
// Keine Sperre, kein Rückruf. Alles hier läuft auf dem Erzeugerfaden,
// vor `IAudioBus::push()`. Thetis nimmt `cs_var` im Geräterückruf;
// CLAUDE.md verbietet das, und Longpaths Ring ist absichtlich sperrfrei.
//
// Meldet der Bus `queuedFrames() == -1` (Füllstand unbekannt — HAL-Shm,
// PipeWire, FIFO), gehen die Rahmen UNVERÄNDERT durch. Ein Regler ohne
// Messgröße ist keiner.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-06 — Created in C++20/Qt6 for Longpath, operator Martin
//                 Fischer (OE5SOS), AI-assisted via Anthropic Claude
//                 Code. Benutzt WDSP `varsamp` (Warren Pratt, NR0V,
//                 GPL-2.0-or-later) als Umtaster, mit denselben
//                 Parametern, die `rmatch.c:142-143` ihm gibt.
// =================================================================

#include "core/audio/AudioRateMatcher.h"

#include <atomic>
#include <QString>

#include <cstdint>
#include <vector>

namespace Longpath {

class RxRatenAngleich {
public:
    /// Zielverzoegerung: der HALBE Ring, wie bei Thetis -- 50 ms.
    /// Betreiberentscheidung 2026-10-06 ("1. wie thetis").
    static constexpr int kZielMs = 50;

    /// Die Ringgroesse, auf deren HAELFTE geregelt wird, in Rahmen.
    ///
    /// Bewusst aus der Zielverzoegerung gerechnet und NICHT beim Bus
    /// erfragt: die 50 ms sind die Entscheidung, nicht die Baugroesse
    /// eines bestimmten Hintergrunds. `PortAudioBus` haelt 100 ms, also
    /// faellt beides zusammen; ein Bus mit anderem Ring bekaeme sonst
    /// stillschweigend eine andere Verzoegerung.
    static constexpr std::int64_t ringRahmenFuer(int rate)
    {
        return (rate > 0 ? rate : 48000) * 2LL * kZielMs / 1000;
    }

    /// `rate` ist die Ausgaberate, `kanaele` 1 oder 2.
    RxRatenAngleich(int rate, int kanaele);
    ~RxRatenAngleich();

    RxRatenAngleich(const RxRatenAngleich&)            = delete;
    RxRatenAngleich& operator=(const RxRatenAngleich&) = delete;

    /// Rahmen durchreichen und dabei die Drift ausgleichen.
    ///
    /// `fuellungRahmen` ist `IAudioBus::queuedFrames()` (-1 = unbekannt),
    /// `ringRahmen` die Gesamtgroesse des Busringes in Rahmen; geregelt
    /// wird auf dessen HAELFTE.
    ///
    /// Gibt einen Zeiger auf die Ausgaberahmen und ihre Anzahl zurueck.
    /// Bei unbekanntem Fuellstand ist das der EINGABEzeiger selbst --
    /// dann wird nichts kopiert und nichts gerechnet.
    struct Ausgabe { const float* rahmen; int anzahl; };
    Ausgabe verarbeite(const float* ein, int rahmen,
                       std::int64_t fuellungRahmen, std::int64_t ringRahmen,
                       std::int64_t verbrauchtGesamt = -1);

    double verhaeltnis() const { return m_regler.verhaeltnis(); }
    bool   regeltSchon() const { return m_regler.regeltSchon(); }
    /// Wie viele Rahmen der Umtaster insgesamt mehr (oder weniger)
    /// ausgegeben hat, als hineingingen. Fuer die Diagnose.
    std::int64_t versatz() const
    {
        return m_versatz.load(std::memory_order_relaxed);
    }

    /// Wie oft ein unplausibler Fuellstandssprung ausgelassen wurde --
    /// im Regelfall das Stummschalten, das den Ring leert. Steht die
    /// Zahl still, ist nichts passiert; waechst sie dauernd, stimmt mit
    /// dem Bus etwas nicht.
    std::int64_t spruenge() const
    {
        return m_spruenge.load(std::memory_order_relaxed);
    }

    /// Wie viele Kanaele ein Rahmen hat. Der Aufrufer braucht das, um aus
    /// der Rahmenzahl die Byteszahl zu rechnen -- und zwar aus DIESER
    /// Quelle, nicht aus einer eigenen 2. Zwei Stellen, die dieselbe Zahl
    /// kennen, laufen auseinander, und hier hiesse das: der doppelte
    /// Puffer wird in den Bus geschoben.
    int kanaele() const { return m_kanaele; }

    /// Wie oft der Umtaster neu angelegt werden musste, weil sich die
    /// Blockgroesse geaendert hat.
    ///
    /// Sollte 1 sein und bleiben. Waechst die Zahl, wechselt die
    /// Blockgroesse im Betrieb -- dann wird bei JEDEM Wechsel neu
    /// zugeteilt und der innere Zustand des Umtasters verworfen, was man
    /// hoert. Lieber sichtbar als still.
    std::int64_t umtasterNeubauten() const
    {
        return m_neubauten.load(std::memory_order_relaxed);
    }

    void zuruecksetzen();

    /// Ab welchem Vielfachen der Blockgroesse ein Fuellstandssprung als
    /// unplausibel gilt und AUSGELASSEN wird. Nur fuer den Pruefstand
    /// veraenderbar: mit einem riesigen Wert verhaelt sich die Klasse wie
    /// vor dem 2026-10-06, und erst dieser Vergleich belegt, dass der
    /// Schutz etwas taugt.
    void setzePlausibelFaktor(int faktor) { m_plausibelFaktor = faktor; }

    /// Eine Zeile fuer das Protokoll, oder leer, wenn nichts zu sagen ist.
    ///
    /// Betreiber am 2026-10-06, zu den gemessenen 4,2 % eines Kerns:
    /// „stören nicht, im auge behalten". Beobachten laesst sich aber nur,
    /// was man sieht -- zur Laufzeit war von diesem Regler bisher nichts
    /// zu bemerken, weder im Guten noch im Schlechten.
    ///
    /// Gibt hoechstens alle `abstandSek` Sekunden etwas zurueck: eine
    /// Zeile je Tonblock waeren hundert je Sekunde, und ein Protokoll,
    /// das zulaeuft, liest niemand.
    /// NICHT vom Tonfaden rufen. `QString` teilt Speicher zu, und
    /// `qCInfo` nimmt eine Sperre -- beides hat im Rueckruf nichts zu
    /// suchen. Gedacht ist die Zeile fuer einen Zeitgeber auf dem
    /// Hauptfaden; die Zahlen darin sind `atomic` und von aussen lesbar.
    QString protokollZeile(std::int64_t jetztMs, int abstandSek = 60);

private:
    void baueUmtaster(int blockRahmen);

    int    m_rate;
    int    m_kanaele;
    int    m_blockRahmen{0};
    void*  m_varsamp{nullptr};        // VARSAMP, ohne WDSP-Kopf hier
    std::vector<double> m_ein;        // komplex verschachtelt (2 je Rahmen)
    std::vector<double> m_aus;
    std::vector<float>  m_ausFloat;
    AudioRateMatcher m_regler;
    // Von aussen gelesen (Protokollzeile auf einem anderen Faden),
    // darum `atomic`. Entspannte Ordnung genuegt: es haengt keine
    // Entscheidung an diesen Zahlen, sie werden nur angesehen.
    std::atomic<std::int64_t> m_versatz{0};
    std::atomic<std::int64_t> m_spruenge{0};
    std::atomic<std::int64_t> m_neubauten{0};
    int          m_plausibelFaktor{4};
    // Fuellstand und Ausgabe des letzten Durchlaufs. Daraus liest der
    // Erzeuger ab, wie viele Rahmen das Geraet inzwischen verbraucht
    // hat -- ohne dass der Rueckruf mitzaehlen oder eine Sperre nehmen
    // muesste. -1 heisst: noch kein Durchlauf.
    std::int64_t m_letzteMeldungMs{0};
    std::atomic<std::int64_t> m_letzteFuellung{-1};
    // Der zuletzt gesehene Stand von `IAudioBus::consumedFrames()`. -1
    // heisst: noch keiner. Der Zaehler ist die bessere Quelle; der
    // Fuellstand bleibt daneben stehen, weil die Regelung ihn fuer den
    // Rueckfuehrungsteil ohnehin braucht.
    std::atomic<std::int64_t> m_letzterVerbrauch{-1};
    int    m_letzteAusgabe{0};
};

}  // namespace Longpath
