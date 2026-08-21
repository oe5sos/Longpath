// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die zwei Knopfreihen im Rotor-Feld sind gefallen (2026-08-21: „under
// der world image you see die N, NE, E ... das gehoert weg"). Dieser
// Test haelt fest, dass dabei NICHTS gestrandet ist.
//
// Das ist die Fehlerklasse, die diese Woche mehrfach zugeschlagen hat:
// eine Faehigkeit verschwindet mit ihrem Knopf, niemand merkt es, und
// erst Wochen spaeter will jemand daran drehen. Hier besonders heikel,
// weil an den Knoepfen GELERNTE Werte hingen — eine gespeicherte
// Peilung, die man nicht mehr aufrufen kann, ist schlimmer als keine.

#include <QtTest>
#include <QMenu>

#include "gui/widgets/RotorLogbookPanel.h"
#include "gui/widgets/RotorDialWidget.h"

using namespace Longpath;

class TstRotorAimMenu : public QObject
{
    Q_OBJECT

private:
    static QStringList labels(const QMenu& m)
    {
        QStringList out;
        for (QAction* a : m.actions()) {
            if (a->isSeparator()) { continue; }
            out << a->text();
            if (a->menu()) {
                for (QAction* s : a->menu()->actions()) {
                    out << QStringLiteral("  %1").arg(s->text());
                }
            }
        }
        return out;
    }

private slots:
    /// Alles, was die zwei Reihen konnten, steht im Menue.
    void nothingTheRowsCouldDoIsLost()
    {
        RotorLogbookPanel panel(nullptr, nullptr, nullptr);
        QMenu menu;
        panel.buildAimMenu(menu, QPoint(10, 10));

        const QString all = labels(menu).join(QLatin1Char('|'));

        // Die acht Himmelsrichtungen
        for (const char* p : {"N ·", "NE ·", "E ·", "SE ·",
                              "S ·", "SW ·", "W ·", "NW ·"}) {
            QVERIFY2(all.contains(QString::fromUtf8(p)),
                     qPrintable(QStringLiteral("Richtung fehlt: %1 — in %2")
                                    .arg(QString::fromUtf8(p), all)));
        }

        // Die vier gelernten Ziele, Park, Gegenkurs, Einstellungen
        QVERIFY2(all.contains(QStringLiteral("Ziel 1")),
                 qPrintable(QStringLiteral("Gelernte Ziele fehlen: %1").arg(all)));
        QVERIFY2(all.contains(QStringLiteral("Ziel 4")),
                 "Nicht alle vier Zielplaetze im Menue");
        QVERIFY2(all.contains(QStringLiteral("Park")),
                 "Park fehlt — die gelernte Parkstellung waere unerreichbar");
        QVERIFY2(all.contains(QStringLiteral("Lernen")),
                 "Ohne 'Lernen' kann man nie wieder ein Ziel speichern");
        QVERIFY2(all.contains(QStringLiteral("Gegenkurs")),
                 "Der lange Weg fehlt");
        QVERIFY2(all.contains(QStringLiteral("Rotor-Einstellungen")),
                 "Der Weg in die Rotor-Einstellungen fehlt");
    }

    /// Und ein Eintrag ZIELT auch wirklich — er sieht nicht nur so aus.
    void aCompassPointActuallyAims()
    {
        RotorLogbookPanel panel(nullptr, nullptr, nullptr);
        auto* dial = panel.findChild<RotorDialWidget*>();
        QVERIFY(dial);

        QMenu menu;
        panel.buildAimMenu(menu, QPoint(10, 10));

        QAction* west = nullptr;
        for (QAction* a : menu.actions()) {
            if (!a->menu()) { continue; }
            for (QAction* s : a->menu()->actions()) {
                if (s->text().startsWith(QStringLiteral("W ·"))) { west = s; }
            }
        }
        QVERIFY2(west, "Kein Eintrag für West");

        west->trigger();
        QVERIFY2(dial->hasTarget(), "Nach dem Zielen gibt es kein Ziel");
        QCOMPARE(qRound(dial->targetBearing()), 270);
    }

    /// Zielen bewegt den Mast NICHT — derselbe Vertrag wie bei den
    /// Knoepfen. Ein Menue, das dreht, waere gefaehrlicher als der
    /// Knopf, den es ersetzt.
    void aimingDoesNotTurnTheMast()
    {
        RotorLogbookPanel panel(nullptr, nullptr, nullptr);
        auto* dial = panel.findChild<RotorDialWidget*>();
        QVERIFY(dial);

        QSignalSpy turned(dial, &RotorDialWidget::rotateRequested);

        QMenu menu;
        panel.buildAimMenu(menu, QPoint(10, 10));
        for (QAction* a : menu.actions()) {
            if (!a->menu()) { continue; }
            for (QAction* s : a->menu()->actions()) {
                if (s->text().startsWith(QStringLiteral("N ·"))) {
                    s->trigger();
                }
            }
        }
        QCOMPARE(turned.count(), 0);
    }
};

QTEST_MAIN(TstRotorAimMenu)
#include "tst_rotor_aim_menu.moc"
