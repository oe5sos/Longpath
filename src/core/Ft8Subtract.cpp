/* The algorithms, source code, look-and-feel of WSJT-X and related
 * programs, and protocol specifications for the modes FSK441, FT8, JT4,
 * JT6M, JT9, JT65, JTMS, QRA64, ISCAT, MSK144, are Copyright © 2001-2017
 * by one or more of the following authors: Joseph Taylor, K1JT; Bill
 * Somerville, G4WJS; Steven Franke, K9AN; Nico Palermo, IV3NWV; Greg Beam,
 * KI7MT; Michael Black, W9MDB; Edson Pereira, PY2SDR; Philip Karn, KA9Q;
 * and other members of the WSJT Development Group.
 *
 * MSHV FT8 Decoder
 * Rewritten into C++ and modified by Hrisimir Hristov, LZ2HV 2015-2020
 * May be used under the terms of the GNU General Public License (GPL)
 */
// =================================================================
// src/core/Ft8Subtract.cpp  (Longpath)
// =================================================================
//
// Ported from MSHV 2.76.7 `src/HvDecoderMs/decoderft8.cpp`:
//   * `DecoderFt8::subtractft8()`           (Zeilen 464..536)
//   * die Filterherstellung im Baukasten    (Zeilen 90..120)
//   * `K_SUB`                               (Zeile 272)
//
// Siehe Ft8Subtract.h fuer das Wofuer und
// docs/attribution/MSHV-PROVENANCE.md fuer die Herkunft.
//
// ── Zwei bewusste Abweichungen vom Vorbild ───────────────────────────
//
// 1. MSHV erzeugt das Referenzsignal mit einem eigenen komplexen
//    GFSK-Modulator (`gen_ft8cwaveRx`, Zeilen 329..379). Longpath hat
//    denselben Modulator bereits im Haus: `synth_gfsk()` aus
//    `third_party/ft8_lib/common/gfsk.c` (MIT, mitgeliefert), die
//    REELLE Fassung derselben Rechnung -- gleiche `dphi_peak`, gleiche
//    Pulsaufsummierung, gleiche Scheinsymbole vorn und hinten, gleiche
//    Rampen. Statt den Modulator ein zweites Mal zu schreiben, wird
//    hier aus der reellen Fassung das analytische Signal gebildet
//    (Negativfrequenzen null, Positivfrequenzen verdoppelt). Fuer ein
//    bandbegrenztes Signal -- und ein FT8-Ton ist 50 Hz breit -- ist
//    das exakt dasselbe Ergebnis, nur ohne zweite Umsetzung derselben
//    Formel.
//
// 2. Statt MSHVs Feinjustierung (`BestIdtft8`: drei Proben bei
//    -90/0/+90 Proben mit Scheitelinterpolation) steht hier eine
//    VOLLE Suche ueber die Kreuzkorrelation. Der Grund ist gemessen,
//    nicht vermutet (tst_ft8_subtraktion, `wieGenauSchaetztDerDekoder`):
//
//        gemeldet f=1000,000 Hz (wahr 1000,000)
//        gemeldet dt=0,6400 s  (wahr 0,5000)
//
//    Die Tonlage stimmt auf das Hertz, der Zeitversatz liegt um
//    0,14 s daneben -- **1680 Proben**. MSHVs Suchbereich ist +/-90.
//    Dort ist der Unterschied klein, weil sein eigener Dekoder den
//    Versatz feiner meldet als `ft8_lib` es tut; mit unseren Zahlen
//    traefe eine Dreipunktsuche nie.
//
//    Die Kreuzkorrelation kostet ein FFT-Paar und findet die Stelle
//    probengenau, egal wie weit sie liegt. Ohne sie zieht der Abzug
//    ins Leere -- genau das fing der Pruefstand ab.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Ported from MSHV 2.76.7 for Longpath by Martin
//                 Fischer (OE5SOS), AI-assisted via Anthropic Claude
//                 (Claude Code).
// =================================================================

#include "core/Ft8Subtract.h"

#include "core/dsp/FftwPlannerLock.h"

extern "C" {
#include "common/gfsk.h"
#include "ft8/constants.h"
}

#include <fftw3.h>

#include <cmath>
#include <algorithm>
#include <complex>
#include <mutex>
#include <vector>

