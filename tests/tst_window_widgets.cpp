// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_window_widgets.cpp  (Longpath)
// =================================================================
// Die eigenen Fenster stehen in der Widget-Auswahl.
//
// Der Betreiber, 2026-08-20: „logbook, rotor, channel strip, antenne
// usw, sind nicht bei den widgets."
//
// Sie standen nicht drin, weil sie keine Applets sind: sie leben in
// eigenen Fenstern und haengen an Menueeintraegen. Fuer den Auswaehler
// ist das aber kein Unterschied — er verwaltet ABSICHTEN („zeig mir
// das"), nicht Widgets.
//
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include "gui/MainWindow.h"
#include "gui/applets/AppletVisibilityController.h"

using namespace Longpath;

class TestWindowWidgets : public QObject
{
    Q_OBJECT
private slots:
    void theOwnWindowsAreInThePicker()
    {
        auto* mwp = new MainWindow();      // bewusst nicht abgeraeumt
        mwp->resize(1280, 800);
        mwp->show();
        QVERIFY(QTest::qWaitForWindowExposed(mwp));
        QTest::qWait(400);

        auto* vis = mwp->findChild<AppletVisibilityController*>();
        QVERIFY2(vis, "es muss einen Sichtbarkeits-Regler geben");

        const QStringList ids = vis->registeredIds();
        for (const QString& want : {QStringLiteral("WinLogbook"),
                                    QStringLiteral("WinRotorLog"),
                                    QStringLiteral("WinChannelStrip"),
                                    QStringLiteral("WinAntenna"),
                                    QStringLiteral("WinSpotHub")}) {
            QVERIFY2(ids.contains(want),
                     qPrintable(QStringLiteral(
                         "%1 fehlt in der Auswahl. Vorhanden: %2")
                         .arg(want, ids.join(QStringLiteral(", ")))));
        }
    }

    // Sie duerfen beim ersten Start nicht von selbst aufgehen.
    void theyStayShutUntilAsked()
    {
        auto* mwp = new MainWindow();
        mwp->resize(1280, 800);
        mwp->show();
        QVERIFY(QTest::qWaitForWindowExposed(mwp));
        QTest::qWait(400);

        auto* vis = mwp->findChild<AppletVisibilityController*>();
        QVERIFY(vis);
        for (const QString& id : {QStringLiteral("WinChannelStrip"),
                                  QStringLiteral("WinAntenna")}) {
            QVERIFY2(!vis->isVisible(id),
                     qPrintable(QStringLiteral(
                         "%1 darf beim ersten Start nicht von selbst "
                         "aufgehen — ein Fenster, das sich ungefragt "
                         "oeffnet, ist eine Zumutung").arg(id)));
        }
    }

    // Und ein Haken MUSS wirken.
    void tickingOneOpensTheWindow()
    {
        auto* mwp = new MainWindow();
        mwp->resize(1280, 800);
        mwp->show();
        QVERIFY(QTest::qWaitForWindowExposed(mwp));
        QTest::qWait(400);

        auto* vis = mwp->findChild<AppletVisibilityController*>();
        QVERIFY(vis);

        // WinChannelStrip, nicht mehr WinAntenna: das Antennenfenster
        // oeffnet seit 2026-09-03 (851c9e08, Betreiber 2026-09-01: "ALLE
        // fliegenden Fenster gehoeren hinter die ConnectMaske") nur noch
        // MIT verbundenem Funkgeraet -- ohne eines ist der Haken dort
        // absichtlich wirkungslos, und dieses Fenster hier hat keines.
        // Der Vertrag "ein Haken MUSS ein Fenster oeffnen" gilt weiter;
        // geprueft wird er an einem Eintrag ohne diese Vorbedingung.
        const int before = QApplication::topLevelWidgets().size();
        vis->setVisible(QStringLiteral("WinChannelStrip"), true);
        QTest::qWait(500);
        const int after = QApplication::topLevelWidgets().size();

        QVERIFY2(after > before,
                 qPrintable(QStringLiteral(
                     "der Haken MUSS ein Fenster oeffnen (vorher %1 "
                     "Fenster, nachher %2) — ein Eintrag, der nichts tut, "
                     "ist schlimmer als keiner")
                     .arg(before).arg(after)));
    }
};
QTEST_MAIN(TestWindowWidgets)
#include "tst_window_widgets.moc"
