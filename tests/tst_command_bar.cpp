// tests/tst_command_bar.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── Die Kopfleiste ───────────────────────────────────────────────────
//
// Ob sie huebsch ist, kann kein Test sagen. Drei Eigenschaften kann er,
// und an genau diesen scheitert so eine Leiste in der Praxis:
//
//   · Genau EINE Pille je Gruppe leuchtet. Zwei sind ein Widerspruch,
//     null heisst, dass der eingestellte Zustand nirgends steht.
//
//   · Sie folgt dem Modell, nicht dem Klick. Ein Knopf, der angeht,
//     egal was das Modell darunter macht, ist eine Luege mit
//     Rueckmeldung.
//
//   · Beim Wechsel der Empfangskette laesst sie die alte los. Sonst
//     haengt sie an beiden und meldet abwechselnd deren Zustand — ein
//     Fehler, der erst beim zweiten Pan auffaellt und dann wie ein
//     Wackelkontakt aussieht.

#include <QtTest>

#include "gui/widgets/CommandBar.h"
#include "models/SliceModel.h"
#include "gui/widgets/DspParamPopup.h"
#include "gui/widgets/DspQuickPopups.h"

#include <QPushButton>
#include <QSlider>

using namespace Longpath;

class TestCommandBar : public QObject
{
    Q_OBJECT

private slots:
    void theGroupsAreNamedAndInOrder()
    {
        // Filter und NR kamen am 2026-08-21 dazu, auf Ansage mit
        // Vorlage: „bitte folgende sachen in die leiste oben!
        // bandbreite ... NR1-NR4 eben so."
        //
        // Filter steht neben Modus, weil beide dasselbe Signal
        // beschreiben; NR ans Ende, weil sie eine Behandlung ist und
        // keine Eigenschaft.
        CommandBar bar;
        const QStringList g = bar.groups();
        QCOMPARE(g.size(), 4);
        QCOMPARE(g.at(0), QStringLiteral("Mode"));
        QCOMPARE(g.at(1), QStringLiteral("Filter"));
        QCOMPARE(g.at(2), QStringLiteral("Step"));
        QCOMPARE(g.at(3), QStringLiteral("NR"));
    }

    void threeVisibleEntriesNotThirteen()
    {
        // Der ganze Punkt der Leiste gegenueber der Filterwand im
        // RX-Panel: drei vorne, der Rest im Menue.
        CommandBar bar;
        QCOMPARE(bar.pillsIn(QStringLiteral("Mode")).size(),
                 CommandBar::kVisiblePerGroup);
        QCOMPARE(bar.pillsIn(QStringLiteral("Step")).size(),
                 CommandBar::kVisiblePerGroup);
        QCOMPARE(bar.pillsIn(QStringLiteral("Filter")).size(),
                 CommandBar::kVisiblePerGroup);
        QCOMPARE(bar.pillsIn(QStringLiteral("NR")).size(),
                 CommandBar::kVisiblePerGroup);
    }

    void withoutASliceNothingIsLit()
    {
        // Eine Leiste ohne Empfangskette darf keinen Zustand behaupten.
        CommandBar bar;
        QVERIFY(bar.activePill(QStringLiteral("Mode")).isEmpty());
        QVERIFY(bar.activePill(QStringLiteral("Step")).isEmpty());
    }

