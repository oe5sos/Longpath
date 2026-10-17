#pragma once

// Eine Logzeile, viele Faeden — die Schreibstelle gehoert an EINEN Ort.
//
// qInstallMessageHandler ruft den Umleiter aus JEDEM Faden auf, und Longpath
// hat ihrer mindestens fuenf (Oberflaeche, Verbindung, Ton/DSP, Spektrum,
// Sendepumpe). Bis zum 2026-10-03 legte jeder Aufruf in main.cpp einen
// eigenen QTextStream auf DIESELBE QFile und schrieb an der gemeinsamen
// Schreibmarke — zwei gleichzeitige Meldungen landen dann uebereinander.
//
// Das ist kein theoretisches Risiko, es stand in den Betriebslogs. Sechs
// Zeilen in drei Dateien vom 2026-10-03, alle mit derselben Handschrift:
// vorne abgeschnitten, hinten ganz.
//
//     TERACTIVE QoS                                    (2x)
//     io() — sending discovery broadcast (bench-...)   (2x)
//     27] INF: DSP thread joined default output workgroup
//     rom MAC: "..." (override via PGXL_FlexRadioSerial key)
//
// Ausgerechnet zwei davon sind die QoS-Meldung, also genau die Zeile, der an
// dem Tag jemand nachgegangen ist. Eine zerschriebene Zeile ist schlimmer als
// eine fehlende: die fehlende vermisst man, die zerschriebene deutet man.
//
// Hier liegt sie, weil sie damit pruefbar wird: `main.cpp` ist nicht Teil der
// Pruefstaende, dieses Modul schon (tests/tst_log_schreibt_atomar.cpp laesst
// acht Faeden gleichzeitig schreiben und zaehlt nach).
//
// Zum Vorbehalt "nie ein Schloss im Tonrueckruf" (CLAUDE.md): an dieser
// Stelle wird ohnehin in eine Datei geschrieben und geleert, und das ist um
// Groessenordnungen teurer als ein unbestrittenes QMutex. Das Schloss macht
// den Pfad nicht langsamer, es macht ihn richtig. Wer Logausgaben aus dem
// Tonrueckruf ganz vermeiden will, muss die Ausgaben loswerden, nicht das
// Schloss.

#include <QString>

class QFile;

namespace Longpath::Log {

/** Legt fest, wohin geschrieben wird. `nullptr` = nur auf den Bildschirm.
 *
 *  Die Datei gehoert weiterhin dem Aufrufer (main.cpp oeffnet, schliesst und
 *  loescht sie). Vor dem Schliessen `setzeDatei(nullptr)` rufen, sonst
 *  schreibt ein spaeter Faden in eine geschlossene Datei.
 */
void setzeDatei(QFile* datei);

/** Schreibt eine FERTIGE Zeile (mit Zeilenende) in Datei und auf den
 *  Bildschirm — unter einem Schloss und mit je einem einzigen Schreibaufruf.
 */
void schreibe(const QString& zeile);

}  // namespace Longpath::Log
