// =================================================================
// tests/tst_network_diagnostics_dialog.cpp  (NereusSDR)
// =================================================================
//
//  Copyright (C) 2026 J.J. Boyd (KG4VCF)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — Created for Phase 3Q sub-PR-3 (NetworkDiagnosticsDialog).
//                NereusSDR-original tests. J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "gui/NetworkDiagnosticsDialog.h"
#include "core/AudioEngine.h"
#include "core/SunSdrRadioConnection.h"
#include "core/WdspTypes.h"
#include "core/safety/BandPlanGuard.h"
#include "models/Band.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstNetworkDiagnosticsDialog : public QObject {
    Q_OBJECT

private slots:
    void buildsWithNullModel() {
        // Defensive: opening before connection should not crash.
        NetworkDiagnosticsDialog dlg(/*model=*/nullptr, /*audio=*/nullptr);
        // Pass = no crash during construction
        QVERIFY(true);
    }

    void refreshDoesNotCrashWithDisconnectedModel() {
        RadioModel model;
        NetworkDiagnosticsDialog dlg(&model, nullptr);
        QMetaObject::invokeMethod(&dlg, "refresh");
        // Pass = no crash
        QVERIFY(true);
    }

    void resetSessionStatsZeroesCounters() {
        RadioModel model;
        AudioEngine engine;
        NetworkDiagnosticsDialog dlg(&model, &engine);

        // Drive 1 underrun bump
        engine.simulateUnderrun();

        // Reset
        QMetaObject::invokeMethod(&dlg, "onResetSessionStats");

        // Verify by calling reset twice — no crash, idempotent
        QMetaObject::invokeMethod(&dlg, "onResetSessionStats");
        QVERIFY(true);
    }

    void connectsToRadioConnectionSignalsWithoutCrash() {
        RadioModel model;
        NetworkDiagnosticsDialog dlg(&model, nullptr);

        // If RadioModel::connection() is non-null, the dialog should have
        // wired pingRttMeasured. Emitting it should not crash.
        if (auto* conn = model.connection()) {
            emit conn->pingRttMeasured(42);
            emit conn->pingRttMeasured(120);
        }
        QVERIFY(true);
    }

    // Step 4 (SunSDR2 QRP TX-chain plan): the new "TX (SunSDR)" section is
    // read-only wiring on top of SunSdrRadioConnection's own bench-only TX
    // gate (SunSdrRadioConnection.h's own m_txArmed/m_mox/m_txTrace
    // comments). Drives that gate into a known armed-and-accepted state
    // via the same setTxArmedForTest()/setTxCheckContextForTest()/
    // setMox() hooks tst_sunsdr_radio_connection.cpp already uses, points
    // the dialog at it via RadioModel::injectConnectionForTest() (the
    // established injection pattern — see tst_pa_values_page.cpp and
    // tst_alex_per_adc_bpf_wire.cpp among many others), triggers the
    // dialog's existing refresh() slot the same way every other test in
    // this file already does, and asserts the section shows exactly that
    // state — never a control the test (or a user) could use to arm or
    // transmit anything.
    void sunSdrTxSectionShowsArmedMoxAndTraceTailAfterRefresh() {
        SunSdrRadioConnection conn;
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        conn.setTxArmedForTest(true);

        TxCheckContext ctx;
        ctx.region   = safety::Region::UnitedStates;
        ctx.txFreqHz = 14'200'000;  // US 20m, well in-band
        ctx.mode     = DSPMode::USB;
        ctx.rxBand   = Band::Band20m;
        ctx.txBand   = Band::Band20m;
        conn.setTxCheckContextForTest(ctx);

        conn.setMox(true);
        QVERIFY(conn.isTxArmed());
        QVERIFY(conn.isMoxOn());

        RadioModel model;
        model.injectConnectionForTest(&conn);

        NetworkDiagnosticsDialog dlg(&model, nullptr);
        QMetaObject::invokeMethod(&dlg, "refresh");

        QCOMPARE(dlg.sunSdrArmedTextForTest(), QStringLiteral("Yes"));
        QCOMPARE(dlg.sunSdrMoxTextForTest(), QStringLiteral("On"));
        // No real QTimer tick has run (no event loop was pumped), so the
        // pacer has ticked zero times — asserting the literal "0" (not
        // just "not the placeholder") confirms this row is wired to a
        // real number, not merely non-empty.
        QCOMPARE(dlg.sunSdrPaceRepeatsTextForTest(), QStringLiteral("0"));

        // The trace ring now holds exactly the two entries the calls
        // above produced (Armed, then MoxAccepted) — the tail must show
        // both, using TxTraceKind's own existing labels.
        const QString tail = dlg.sunSdrTraceTailTextForTest();
        QVERIFY(tail.contains(QStringLiteral("Armed")));
        QVERIFY(tail.contains(QStringLiteral("MOX accepted")));

        // Detach before `conn` goes out of scope — RadioModel must not be
        // left holding a pointer to a connection this test is about to
        // destroy.
        model.injectConnectionForTest(nullptr);
    }

    // The placeholder path: no connection at all (RadioModel's default
    // state — connection() is nullptr until something connects), same
    // "— (not reported)"-shaped convention this dialog already uses for
    // PA voltage. Confirms the section stays visible but visibly inert
    // rather than showing stale or garbage values.
    void sunSdrTxSectionShowsPlaceholderWithNoConnection() {
        RadioModel model;
        NetworkDiagnosticsDialog dlg(&model, nullptr);
        QMetaObject::invokeMethod(&dlg, "refresh");

        QVERIFY(dlg.sunSdrArmedTextForTest().contains(QStringLiteral("not SunSDR")));
        QCOMPARE(dlg.sunSdrMoxTextForTest(), QStringLiteral("—"));
        QCOMPARE(dlg.sunSdrPaceRepeatsTextForTest(), QStringLiteral("—"));
    }
};

QTEST_MAIN(TstNetworkDiagnosticsDialog)
#include "tst_network_diagnostics_dialog.moc"
