// =================================================================
// tests/tst_auxiliary_window_leveler.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. AuxiliaryWindowLeveler (Betreiber 2026-09-21:
// "wenn ich ein Channel Strip oder etwas anderes aufmache, ist es im
// Hintergrund"): welche Fenster der App-weite Filter anfasst und welche
// nicht. Die eigentliche Fensterebene ist Cocoa-Sache und unter der
// Offscreen-Plattform ein No-Op -- hier zaehlt die Auswahl.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QDialog>
#include <QMessageBox>
#include <QMenu>

#include "gui/AuxiliaryWindowLeveler.h"

using namespace Longpath;

class TstAuxiliaryWindowLeveler : public QObject {
    Q_OBJECT

private slots:
    void dialogsAndSecondaryWindowsAreLifted()
    {
        AuxiliaryWindowLeveler leveler;
        qApp->installEventFilter(&leveler);

        QWidget main;
        main.show();
        QVERIFY(QTest::qWaitForWindowExposed(&main));
        // Das Hauptfenster selbst hat kein Elternteil -- nicht angefasst.
        QVERIFY(!AuxiliaryWindowLeveler::isAuxiliaryWindow(&main));
        QVERIFY(!leveler.trackedForTest().contains(&main));

        // Ein modeless QDialog aus dem Hauptfenster heraus: der Fall
        // Channel Strip / Logbuch / Setup.
        QDialog dialog(&main);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(AuxiliaryWindowLeveler::isAuxiliaryWindow(&dialog));
        QVERIFY(leveler.trackedForTest().contains(&dialog));

        // Ein gewoehnliches Qt::Window mit Elternteil ebenso.
        QWidget window(&main, Qt::Window);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(leveler.trackedForTest().contains(&window));

        // Ein QMessageBox mit Elternteil auch -- der "eingefrorene"
        // Fall, wenn die Meldung hinter den Paletten steht.
        QMessageBox box(&main);
        // Nicht nativ: auf macOS mit Qt 6.11 wird ein nativer QMessageBox
        // beim show() zu [NSAlert runModal] -- eine modale Schleife, die
        // erst endet, wenn jemand klickt. Hier klickt niemand; der Test
        // hing 300 s (2026-09-24, mit `sample` belegt). Das Programm
        // zeigt seine nicht blockierenden Meldungen ebenfalls nicht nativ.
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        box.setOption(QMessageBox::Option::DontUseNativeDialog);
#endif
        box.show();
        QVERIFY(QTest::qWaitForWindowExposed(&box));
        QVERIFY(leveler.trackedForTest().contains(&box));

        qApp->removeEventFilter(&leveler);
    }

    void palettesPopupsAndStayOnTopAreLeftAlone()
    {
        AuxiliaryWindowLeveler leveler;
        qApp->installEventFilter(&leveler);

        QWidget main;
        main.show();
        QVERIFY(QTest::qWaitForWindowExposed(&main));

        // Die Paletten selbst (Qt::Tool) liegen schon auf der Ebene.
        QWidget tool(&main, Qt::Tool);
        tool.show();
        QVERIFY(!AuxiliaryWindowLeveler::isAuxiliaryWindow(&tool));
        QVERIFY(!leveler.trackedForTest().contains(&tool));

        // Menues sind Popups.
        QMenu menu(&main);
        menu.addAction(QStringLiteral("x"));
        menu.popup(QPoint(10, 10));
        QVERIFY(!AuxiliaryWindowLeveler::isAuxiliaryWindow(&menu));
        QVERIFY(!leveler.trackedForTest().contains(&menu));
        menu.close();

        // "Bleibt oben" liegt bei Qt ohnehin ueber den Paletten.
        QDialog onTop(&main, Qt::WindowStaysOnTopHint);
        onTop.show();
        QVERIFY(!AuxiliaryWindowLeveler::isAuxiliaryWindow(&onTop));
        QVERIFY(!leveler.trackedForTest().contains(&onTop));

        // Ein Kind-Widget im Fenster ist kein Fenster.
        QWidget child(&main);
        child.show();
        QVERIFY(!AuxiliaryWindowLeveler::isAuxiliaryWindow(&child));

        qApp->removeEventFilter(&leveler);
    }

    void aDestroyedWindowLeavesTheSet()
    {
        AuxiliaryWindowLeveler leveler;
        qApp->installEventFilter(&leveler);
        QWidget main;
        main.show();
        QVERIFY(QTest::qWaitForWindowExposed(&main));
        auto* dialog = new QDialog(&main);
        dialog->show();
        QVERIFY(QTest::qWaitForWindowExposed(dialog));
        QVERIFY(leveler.trackedForTest().contains(dialog));
        delete dialog;
        QVERIFY(leveler.trackedForTest().isEmpty());
        qApp->removeEventFilter(&leveler);
    }
};

QTEST_MAIN(TstAuxiliaryWindowLeveler)
#include "tst_auxiliary_window_leveler.moc"
