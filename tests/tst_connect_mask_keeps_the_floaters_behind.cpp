// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die schwebenden Fenster bleiben HINTER der Connect-Maske -- auch in
// der Sekunde, in der die Verbindung schon steht.
//
// Betreiber am 2026-09-22 zur SunSDR2 QRP: „uebrigens das erscheint
// immer nach oeffnen von qrp, beim anan usw. ist das nicht so" -- auf
// dem Bild steht das Rotor/Log-Fenster mitten ueber der noch offenen
// „Connect to Radio"-Maske.
//
// Der Weg dahin: showConnectionPanel() raeumt alle schwebenden Fenster
// weg (die Regel, die der Betreiber am 2026-09-01 eingefordert hat),
// die Verbindung kommt zustande -- und der Connected-Zweig holte sie
// sofort zurueck, waehrend die Maske selbst erst eine Sekunde spaeter
// zugeht (Phase 3Q Task 5). In dieser Sekunde stehen sie vor ihr, und
// zwar nicht zufaellig: die Schwebefenster sind Qt::Tool, auf macOS
// NSPanels auf einer hoeheren Ebene als ein gewoehnlicher QDialog. Ein
// raise() auf die Maske kaeme dagegen nicht an.
//
// Also nicht heben, sondern warten. Dieser Pruefstand haelt beides
// fest: solange die Maske steht, bleibt die Liste liegen; ist sie weg,
// kommt alles zurueck.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-23 — Created for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>
#include <QApplication>
#include <QPointer>
#include <QWidget>

#include "gui/ConnectionPanel.h"
#include "gui/MainWindow.h"

using namespace Longpath;

class TstConnectMaskKeepsTheFloatersBehind : public QObject {
    Q_OBJECT
private slots:

    void restoreWaitsForTheMaskToGo()
    {
        auto* mw = new MainWindow();
        mw->resize(1280, 800);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));

        // Der Kaltstart oeffnet die Maske per singleShot(0); ihr
        // Konstruktor allein braucht gut zwei Sekunden (NIC-Suche),
        // unter Testlast mehr. Siehe tst_real_pan_float fuer dieselbe
        // Wartezeile und ihre Begruendung.
        QTRY_VERIFY_WITH_TIMEOUT(mw->findChild<ConnectionPanel*>() != nullptr,
                                 20000);
        ConnectionPanel* mask = mw->findChild<ConnectionPanel*>();
        QVERIFY(mask->isVisible());

        // Die Maske hat beim Oeffnen schon weggeraeumt, was das Profil
        // gerade schweben liess -- das zaehlt mit, also merken wir uns
        // den Stand und pruefen die Differenz.
        const int before = mw->floatingWindowsWaitingForConnectMaskForTest();

        // Ein Stellvertreter fuer das Rotor/Log-Fenster: angelegt,
        // gezeigt, dann wie von der Maske weggeraeumt.
        auto* floater = new QWidget(nullptr, Qt::Tool);
        floater->setAttribute(Qt::WA_DeleteOnClose, false);
        floater->resize(200, 120);
        floater->show();
        QVERIFY(QTest::qWaitForWindowExposed(floater));
        mw->rememberFloatingWindowBehindConnectMaskForTest(floater);
        QCOMPARE(mw->floatingWindowsWaitingForConnectMaskForTest(), before + 1);
        QVERIFY(!floater->isVisible());

        // Das ist der Moment, in dem frueher der Connected-Zweig
        // zugeschlagen hat: Verbindung steht, Maske steht auch noch.
        mw->restoreFloatingWindowsForTest();
        QVERIFY2(!floater->isVisible(),
                 "solange die Maske steht, darf nichts davor auftauchen");
        QCOMPARE(mw->floatingWindowsWaitingForConnectMaskForTest(), before + 1);

        // Maske zu -- und jetzt gehoert dem Betreiber sein Layout zurueck.
        mask->close();
        QTRY_VERIFY_WITH_TIMEOUT(mw->findChild<ConnectionPanel*>() == nullptr,
                                 5000);
        QTRY_VERIFY_WITH_TIMEOUT(floater->isVisible(), 5000);
        QCOMPARE(mw->floatingWindowsWaitingForConnectMaskForTest(), 0);

        floater->close();
        delete floater;
        mw->close();
        delete mw;
    }
};

QTEST_MAIN(TstConnectMaskKeepsTheFloatersBehind)
#include "tst_connect_mask_keeps_the_floaters_behind.moc"