namespace Longpath {

namespace {

using Komplex = std::complex<double>;

// From MSHV decoderft8.cpp:89-92 [2.76.7]
//   const int NMAX=180000;//15*DEC_SAMPLE_RATE;//=180000
//   const int NFFT=180000;//NMAX; need to be number if no crash  180000
//   const int NFILT=4000;//3700;//old NFILT=1400; 4000
constexpr int kNmax  = 180000;   // 15 s bei 12 kHz
constexpr int kNfft  = 180000;
constexpr int kNfilt = 4000;

// From MSHV decoderft8.cpp:272 [2.76.7] — `#define K_SUB 1.9962 //2.70`
// Nicht 2,0: der Wert ist beim Vorbild ueber Jahre nachgezogen worden
// (1,92 -> 1,93 -> 1,94 -> 1,97 -> 1,9840 -> 1,9842 -> 1,9962, die
// alten Werte stehen dort auskommentiert daneben). Er bleibt, wie er
// ist -- eine selbst gewaehlte Zahl waere geraten.
constexpr double kSub = 1.9962;

/// Das Tiefpassfenster und die Kantenkorrektur, einmal gerechnet.
///
/// From MSHV decoderft8.cpp:90-120 [2.76.7] — die Herstellung steht
/// dort im Baukasten des Dekoders, also ebenfalls genau einmal.
struct Filter {
    std::vector<Komplex> cw;              ///< im Frequenzbereich, NFFT lang
    std::vector<double>  endkorrektur;    ///< NFILT/2+1 lang

