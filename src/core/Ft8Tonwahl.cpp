// =================================================================
// src/core/Ft8Tonwahl.cpp  (Longpath)
// =================================================================
//
// Siehe Ft8Tonwahl.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "Ft8Tonwahl.h"

#include <algorithm>
#include <cstdlib>

namespace Longpath {

bool stelleBelegt(const QVector<int>& belegt, int hz, int abstand)
{
    for (const int b : belegt) {
        if (std::abs(b - hz) < abstand) { return true; }
    }
    return false;
}

int freieTonlage(const QVector<int>& belegt, int von, int bis,
                 int abstand, int wunsch)
{
    if (bis <= von || abstand <= 0) { return wunsch; }

    // Der Wunsch gewinnt, wenn er im Bereich liegt und frei ist.
    if (wunsch >= von && wunsch <= bis
        && !stelleBelegt(belegt, wunsch, abstand)) {
        return wunsch;
    }

    // Von der gewuenschten Stelle aus nach beiden Seiten gehen und die
    // erste freie nehmen. Schrittweite 10 Hz: feiner waere Zierrat
    // (ein Signal ist 50 Hz breit), groeber liesze Luecken aus.
    constexpr int kSchritt = 10;
    const int start = std::clamp(wunsch, von, bis);
    const int weite = std::max(start - von, bis - start);

    for (int d = 0; d <= weite; d += kSchritt) {
        // Erst nach unten, dann nach oben: bei gleichem Abstand ist
        // die tiefere Stelle die ruhigere (weniger Nachbarn oberhalb
        // im ueblichen FT8-Gedraenge).
        for (const int hz : {start - d, start + d}) {
            if (hz < von || hz > bis) { continue; }
            if (!stelleBelegt(belegt, hz, abstand)) { return hz; }
            if (d == 0) { break; }   // start nur einmal pruefen
        }
    }

    // Alles belegt. Dann lieber nichts aendern, als eine Stelle
    // auszugeben, die genauso belegt ist -- der Aufrufer sagt dem
    // Betreiber, dass nichts frei war.
    return wunsch;
}

} // namespace Longpath
