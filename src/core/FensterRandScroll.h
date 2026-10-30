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

} // namespace FensterRandScroll
} // namespace Longpath
