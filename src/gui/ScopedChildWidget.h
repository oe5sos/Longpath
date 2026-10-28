// =================================================================
// src/gui/ScopedChildWidget.h  (Longpath)
// =================================================================
//
// Ported from AetherSDR source:
//   src/gui/ScopedChildWidget.h [@b9e96a94]
//
// AetherSDR (https://github.com/aethersdr/AetherSDR) fuehrt keine
// Kopfzeile je Datei; es gilt die Projektlizenz (GPLv3), dieselbe wie
// hier. Attribution auf Projektebene nach HOW-TO-PORT.md Regel 6.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-22 — Uebernommen fuer Longpath von Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude. Namensraum geaendert,
//                 Begruendung ausgeschrieben.
//   2026-10-28 — Weiterleitender Konstruktor, damit auch Dialoge mit
//                 mehrstelligem Konstruktor durch den Halter gehen.
//                 Martin Fischer (OE5SOS), AI-assisted via Anthropic Claude.
// =================================================================
//
// Ein QMenu oder QDialog auf dem Stapel, dessen Elternteil waehrend
// exec() stirbt, wird ZWEIMAL abgeraeumt: einmal von Qt (jedes Kind
// stirbt mit seinem Elternteil) und einmal beim Verlassen des Rahmens.
// Das passiert nicht nur beim Beenden — jede Aktion, die das eigene
// Fenster schliesst, kann es ausloesen, denn exec() dreht eine eigene
// Ereignisschleife, und in der laeuft alles weiter.
//
// Dieser Halter legt das Kind auf den Haufen, behaelt es in einem
// QPointer und raeumt es beim Verlassen des Rahmens auf — aber nur,
// wenn es noch da ist. Elternschaft, Menueverhalten und Modalitaet
// bleiben unveraendert; nur der Besitz wandert.
//
//     ScopedChildWidget<QMenu> owner(this);
//     QMenu& menu = *owner.get();
//     ...
//     if (menu.exec(pos) == action) { ... }   // auch wenn `this` stirbt
//
// Wer nach exec() noch etwas am Elternteil tut, prueft zusaetzlich mit
// einem eigenen QPointer, dass es das Elternteil noch gibt.
// =================================================================

#pragma once

#include <concepts>
#include <utility>

#include <QPointer>
#include <QWidget>

namespace Longpath {

template <typename Widget>
class ScopedChildWidget final {
public:
    // Reicht durch, was der Aufrufer angibt. Die einstellige Form
    // ScopedChildWidget<QMenu>(this) ist der Normalfall; die mehrstellige
    // braucht jeder Dialog, der sein Elternteil erst am Ende nimmt, etwa
    // QMessageBox(Icon, Titel, Text, Knoepfe, parent).
    //
    // `constructible_from` haelt den Kopierkonstruktor heraus: ein QMenu
    // laesst sich nicht aus einem ScopedChildWidget bauen, also greift
    // diese Vorlage dort nicht und das geloeschte Kopieren bleibt stehen.
    template <typename... Args>
        requires std::constructible_from<Widget, Args...>
    explicit ScopedChildWidget(Args&&... args)
        : m_widget(new Widget(std::forward<Args>(args)...))
    {
    }

    ~ScopedChildWidget()
    {
        // Der QPointer ist leer, wenn das Elternteil schon aufgeraeumt hat.
        delete m_widget.data();
    }

    ScopedChildWidget(const ScopedChildWidget&) = delete;
    ScopedChildWidget& operator=(const ScopedChildWidget&) = delete;

    Widget* get() const { return m_widget.data(); }
    explicit operator bool() const { return !m_widget.isNull(); }

private:
    QPointer<Widget> m_widget;
};

} // namespace Longpath
