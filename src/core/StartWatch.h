// no-port-check: Longpath-original file. Thetis hat nichts dergleichen;
// hier ist nichts portiert.

#pragma once

// Der Startwaechter: merkt, wenn Longpath zweimal hintereinander beim
// Hochfahren steckenbleibt, und faehrt beim dritten Mal ohne die
// gespeicherte Anordnung hoch.
//
// Warum das noetig ist: eine kaputte Einstellung kann das Programm
// unstartbar machen -- ein Applet-Layout, das auf etwas zeigt, das es
// nicht mehr gibt, ein Geraet, das beim Autoconnect haengt. Und dann
// kommt man an die Einstellungen nicht mehr heran, um es zu reparieren,
// weil man dafuer das Programm braeuchte. Dieselbe Klemme wie bei einer
// unsinnigen Darstellungsgroesse (siehe UiScale.h), nur schlimmer.
//
// Wie es funktioniert:
//
//   1. `beginStart()` schreibt eine Marke, BEVOR irgendetwas geladen
//      wird. Sofort auf die Platte -- ein Absturz raeumt nichts mehr
//      auf, und genau darauf beruht das Verfahren.
//   2. Steht die Oberflaeche `kSettleSeconds` lang, ruft das Programm
//      `markRunning()`. Die Marke verschwindet, der Zaehler faellt auf
//      null.
//   3. Findet `beginStart()` die Marke noch vor, ist der vorige Start
//      nicht durchgekommen. Der Zaehler steigt.
//   4. Ab `kSafeThreshold` faehrt Longpath ohne die gespeicherte
//      Anordnung hoch und sagt, warum.
//
// Die Marke ist eine EIGENE kleine Datei, nicht ein Eintrag in den
// Einstellungen. Die Einstellungen werden beim Beenden geschrieben --
// ein Absturz ueberspringt das, und dann stuende dort nie etwas. Eine
// Datei, die beim Start geschrieben und beim Gelingen geloescht wird,
// ueberlebt genau das.
//
// "Durchgekommen" heisst Laufzeit, nicht sauberes Beenden. Wer das
// Programm hart abschiesst, hat keinen Startfehler -- mit dem sauberen
// Beenden als Maßstab zaehlte jedes `kill` als Absturz.

#include <QString>

namespace Longpath::StartWatch {

/// Wie lange die Oberflaeche stehen muss, damit der Start als gelungen
/// gilt. Ein Startfehler zeigt sich in den ersten Sekunden; wer so
/// lange laeuft, ist durch.
inline constexpr int kSettleSeconds = 20;

/// Ab wievielen steckengebliebenen Starts in Folge sicher hochgefahren
/// wird. Zwei, nicht einer: ein einzelner Absturz kann alles Moegliche
/// sein (Stromausfall, ein Absturz beim Beenden, ein hartes Abschiessen
/// kurz nach dem Start). Erst die Wiederholung deutet auf etwas
/// Gespeichertes.
inline constexpr int kSafeThreshold = 2;

/// Der Weg zur Marke. Liegt neben den Einstellungen.
QString markerPath();

/// Am Anfang des Starts aufrufen, vor allem Laden. Liefert, wieviele
/// Starts in Folge nicht durchgekommen sind -- diesen eingerechnet
/// noch nicht, also 0 beim ersten Versuch nach einem gelungenen Start.
int beginStart();

/// Aufrufen, sobald die Oberflaeche `kSettleSeconds` steht. Loescht die
/// Marke und setzt den Zaehler zurueck.
void markRunning();

/// Wieviele Starts in Folge nicht durchkamen, ohne etwas zu aendern.
int failedStarts();

/// Soll dieser Start ohne die gespeicherte Anordnung hochfahren?
bool shouldStartSafely();

/// Marke und Zaehler loeschen. Fuer den Pruefstand und fuer den Fall,
/// dass der Benutzer sagt "alles gut".
void reset();

/// Der Text, den der Benutzer nach einem sicheren Start sieht.
QString safeStartNotice(int failedCount);

} // namespace Longpath::StartWatch
