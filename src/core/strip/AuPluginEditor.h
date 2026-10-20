#pragma once

// =================================================================
// src/core/strip/AuPluginEditor.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Das eigene Fenster eines Plugins, in einem eigenen Fenster.
//
// In AudioPlugin.h steht, warum die Parameter zunaechst in UNSEREN
// Reglern gezeichnet wurden: Hausstil, Plattformgleichheit, und das
// Einbetten fremder Ansichten in Qt ist die uebliche Bruchstelle.
// Der Preis stand gleich daneben -- „ein Plugin, dessen Koennen in
// seiner Oberflaeche steckt [...] ist hier eine Liste von Zahlen" --
// und der Betreiber hat ihn genau so bemerkt: „haette gehofft, dass
// die plugins ev. grafisch vorhanden sind."
//
// Er hat recht. Bei einem Entzerrer wie TDR Nova steckt das halbe
// Werkzeug in der Zeichnung; eine Spalte Zahlen ist davon kein
// Ersatz.
//
// Die Bruchstelle wird trotzdem nicht betreten: die Ansicht kommt in
// ein EIGENES Cocoa-Fenster, nicht in ein Qt-Widget. Qt und AppKit
// teilen sich dann nur den Bildschirm, nicht einen Fensterbaum --
// und genau das Einbetten war der Teil, an dem solche Vorhaben
// scheitern (siehe auch die Warnung in MacFloatingWindowBehavior.mm
// ueber winId() unter QT_QPA_PLATFORM=offscreen).
//
// Zwei Wege hinein, in dieser Reihenfolge:
//
//   1. Die EIGENE Ansicht des Plugins (kAudioUnitProperty_CocoaUI).
//      Das ist das, was man aus einer DAW kennt.
//   2. AUGenericView aus CoreAudioKit -- Apples Regler-Tafel, die
//      jede Audio Unit ohne eigene Oberflaeche bedienbar macht.
//
// Unsere eigenen Regler bleiben daneben stehen. Sie sind nicht das
// schlechtere Fenster, sondern das, was auch dann noch da ist, wenn
// ein Plugin gar keine Oberflaeche mitbringt -- und das Einzige, was
// spaeter auf Windows und Linux genauso aussieht.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Objective-C++, AI-assisted via
//                 Anthropic Claude (Claude Code), operator Martin
//                 Fischer.
// =================================================================

#include <QString>

namespace Longpath::AuPluginEditor {

// Gibt es auf dieser Plattform ueberhaupt Fenster dieser Art?
bool available();

// Das Fenster zu dieser Audio Unit zeigen (oder nach vorne holen).
// `unit` ist eine AudioComponentInstance; der Typ bleibt hier
// absichtlich untypisiert, damit diese Kopfdatei ohne AudioToolbox
// eingebunden werden kann.
//
// false heisst: es liess sich keine Ansicht bauen -- dann bleiben
// unsere eigenen Regler die Bedienung, und das ist kein Fehlerfall.
bool show(void* unit, const QString& title);

// Zumachen und vergessen. MUSS gerufen werden, bevor die Audio Unit
// zerstoert wird: ein Fenster, dessen Ansicht auf ein freigegebenes
// Plugin zeigt, stuerzt beim naechsten Mausklick ab.
void close(void* unit);

} // namespace Longpath::AuPluginEditor
