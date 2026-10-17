#pragma once

// Welche Spots gehören in ein Bild — und welche nicht.
//
// Longpath sammelt Spots aus Cluster, RBN, POTA und SpotCollector in EINEM
// `SpotModel`. Auf dem Pult filtert der Spot-Hub-Dialog danach, was angezeigt
// wird; das Modell selbst enthält alles. Für die App ist genau das die Gefahr:
// RBN allein liefert in einer guten Stunde Hunderte Meldungen, und wer die
// über die Steuerleitung schiebt, blockiert damit jede Bedienung dahinter.
//
// Darum wird hier NICHT nach Quelle oder Land gefiltert, sondern nach dem
// einzigen Kriterium, das auf einem Telefon zählt: **passt der Spot ins Bild?**
// Was außerhalb des sichtbaren Ausschnitts liegt, kann die Seite ohnehin nicht
// zeichnen. Der Filter ist damit zugleich der Flutschutz, und er braucht keine
// zweite Einstellung, die mit der ersten auseinanderlaufen kann.
//
// Rein: diese Datei kennt weder TCI noch Fenster. Sie bekommt die Spots, die
// Mitte, die Spanne und die Zeit — und gibt zurück, was zu zeichnen ist.

#include "models/SpotModel.h"

#include <QMap>
#include <QString>
#include <QVector>

namespace Longpath {
namespace SpotAuswahl {

/// Ein Spot, so wie die App ihn braucht — und nicht mehr.
struct Zeile {
    int     index{-1};      ///< der Index aus dem SpotModel
    qint64  hz{0};          ///< Empfangsfrequenz
    QString ruf;
    QString mode;
    QString quelle;         ///< "CLUSTER", "RBN", "POTA", …
    int     alterSek{0};    ///< seit der letzten Meldung — zum Verblassen
};

/// Fällt zurück, wenn die Spanne unbekannt ist (der Client hat noch kein
/// Spektrum abonniert). 48 kHz ist die Vorgabe der App; eine leere Antwort
/// wäre an der Stelle die schlechtere Lüge — sie sähe aus wie "keine Spots".
constexpr int kSpanneVorgabeHz = 48000;

/// Höchstens so viele Zeilen je Abfrage. 40 passt auf kein Telefon, aber die
/// Grenze steht gegen das Fluten, nicht gegen die Anzeige.
constexpr int kHoechstens = 40;

/// Die Spots im sichtbaren Ausschnitt, nach Frequenz sortiert.
///
/// `mitteHz` ist die Mitte des Bildes, `spanneHz` seine Breite. Abgelaufene
/// Spots fallen weg: der Kehrbesen im SpotModel läuft nur alle 30 s, und ein
/// Spot, der seine Lebenszeit überschritten hat, ist kein Spot mehr — er
/// stünde sonst bis zu einer halben Minute länger im Bild als am Pult.
///
/// Muss gekürzt werden, bleiben die Spots NAHE DER MITTE. Das ist die Stelle,
/// auf die der Bediener gerade abgestimmt hat; die Ränder des Bildes sind
/// das, was man opfert.
QVector<Zeile> sichtbare(const QMap<int, SpotData>& spots, qint64 mitteHz,
                         int spanneHz, qint64 jetztMs,
                         int hoechstens = kHoechstens);

}  // namespace SpotAuswahl
}  // namespace Longpath
