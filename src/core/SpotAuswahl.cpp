#include "core/SpotAuswahl.h"

#include <algorithm>
#include <cmath>

namespace Longpath {
namespace SpotAuswahl {

QVector<Zeile> sichtbare(const QMap<int, SpotData>& spots, qint64 mitteHz,
                         int spanneHz, qint64 jetztMs, int hoechstens)
{
    QVector<Zeile> aus;
    if (hoechstens <= 0) { return aus; }

    const qint64 spanne = (spanneHz > 0) ? spanneHz : kSpanneVorgabeHz;
    const qint64 halb   = spanne / 2;
    const qint64 von    = mitteHz - halb;
    const qint64 bis    = mitteHz + halb;

    // Erst sammeln, was ins Bild gehört — mit dem Abstand zur Mitte, weil
    // danach gekürzt wird.
    struct MitAbstand { Zeile z; qint64 abstand; };
    QVector<MitAbstand> gefunden;
    gefunden.reserve(spots.size());

    for (auto it = spots.cbegin(); it != spots.cend(); ++it) {
        const SpotData& s = it.value();
        if (s.callsign.trimmed().isEmpty()) { continue; }

        // Empfangsfrequenz ist die, auf der man hört. Fehlt sie, bleibt die
        // Sendefrequenz -- bei Split ist das nicht dasselbe, aber ein Spot
        // ohne jede Frequenz ist gar keiner.
        const double mhz = (s.rxFreqMhz > 0.0) ? s.rxFreqMhz : s.txFreqMhz;
        if (mhz <= 0.0) { continue; }
        const qint64 hz = static_cast<qint64>(std::llround(mhz * 1e6));
        if (hz < von || hz > bis) { continue; }

        // Abgelaufene fallen weg. Der Kehrbesen im SpotModel laeuft nur alle
        // 30 s; ohne diese Pruefung stuende ein toter Spot auf dem Telefon
        // laenger als am Pult.
        const qint64 gesehen = (s.lastSeenMs > 0) ? s.lastSeenMs : s.addedMs;
        if (gesehen > 0 && s.lifetimeSeconds > 0) {
            const qint64 endeMs = gesehen
                + static_cast<qint64>(s.lifetimeSeconds) * 1000;
            if (endeMs < jetztMs) { continue; }
        }

        Zeile z;
        z.index  = it.key();
        z.hz     = hz;
        z.ruf    = s.callsign.trimmed().toUpper();
        z.mode   = s.mode.trimmed().toUpper();
        z.quelle = s.source.trimmed().toUpper();
        z.alterSek = (gesehen > 0 && jetztMs > gesehen)
                         ? static_cast<int>((jetztMs - gesehen) / 1000)
                         : 0;
        gefunden.append({z, std::llabs(hz - mitteHz)});
    }

    // Kuerzen: die nahe der Mitte bleiben. Stabil, damit zwei Spots mit
    // demselben Abstand nicht bei jeder Abfrage die Plaetze tauschen.
    if (gefunden.size() > hoechstens) {
        std::stable_sort(gefunden.begin(), gefunden.end(),
                         [](const MitAbstand& a, const MitAbstand& b) {
            return a.abstand < b.abstand;
        });
        gefunden.resize(hoechstens);
    }

    // Ausgegeben wird nach Frequenz -- so steht die Liste in derselben
    // Reihenfolge wie das Bild, und ein Spot wandert nicht durch die Liste,
    // nur weil sich die Mitte verschoben hat.
    std::stable_sort(gefunden.begin(), gefunden.end(),
                     [](const MitAbstand& a, const MitAbstand& b) {
        return a.z.hz < b.z.hz;
    });

    aus.reserve(gefunden.size());
    for (const MitAbstand& m : gefunden) { aus.append(m.z); }
    return aus;
}

}  // namespace SpotAuswahl
}  // namespace Longpath
