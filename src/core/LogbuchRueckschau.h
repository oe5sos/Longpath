#pragma once

// Rückschau ins Logbuch — die letzten Kontakte, und "hatte ich den schon?"
//
// Das Gegenstück zu `LogbookDatei` (dem einen Schreiber). Gelesen wird hier
// ABSICHTLICH anders als in `AdifLog::read`: der holt die ganze Datei in
// LogEntry-Objekte, samt Entfernung und Peilung je Kontakt. Das ist richtig
// für das Logbuchfenster, das alles zeigen und sortieren muss — und falsch
// für eine Fernbedienung, die zwei Fragen stellt:
//
//   1. "Zeig mir die letzten zehn."      -> nur das Dateiende lesen
//   2. "Hatte ich OE5XYZ schon?"         -> nur die Datensätze lesen,
//                                            in denen das Rufzeichen steht
//
// GEMESSEN an Martins Logbuch (6,6 MB, 9271 Datensätze, 2026-10-04):
//
//     voller Durchlauf              4,1 ms
//     Suche nach einem Rufzeichen   3,8 ms   (3 Treffer)
//     letzte 64 kB                  0,04 ms  (78 Datensätze)
//
// Deshalb steht hier KEIN zweiter Index neben `WorkedBefore`. Ein Index
// müsste gepflegt und bei jedem fremden Schreibzugriff verworfen werden;
// ein Durchlauf von 4 ms kann nicht veralten. (`WorkedBefore` selbst
// gehört `RotorLogbookPanel` — einem Fenster. Der TCI-Server darf nicht
// hineingreifen, sonst hängt der Netzdienst an der Oberfläche.)
//
// Die Byte-Funktionen sind rein: sie bekommen Dateiinhalt, keinen Pfad.
// Nur `letzte()` und `rueckschau()` fassen die Datei an.

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVector>

namespace Longpath {

struct LogEntry;

namespace LogbuchRueckschau {

/// Was über ein Rufzeichen im Logbuch steht.
struct Befund {
    int       anzahl{0};        ///< Datensätze mit genau diesem Rufzeichen
    QDateTime zuletzt;          ///< UTC, ungültig wenn noch nie
    QString   letztesBand;      ///< vom jüngsten Kontakt
    QString   letzterMode;      ///< vom jüngsten Kontakt
    bool      gleichesBand{false};  ///< schon auf dem gefragten Band
    bool      gleicherMode{false};  ///< schon in der gefragten Betriebsart
                                    ///< (auf dem gefragten Band)
    bool kennt() const { return anzahl > 0; }
};

/// Schneidet ein Stück vom Dateiende auf eine Datensatzgrenze zurecht.
///
/// WARUM NÖTIG: ein Schnitt an einer willkürlichen Byte-Stelle beginnt
/// mitten in einem Datensatz. Der ADIF-Parser sieht dann die hintere Hälfte
/// eines Kontakts und macht daraus einen eigenen — ein Rufzeichen, das
/// niemand gearbeitet hat, oder eine Zeit ohne Datum. Lieber einen echten
/// Datensatz zu wenig als einen erfundenen zu viel.
///
/// Enthält das Stück den Dateikopf (`<EOH>`), beginnt die Ausgabe dahinter.
/// Sonst hinter dem ersten `<EOR>`. Findet sich keine Grenze, ist die
/// Ausgabe leer — dann war das Stück ein einziger abgeschnittener Datensatz.
QByteArray abDatensatzGrenze(const QByteArray& stueck);

/// Die Rohbytes aller Datensätze, in denen `nadel` vorkommt (Groß- und
/// Kleinschreibung gleich). Vorsieb: nur diese paar Datensätze müssen
/// durch den vollen Parser, nicht die ganze Datei.
///
/// Siebt absichtlich zu großzügig — ein Rufzeichen in einer Bemerkung
/// kommt mit. Welcher Datensatz wirklich gilt, entscheidet danach das
/// geparste `CALL`-Feld, nicht diese Bytesuche.
QByteArray datensaetzeMit(const QByteArray& inhalt, const QByteArray& nadel);

/// Fasst zusammen, was diese Datensätze über `rufzeichen` sagen.
/// `band` und `mode` dürfen leer sein — dann bleiben die beiden Flaggen
/// falsch, statt auf Verdacht wahr zu werden.
Befund fasseZusammen(const QVector<LogEntry>& treffer, const QString& rufzeichen,
                     const QString& band, const QString& mode);

/// Die letzten `n` Kontakte aus der Logbuchdatei, der jüngste zuerst.
///
/// Liest vom Dateiende her und verdoppelt das Fenster, bis genug
/// Datensätze zusammen sind oder die ganze Datei gelesen ist. Eine fehlende
/// Datei ist kein Fehler, sondern ein leeres Logbuch.
QVector<LogEntry> letzte(int n, const QString& pfad = QString());

/// Rückschau auf ein Rufzeichen, aus der Logbuchdatei.
Befund rueckschau(const QString& rufzeichen, const QString& band,
                  const QString& mode, const QString& pfad = QString());

}  // namespace LogbuchRueckschau
}  // namespace Longpath
