// SPDX-License-Identifier: GPL-3.0-or-later
//
// KiwiSDR Stufe 3b (2026-09-21): der Kiwi folgt der zugeordneten
// Scheibe. Bis dahin wurde er genau einmal abgestimmt -- bei der
// Zuordnung -- und danach nie wieder: KiwiSdrManager::updateSliceTracking
// war aus AetherSDR portiert, aber im MainWindow nirgends aufgerufen
// (Aether haengt es in MainWindow_Wiring.cpp an frequency/mode/filter/
// panId jeder Scheibe). Dazu bekam der Kiwi bei der Zuordnung die
// CW-Tonhoehe 0, womit der Traeger auf 0 Hz und damit ausserhalb jedes
// CW-Durchlasses lag.
//
// Beobachtet wird die Kante KiwiSdrManager::sliceTrackingUpdated -- der
// Client selbst lebt auf seinem eigenen Faden und schickt erst, wenn
// er verbunden ist (hier: nie, kiwi.example.at gibt es nicht).
//
// Longpath-eigener Pruefstand, kein Port.
#include <QtTest>
#include <QScopeGuard>
#include <QSignalSpy>

#include "core/KiwiSdrManager.h"
#include "gui/MainWindow.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

void clearProfiles(KiwiSdrManager* mgr)
{
    if (!mgr) { return; }
    const QVector<KiwiSdrAntennaProfile> existing = mgr->profiles();
    for (const KiwiSdrAntennaProfile& p : existing) {
        mgr->removeProfile(p.id);
    }
}

} // namespace

class TstKiwiTrackingFollowsTheSlice : public QObject
{
    Q_OBJECT

private slots:
    void abstimmungBetriebsartUndFilterFolgenDerScheibe()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });

        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        QVERIFY(mgr);
        clearProfiles(mgr);
        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);

        QSignalSpy tracked(mgr, &KiwiSdrManager::sliceTrackingUpdated);
        mw->addKiwiSdrReceiverForTest(QStringLiteral("Gmunden"),
                                      QStringLiteral("kiwi.example.at:8073"));
        const QVector<KiwiSdrAntennaProfile> profiles = mgr->profiles();
        QVERIFY(!profiles.isEmpty());
        const int sliceId = mgr->assignedSliceForProfile(profiles.first().id);
        QVERIFY2(sliceId >= 0, "nicht zugeordnet -- der Rest prueft nichts");
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);

        // Die Zuordnung selbst ist die erste Nachfuehrung -- mit der echten
        // CW-Tonhoehe, nicht mit 0.
        QCOMPARE(tracked.count(), 1);
        QCOMPARE(tracked.last().at(0).toInt(), sliceId);
        QCOMPARE(tracked.last().at(5).toInt(), SliceModel::cwPitchHz());
        QVERIFY(SliceModel::cwPitchHz() >= 100);

        // Drehen: die neue Frequenz geht an den Kiwi.
        tracked.clear();
        slice->setFrequency(10.144e6);
        QVERIFY2(tracked.count() >= 1, "die Frequenz wurde dem Kiwi nicht nachgefuehrt");
        QCOMPARE(tracked.last().at(0).toInt(), sliceId);
        QCOMPARE(tracked.last().at(1).toDouble(), 10.144);

        // Betriebsart CWU: Modusname und der tonhoehen-zentrierte Filter
        // der Scheibe gehen mit -- genau die Zahlen, die der Client dann
        // traegersymmetrisch macht (KiwiSdrProtocol::carrierSymmetricCwPassband).
        tracked.clear();
        slice->setDspMode(DSPMode::CWU);
        QVERIFY2(tracked.count() >= 1, "die Betriebsart wurde dem Kiwi nicht nachgefuehrt");
        QCOMPARE(tracked.last().at(2).toString(), QStringLiteral("CWU"));
        QCOMPARE(tracked.last().at(3).toInt(), slice->filterLow());
        QCOMPARE(tracked.last().at(4).toInt(), slice->filterHigh());
        QCOMPARE(tracked.last().at(5).toInt(), SliceModel::cwPitchHz());

        // Filter drehen: die neuen Flanken gehen mit.
        tracked.clear();
        const int pitch = SliceModel::cwPitchHz();
        slice->setFilter(pitch - 100, pitch + 100);
        QVERIFY2(tracked.count() >= 1, "der Filter wurde dem Kiwi nicht nachgefuehrt");
        QCOMPARE(tracked.last().at(3).toInt(), slice->filterLow());
        QCOMPARE(tracked.last().at(4).toInt(), slice->filterHigh());

        // Zuordnung geloest: die Scheibe steuert den Kiwi nicht mehr.
        mgr->clearSliceAssignment(sliceId);
        tracked.clear();
        slice->setFrequency(7.030e6);
        QCOMPARE(tracked.count(), 0);
    }

    void eineUebernommeneScheibeSteuertDenKiwiNichtMehr()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));
        auto closeGuard = qScopeGuard([&]{ mw->close(); });

        KiwiSdrManager* mgr = mw->kiwiSdrManagerForTest();
        QVERIFY(mgr);
        clearProfiles(mgr);
        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);

        mw->addKiwiSdrReceiverForTest(QStringLiteral("Gmunden"),
                                      QStringLiteral("kiwi.example.at:8073"));
        const QVector<KiwiSdrAntennaProfile> profiles = mgr->profiles();
        QVERIFY(!profiles.isEmpty());
        const int sliceId = mgr->assignedSliceForProfile(profiles.first().id);
        QVERIFY2(sliceId >= 0, "nicht zugeordnet -- der Rest prueft nichts");
        SliceModel* slice = model->sliceById(sliceId);
        QVERIFY(slice);

        // Ein echtes Funkgeraet uebernimmt die Scheibe: dieselbe Schranke
        // wie fuer Ton und Wasserfall (kiwiControllableSlice) -- keine
        // Nachfuehrung mehr, selbst wenn die Zuordnung noch stuende.
        QSignalSpy tracked(mgr, &KiwiSdrManager::sliceTrackingUpdated);
        slice->setStreamIndex(0);
        tracked.clear();
        slice->setFrequency(14.060e6);
        QCOMPARE(tracked.count(), 0);
    }
};

QTEST_MAIN(TstKiwiTrackingFollowsTheSlice)
#include "tst_kiwi_tracking_follows_the_slice.moc"
