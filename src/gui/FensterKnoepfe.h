#pragma once

// =================================================================
// src/gui/FensterKnoepfe.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Die drei Punkte links oben — rot, gelb, grün — auch an den
// Werkzeugfenstern.
//
// „ich möchte übrigens, dass ich das auch einfach in die taskleiste
//  runter legen kann, leider sind aber auch hier keine symbole …
//  bei apple sind links oben 3 farbige symbole, rot, gelb und grün"
//
// ── Woran es lag ─────────────────────────────────────────────────────
//
// Fast jedes Fenster in Longpath, das kein Hauptfenster ist, ist ein
// QDialog: der Kanalzug, „Connect to Radio", das Logbuch, die
// Speicher, die Antennen. Ein QDialog bekommt von Qt auf macOS nur
// den Schließknopf; Zuklappen und Zoomen sind gar nicht erst
// angefordert. Das System zeichnet die drei Punkte dann grau und
// tot — es sieht aus wie ein Fehler und ist einer: ein Fenster, das
// man nicht ins Dock legen kann, muss man schließen, und damit ist
// die Einstellungsarbeit darin weg.
//
// ── Warum ein Filter und keine 27 Änderungen ─────────────────────────
//
// Es sind siebenundzwanzig Klassen. In jede eine Zeile zu setzen
// heißt, dass die achtundzwanzigste sie vergisst — und niemand merkt
// es, bis jemand das Fenster wegräumen will. Der Filter greift
// einmal, bei QEvent::Polish, also bevor das Fenster beim System
// angelegt wird; danach sind die Flaggen gesetzt und das Fenster
// entsteht gleich richtig.
//
// ── Was er ausdrücklich NICHT anfasst ────────────────────────────────
//
//   * Modale Fenster. Ein modaler Dialog, der ins Dock wandert,
//     sperrt das Programm und ist nicht mehr erreichbar.
//   * Popups, Werkzeugfenster (Qt::Tool), rahmenlose und Sheets.
//     Die sind absichtlich so, und ein Zuklappknopf an einem Popup
//     ist Unsinn.
//   * Alles, was die Flagge schon trägt.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-10 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include <QObject>

namespace Longpath {

// In main.cpp auf die QApplication gesetzt, neben ThemeFilter und
// AuxiliaryWindowLeveler.
class FensterKnoepfe : public QObject {
    Q_OBJECT
public:
    explicit FensterKnoepfe(QObject* parent = nullptr) : QObject(parent) {}

    // Für Prüfstände: dieselbe Entscheidung ohne Ereignisschlange.
    // Gibt zurück, ob an diesem Fenster etwas geändert wurde.
    static bool knoepfeNachruesten(class QWidget* w);

protected:
    bool eventFilter(QObject* o, QEvent* e) override;
};

} // namespace Longpath
