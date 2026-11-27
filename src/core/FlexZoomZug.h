#pragma once

// =================================================================
// src/core/FlexZoomZug.h  (Longpath)
// =================================================================
//
// Zoomt der Betreiber den Panadapter, bekommt die FLEX-8400 den neuen
// Ausschnitt — Mitte UND Breite. Sonst zoomt Longpath nur LOKAL in die
// 200 kHz der Maschine hinein.
//
// Anlass (2026-11-27, longpath-20261127-102325.log, Zeilen „Kachel:"):
// Kachel 20.9586–21.1892 MHz ueber 2460 Bins, also rund 94 Hz je Bin.
// Das Fenster zeigte 7,9 kHz auf 1298 Punkten — das sind etwa 84
// Bins, jeder rund 15 Punkte breit. Betreiber: „warum ist das blau so
// pixelig?". Longpath hatte der Maschine nie eine Breite geschickt.
//
// Bei SmartSDR IST der Panadapter das Fenster. AetherSDR macht es
// genauso: jede Zoomgeste geht als
//     display pan set <id> center=<MHz> bandwidth=<MHz>
// hinaus, Mitte und Breite IMMER ZUSAMMEN
// (RadioModel::dispatchPanCenterBandwidth, RadioModel.cpp:5513
// [@d58e2b8a]: „Center and bandwidth must travel together when both
// are requested; splitting them produced the P1/P2 waterfall-loss and
// zoom-drift bugs."). Die Breite klemmt AetherSDR auf 10 kHz bis
// 5,4 MHz (SpectrumWidget.h:1495-1496 [@d58e2b8a]), und den Endstand
// einer Geste schickt es nach 300 ms Ruhe (kFrequencyRangeSettleMs,
// SpectrumWidget.cpp:367 [@d58e2b8a]).
//
// Zwei Fallen, die hier entschieden werden:
//
//   * KEIN Echo-Kreislauf. Auf den angenommenen Befehl fuehrt
//     Longpath den Ausschnitt selbst nach (die Maschine echot ihn
//     nicht verlaesslich, siehe RadioModel::zieheFlexPanadapterAuf).
//     Diese Rueckmeldung darf das Fenster nicht verstellen, sonst
//     meldet das Fenster einen neuen Zoom, und der geht wieder hinaus.
//   * KEIN Doppel-Zoom. Das Fenster bleibt, wie der Betreiber es
//     gezoomt hat; der lokale Bin-Ausschnitt (`visibleBinRange`)
//     rechnet gegen die NEUE Kachel und nimmt damit von selbst alle
//     Bins. Nur unter der Mindestbreite bleibt ein lokaler Rest —
//     dann aber aus einer Kachel mit zwanzigfach feineren Bins.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-27 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
//                 Werte und Regel aus AetherSDR [@d58e2b8a].
// =================================================================

#include <algorithm>
#include <cmath>

