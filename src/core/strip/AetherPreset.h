#pragma once

// =================================================================
// src/core/strip/AetherPreset.h  (Longpath)
// =================================================================
//
// Longpath-original. Liest AetherSDRs Kanalzug-Profile
// ("AetherSDR ChannelStripPreset v1", eine JSON-Datei) in unsere
// Kette.
//
// Warum es das gibt: der Betreiber fuhr denselben Kanalzug unter
// Windows mit AetherSDR und einem Teufel-Headset und hat ihn dort
// fertig eingestellt -- „die einstellungen bei aether perfekt! wir
// müssen unsere grafik verwenden, aber alle einstellungen usw. von
// aether verwenden" (2026-10-13). Seine Datei ist damit nicht eine
// Voreinstellung unter vielen, sondern die GRUNDLAGE.
//
// Das ist nur deshalb ueberhaupt moeglich, weil unsere Stufen aus
// AetherSDR portiert sind: ClientEq, ClientComp, ClientGate,
// ClientDeEss, ClientTube, ClientPudu, ClientReverb und
// ClientFinalLimiter rechnen Zeile fuer Zeile dasselbe. Jeder Wert
// aus seiner Datei bedeutet bei uns dasselbe -- mit genau den
// Ausnahmen, die unten stehen und die der Importeur selbst meldet,
// statt sie zu verschweigen.
//
// Was NICHT abgebildet werden kann, und warum:
//
//   Der rx-Block. Aether traegt eine zweite Kette fuer den EMPFANG
//   mit; Longpath hat auf der Empfangsseite keinen Kanalzug. Wird
//   ueberlesen und gemeldet.
//
//   Aethers Regel „was nicht in der Liste steht, laeuft gar nicht"
//   bilden wir ueber die Stufenschalter ab: eine Stufe, die in
//   seiner Kette fehlt, wird bei uns ausgeschaltet.
//
//   Unsere eigenen Stufen (Clipper, Eingangsautomatik) kennt Aether
//   nicht. Sie werden ausdruecklich abgeschaltet -- ein importiertes
//   Profil darf nichts mitlaufen lassen, das der Betreiber in Aether
//   nie gehoert hat. Dasselbe gilt fuer Werte, die seine Datei nicht
//   traegt (Kompressor-Drive, Phasendreher, Auto-Ausgleich): sie
//   werden auf neutral gesetzt, nicht geerbt. Sonst klaenge dieselbe
//   Datei auf zwei Rechnern verschieden.
//
// Der Hauptschalter der Kette bleibt unberuehrt. Ein Profil
// entscheidet, WIE die Kette klingt, nicht OB sie im Sendeweg steht
// -- dieselbe Regel wie bei StripSettings::applyBuiltIn().
//
// =================================================================
// Modification history (Longpath):
//   2026-10-13 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QString>
#include <QStringList>

class QJsonObject;

namespace Longpath {

class StripChain;

namespace AetherPreset {

// Das Etikett, das Aether in jede Datei schreibt. Aether selbst
// prueft es nie (ChannelStripPresets.cpp:341-394 nimmt jedes
// JSON-Objekt an) -- wir schon, sonst schluckt der Importeur auch
// eine Datei, die gar kein Kanalzugprofil ist.
inline constexpr char kSchema[] = "AetherSDR ChannelStripPreset v1";

struct Ergebnis {
    bool        ok{false};
    QString     name;        // ohne Aethers Anhaengsel " (imported)"
    QString     fehler;      // gesetzt, wenn ok == false
    QStringList hinweise;    // was nicht 1:1 uebernommen werden konnte
};

// Aus einer Datei. Liest, prueft und wendet an.
Ergebnis applyFile(const QString& pfad, StripChain& chain);

// Aus einem bereits gelesenen Objekt -- fuer Pruefstaende und fuer
// den Fall, dass die Datei anderswo herkommt.
Ergebnis apply(const QJsonObject& wurzel, StripChain& chain);

} // namespace AetherPreset
} // namespace Longpath
