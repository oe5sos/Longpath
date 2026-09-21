// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_radio_status_follows_mox.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// RadioStatus muss vom Senden erfahren. Bis 2026-09-21 rief kein
// Produktionscode setActivePttSource()/setTransmitting() — die
// Diagnoseseite „Radio Status" zeigte bei jedem Senden „— W" und
// „RX (idle)", die PTT-Quelle blieb „none", die Ereignisliste leer.
// Aufgefallen an der Simulator-Werkbank (tst_hpsdr_sim_workbench).
//
// Die Verdrahtung haengt am MoxController::moxStateChanged des Modells.
// Ohne Geraet laesst sich der Controller nicht bis Tx fahren, also wird
// hier das Signal selbst ausgeloest — geprueft wird die Abbildung auf
// RadioStatus, nicht der Controller.

#include <QtTest>

#include "core/MoxController.h"
#include "core/PttSource.h"
#include "core/RadioStatus.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

class TstRadioStatusFollowsMox : public QObject { Q_OBJECT
private slots:
    void moxEngageAndRelease()
    {
        RadioModel model;
        const RadioStatus& rs = model.radioStatus();
        QVERIFY(!rs.isTransmitting());

        emit model.moxController()->moxStateChanged(true);
        QVERIFY(rs.isTransmitting());
        QCOMPARE(rs.activePttSource(), PttSource::Mox);

        emit model.moxController()->moxStateChanged(false);
        QVERIFY(!rs.isTransmitting());
        QCOMPARE(rs.activePttSource(), PttSource::None);
        QVERIFY(!rs.recentPttEvents().isEmpty());
    }

    void tuneIsReportedAsTune()
    {
        RadioModel model;
        model.transmitModel().setTune(true);
        emit model.moxController()->moxStateChanged(true);
        QVERIFY(model.radioStatus().isTransmitting());
        QCOMPARE(model.radioStatus().activePttSource(), PttSource::Tune);
        emit model.moxController()->moxStateChanged(false);
        model.transmitModel().setTune(false);
        QVERIFY(!model.radioStatus().isTransmitting());
    }

    void pttModeNamesTheSource()
    {
        RadioModel model;
        model.moxController()->setPttMode(PttMode::Cat);
        emit model.moxController()->moxStateChanged(true);
        QCOMPARE(model.radioStatus().activePttSource(), PttSource::Cat);
        emit model.moxController()->moxStateChanged(false);

        model.moxController()->setPttMode(PttMode::Vox);
        emit model.moxController()->moxStateChanged(true);
        QCOMPARE(model.radioStatus().activePttSource(), PttSource::Vox);
        emit model.moxController()->moxStateChanged(false);
    }
};

QTEST_MAIN(TstRadioStatusFollowsMox)
#include "tst_radio_status_follows_mox.moc"
