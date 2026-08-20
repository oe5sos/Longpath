// tests/tst_rx_dashboard.cpp
//
// Phase 3Q Sub-PR-5 (E.1) — RxDashboard widget tests.
//
// 4 tests: unbound construction, bind, rebind, active-only badge visibility.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "gui/widgets/RxDashboard.h"
#include "gui/widgets/StatusBadge.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstRxDashboard : public QObject {
    Q_OBJECT

private slots:

    void unboundDoesNotCrash() {
        RxDashboard d;
        // No slice bound — widget constructs cleanly with placeholder state.
        QVERIFY(d.slice() == nullptr);
    }

    void bindSliceDoesNotCrash() {
        RxDashboard d;
        SliceModel slice;
        d.bindSlice(&slice);
        QCOMPARE(d.slice(), &slice);
    }

    void rebindDisconnectsOldSlice() {
        RxDashboard d;
        SliceModel a;
        SliceModel b;
        d.bindSlice(&a);
        d.bindSlice(&b);
        QCOMPARE(d.slice(), &b);
        // Verify a no longer drives the dashboard (no crash on destroy)
    }

    void activeOnlyBadgesHiddenWhenFeaturesOff() {
        // Task A8 fix round 1: RxDashboard no longer setVisible()'s these
        // pills itself (ChromeBarController owns visibility once
        // registered); it reports DSP-active state via
        // badgeAvailabilityChanged instead. isVisible() is not a
        // meaningful check here any more -- and was already a weak one
        // even before this change, since an unshown, unparented top-level
        // RxDashboard reports isVisible()==false for EVERY child
        // regardless of intent, mode/filter/AGC included, not just the
        // 4 that are meant to start off.
        RxDashboard d;
        QSignalSpy spy(&d, &RxDashboard::badgeAvailabilityChanged);
        SliceModel slice;
        d.bindSlice(&slice);
        // Default SliceModel state: NR=Off, NB=Off, APF=off, ssql=off
        // → each of the 4 active-only rungs reports available=false
        // during the bind-time seed pass. AGC has no off state.
        QHash<int, bool> lastByRung;
        for (const QList<QVariant>& call : spy) {
            lastByRung[call.at(0).toInt()] = call.at(1).toBool();
        }
        QVERIFY(lastByRung.contains(5));  // SQL
        QVERIFY(lastByRung.contains(6));  // APF
        QVERIFY(lastByRung.contains(7));  // NB
        QVERIFY(lastByRung.contains(8));  // NR
        QCOMPARE(lastByRung.value(5), false);
        QCOMPARE(lastByRung.value(6), false);
        QCOMPARE(lastByRung.value(7), false);
        QCOMPARE(lastByRung.value(8), false);
    }

    void sliceLetterRoundTrips() {
        RxDashboard d;
        d.setSliceLetter(QLatin1Char('B'));
        QCOMPARE(d.sliceLetter(), QLatin1Char('B'));
    }

    void badgeForRungMapsTheLadder() {
        RxDashboard d;
        QVERIFY(d.badgeForRung(5) != nullptr);   // SQL
        QVERIFY(d.badgeForRung(6) != nullptr);   // APF
        QVERIFY(d.badgeForRung(7) != nullptr);   // NB
        QVERIFY(d.badgeForRung(8) != nullptr);   // NR
        QVERIFY(d.badgeForRung(9) != nullptr);   // AGC
    }

    void modeAndFilterAreNotOnTheLadder() {
        RxDashboard d;
        // Rungs 0 through 4 belong to other banner items; the dashboard
        // must not claim mode or filter, which never fold.
        for (int rung = 0; rung <= 4; ++rung) {
            QCOMPARE(d.badgeForRung(rung), nullptr);
        }
        // 10..12 hat diese Pruefung bis 2026-08-17 ebenfalls als „nicht
        // unsere" gefuehrt. Sie sind es seither: VAX, ANT und RIT sind
        // dazugekommen, weil sie beim Wegfallen der VFO-Flagge sonst
        // nirgends mehr staenden (OE5SOS). Die Zusicherung wandert
        // deshalb eine Sprosse hoeher — ueber 12 gehoert dem Dashboard
        // weiterhin nichts.
        for (int rung = 13; rung <= 20; ++rung) {
            QCOMPARE(d.badgeForRung(rung), nullptr);
        }
    }

    // Die drei Zugaenge sind auf der Leiter, und zwar in dieser
    // Reihenfolge. Niedrige Rungs falten zuerst: VAX (ein Laempchen)
    // vor ANT (beim Bandwechsel gebraucht) vor RIT (verschiebt still
    // die Frequenz). Vertauscht sie jemand, faellt das hier auf.
    void theFourNewPillsSitWhereTheyWereDecided() {
        RxDashboard d;
        QVERIFY(d.badgeForRung(10) != nullptr);   // VAX
        QVERIFY(d.badgeForRung(11) != nullptr);   // ANT
        QVERIFY(d.badgeForRung(12) != nullptr);   // RIT
        QVERIFY(d.badgeForRung(10) != d.badgeForRung(11));
        QVERIFY(d.badgeForRung(11) != d.badgeForRung(12));
    }

    // ── Und sie zeigen auch etwas ───────────────────────────────────
    //
    // Die Faelle oben pruefen die STELLE auf der Faltleiter. Seit die
    // VFO-Flagge am 2026-08-18 geloescht ist, sind diese Pillen die
    // EINZIGE Stelle, an der X/RIT und die VAX-Zuordnung ueberhaupt zu
    // sehen sind — vorher standen sie auch auf der Flagge. Eine Pille
    // an der richtigen Sprosse, die nichts anzeigt, faellt niemandem
    // auf; deshalb hier der Inhalt.

    // Die Pille sagt den ZUSTAND, nicht den Wert. Der Versatz in Hertz
    // steht in der RxApplet (m_ritLabel, „+250 Hz") — dort, wo man ihn
    // auch verstellt. Auf der Leiste, die faltet, waere eine Zahl
    // schlechter als ein Wort: „RIT" heisst „dein Empfang sitzt nicht,
    // wo du denkst", und das ist der Teil, der auf einen Blick gehoert.
    //
    // Beide zugleich muss unterscheidbar bleiben, sonst sieht „nur
    // RIT" aus wie „RIT und XIT".
    void ritAndXitShowTheirState() {
        RadioModel m;
        SliceModel* s = m.sliceById(0);
        if (!s) { s = m.sliceById(m.addSlice()); }
        QVERIFY(s);
        RxDashboard d;
        d.bindSlice(s);

        StatusBadge* rit = d.badgeForRung(12);
        QVERIFY(rit);
        QVERIFY2(!rit->isVisible() || rit->label().isEmpty(),
                 "ohne RIT und XIT darf die Pille nichts behaupten");

        s->setRitEnabled(true);
        s->setRitHz(250);
        QCOMPARE(rit->label(), QStringLiteral("RIT"));

        s->setXitEnabled(true);
        QCOMPARE(rit->label(), QStringLiteral("R+X"));

        s->setRitEnabled(false);
        QCOMPARE(rit->label(), QStringLiteral("XIT"));
    }

    void theVaxPillNamesItsChannel() {
        RadioModel m;
        SliceModel* s = m.sliceById(0);
        if (!s) { s = m.sliceById(m.addSlice()); }
        QVERIFY(s);
        RxDashboard d;
        d.bindSlice(s);

        StatusBadge* vax = d.badgeForRung(10);
        QVERIFY(vax);

        s->setVaxChannel(2);
        QVERIFY2(vax->label().contains(QStringLiteral("2")),
                 qPrintable(QStringLiteral("VAX-Pille sagt %1")
                                .arg(vax->label())));

        s->setVaxChannel(0);
        QVERIFY2(vax->label().isEmpty() || !vax->isVisible(),
                 "ohne VAX-Kanal darf die Pille keinen behaupten");
    }

    void theAntennaPillNamesTheAntenna() {
        RadioModel m;
        SliceModel* s = m.sliceById(0);
        if (!s) { s = m.sliceById(m.addSlice()); }
        QVERIFY(s);
        RxDashboard d;
        d.bindSlice(s);

        s->setRxAntenna(QStringLiteral("ANT2"));
        StatusBadge* ant = d.badgeForRung(11);
        QVERIFY(ant);
        QCOMPARE(ant->label(), QStringLiteral("ANT2"));
    }

    // TX faltet NIE — es sagt, welche Scheibe sendet, und das ist eine
    // Sicherheitsangabe. Es darf deshalb auf keiner Sprosse stehen und
    // muss stattdessen in residualWidth mitzaehlen.
    void txNeverFolds() {
        RxDashboard d;
        for (int rung = 0; rung <= 20; ++rung) {
            StatusBadge* b = d.badgeForRung(rung);
            if (!b) { continue; }
            QVERIFY2(b->label() != QStringLiteral("TX"),
                     qPrintable(QStringLiteral(
                         "TX steht auf Sprosse %1 und wuerde falten")
                             .arg(rung)));
        }
    }

    void rebindingSwitchesTheObservedSlice() {
        SliceModel a(0);
        SliceModel b(1);
        RxDashboard d;
        d.bindSlice(&a);
        QCOMPARE(d.slice(), &a);
        d.bindSlice(&b);
        QCOMPARE(d.slice(), &b);
        // The old slice must no longer drive the badges.
        a.setDspMode(Longpath::DSPMode::CWU);
        QVERIFY(!d.modeText().contains(QStringLiteral("CW")));
    }
};

QTEST_MAIN(TstRxDashboard)
#include "tst_rx_dashboard.moc"
