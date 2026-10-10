// no-port-check: Longpath-original file. Thetis skaliert seine
// Oberflaeche ueber WinForms-AutoScale, das hat mit diesem Weg nichts
// zu tun -- hier ist nichts portiert.

#pragma once

// Die Darstellungsgroesse der Oberflaeche.
//
// Qt liest `QT_SCALE_FACTOR` genau einmal: im QApplication-Konstruktor.
// Darum liest `main.cpp` die Einstellung `UiScalePercent` VOR
// QApplication aus der Datei und setzt die Umgebungsvariable daraus --
// und darum greift jede Aenderung erst beim naechsten Start. Eine
// laufende Oberflaeche liesse sich nicht umskalieren, ohne jedes
// Fenster neu aufzubauen.
//
// Diese Datei haelt die Regeln dazu an einer Stelle, statt sie im
// Hauptfenster zu vergraben: die erlaubten Stufen, die Grenzen und das
// Schreiben. So laesst sich das Verhalten pruefen, ohne ein ganzes
// MainWindow zu bauen.

#include <QList>
#include <QString>

namespace Longpath::UiScale {

/// Der Schluessel in den Einstellungen. Dieselbe Zeichenkette liest
/// `main.cpp` von Hand aus der XML-Datei, noch bevor es AppSettings
/// gibt -- wer sie hier aendert, muss sie dort mitaendern.
inline constexpr char kSettingsKey[] = "UiScalePercent";

/// Die angebotenen Stufen in Prozent.
///
/// 75 bis 200 war schon vorgesehen; 115 und 130 sind dazugekommen, weil
/// der Sprung von 100 auf 125 gross ist und der Bedarf genau dazwischen
/// liegt.
const QList<int>& steps();

/// Ist `percent` ein brauchbarer Wert? Die Grenzen sind weit (50..300),
/// damit ein von Hand eingetragener Zwischenwert gilt -- aber nicht so
/// weit, dass eine vertippte Zahl die Oberflaeche unbedienbar macht.
bool isValid(int percent);

/// Die gemerkte Groesse, oder 100. Ein unsinniger Wert in der Datei
/// liefert ebenfalls 100: eine Oberflaeche bei 10 % waere nicht mehr zu
/// treffen, und der Benutzer kaeme an das Menue nicht mehr heran, um es
/// zurueckzustellen.
int current();

/// Merkt sich `percent` und schreibt die Einstellungen sofort auf die
/// Platte. Sofort, weil `main.cpp` beim naechsten Start die DATEI liest,
/// nicht den Speicher -- ohne das waere die Einstellung weg, wenn das
/// Programm nicht sauber beendet wird. Liefert false, wenn `percent`
/// nicht gilt; dann bleibt alles, wie es war.
bool store(int percent);

/// Der Text fuer die Rueckmeldung nach dem Umstellen.
QString restartHint(int percent);

} // namespace Longpath::UiScale