    Filter()
    {
        // ! Create and normalize the filter
        const int offset = kNfilt / 2 + 25;
        std::vector<double> fenster(static_cast<size_t>(kNfilt) + 250, 0.0);
        const double fac = 1.0 / double(kNfft);
        double sumw = 0.0;
        for (int j = -kNfilt / 2; j < kNfilt / 2; ++j) {
            const double w = std::cos(M_PI * double(j) / double(kNfilt))
                           * std::cos(M_PI * double(j) / double(kNfilt));
            fenster[static_cast<size_t>(j + offset)] = w;
            sumw += w;
        }
        if (sumw <= 0.0) { sumw = 0.01; }   // no devide by zero

        cw.assign(static_cast<size_t>(kNmax) + 25, Komplex(0.0, 0.0));
        for (int i = 0; i < kNfilt + 1; ++i) {
            cw[static_cast<size_t>(i)] =
                fenster[static_cast<size_t>(i + offset - kNfilt / 2)] / sumw;
        }
        // cw=cshift(cw,NFILT/2+1) -- das Fenster um seine halbe Breite
        // nach LINKS drehen, damit es im Frequenzbereich reell wird.
        std::rotate(cw.begin(), cw.begin() + (kNfilt / 2 + 1), cw.end());

        // four2a(cw,nfft,1,-1,1) und danach mal 1/NFFT
        {
            auto sperre = fftwPlannerLock();
            fftw_plan plan = fftw_plan_dft_1d(
                kNfft,
                reinterpret_cast<fftw_complex*>(cw.data()),
                reinterpret_cast<fftw_complex*>(cw.data()),
                FFTW_FORWARD, FFTW_ESTIMATE);
            fftw_execute(plan);
            fftw_destroy_plan(plan);
        }
        for (int i = 0; i < kNmax; ++i) { cw[static_cast<size_t>(i)] *= fac; }

        // endcorrection(j)=1.0/(1.0-sum(window(j-1:NFILT/2))/sumw)
        endkorrektur.assign(static_cast<size_t>(kNfilt) / 2 + 1, 1.0);
        for (int j = 0; j < kNfilt / 2 + 1; ++j) {
            double summ = 0.0;
            for (int z = j; z < kNfilt / 2; ++z) {
                summ += fenster[static_cast<size_t>(z + offset)];
            }
            double dell = 1.0 - summ / sumw;
            if (dell <= 0.0) { dell = 0.0001; }
            endkorrektur[static_cast<size_t>(j)] = 1.0 / dell;
        }
    }
};

const Filter& filter()
{
    static const Filter f;
    return f;
}

/// Das analytische Signal zu einer reellen Folge.
///
/// Siehe Abweichung 1 im Kopf dieser Datei: so entsteht aus
/// `synth_gfsk()` dasselbe komplexe Referenzsignal, das MSHV mit einem
/// eigenen Modulator erzeugt.
void analytisch(const std::vector<float>& reell, std::vector<Komplex>* aus)
{
    const int n = static_cast<int>(reell.size());
    aus->assign(static_cast<size_t>(n), Komplex(0.0, 0.0));
    for (int i = 0; i < n; ++i) {
        (*aus)[static_cast<size_t>(i)] = Komplex(double(reell[static_cast<size_t>(i)]), 0.0);
    }
    auto sperre = fftwPlannerLock();
    fftw_plan hin = fftw_plan_dft_1d(
        n, reinterpret_cast<fftw_complex*>(aus->data()),
        reinterpret_cast<fftw_complex*>(aus->data()),
        FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan zurueck = fftw_plan_dft_1d(
        n, reinterpret_cast<fftw_complex*>(aus->data()),
        reinterpret_cast<fftw_complex*>(aus->data()),
        FFTW_BACKWARD, FFTW_ESTIMATE);
    sperre.unlock();

    fftw_execute(hin);
    // Negativfrequenzen null, Positivfrequenzen verdoppelt; Gleichanteil
    // und Nyquist bleiben, wie sie sind.
    const int haelfte = n / 2;
    for (int i = 1; i < haelfte; ++i) {
        (*aus)[static_cast<size_t>(i)] *= 2.0;
    }
    for (int i = haelfte + 1; i < n; ++i) {
        (*aus)[static_cast<size_t>(i)] = Komplex(0.0, 0.0);
    }
    fftw_execute(zurueck);
    const double fac = 1.0 / double(n);
    for (int i = 0; i < n; ++i) { (*aus)[static_cast<size_t>(i)] *= fac; }

    auto sperre2 = fftwPlannerLock();
    fftw_destroy_plan(hin);
    fftw_destroy_plan(zurueck);
}

} // namespace

bool Ft8Subtract::zieheAb(float* dd, int ddCount,
                          const uint8_t* toene, int toeneAnzahl,
                          double f0Hz, double dtSekunden)
{
    if (dd == nullptr || toene == nullptr) { return false; }
    if (toeneAnzahl != kSymbols) { return false; }
    if (ddCount < kNmax) { return false; }
    if (!(f0Hz > 0.0) || !std::isfinite(f0Hz)) { return false; }
    if (!std::isfinite(dtSekunden)) { return false; }

    const int nframe = kSymbols * kSamplesPerSymbol;   // 151680

    // ── Das Referenzsignal ───────────────────────────────────────────
    //
    // Derselbe Modulator wie beim Senden (ft8_lib `synth_gfsk`), nur
    // ohne die Stille davor und danach: hier zaehlt der Rahmen, und wo
    // er im Fenster liegt, sagt `dtSekunden`.
    std::vector<float> reell(static_cast<size_t>(nframe), 0.0f);
    synth_gfsk(toene, kSymbols, static_cast<float>(f0Hz),
               2.0f, FT8_SYMBOL_PERIOD, kSampleRate, reell.data());

    std::vector<Komplex> cref;
    analytisch(reell, &cref);

    // ── Wo liegt der Rahmen wirklich? ───────────────────────────────
    //
    // Siehe Abweichung 2 im Kopf: der gemeldete Zeitversatz kann um
    // Tausende Proben danebenliegen. Die Kreuzkorrelation findet die
    // Stelle probengenau:
    //
    //     korr[k] = Summe ueber i von dd[k+i] * conj(cref[i])
    //
    // ueber die FFT gerechnet, also ein Paar statt tausender Summen.
    int nstart = static_cast<int>(dtSekunden * kSampleRate + 1.0);
    {
        std::vector<Komplex> a(static_cast<size_t>(kNfft), Komplex(0.0, 0.0));
        std::vector<Komplex> b(static_cast<size_t>(kNfft), Komplex(0.0, 0.0));
        for (int i = 0; i < kNmax; ++i) {
            a[static_cast<size_t>(i)] = Komplex(double(dd[static_cast<size_t>(i)]), 0.0);
        }
        for (int i = 0; i < nframe; ++i) { b[static_cast<size_t>(i)] = cref[static_cast<size_t>(i)]; }

        auto sperre = fftwPlannerLock();
        fftw_plan pa = fftw_plan_dft_1d(kNfft,
            reinterpret_cast<fftw_complex*>(a.data()),
            reinterpret_cast<fftw_complex*>(a.data()), FFTW_FORWARD, FFTW_ESTIMATE);
        fftw_plan pb = fftw_plan_dft_1d(kNfft,
            reinterpret_cast<fftw_complex*>(b.data()),
            reinterpret_cast<fftw_complex*>(b.data()), FFTW_FORWARD, FFTW_ESTIMATE);
        fftw_plan pz = fftw_plan_dft_1d(kNfft,
            reinterpret_cast<fftw_complex*>(a.data()),
            reinterpret_cast<fftw_complex*>(a.data()), FFTW_BACKWARD, FFTW_ESTIMATE);
        sperre.unlock();

        fftw_execute(pa);
        fftw_execute(pb);
        for (int i = 0; i < kNfft; ++i) {
            a[static_cast<size_t>(i)] *= std::conj(b[static_cast<size_t>(i)]);
        }
        fftw_execute(pz);

        auto sperre2 = fftwPlannerLock();
        fftw_destroy_plan(pa);
        fftw_destroy_plan(pb);
        fftw_destroy_plan(pz);
        sperre2.unlock();

        // Nur dort suchen, wo der Rahmen ueberhaupt ganz hineinpasst.
        double beste = -1.0;
        int besterK = nstart - 1;
        const int grenze = kNmax - nframe;
        for (int k = 0; k <= grenze; ++k) {
            const double m = std::norm(a[static_cast<size_t>(k)]);
            if (m > beste) { beste = m; besterK = k; }
        }
        nstart = besterK + 1;
    }

    // ── Die gemessene Huellkurve des Signals ─────────────────────────
    //
    // From MSHV decoderft8.cpp:500-510 [2.76.7]
    //   cfilt(1:nframe)=camp(1:nframe)
    std::vector<Komplex> cfilt(static_cast<size_t>(kNfft), Komplex(0.0, 0.0));
    for (int i = 0; i < nframe; ++i) {
        const int id = nstart + i - 1;
        if (id >= 0 && id < kNmax) {
            cfilt[static_cast<size_t>(i)] =
                double(dd[static_cast<size_t>(id)])
                * std::conj(cref[static_cast<size_t>(i)]);
        }
    }

    // Tiefpass im Frequenzbereich: hin, mal Fenster, zurueck.
    {
        auto sperre = fftwPlannerLock();
        fftw_plan hin = fftw_plan_dft_1d(
            kNfft, reinterpret_cast<fftw_complex*>(cfilt.data()),
            reinterpret_cast<fftw_complex*>(cfilt.data()),
            FFTW_FORWARD, FFTW_ESTIMATE);
        fftw_plan zurueck = fftw_plan_dft_1d(
            kNfft, reinterpret_cast<fftw_complex*>(cfilt.data()),
            reinterpret_cast<fftw_complex*>(cfilt.data()),
            FFTW_BACKWARD, FFTW_ESTIMATE);
        sperre.unlock();

        fftw_execute(hin);
        const Filter& f = filter();
        for (int i = 0; i < kNfft; ++i) {
            cfilt[static_cast<size_t>(i)] *= f.cw[static_cast<size_t>(i)];
        }
        fftw_execute(zurueck);

        auto sperre2 = fftwPlannerLock();
        fftw_destroy_plan(hin);
        fftw_destroy_plan(zurueck);
    }

    // Die Kanten: am Anfang und am Ende sieht der Tiefpass nur einen
    // Teil des Signals, darum die Korrektur.
    // From MSHV decoderft8.cpp:514-523 [2.76.7]
    {
        const Filter& f = filter();
        for (int i = 0; i < kNfilt / 4; ++i) {
            cfilt[static_cast<size_t>(i)] *= f.endkorrektur[static_cast<size_t>(i)];
        }
        int revv = nframe - 1;
        for (int i = 0; i < kNfilt / 4; ++i) {
            cfilt[static_cast<size_t>(revv)] *= f.endkorrektur[static_cast<size_t>(i)];
            --revv;
        }
    }

    // ── Und abziehen ─────────────────────────────────────────────────
    // From MSHV decoderft8.cpp:525-532 [2.76.7]
    //   dd[j]-=K_SUB*creal(cfr);
    for (int i = 0; i < nframe; ++i) {
        const int j = nstart + i - 1;
        if (j >= 0 && j < kNmax) {
            const Komplex cfr = cfilt[static_cast<size_t>(i)]
                              * cref[static_cast<size_t>(i)];
            dd[static_cast<size_t>(j)] -=
                static_cast<float>(kSub * cfr.real());
        }
    }
    return true;
}

} // namespace Longpath
