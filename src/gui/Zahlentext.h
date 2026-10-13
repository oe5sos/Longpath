// =================================================================
// src/gui/Zahlentext.h  (Longpath)
// =================================================================
// Longpath-eigen, kein Thetis-Bezug.
//
// Modification history (Longpath):
//   2026-10-13 — Angelegt, AI-assisted via Anthropic Claude Code.
//                 Anlass: der Betreiber hat am 2026-10-09 notiert, dass
//                 EIN Feld zwei Trennzeichen zeigt -- der Balken
//                 schreibt "1,3", die Beschriftung daneben "47.5V".
// =================================================================

#pragma once

#include <QLatin1Char>
#include <QLocale>
#include <QString>

namespace Longpath {

/// Eine Zahl, die der Betreiber LIEST.
///
/// Nimmt das Trennzeichen des Systems -- im deutschen Longpath also das
/// Komma. Ohne Tausenderpunkte: eine Frequenz als "1.296,000000" ist in
/// einer Zeile mit Komma-Dezimaltrennung nicht mehr eindeutig zu lesen,
/// und bei QLocale ist die Gruppierung standardmaessig AN. Genau dieses
/// Detail haette der Durchgang sonst eingeschleppt.
///
/// Warum es diese Funktion gibt: bis zum 2026-10-13 standen in der
/// Oberflaeche zwei Schreibweisen nebeneinander. HGauge und der
/// FreeDV-Melder nahmen QLocale::system() (Komma), vierzehn von Hand
/// gesetzte Beschriftungen in den Applets nahmen QString::arg(double),
/// und das ist IMMER ein Punkt -- QString::arg kennt keine Locale. Ein
/// Feld konnte darum beides zugleich zeigen.
inline QString anzeigeZahl(double wert, int stellen)
{
    QLocale l = QLocale::system();
    l.setNumberOptions(QLocale::OmitGroupSeparator);
    return l.toString(wert, 'f', stellen);
}

/// Eine Zahl, die in eine DATEI oder auf die Leitung geht.
///
/// Immer mit Punkt und ohne Gruppierung, unabhaengig vom System. Fuer
/// ADIF-Felder, Beschreibungsdateien, Protokolle, Wireformate.
///
/// Diese Funktion ist der Grund, warum die andere einen so sperrigen
/// Namen hat: die Unterscheidung muss an der AUFRUFSTELLE sichtbar
/// sein. Ein `QString::number(f, 'f', 6)` sieht harmlos aus, und
/// niemand sieht daran, ob die Zahl gelesen oder gespeichert wird --
/// bis jemand sie "der Einheitlichkeit wegen" auf Komma umstellt und
/// damit eine Datei verdirbt, die ein anderes Programm liest.
inline QString datenZahl(double wert, int stellen)
{
    return QString::number(wert, 'f', stellen);
}

} // namespace Longpath
