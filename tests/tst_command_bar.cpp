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

using namespace NereusSDR;

class TestCommandBar : public QObject
{
    Q_OBJECT

private slots:
    void theGroupsAreNamedAndInOrder()
    {
        CommandBar bar;
        const QStringList g = bar.groups();
        QCOMPARE(g.size(), 2);
        QCOMPARE(g.at(0), QStringLiteral("Mode"));
        QCOMPARE(g.at(1), QStringLiteral("Step"));
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

        for (const QString& g : bar.groups()) {
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
};

QTEST_MAIN(TestCommandBar)
#include "tst_command_bar.moc"
