#pragma once

// =================================================================
// src/gui/FramelessMoveHelper.h  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPL. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-20 — Portiert nach C++20/Qt6 fuer Longpath von Martin
//                 Fischer (OE5SOS), KI-gestuetzt ueber Anthropic
//                 Claude (Cowork). Port von AetherSDR
//                 `src/gui/FramelessMoveHelper.h` @ 31b29583.
//                 Geaendert wurde NUR der Namensraum
//                 (AetherSDR -> Longpath).
//
//   WARUM DIESE DATEI HIER LANDET: der Betreiber hat ueber Stunden
//   verlangt, dass sich jedes Fenster an der Titelleiste ueberall
//   hinschieben und unten rechts in der Groesse aendern laesst. Genau
//   das leisten diese beiden Helfer — und sie fehlten bei uns
//   VOLLSTAENDIG. Ich habe stattdessen einen halben Tag lang Kacheln
//   gebaut, die hinter dem nativen Panadapter lagen.
//
//   „bitte schau bei aether genau nach" war der Hinweis, der es
//   gefunden hat. Er hatte recht, und zwar von Anfang an.
//
//   Das wichtigste Detail steht in FramelessMoveHelper: auf macOS wird
//   startSystemMove NICHT benutzt (#ifndef Q_OS_MAC), sondern von Hand
//   mit grabMouse gezogen. Das ist erlernt, nicht zufaellig — wer es
//   „vereinfacht", bekommt ein Fenster, das sich auf macOS nicht
//   bewegt.
// =================================================================


#include <QMouseEvent>
#include <QPoint>
#include <QWidget>
#include <QWindow>

namespace Longpath::FramelessMoveHelper {

namespace Detail {

constexpr const char* kActiveProperty = "_aetherManualWindowMoveActive";
constexpr const char* kPressGlobalProperty = "_aetherManualWindowMovePressGlobal";
constexpr const char* kStartPosProperty = "_aetherManualWindowMoveStartPos";

inline void startManualMove(QWidget* handle, QWidget* window, QMouseEvent* ev)
{
    handle->setProperty(kActiveProperty, true);
    handle->setProperty(kPressGlobalProperty, ev->globalPosition().toPoint());
    handle->setProperty(kStartPosProperty, window->pos());
    handle->grabMouse();
    ev->accept();
}

inline bool manualMoveActive(QWidget* handle)
{
    return handle && handle->property(kActiveProperty).toBool();
}

} // namespace Detail

inline bool start(QWidget* handle, QMouseEvent* ev)
{
    if (!handle || !ev || ev->button() != Qt::LeftButton) {
        return false;
    }

    QWidget* window = handle->window();
    if (!window) {
        return false;
    }

#ifndef Q_OS_MAC
    if (QWindow* windowHandle = window->windowHandle()) {
        if (windowHandle->startSystemMove()) {
            ev->accept();
            return true;
        }
    }
#endif

    Detail::startManualMove(handle, window, ev);
    return true;
}

inline bool finish(QWidget* handle, QMouseEvent* ev)
{
    if (!Detail::manualMoveActive(handle)) {
        return false;
    }

    handle->setProperty(Detail::kActiveProperty, false);
    handle->releaseMouse();
    if (ev) {
        ev->accept();
    }
    return true;
}

inline bool move(QWidget* handle, QMouseEvent* ev)
{
    if (!Detail::manualMoveActive(handle) || !ev) {
        return false;
    }

    if (!(ev->buttons() & Qt::LeftButton)) {
        return finish(handle, ev);
    }

    QWidget* window = handle->window();
    if (window) {
        const QPoint pressGlobal =
            handle->property(Detail::kPressGlobalProperty).toPoint();
        const QPoint startPos =
            handle->property(Detail::kStartPosProperty).toPoint();
        window->move(startPos + ev->globalPosition().toPoint() - pressGlobal);
    }

    ev->accept();
    return true;
}

inline void toggleMaximized(QWidget* handle)
{
    if (!handle) {
        return;
    }

    QWidget* window = handle->window();
    if (!window) {
        return;
    }

    if (window->isMaximized()) {
        window->showNormal();
    } else {
        window->showMaximized();
    }
}

} // namespace Longpath::FramelessMoveHelper