    void attachingShowsWhatTheModelSays()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::USB);
        slice.setStepHz(100);

        CommandBar bar;
        bar.attach(&slice);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("USB"));
        QCOMPARE(bar.activePill(QStringLiteral("Step")),
                 QStringLiteral("100 Hz"));
    }

    void aClickReachesTheModel()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::USB);
        CommandBar bar;
        bar.attach(&slice);

        QVERIFY(bar.clickPill(QStringLiteral("Mode"), QStringLiteral("LSB")));
        QCOMPARE(slice.dspMode(), DSPMode::LSB);
    }

    // ── Der Weg zurueck, und der ist der wichtigere ──────────────────
    void aChangeMadeElsewhereMovesTheBar()
    {
        // Der Modus laesst sich auch ueber das Menue, per CAT und per
        // Bandwechsel aendern. Eine Leiste, die nur ihren eigenen Klick
        // kennt, steht dann falsch da.
        SliceModel slice;
        slice.setDspMode(DSPMode::LSB);
        CommandBar bar;
        bar.attach(&slice);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("LSB"));

        slice.setDspMode(DSPMode::USB);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("USB"));
    }

    void exactlyOnePillIsLitPerGroup()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::CWL);
        slice.setStepHz(1000);
        CommandBar bar;
        bar.attach(&slice);

        // NR ist ausgenommen, und das ist kein Schoenheitsfehler:
        // Modus, Schrittweite und Filter haben IMMER einen Wert, die
        // Rauschminderung hat einen Aus-Zustand (NrSlot::Off). Zu
        // verlangen, dass dort immer eine Pille leuchtet, hiesse zu
        // verlangen, dass immer eine Rauschminderung laeuft.
        //
        // Der Test hat beim Einbau von NR am 2026-08-21 genau darauf
        // hingewiesen — und vorher schon einen echten Entwurfsfehler
        // bei den Filterpillen aufgedeckt.
        for (const QString& g : bar.groups()) {
            if (g == QStringLiteral("NR")) { continue; }
            int lit = 0;
            for (const QString& p : bar.pillsIn(g)) {
                if (bar.activePill(g) == p) { ++lit; }
            }
            QVERIFY2(lit == 1,
                     qPrintable(QStringLiteral("Gruppe %1: %2 Pillen an")
                                    .arg(g).arg(lit)));
        }
    }

    // ── Ein Modus, der nicht zu den ersten dreien gehoert ────────────
    void aModeOutsideTheFirstThreeTakesTheLastSlot()
    {
        // Sonst zeigt die Leiste drei Pillen, von denen keine an ist,
        // und der eingestellte Modus steht nirgends auf dem Schirm.
        SliceModel slice;
        slice.setDspMode(DSPMode::AM);      // Platz 5 der Gesamtliste
        CommandBar bar;
        bar.attach(&slice);

        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("AM"));
        QVERIFY2(bar.pillsIn(QStringLiteral("Mode"))
                     .contains(QStringLiteral("AM")),
                 "AM ist eingeschaltet, steht aber auf keiner Pille");
        // Und es bleiben drei. Die Leiste waechst nicht.
        QCOMPARE(bar.pillsIn(QStringLiteral("Mode")).size(),
                 CommandBar::kVisiblePerGroup);
    }

    // ── Der Wackelkontakt ────────────────────────────────────────────
    void switchingSlicesLetsGoOfTheOldOne()
    {
        SliceModel a;
        SliceModel b;
        a.setDspMode(DSPMode::LSB);
        b.setDspMode(DSPMode::USB);

        CommandBar bar;
        bar.attach(&a);
        bar.attach(&b);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("USB"));

        // Die alte Kette darf die Leiste nicht mehr bewegen.
        a.setDspMode(DSPMode::CWL);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("USB"));

        // Die neue schon.
        b.setDspMode(DSPMode::CWU);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("CWU"));
    }

    void detachingIsAllowedAndQuiet()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::USB);
        CommandBar bar;
        bar.attach(&slice);
        bar.attach(nullptr);
        // Kein Absturz, und die alte Kette bewegt nichts mehr.
        slice.setDspMode(DSPMode::LSB);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("USB"));
    }

    /// Die drei vorderen Breiten sind 2,9 / 3,2 / 3,8 — und eine
    /// Filterpille setzt wirklich den Filter.
    ///
    /// Sie sind KEINE Presets, sondern Breiten aus den Einstellungen
    /// („CommandBarFilterFront"). Der Betreiber hat die Auswahl am
    /// 2026-08-21 innerhalb weniger Minuten zweimal geaendert; waere
    /// sie fest verdrahtet, waere jede Meinungsaenderung eine
    /// Uebersetzung.
    void theThreeFrontWidthsAreTheOnesAsked()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::LSB);

        CommandBar bar;
        bar.attach(&slice);

        const QStringList labels = bar.pillsIn(QStringLiteral("Filter"));
        QCOMPARE(labels.size(), 3);
        QCOMPARE(labels.at(0), QStringLiteral("2.9k"));
        QCOMPARE(labels.at(1), QStringLiteral("3.2k"));
        QCOMPARE(labels.at(2), QStringLiteral("3.8k"));
    }

    /// Ein Klick setzt die Breite und laesst die MITTE stehen.
    ///
    /// Sonst spraenge ein LSB-Filter auf die andere Seite des
    /// Traegers, sobald man eine Breite waehlt — aus -2900…0 wuerde
    /// -1600…+1600.
    void aFilterPillKeepsTheCentre()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::LSB);
        slice.setFilter(-2900, 0);

        CommandBar bar;
        bar.attach(&slice);

        const double centreBefore =
            (slice.filterLow() + slice.filterHigh()) / 2.0;

        QVERIFY(bar.clickPill(QStringLiteral("Filter"),
                              QStringLiteral("3.8k")));

        QCOMPARE(qAbs(slice.filterHigh() - slice.filterLow()), 3800);
        const double centreAfter =
            (slice.filterLow() + slice.filterHigh()) / 2.0;
        QVERIFY2(qAbs(centreAfter - centreBefore) <= 1.0,
                 qPrintable(QStringLiteral(
                     "Die Mitte ist gewandert: %1 -> %2")
                     .arg(centreBefore).arg(centreAfter)));
    }

    /// Beim Moduswechsel werden die Pillen NEU BESCHRIFTET.
    ///
    /// Feste Beschriftungen haetten in CW die Breiten von SSB
    /// angeboten — ein Knopf, der 2.9k sagt und 500 Hz schaltet.
    void theFilterPillsFollowTheMode()
    {
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);

        slice.setDspMode(DSPMode::LSB);
        const QStringList ssb = bar.pillsIn(QStringLiteral("Filter"));
        slice.setDspMode(DSPMode::CWL);
        const QStringList cw = bar.pillsIn(QStringLiteral("Filter"));

        QVERIFY2(ssb != cw,
                 qPrintable(QStringLiteral(
                     "SSB und CW zeigen dieselben Breiten: %1")
                     .arg(ssb.join(QLatin1Char('|')))));
    }

    /// NR schaltet ein — und ein zweiter Klick wieder aus.
    void anNrPillTogglesTheModel()
    {
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);

        QCOMPARE(slice.activeNr(), NrSlot::Off);

        QVERIFY(bar.clickPill(QStringLiteral("NR"), QStringLiteral("NR1")));
        QCOMPARE(slice.activeNr(), NrSlot::NR1);
        QCOMPARE(bar.activePill(QStringLiteral("NR")),
                 QStringLiteral("NR1"));

        // Derselbe Knopf noch einmal: das ist der einzige Weg zu
        // „keine", ohne einen achten Knopf „AUS".
        QVERIFY(bar.clickPill(QStringLiteral("NR"), QStringLiteral("NR1")));
        QCOMPARE(slice.activeNr(), NrSlot::Off);
        QVERIFY(bar.activePill(QStringLiteral("NR")).isEmpty());
    }

    /// Laeuft eine Rauschminderung, die nicht unter den ersten dreien
    /// ist, rueckt sie an die letzte sichtbare Stelle.
    ///
    /// Der erste Entwurf hatte hier eine EIGENE Regel: die Marke am
    /// „…" statt Nachruecken. Der bestehende Test
    /// exactlyOnePillIsLitPerGroup hat sie sofort verworfen — zu
    /// Recht, zwei Regeln fuer dieselbe Leiste sind eine zu viel.
    void anNrOutsideTheFirstThreeTakesTheLastSlot()
    {
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);

        slice.setActiveNr(NrSlot::DFNR);

        QCOMPARE(bar.activePill(QStringLiteral("NR")),
                 QStringLiteral("DFNR"));
        QCOMPARE(bar.pillsIn(QStringLiteral("NR")).last(),
                 QStringLiteral("DFNR"));

        // Und der nachgerueckte Knopf schaltet auch wirklich DFNR —
        // nicht das, was vorher an seiner Stelle stand. Die Nutzlast
        // haengt am Knopf, nicht an seiner Stelle in der Liste.
        slice.setActiveNr(NrSlot::Off);
        QVERIFY(bar.clickPill(QStringLiteral("NR"),
                              QStringLiteral("DFNR")));
        QCOMPARE(slice.activeNr(), NrSlot::DFNR);
    }

    /// Rechtsklick auf eine NR-Pille oeffnet ihre Schnellregler.
    ///
    /// Umgezogen aus tst_rx_applet_inherited am 2026-08-21, zusammen
    /// mit der Faehigkeit selbst. Die Begruendung von dort gilt
    /// unveraendert: der Rechtsklick oeffnet nicht die
    /// Einstellungsseite, sondern die drei bis fuenf Regler GENAU
    /// DIESER Minderung; die Einstellungsseite ist darin nur der
    /// Verweis ganz unten. Ohne diesen Weg waeren die Regler nur noch
    /// ueber das Setup-Menue erreichbar.
    void rightClickOnAnNrPillOpensItsQuickControls()
    {
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);

        QPushButton* nr1 = nullptr;
        for (QPushButton* b : bar.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("NR1")) { nr1 = b; break; }
        }
        QVERIFY2(nr1, "Keine NR1-Pille in der Leiste");

        emit nr1->customContextMenuRequested(QPoint(2, 2));

        DspParamPopup* popup = nullptr;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (auto* p = qobject_cast<DspParamPopup*>(w)) { popup = p; }
        }
        if (!popup) { popup = bar.findChild<DspParamPopup*>(); }

        QVERIFY2(popup,
                 "Rechtsklick oeffnet keinen Schnellregler — dann sind "
                 "die NR-Parameter nur noch ueber das Setup-Menue "
                 "erreichbar");
        QVERIFY2(!popup->findChildren<QSlider*>().isEmpty(),
                 "der Schnellregler hat keine Regler");
        popup->close();
    }

    /// Und aus dem Schnellregler heraus fuehrt weiter ein Weg auf die
    /// volle Einstellungsseite. Ebenfalls umgezogen aus
    /// tst_rx_applet_inherited (2026-08-21), Mechanismus unveraendert.
    void theQuickControlsStillLinkToTheFullSetupPage()
    {
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);

        QSignalSpy spy(&bar, &CommandBar::openNrSetupRequested);

        QPushButton* nr3 = nullptr;
        for (QPushButton* b : bar.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("NR3")) { nr3 = b; break; }
        }
        QVERIFY(nr3);
        emit nr3->customContextMenuRequested(QPoint(2, 2));

        DspParamPopup* popup = bar.findChild<DspParamPopup*>();
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (auto* p = qobject_cast<DspParamPopup*>(w)) { popup = p; }
        }
        QVERIFY(popup);

        QPushButton* more = nullptr;
        for (QPushButton* b : popup->findChildren<QPushButton*>()) {
            if (b && b->text().contains(QStringLiteral("More"))) { more = b; }
        }
        QVERIFY2(more, "kein Verweis auf die volle Einstellungsseite");
        more->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<NrSlot>(), NrSlot::NR3);
    }
};

QTEST_MAIN(TestCommandBar)
#include "tst_command_bar.moc"
