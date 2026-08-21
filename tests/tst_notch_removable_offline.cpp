// SPDX-License-Identifier: GPL-3.0-or-later
//
// Notch-Filter muessen sich auch OHNE Funkgeraet wieder loeschen
// lassen.
//
// Der Betreiber, 2026-08-21, mit einem Bildschirmfoto von sechs gelben
// Balken auf 40 m: „wie kann ich diese filter? loeschen? — app ist
// geschlossen." Die Balken sind Notch-Filter (TNF): gelb aktiv, gruen
// der gerade ausgewaehlte.
//
// Der Weg dahin ist ein Rechtsklick auf den Balken. Ob der im
// getrennten Zustand ankommt, ist nicht selbstverstaendlich: der
// Panadapter schluckt dort Klicks und oeffnet stattdessen das
// Verbindungsfeld (Phase 3Q-8). Er schluckt sie NUR links — aber das
// ist eine Zeile, die jemand aendern kann, ohne diesen Fall zu
// bedenken. Also festgenagelt.

#include <QtTest>
#include <QMenu>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

class TstNotchRemovableOffline : public QObject
{
    Q_OBJECT

private slots:
    void aNotchCanBeRemovedWhileDisconnected()
    {
        SpectrumWidget w;
        w.resize(1200, 700);
        w.setConnectionState(ConnectionState::Disconnected);
        w.setFrequencyRange(7'131'200.0, 200'000.0);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QVector<SpectrumWidget::NotchMarker> notches;
        SpectrumWidget::NotchMarker n;
        n.id = 4;
        n.freqMhz = 7.1312;
        n.widthHz = 400.0;
        n.active = true;
        notches.append(n);
        w.setNotchMarkers(notches);
        QCoreApplication::processEvents();

        // Die Stelle NICHT rechnen, sondern suchen. Die Mitte des
        // Fensters ist nicht die Mitte des Spektrums — rechts sitzt
        // der dBm-Streifen. Genau diese Verwechslung hat in dieser
        // Sitzung schon einmal einen Test fuer nichts gruen gemacht.
        int x = -1;
        for (int probe = 0; probe < w.width(); ++probe) {
            if (w.notchAtPixelForTest(probe) == 4) { x = probe; break; }
        }
        QVERIFY2(x >= 0, "Der Notch liegt nirgends auf dem Bild — dann "
                         "prueft der Rest dieses Tests nichts");
        const int y = 80;

        QSignalSpy gone(&w, &SpectrumWidget::notchRemoveRequested);
        QTest::mouseClick(&w, Qt::RightButton, Qt::NoModifier, QPoint(x, y));
        QCoreApplication::processEvents();

        // Das Menue oeffnet mit popup(), nicht exec() — es haengt also
        // als Kind am Widget und laesst sich hier befragen.
        QMenu* menu = w.findChild<QMenu*>();
        QVERIFY2(menu, "Rechtsklick auf einen Notch oeffnet kein Menue — "
                       "im getrennten Zustand kaeme man dann nicht mehr an "
                       "seine Filter heran");

        QAction* remove = nullptr;
        for (QAction* a : menu->actions()) {
            if (a->text().contains(QLatin1String("Remove"))) {
                remove = a;
                break;
            }
        }
        QVERIFY2(remove, "Kein 'Remove Notch' im Menue");
        remove->trigger();
        QCoreApplication::processEvents();

        QCOMPARE(gone.count(), 1);
        QCOMPARE(gone.at(0).at(0).toInt(), 4);
    }
};

QTEST_MAIN(TstNotchRemovableOffline)
#include "tst_notch_removable_offline.moc"
