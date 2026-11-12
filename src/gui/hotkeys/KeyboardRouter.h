// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/hotkeys/KeyboardRouter.h  (Longpath)
// =================================================================
//
// Die Tastaturweiche: sieht jeden Tastendruck des Programms, prueft die
// Regeln, wo Tasten gelten, und ruft dann den Verteiler. Setzt nie den
// Fokus. Nur Hauptfaden.
//
// Uebernommen aus dem Upstream NereusSDR (`src/gui/hotkeys/
// KeyboardRouter.h/.cpp` @ 9036ffc04, J.J. Boyd KG4VCF).
//
// ── Longpath-Abweichungen ───────────────────────────────────────────
//
// * Welche Fenster Tasten nehmen: in Longpath stehen die Panels in
//   eigenen Fenstern (AppletFloatingWindow, SideAreaWindow, ToolWindow)
//   neben dem Hauptfenster, dem schwebenden Panorama und dem schwebenden
//   Behaelter. In allen gelten die Tasten; in einem Dialog nicht.
// * Kein Anordnen-Modus an den Behaeltern -- Upstreams Ausnahme fuer
//   Pfeiltasten beim Anordnen entfaellt.
// * takesKey: eine Taste, die gerade nichts zu tun hat (Esc ohne
//   laufende Ansage), wird nicht geschluckt.
//
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan.
//                AI-assisted implementation via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-10 - Uebernommen fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.
#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

class QAction;
class QKeyEvent;
class QMenuBar;
class QWidget;

namespace Longpath {

class HotKeyBindings;
class HotKeyDispatcher;
class MainWindow;

class KeyboardRouter : public QObject {
    Q_OBJECT

public:
    KeyboardRouter(HotKeyDispatcher* dispatcher, HotKeyBindings* bindings, MainWindow* window);

    // Canonical chord text, or empty for a bare modifier or a key with no
    // portable name. Num comes off the arrows, which macOS reports as keypad keys.
    static QString chordFromEvent(const QKeyEvent* event);

    // What pairs a press with its release: one physical key, whatever its
    // modifiers did in between. `preferScanCode` is true on Linux.
    static qint64 physicalKeyOf(quint32 nativeScanCode, quint32 nativeVirtualKey, int key, bool preferScanCode);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool handlePress(QWidget* receiver, QKeyEvent* event);
    bool handleRelease(const QKeyEvent* event);
    bool handlePressOfKeyDown(QWidget* receiver, QKeyEvent* event, QHash<qint64, QString>::iterator down);
    static bool pressAfterLostRelease(const QKeyEvent* event, const QString& heldId);
    void noteFocusIn(QWidget* widget, Qt::FocusReason reason);
    void watchMenuBar(const QMenuBar* bar);
    void watchMenu(const QAction* action);
    void dropHolds();
    bool windowTakesKeys(const QWidget* receiver) const;
    bool focusKeeps(const QWidget* receiver, const QKeyEvent* event) const;

    HotKeyDispatcher* m_dispatcher;
    HotKeyBindings* m_bindings;
    MainWindow* m_window;

    QHash<qint64, QString> m_down;      // keys this router ate the press of, by physical key
    QPointer<QWidget> m_tabFocused;     // reached with Tab or Shift+Tab, until focus moves on
    quint64 m_dropCount{0};
};

} // namespace Longpath
