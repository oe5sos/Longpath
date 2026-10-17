#pragma once

// Die Logbuchdatei — EIN Schreiber, eine Stelle.
//
// Bis hierher kannte nur `RotorLogbookPanel` den Weg zur Datei und hängte
// Einträge selbst an. Das war richtig, solange das Pult der einzige Ort war,
// an dem geloggt wird. Mit dem QSO-Eintrag vom Telefon (TCI `log_qso:`) gibt
// es einen zweiten Anlass — und zwei Stellen, die dieselbe Datei anfassen,
// sind genau die Lage, die am 2026-10-03 die Logzeilen zerschrieben hat.
// Dort ging es um Diagnoseausgaben; hier ginge es um Martins Kontakte.
//
// Also: beide gehen durch diese eine Stelle. Sie ist bewusst klein — Pfad
// und Anhängen, mehr nicht. Lesen, Zusammenführen und Schreiben der ganzen
// Datei bleiben, wo sie sind (`AdifLog`, `LogbookWindow`).
//
// Nicht hier und mit Absicht: Löschen oder Ändern. Wer einen Kontakt
// korrigieren will, tut das am Pult, wo er ihn sieht.

#include <QString>

namespace Longpath {

struct LogEntry;

namespace LogbookDatei {

/// `<dataDir>/logbook.adi`. Legt den Ordner an, wenn er fehlt.
QString pfad();

/// Hängt einen Eintrag an. Gibt es die Datei noch nicht, bekommt sie zuerst
/// einen ADIF-Kopf — strenge Importeure lehnen eine Datei ab, deren erstes
/// Zeichen ein Datensatz statt eines mit `<EOH>` beendeten Kopfes ist.
///
/// Rückgabe false mit Grund in `fehler`, wenn die Datei nicht zu öffnen war.
bool anhaengen(const LogEntry& eintrag, QString* fehler = nullptr);

}  // namespace LogbookDatei
}  // namespace Longpath
