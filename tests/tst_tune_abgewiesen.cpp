// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Ein abgewiesenes TUNE setzt keine Merker (2026-11-08) ─────────────────
//
// Thetis console.cs:30061-30069 [@852bf0e] setzt _manual_mox und den
// PTT-Modus erst, NACHDEM MOX wirklich steht (`if (!_mox) return;`).
// Longpath setzt sie davor (Reihenfolge, siehe MoxController::setTune) und
// hatte keinen Rueckweg: ein abgewiesenes TUNE meldete manualMoxChanged(true),
// RadioModel gab das als "transmit tune=1" an TGXL/PGXL weiter.
//
//   1. Bandplan sagt nein: kein Merker, kein manualMoxChanged, PTT bleibt.
//   2. Nein erst in setMox (eine spaetere Sperre): Merker zurueck.
//   3. Erlaubt: wie bisher -- MOX, Handbetrieb, PTT manuell.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "core/MoxController.h"
#include "core/safety/BandPlanGuard.h"

using namespace Longpath;

class TstTuneAbgewiesen : public QObject {
    Q_OBJECT
private slots:
    void abgewiesenSetztKeineMerker()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setMoxCheck([]() -> safety::BandPlanGuard::MoxCheckResult {
            return {false, QStringLiteral("nein")};
        });
        const PttMode vorher = ctrl.pttMode();
        QSignalSpy abgewiesen(&ctrl, &MoxController::moxRejected);
        QSignalSpy handbetrieb(&ctrl, &MoxController::manualMoxChanged);

        ctrl.setTune(true);

        QCOMPARE(abgewiesen.size(), 1);
        QVERIFY2(handbetrieb.isEmpty(), "ein abgewiesenes TUNE meldete Handbetrieb");
        QVERIFY(!ctrl.isManualMox());
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), vorher);
    }

    void spaetAbgewiesenNimmtDieMerkerZurueck()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        int fragen = 0;
        // Die erste Frage (in setTune) sagt ja, die zweite (in setMox) nein --
        // so wie eine Sperre, die erst beim Tasten greift.
        ctrl.setMoxCheck([&fragen]() -> safety::BandPlanGuard::MoxCheckResult {
            ++fragen;
            return {fragen == 1, QStringLiteral("spaet")};
        });
        const PttMode vorher = ctrl.pttMode();
        QSignalSpy handbetrieb(&ctrl, &MoxController::manualMoxChanged);

        ctrl.setTune(true);

        QVERIFY(!ctrl.isMox());
        QVERIFY2(!ctrl.isManualMox(), "Handbetrieb blieb nach verweigertem MOX stehen");
        QCOMPARE(ctrl.pttMode(), vorher);
        QCOMPARE(handbetrieb.size(), 2);
        QCOMPARE(handbetrieb.at(0).at(0).toBool(), true);
        QCOMPARE(handbetrieb.at(1).at(0).toBool(), false);
    }

    void erlaubtWieBisher()
    {
        MoxController ctrl;
        ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
        ctrl.setMoxCheck([]() -> safety::BandPlanGuard::MoxCheckResult {
            return {true, {}};
        });
        ctrl.setTune(true);
        QVERIFY(ctrl.isMox());
        QVERIFY(ctrl.isManualMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Manual);
        ctrl.setTune(false);
        QVERIFY(!ctrl.isMox());
        QVERIFY(!ctrl.isManualMox());
    }
};

QTEST_GUILESS_MAIN(TstTuneAbgewiesen)
#include "tst_tune_abgewiesen.moc"
