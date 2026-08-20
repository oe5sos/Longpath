// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/WindowChrome.h  (Longpath)
// =================================================================
//
// Titelleiste und Anfasser fuer rahmenlose Fenster.
//
// Strukturell nach dem Vorbild von AetherSDR
// src/gui/FloatingContainerWindow.{h,cpp} [@31b2958], das dieselben
// zwei Teile — eine Leiste zum Ziehen und einen Anfasser zum
// Groessenaendern — als Kopf eines rahmenlosen Fensters fuehrt.
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; siehe LICENSE
//       und den Ueber-Dialog fuer die aktuelle Mitwirkendenliste)
//
// Modification history (Longpath)
//   2026-08-20  Martin Fischer (OE5SOS), mit Claude (Opus 5) —
//               Neu angelegt. Bis dahin trugen die eigenen Fenster den
//               Rahmen des Betriebssystems; sie liessen sich nur an
//               dessen grauer Leiste ziehen und hatten keinen sichtbaren
//               Anfasser. Der Betreiber hat ueber Tage hinweg wiederholt
//               verlangt, dass sich JEDES Fenster an der eigenen oberen
//               Leiste in alle vier Richtungen schieben und unten rechts
//               in x und y groesser und kleiner ziehen laesst — so wie
//               bei Zeus und AetherSDR.
// =================================================================

#pragma once

#include <QWidget>

class QLabel;

namespace Longpath {

// ── Die obere Leiste ────────────────────────────────────────────────
//
// Sie ist der Griff, an dem das ganze Fenster haengt. Ein Druck darauf
// beginnt den Zug, das Loslassen beendet ihn; dazwischen folgt das
// Fenster dem Zeiger — nach rechts, nach links, hinauf und hinunter,
// ohne Anschlag.
//
// Links steht ein farbiger Strich. Das ist keine Zierde: bei Zeus
// markiert er, wo man anfassen darf, und der Betreiber hat ihn
// ausdruecklich als Vorbild genannt („oder wie bei zeus der gelbe
// strich"). Ein Fenster ohne diese Marke sieht aus wie eines, das man
// nicht anfassen kann.
class WindowTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit WindowTitleBar(const QString& title, QWidget* parent = nullptr);

    void setTitle(const QString& title);

    // Doppelklick auf die Leiste heisst „andocken", nicht „maximieren".
    // Ein abgeloestes Fenster gehoert eigentlich in die Spalte zurueck;
    // das ist die Bewegung, die man erwartet.
signals:
    void closeRequested();
    void dockRequested();

protected:
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;

private:
    QLabel* m_label{nullptr};
};

// ── Der Anfasser unten rechts ───────────────────────────────────────
//
// FramelessResizer horcht schon auf alle Kanten, aber unsichtbar: man
// muss die Ecke erst finden. Dieser Anfasser macht sie sichtbar — drei
// Schraegstriche, wie sie jedes Fenstersystem dort zeichnet — und
// nimmt den Zug selbst entgegen, damit er auch dann greift, wenn der
// Inhalt bis in die Ecke reicht und die Maus vorher abfaengt.
//
// Er zieht in beide Achsen zugleich: nach rechts und nach unten
// groesser, nach links und nach oben kleiner.
class ResizeGrip : public QWidget {
    Q_OBJECT
public:
    explicit ResizeGrip(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;

private:
    bool   m_dragging{false};
    QPoint m_pressGlobal;
    QSize  m_startSize;
};

// Haengt den Anfasser unten rechts in ein Fenster und haelt ihn dort,
// auch wenn sich die Groesse aendert. Gibt den Anfasser zurueck.
ResizeGrip* attachResizeGrip(QWidget* window);

} // namespace Longpath
