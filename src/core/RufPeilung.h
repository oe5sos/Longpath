#pragma once

// Welche Richtung zu einem Rufzeichen — ohne Netz, ohne Fenster.
//
// Fuer die App (TCI `rotor_peilung:<ruf>;`, 2026-11-26). Gerechnet wird genau
// so wie im Rotor-Feld am Mac, bevor QRZ antwortet
// (RotorLogbookPanel::onCallsignEdited): Prefix -> DXCC-Einheit aus cty.dat,
// deren Mitte als Locator, Peilung und Entfernung vom eigenen Locator aus.
// Dieselbe Rechnung heisst: App und Mac zeigen dieselbe Zahl.
//
// Das ist eine SCHAETZUNG auf die Mitte des Landes. Fuer kleine Laender ist
// sie gut, fuer die USA oder Russland kann sie zehn Grad und mehr daneben
// liegen -- der genaue Locator kommt am Mac erst ueber QRZ. Darum steht das
// Land in der Antwort: wer "United States" liest, weiss, wie grob es ist.
//
// Rein: diese Datei kennt weder TCI noch den Rotor. Ob ein Weg im erlaubten
// Drehbereich liegt, entscheidet der Aufrufer (RotorController::zielErlaubt).

#include <QString>

namespace Longpath {

class CtyDatParser;

namespace RufPeilung {

struct Ergebnis {
    bool    ok{false};
    QString grund;          ///< bei !ok: kurz, fuer die Gegenseite lesbar
    double  kurzGrad{0.0};  ///< kurzer Weg, 0..360
    double  langGrad{0.0};  ///< langer Weg = kurz + 180
    double  km{0.0};
    QString land;           ///< Name der DXCC-Einheit aus cty.dat
};

/// `eigenerLocator` ist der Stationslocator (AppSettings StationGridSquare).
Ergebnis rechne(const CtyDatParser& cty, const QString& rufzeichen,
                const QString& eigenerLocator);

}  // namespace RufPeilung
}  // namespace Longpath
