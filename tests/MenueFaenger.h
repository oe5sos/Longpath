#pragma once

// =================================================================
// tests/MenueFaenger.h  (Longpath)
// =================================================================
//
// Ein Menue im Prueflauf oeffnen, ansehen und wieder schliessen —
// ohne Taktgeber.
//
// ── Warum es diesen Helfer gibt ─────────────────────────────────────
//
// Drei Prueflaeufe trugen je eine eigene Abschrift derselben Zeilen:
//
//     QTimer::singleShot(150, qApp, [&]() {
//         if (auto* m = qobject_cast<QMenu*>(
//                 QApplication::activePopupWidget())) { … m->close(); }
//     });
//
// Das laeuft meistens und manchmal nicht:
//
//   * `QMenu::exec()` dreht auf macOS eine COCOA-Schleife
//     (`-[NSApplication run]`, mit `sample` am haengenden Prozess
//     gemessen). Darin werden Qt-Taktgeber nicht zuverlaessig bedient
//     — `tst_real_menus_hang_on_the_window` lief darum wochenlang in
//     CTests 120-Sekunden-Grenze.
//   * Manche Menues bekommen im Prueflauf die Hoehe NULL
//     („166x0 outside any known screen"), werden nie sichtbar und
//     damit nie `activePopupWidget()`.
//   * Wer `parentSeen` gleich nach dem Klick liest, liest unter Last
//     den Stand von VORHER — daher „Compared QObject pointers are not
//     the same" in `tst_menus_of_native_fields` und `tst_command_bar`,
//     und zwar nur im `-j6`-Lauf.
//
// Ein Ereignisfilter laeuft dagegen SYNCHRON in `sendEvent`, also noch
// waehrend `exec()` das Menue zeigt und bevor es in die Schleife
// eintritt. Das Schliessen wird von dort GEPOSTET; beim Eintritt in
// die Schleife arbeitet Qt die eigene Warteschlange ab.
//
// Wichtig fuer den naechsten Anlauf: ein zusaetzliches DIREKTES
// `close()` im Show-Handler bricht die vorher laufenden Faelle. Nur
// die gepostete Schliessung.
//
// Ausfuehrlich: docs/architecture/2026-10-15-pruefstaende-und-der-schirm.md
//
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QStringList>
#include <QTest>
#include <QWidget>

#include <functional>

namespace Longpath::Test {

/// Was von einem geoeffneten Menue zu sehen war.
struct MenueFund {
    QWidget*    parent{nullptr};
    QStringList entries;
    bool        seen{false};
};

class MenueFaenger : public QObject {
public:
    explicit MenueFaenger(MenueFund* out) : m_out(out)
    {
        qApp->installEventFilter(this);
    }

    ~MenueFaenger() override
    {
        if (qApp) { qApp->removeEventFilter(this); }
    }

    bool eventFilter(QObject* obj, QEvent* ev) override
    {
        if (ev->type() == QEvent::Show && !m_out->seen) {
            if (auto* m = qobject_cast<QMenu*>(obj)) {
                m_out->parent = m->parentWidget();
                for (QAction* a : m->actions()) { m_out->entries << a->text(); }
                m_out->seen = true;
                QMetaObject::invokeMethod(m, &QMenu::close,
                                          Qt::QueuedConnection);
            }
        }
        return QObject::eventFilter(obj, ev);
    }

private:
    MenueFund* m_out;
};

/// Oeffnet ueber `trigger` ein Menue — exec() oder popup() — und
/// schliesst es, sobald es sich zeigt.
inline MenueFund oeffneMenue(const std::function<void()>& trigger,
                             int wartenMs = 2000)
{
    MenueFund o;
    MenueFaenger faenger(&o);
    trigger();
    QTest::qWaitFor([&o]() { return o.seen; }, wartenMs);
    QTest::qWait(50);
    return o;
}

} // namespace Longpath::Test
