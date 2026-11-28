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
// LOCATOR STATT RUFZEICHEN (2026-11-27): steht statt eines Rufzeichens ein
// Locator da (4 oder 6 Zeichen, isValidGridSquare -- z. B. "PM95" oder
// "PM95tq"), gilt GENAU dieser Ort, nicht die Mitte eines Landes. Das ist der
// genaue Weg fuer jemanden, der den Locator der Gegenstation kennt (FT8, QRZ,
// Contest-Austausch). Ein Rufzeichen hat nie die Form zwei Buchstaben + zwei
// Ziffern (+ zwei Buchstaben); die Unterscheidung ist darum eindeutig.
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
    QString land;           ///< Name der DXCC-Einheit aus cty.dat (leer bei Locator)
    bool    ausLocator{false};  ///< true: genauer Ort aus dem Locator, nicht Landesmitte
};

/// Ist `text` ein Locator (4 oder 6 Zeichen) und kein Rufzeichen?
bool istLocator(const QString& text);

/// `rufzeichen` darf auch ein Locator sein (siehe oben).
/// `eigenerLocator` ist der Stationslocator (AppSettings StationGridSquare).
Ergebnis rechne(const CtyDatParser& cty, const QString& rufzeichen,
                const QString& eigenerLocator);

}  // namespace RufPeilung
}  // namespace Longpath
