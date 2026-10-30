// =================================================================
// tests/tst_ft8_plaetze_weg.cpp  (Longpath)
// =================================================================
//
// Der WEG zum Plaetze-Fenster, im echten MainWindow gefahren.
//
// ── Warum es diese Pruefung gibt ────────────────────────────────────
//
// Am 2026-10-30 meldete der Betreiber FUENFMAL hintereinander, dass
// das Plaetze-Panel nicht aufgeht. Fuenf verschiedene Ursachen, und
// keine war die, die ich beim vorigen Mal vermutet hatte -- jede
// kostete eine CI-Runde, einen Bau und einen Neustart bei ihm:
//
//   1. `isEffectivelyVisible()` gefragt -- der Merker stand True vom
//      vorigen, ins Leere gelaufenen Versuch.
//   2. `QWidget::isVisible()` gefragt -- wahr, aber das Applet steckte
//      in einer Spalte ohne Breite.
//   3. „steht die Spalte offen" gefragt -- ein Panel kann in offener
//      Spalte ausgeblendet sein.
//   4. `meldeStand()` stand hinter `if (!m_arbeit) return;` -- die
//      Automatik bekam ueberhaupt nichts.
//   5. Das Fenster kam, aber `applyDefaultSize()` fragt `sizeHint()`
//      und nicht `minimumHeight()`, und ein verstecktes Widget zaehlt
//      im Layout nicht mit. Es ging als Streifen auf.
//
// Alle fuenf haetten in einem Lauf wie diesem hier gestanden. Es gab
// nur keinen: `tst_ft8_plaetze_panel` prueft die Flaeche,
// `tst_panel_automatik` die drei Regeln -- den ANSCHLUSS zwischen
// beiden fuhr niemand. Schlimmer: er WAR nicht fahrbar, weil
// `QStandardPaths::isTestModeEnabled()` am Anfang des Anschlusses
// stand und die ganze Automatik in jedem Lauf abschaltete.
//
// Mit dem Tor beim Aufrufer statt in der Entscheidung ist der Weg
// fahrbar, und beim ersten Lauf fiel sofort der SECHSTE Grund heraus:
//
//   zeigeFt8SlotsFenster() -> ensureVisible() -> visibilityChanged
//   (feuert dort IMMER) -> betreiberHatGegriffen()
//
// Die Automatik machte das Panel genau einmal auf und schaltete sich
// dabei im selben Durchgang selbst ab. Genau das Verhalten, das der
// Vorgaenger `m_ft8SlotsSchonGezeigt` hatte und das dieser Weg auf
// Wunsch des Betreibers ersetzen sollte („schoen waere, wenn hier die
// fenster eigens aufgehen und danach wieder zu gehen").
//
// Diese Pruefung faengt bei der Groesse an, die der Betreiber meint:
// HAT es ein eigenes Fenster. Nicht der Merker, nicht Qt.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>