namespace Longpath {
namespace FlexZoomZug {

/// Schmaler laesst AetherSDR den Panadapter nicht werden
/// (SpectrumWidget.h:1495 [@d58e2b8a], `m_minBwMhz{0.010}`).
inline constexpr double kMindestBreiteHz = 10'000.0;

/// Breiter auch nicht (SpectrumWidget.h:1496 [@d58e2b8a],
/// `m_maxBwMhz{5.400}` — „safe default for unknown radios").
inline constexpr double kHoechstBreiteHz = 5'400'000.0;

/// So lange nach dem letzten Ruck einer Geste wird gewartet, bevor der
/// Endstand hinausgeht (SpectrumWidget.cpp:367 [@d58e2b8a]).
inline constexpr int kRuheMs = 300;

/// Ab welcher Abweichung eine Breite als ANDERE gilt — ein Prozent,
/// dasselbe Mass wie das Totband des Nachzugs (FlexFensterZug.h).
inline constexpr double kToleranzAnteil = 0.01;

struct Ausschnitt {
    double mitteHz{0.0};
    double breiteHz{0.0};
    bool gueltig() const { return mitteHz > 0.0 && breiteHz > 0.0; }
};

/// Auf das Hertz gerundet, wie es auf den Draht geht (`'f', 6` in MHz).
/// Was Longpath danach selbst nachfuehrt, muss GENAU das sein, was die
/// Maschine bekam — sonst unterscheidet sich ihr Echo um Bruchteile
/// eines Hertz und gilt als neuer Ausschnitt.
inline double aufDrahtGerundet(double hz)
{
    return std::round(hz);
}

/// Der Ausschnitt, den die Maschine fuer dieses Fenster bekommen soll.
/// Die Mitte ist die des Fensters; die Breite die des Fensters,
/// geklemmt auf das, was die Maschine kann. Der untere Rand bleibt
/// ueber 0 Hz (dieselbe Klemme wie AetherSDR,
/// RadioModel.cpp:5498 [@d58e2b8a]).
inline Ausschnitt wunschFuerFenster(double fensterMitteHz,
                                    double fensterBreiteHz,
                                    double mindestHz = kMindestBreiteHz,
                                    double hoechstHz = kHoechstBreiteHz)
{
    if (!(fensterMitteHz > 0.0) || !(fensterBreiteHz > 0.0)) { return {}; }
    if (!(mindestHz > 0.0))       { mindestHz = kMindestBreiteHz; }
    if (!(hoechstHz >= mindestHz)) { hoechstHz = kHoechstBreiteHz; }
    Ausschnitt a;
    a.breiteHz = aufDrahtGerundet(std::clamp(fensterBreiteHz, mindestHz, hoechstHz));
    a.mitteHz  = aufDrahtGerundet(std::max(fensterMitteHz, a.breiteHz * 0.5));
    return a;
}

/// Muss die Maschine einen neuen Ausschnitt bekommen?
///
/// Nur wenn sich die BREITE wirklich unterscheidet. Eine reine
/// Mittenverschiebung ist Sache des Nachzugs (MainWindow_Flex.cpp,
/// FlexFensterZug.h) und wird hier nicht ein zweites Mal geschickt.
/// Solange die Maschine noch keinen Ausschnitt gemeldet hat, wird nichts
/// geschickt — dann gibt es nichts, wogegen man vergleichen koennte.
inline bool zugNoetig(double wunschBreiteHz, double panBreiteHz)
{
    if (!(wunschBreiteHz > 0.0) || !(panBreiteHz > 0.0)) { return false; }
    return std::abs(wunschBreiteHz - panBreiteHz) > panBreiteHz * kToleranzAnteil;
}

/// Kommt diese Ausschnittsmeldung aus UNSEREM Zoomzug? Dann bleibt das
/// Fenster genau, wie es ist: es war ja die Quelle. (Bis auf Rundung
/// auf das Hertz dieselbe Breite, die wir geschickt haben.)
///
/// \param gemeldeteBreiteHz Breite der Meldung
/// \param zugBreiteHz       Breite unseres offenen Zoomzugs (0 = keiner)
/// \param eigenerZug        stammt die Meldung von uns, nicht von der Maschine?
inline bool fensterBleibt(double gemeldeteBreiteHz, double zugBreiteHz,
                          bool eigenerZug)
{
    if (!eigenerZug || !(zugBreiteHz > 0.0)) { return false; }
    return std::abs(gemeldeteBreiteHz - zugBreiteHz) <= 1.0;
}

/// Bis wohin darf der Betreiber das Fenster aufzoomen?
///
/// Bisher war die Decke die Abtastrate des Bildes — bei der Flex die
/// Breite ihres Panadapters. Folgt der Panadapter dem Zoom, waere das
/// eine Falle: nach dem Hineinzoomen auf 10 kHz waere die Decke 10 kHz,
/// und herauszoomen ginge nicht mehr. Die Decke ist darum, was die
/// Maschine kann.
inline double zoomDecke(double maschinenHoechstHz)
{
    return maschinenHoechstHz > 0.0 ? maschinenHoechstHz : kHoechstBreiteHz;
}

} // namespace FlexZoomZug
} // namespace Longpath
