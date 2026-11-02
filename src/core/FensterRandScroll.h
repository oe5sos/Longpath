#pragma once

// =================================================================
// src/core/FensterRandScroll.h  (Longpath)
// =================================================================
//
// no-port-check: hier steht KEINE portierte Logik. Die Thetis-Regel
// selbst (console.cs:31371-31385, samt ihrer //-W2PA-Zeile) liegt
// unveraendert in SpectrumWidget::setVfoFrequency und ist dort
// attribuiert; dieser Kopf nennt sie nur als Herkunft des Verhaltens
// und rechnet eine FOLGE davon aus. Die Nennung des Dateinamens ist
// der Grund fuer diesen Vermerk.
//
// Woher die 80 kHz kamen — gerechnet, nicht geraten.
//
// Im herkoemmlichen Betrieb (CTUN aus) scrollt das Fenster mit, sobald
// der VFO in den Randstreifen laeuft. Die Regel stammt aus Thetis
// (console.cs:31371-31385, Zeile `//-W2PA` dort verbatim erhalten) und
// steht unveraendert in SpectrumWidget::setVfoFrequency: die neue Mitte
// wird so gelegt, dass der VFO genau auf dem Rand des Randstreifens
// sitzt.
//
// Die neue Mitte liegt damit NICHT auf dem VFO, sondern um
//
//     Breite/2 − Breite·Randanteil
//
// daneben. Bei 200 kHz Ausschnitt und zehn Prozent Rand sind das
//
//     100 kHz − 20 kHz = 80 kHz
//
// — genau der Versatz, der beim Verbinden mit der FLEX-8400 seit dem
// 2026-10-29 vier Mal gemessen wurde (7.179→7.259, 7.3086→7.3886,
// 7.197→7.277, 7.074→7.154). Konstant, und unabhaengig von der
// Frequenz, weil er nur an der Breite haengt.
//
// Am 2026-10-30 nachgerechnet am Protokoll des laufenden Betriebs:
// das Fenster stand auf 7.175 MHz (200 kHz), die Maschine setzte die
// Scheibe auf 7.074 MHz, 7.074 < 7.075 + 0.020 — also scrollte das
// Fenster, und die neue Mitte war 7.074 + 0.100 − 0.020 = 7.154 MHz.
//
// Der Fehler ist nicht das Scrollen. Der Fehler ist, dass diese
// Mittenaenderung danach als ABSTIMMBEFEHL in die Scheibe
// zurueckgeschrieben wurde (MainWindow, `centerChanged` ohne CTUN):
// damit zieht das Fenster den VFO um 80 kHz hinter sich her, obwohl
// der VFO schon dort stand, wo er hingehoert. Dieselbe Klasse wie
// „eigene Bewegung ist kein Ziehen" (2026-10-20).
//
// An einem OpenHPSDR-Geraet fiel es nicht auf, weil dort CTUN im
// Regelfall EIN ist und der Zweig gar nicht laeuft. Der Flex-Weg
// fuehrt ihn mit CTUN aus.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
//   2026-11-02 — mitteIstDdcBefehl: dieselbe Unterscheidung fuer den
//                 CTUN-Zweig, wo die Mitte die DDC bewegt.
// =================================================================

namespace Longpath {
namespace FensterRandScroll {

/// Der Randstreifen, in dem das Fenster mitscrollt — zehn Prozent der
/// Breite, wie in Thetis.
inline constexpr double kRandAnteil = 0.10;

/// Wie weit liegt die neue Fenstermitte neben dem VFO, nachdem das
/// Fenster am Rand mitgescrollt ist?
///
/// Das ist der gesuchte Versatz: bei 200 kHz und zehn Prozent Rand
/// genau 80 kHz.
inline double mitteNebenVfoHz(double breiteHz,
                              double randAnteil = kRandAnteil)
{
    if (!(breiteHz > 0.0)) { return 0.0; }
    return breiteHz * 0.5 - breiteHz * randAnteil;
}

/// Darf eine Mittenaenderung des Fensters als Abstimmbefehl in die
/// Scheibe geschrieben werden?
///
/// \param ctun        true, wenn CTUN laeuft (dann ist die Mitte
///                    ohnehin nicht die Frequenz)
/// \param folgtDemVfo true, wenn das Fenster nur mitgescrollt ist,
///                    WEIL der VFO sich bewegt hat
///
/// Nur ein Zug des Bedienenden ist ein Befehl. Das Mitscrollen ist die
/// Folge einer Abstimmung, nicht ihre Ursache — es zurueckzuschreiben
/// ist eine Rueckkopplung.
inline bool mitteIstAbstimmbefehl(bool ctun, bool folgtDemVfo)
{
    if (ctun)        { return false; }
    if (folgtDemVfo) { return false; }
    return true;
}

/// Darf eine Mittenaenderung des Fensters die DDC verstellen?
///
/// Die Schwesterfrage zu der darueber, fuer den CTUN-Betrieb. Dort ist
/// die Mitte nie ein Abstimmbefehl (siehe oben), aber ein ZUG am
/// Panadapter ist sehr wohl die Bitte, die DDC zu verschieben -- das
/// ist, was CTUN ausmacht.
///
/// \param folgtDemVfo true, wenn das Fenster sich SELBST auf den VFO
///                    gestellt hat (Zoomen, recenterOnVfo, Randscroll)
///
/// Gefunden am 2026-11-02: drei Stellen stellen das Fenster von selbst
/// auf den VFO und melden eine neue Mitte -- Zoomziehen am
/// Frequenzmassstab, Zoomen mit Strg/Cmd+Rad und `recenterOnVfo()`
/// (SpectrumWidget; die Aufzaehlung steht seit dem 2026-10-27 in
/// PanadapterApplet.cpp). In CTUN nahm MainWindow jede davon fuer
/// einen Zug und verstellte die DDC auf den VFO -- das Gegenteil
/// dessen, was CTUN verspricht: die DDC steht fest, der VFO wandert
/// darin. Ein zweiter Slice im selben Fenster wurde mitgezogen.
///
/// Die vierte Stelle, das Randscrollen, laeuft in CTUN gar nicht
/// (setVfoFrequency prueft `!m_ctunEnabled`) -- und gerade deshalb fiel
/// es nicht auf: die eine Stelle, die seit dem 2026-10-30 eine Fahne
/// traegt, ist die eine, die in CTUN nie feuert.
inline bool mitteIstDdcBefehl(bool folgtDemVfo)
{
    return !folgtDemVfo;
}

} // namespace FensterRandScroll
} // namespace Longpath