#include "gui/MainWindow.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/applets/PanelAutomatik.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstFt8PlaetzeWeg : public QObject
{
    Q_OBJECT

private slots:
    // Regel 1 UND Regel 2 am Stueck: ein QSO macht das Fenster auf,
    // nach der Nachlaufzeit geht es wieder zu. Das war der Wunsch des
    // Betreibers, und bis zur Durchsicht des 2026-10-30 lief nur die
    // erste Haelfte -- der zweite Teil dieses Laufs war rot.
    void einQsoMachtDasFensterAufUndDanachWiederZu()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));

        // Ab Werk hat es kein eigenes Fenster.
        QVERIFY(!mw->ft8SlotsHatFensterForTest());
        QVERIFY(!mw->ft8SlotsGehoertDemBetreiberForTest());

        // Ein QSO laeuft.
        qint64 t = 1'000'000;
        mw->ft8SlotsAutomatikSchrittForTest(true, t);
        QTRY_VERIFY_WITH_TIMEOUT(mw->ft8SlotsHatFensterForTest(),
                                 kFensterFrist);

        // ── Der Kern dieses Laufs ──────────────────────────────
        //
        // Das Panel darf dem Betreiber NICHT gehoeren: er hat nichts
        // angefasst, die Automatik hat selbst geschaltet. Stand hier
        // True, war die Automatik nach dem ersten Aufmachen tot --
        // genau der Fehler vom 2026-10-30.
        QVERIFY(!mw->ft8SlotsGehoertDemBetreiberForTest());

        // Kein QSO mehr. Der erste leere Durchgang merkt nur den
        // Augenblick; das Fenster bleibt stehen.
        t += 1000;
        mw->ft8SlotsAutomatikSchrittForTest(false, t);
        QVERIFY(mw->ft8SlotsHatFensterForTest());

        // Innerhalb der Nachlaufzeit bleibt es stehen -- zwischen zwei
        // FT8-QSOs liegt eine halbe Minute, kuerzer liesse das Fenster
        // im Takt des Bandes blinken.
        t += PanelAutomatik::kNachlaufMs - 1000;
        mw->ft8SlotsAutomatikSchrittForTest(false, t);
        QVERIFY(mw->ft8SlotsHatFensterForTest());

        // Danach geht es zu.
        t += 2000;
        mw->ft8SlotsAutomatikSchrittForTest(false, t);
        QTRY_VERIFY_WITH_TIMEOUT(!mw->ft8SlotsHatFensterForTest(),
                                 kFensterFrist);

        // Und auch das Zumachen war unser Griff, nicht seiner.
        QVERIFY(!mw->ft8SlotsGehoertDemBetreiberForTest());

        delete mw;
    }

    // Und dann wieder auf, sobald der Naechste ruft. Ohne den Merker
    // war genau das unmoeglich: das erste Aufmachen hatte die
    // Automatik abgeschaltet.
    void nachDemZumachenGehtEsWiederAuf()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));

        qint64 t = 2'000'000;
        mw->ft8SlotsAutomatikSchrittForTest(true, t);
        QTRY_VERIFY_WITH_TIMEOUT(mw->ft8SlotsHatFensterForTest(),
                                 kFensterFrist);

        t += 1000;
        mw->ft8SlotsAutomatikSchrittForTest(false, t);
        t += PanelAutomatik::kNachlaufMs + 1000;
        mw->ft8SlotsAutomatikSchrittForTest(false, t);
        QTRY_VERIFY_WITH_TIMEOUT(!mw->ft8SlotsHatFensterForTest(),
                                 kFensterFrist);

        // Der Naechste ruft.
        t += 30'000;
        mw->ft8SlotsAutomatikSchrittForTest(true, t);
        QTRY_VERIFY_WITH_TIMEOUT(mw->ft8SlotsHatFensterForTest(),
                                 kFensterFrist);

        delete mw;
    }

    // Regel 3 muss trotzdem greifen: wer selbst greift, bestimmt. Der
    // Merker darf nicht so breit sein, dass er auch den echten Griff
    // des Betreibers verschluckt.
    void einGriffDesBetreibersZaehltWeiterhin()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));

        // Vorzustand herstellen: die Faelle teilen EINE
        // Einstellungsdatei, und `setVisible` schweigt, wenn der Wert
        // schon steht. Erst aus, dann die Automatik von vorn -- sonst
        // zaehlt dieses Ausschalten selbst schon als sein Griff.
        auto* vis = mw->appletVisibilityForTest();
        QVERIFY(vis);
        vis->setVisible(QStringLiteral("Ft8Slots"), false);
        mw->ft8SlotsAutomatikZuruecksetzenForTest();
        QVERIFY(!mw->ft8SlotsGehoertDemBetreiberForTest());

        // Er schaltet das Panel selbst ein -- so, wie es der
        // Auswaehler im Panelkopf tut.
        vis->setVisible(QStringLiteral("Ft8Slots"), true);
        QVERIFY(mw->ft8SlotsGehoertDemBetreiberForTest());

        // Ab jetzt ruehrt die Automatik es nicht mehr an: kein QSO
        // macht es auf, keine Nachlaufzeit macht es zu.
        const bool vorher = mw->ft8SlotsHatFensterForTest();
        mw->ft8SlotsAutomatikSchrittForTest(true, 3'000'000);
        QCOMPARE(mw->ft8SlotsHatFensterForTest(), vorher);

        delete mw;
    }

    // Das Fenster muss hoch genug sein, dass eine Kachel hineinpasst.
    // Grund 5 vom 2026-10-30: es ging als Streifen auf, weil
    // `applyDefaultSize()` `sizeHint()` liest und ein verstecktes
    // Widget im Layout nicht mitzaehlt. Der Betreiber sah ein Fenster
    // ohne Inhalt und meldete „wieder kein eigenes fenster".
    void dasFensterIstHochGenugFuerEineKachel()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));

        mw->ft8SlotsAutomatikSchrittForTest(true, 4'000'000);
        QTRY_VERIFY_WITH_TIMEOUT(mw->ft8SlotsHatFensterForTest(),
                                 kFensterFrist);

        auto* fenster = mw->ft8SlotsFensterForTest();
        QVERIFY(fenster);
        // Eine Kachel ist 92 Punkt hoch (`Ft8PlaetzeFlaeche::kKachelH`),
        // dazu Kopf und Rand. Unter 100 ist es ein Streifen.
        QVERIFY2(fenster->height() >= 100,
                 qPrintable(QStringLiteral("Fensterhoehe %1 -- das ist ein "
                                           "Streifen, keine Kachel")
                                .arg(fenster->height())));

        delete mw;
    }
};

QTEST_MAIN(TstFt8PlaetzeWeg)
#include "tst_ft8_plaetze_weg.moc"
