#pragma once

// Eine Peilung vom Netz annehmen — oder ablehnen.
//
// Longpath-original, kopflastig fuer vier Zeilen Rechnung. Der Grund steht
// am anderen Ende der Leitung: ein Mast mit einer Antenne darauf, der sich
// auf Zuruf dreht. Was hier durchrutscht, dreht echtes Metall.
//
// Darum nimmt diese Stelle NICHT einfach `toDouble()`:
//
//   * `toDouble()` macht aus "" eine 0 und aus "abc" eine 0. Null Grad ist
//     Nord — eine leere Zeile wuerde die Antenne nach Norden drehen.
//   * NaN und Unendlich rutschen durch jeden Bereichsvergleich: `nan < 0`
//     ist falsch, `nan > 360` ist auch falsch.
//   * 360 ist dieselbe Richtung wie 0 und muss angenommen werden; 361 ist
//     ein Tippfehler und darf es nicht.
//
// Negative Werte und Werte ueber 360 werden ABGELEHNT statt umgerechnet.
// Ein Umrechnen waere bequem und falsch: wer -90 schickt, hat sich vertan,
// und 270 Grad sind eine andere Antwort als "ich habe mich vertan".

#include <QString>

#include <cmath>

namespace Longpath {
namespace RotorPeilung {

/// Groesster erlaubter Wert. 360 gilt und bedeutet Nord.
constexpr double kHoechstens = 360.0;

/// Liest eine Peilung aus dem Befehlstext.
///
/// Gibt `false` zurueck, wenn nichts Brauchbares dasteht; `grad` bleibt dann
/// unberuehrt. 360 wird auf 0 gelegt, weil der Rotor dieselbe Richtung
/// meint und manche Steuerungen 360 nicht annehmen.
inline bool lies(const QString& text, double* grad)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) { return false; }
    bool ok = false;
    const double w = t.toDouble(&ok);
    if (!ok) { return false; }
    // Reihenfolge wichtig: zuerst auf endlich pruefen. NaN besteht jeden
    // Bereichsvergleich, weil jeder Vergleich mit NaN falsch ist.
    if (!std::isfinite(w)) { return false; }
    if (w < 0.0 || w > kHoechstens) { return false; }
    if (grad) { *grad = (w == kHoechstens) ? 0.0 : w; }
    return true;
}

}  // namespace RotorPeilung
}  // namespace Longpath
