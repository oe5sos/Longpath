// =================================================================
// src/gui/AuxiliaryWindowLeveler.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Zweck und Begruendung: AuxiliaryWindowLeveler.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "AuxiliaryWindowLeveler.h"

#include "gui/MacFloatingWindowBehavior.h"

#include <QEvent>
#include <QGuiApplication>
#include <QWidget>

namespace Longpath {

AuxiliaryWindowLeveler::AuxiliaryWindowLeveler(QObject* parent)
    : QObject(parent)
{
    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState) { onApplicationStateChanged(); });
}

bool AuxiliaryWindowLeveler::isAuxiliaryWindow(const QWidget* w)
{
    if (!w || !w->isWindow() || !w->parentWidget()) { return false; }
    const Qt::WindowFlags flags = w->windowFlags();
    const Qt::WindowType type = static_cast<Qt::WindowType>(
        static_cast<int>(flags & Qt::WindowType_Mask));
    if (type != Qt::Window && type != Qt::Dialog) { return false; }
    // "Bleibt oben" liegt bei Qt ohnehin ueber den Paletten.
    if (flags.testFlag(Qt::WindowStaysOnTopHint)) { return false; }
    return true;
}

bool AuxiliaryWindowLeveler::eventFilter(QObject* watched, QEvent* event)
{
    // QEvent::Show kommt VOR dem eigentlichen Anzeigen (show_sys), das
    // native Fenster existiert aber schon -- die Ebene greift also fuer
    // das erste orderFront. Beim erneuten Zeigen kommt es wieder.
    if (event->type() == QEvent::Show) {
        if (auto* w = qobject_cast<QWidget*>(watched); w && isAuxiliaryWindow(w)) {
            track(w);
            applyLevel(w);
        }
    }
    return QObject::eventFilter(watched, event);
}

void AuxiliaryWindowLeveler::track(QWidget* w)
{
    if (m_tracked.contains(w)) { return; }
    m_tracked.insert(w);
    connect(w, &QObject::destroyed, this, [this, w]() { m_tracked.remove(w); });
}

void AuxiliaryWindowLeveler::applyLevel(QWidget* w) const
{
    const bool active = qApp->applicationState() == Qt::ApplicationActive;
    setPaletteWindowLevel(w, active);
}

void AuxiliaryWindowLeveler::onApplicationStateChanged()
{
    for (QWidget* w : m_tracked) {
        if (w->isVisible()) { applyLevel(w); }
    }
}

} // namespace Longpath
