// SPDX-License-Identifier: GPL-3.0-or-later
//
// Rechtsklick auf einen Notch-Balken — im ECHTEN Hauptfenster.
//
// Der Betreiber, 2026-08-21: „rechte mouse taste funktioniert nicht."
// Zu diesem Zeitpunkt waren gruen: ein Rechtsklick auf einen
// freistehenden SpectrumWidget, ein nachgebauter QContextMenuEvent mit
// Elternteil, und ein echter Rechtsklick mit Elternteil. Drei
// Pruefungen, alle bestanden, und in der App ging es trotzdem nicht.
//
// Der Unterschied ist immer derselbe gewesen: was ich baue, ist nicht,
// was die App baut. Der Panadapter haengt dort in einem Applet, das
// Applet in einem Stapel, der Stapel in einem Behaelter — und jede
// dieser Schichten kann Mausereignisse abfangen, bevor sie unten
// ankommen.
//
// Also wird hier nichts mehr nachgebaut: MainWindow, wie sie startet.

#include <QtTest>
#include <QMenu>

#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/NotchModel.h"

using namespace Longpath;

class TstRealNotchRightClick : public QObject
{
    Q_OBJECT

private slots:
    void rightClickOnANotchOpensTheNotchMenu()
    {
        auto* mw = new MainWindow();
        mw->resize(1800, 1100);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(300);

        auto* pan = mw->findChild<SpectrumWidget*>();
        QVERIFY2(pan, "Kein Panadapter im Hauptfenster gefunden");
        QVERIFY2(pan->isVisible(), "Der Panadapter ist nicht zu sehen");

        // Einen Notch dort anlegen, wo der Panadapter gerade hinsieht —
        // ueber das ECHTE Modell, denselben Weg wie ein Cmd-Klick.
        RadioModel* radio = mw->findChild<RadioModel*>();
        if (!radio) { QSKIP("Kein RadioModel — Umgebung ohne Geraetemodell"); }
        NotchModel* notches = radio->notchModel();
        QVERIFY(notches);

        const double centreHz = pan->centerFrequency();
        QVERIFY2(centreHz > 0.0, "Der Panadapter hat keine Mittenfrequenz");
        const int id = notches->addNotch(centreHz, 400.0);
        QVERIFY2(id >= 0, "Notch liess sich nicht anlegen");
        for (int i = 0; i < 8; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(120);

        // Wo liegt er auf dem Bild? Nicht rechnen — suchen.
        int x = -1;
        for (int probe = 0; probe < pan->width(); ++probe) {
            if (pan->notchAtPixelForTest(probe) == id) { x = probe; break; }
        }
        QVERIFY2(x >= 0,
                 "Der angelegte Notch liegt nirgends auf dem Bild — dann "
                 "prueft der Rest dieses Tests nichts");

        QTest::mouseClick(pan, Qt::RightButton, Qt::NoModifier, QPoint(x, 60));
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(80);

        // Irgendwo im Programm muss jetzt ein sichtbares Menue stehen,
        // und es muss das des Notches sein — nicht das des Pans.
        QMenu*   shown = nullptr;
        QStringList entries;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            for (QMenu* m : w->findChildren<QMenu*>()) {
                if (!m->isVisible()) { continue; }
                shown = m;
                for (QAction* a : m->actions()) { entries << a->text(); }
            }
        }
        QVERIFY2(shown,
                 "Rechtsklick auf den Balken oeffnet im echten Fenster "
                 "KEIN Menue");

        const QString all = entries.join(QLatin1Char('|'));
        QVERIFY2(!all.contains(QStringLiteral("this pan")),
                 qPrintable(QStringLiteral(
                     "Das Pan-Menue ist aufgegangen statt des Notch-Menues: %1")
                     .arg(all)));
        QVERIFY2(all.contains(QStringLiteral("Notch")),
                 qPrintable(QStringLiteral(
                     "Ein Menue steht da, aber ohne Notch-Eintraege: %1")
                     .arg(all)));

        if (shown) { shown->close(); }

        // close() vor delete: der Destruktor allein haelt den
        // SpectrumThread nicht an, und Qt bricht dann mit „QThread:
        // Destroyed while thread is still running" ab. Das Anhalten
        // steht im closeEvent — dort, wo es auch beim Beenden des
        // Programms passiert.
        mw->close();
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        delete mw;
    }
};

QTEST_MAIN(TstRealNotchRightClick)
#include "tst_real_notch_rightclick.moc"
