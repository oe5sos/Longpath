// =================================================================
// src/gui/AuxiliaryWindowLeveler.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Betreiber 2026-09-21: "wenn ich ein Channel Strip oder etwas anderes
// aufmache, ist es im Hintergrund." Die schwebenden Paletten (Applets,
// Panadapter, Rotor/Log, Bandbreitenfilter ...) sind Qt::Tool -- auf
// macOS NSPanels auf der SCHWEBENDEN Fensterebene. Jedes gewoehnliche
// Fenster, das aus dem Hauptfenster heraus aufgeht (Channel Strip,
// Logbuch, Setup, Spot-Hub, ein QMessageBox ...), liegt auf der normalen
// Ebene darunter; raise()/activateWindow() hebt nur innerhalb der
// eigenen Ebene und ist gegen ein NSPanel darueber machtlos. Das
// Antennenfenster bekam dafuer am 2026-09-01 von Hand Qt::Tool -- der
// dritte Fall zeigt, dass das keine Frage einzelner Fenster ist.
//
// Dieser Filter haengt am QApplication-Objekt und hebt JEDES
// Nebenfenster beim Anzeigen auf die Ebene der Paletten: ein
// Top-Level-Widget mit Elternteil (also aus einem anderen Fenster heraus
// geoeffnet), das selbst keine Palette, kein Popup, kein Tooltip und
// kein Splash ist. Es behaelt sein normales Aussehen (kein
// Werkzeugfenster-Titel) und liegt mit den Paletten auf einer Ebene, wo
// das zuletzt gezeigte vorne steht -- wie man es erwartet.
//
// Ein NSWindow auf der schwebenden Ebene stuende ueber jeder anderen
// App (der Fehler vom 2026-08-31 bei den Paletten; die haben dafuer
// hidesOnDeactivate, was nur NSPanel kennt). Darum wechselt der Filter
// beim Deaktivieren von Longpath alle gehobenen Fenster auf die normale
// Ebene zurueck und beim Aktivieren wieder hoch.
//
// Auf allen anderen Plattformen ein No-Op (setPaletteWindowLevel ist
// dort leer): unter Windows/X11 liegen Werkzeugfenster nicht auf einer
// hoeheren Ebene, dort war nie etwas verdeckt.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#pragma once

#include <QObject>
#include <QSet>

class QWidget;

namespace Longpath {

class AuxiliaryWindowLeveler : public QObject {
    Q_OBJECT
public:
    explicit AuxiliaryWindowLeveler(QObject* parent = nullptr);

    // Ein Fenster, das der Filter anfasst: Top-Level, mit Elternteil,
    // gewoehnlicher Typ (Window/Dialog), nicht "bleibt oben".
    static bool isAuxiliaryWindow(const QWidget* w);

    bool eventFilter(QObject* watched, QEvent* event) override;

    // Fuer Tests: was der Filter gerade gehoben hat.
    QSet<QWidget*> trackedForTest() const { return m_tracked; }

private:
    void track(QWidget* w);
    void applyLevel(QWidget* w) const;
    void onApplicationStateChanged();

    QSet<QWidget*> m_tracked;
};

} // namespace Longpath
