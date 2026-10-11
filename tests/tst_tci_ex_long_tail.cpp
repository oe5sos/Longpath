// no-port-check: Longpath-original test for the Thetis v2.10.3.14 bespoke
// _ex long tail (2026-10-11).
//
// tst_tci_matrix_runner drives the same commands against
// TestMockRadioModel, which proves the PROTOCOL side: argument validation,
// wire format, which frame comes back as a response and which as a
// notification.  This file pins the PRODUCTION side -- that each shim
// reaches real RadioModel / SliceModel / StepAttenuatorController state --
// the same division of labour as tst_tci_radio_model_shims.
//
// Every shim is invoked through QMetaObject::invokeMethod, the path
// TciProtocol uses, so a missing Q_INVOKABLE fails here rather than
// silently at runtime against a real TCI client.
//
// Source of truth for the expected behaviour:
//   Thetis TCIServer.cs:4576-4997 [v2.10.3.15] (the handlers)
//   Thetis console.cs:12481 / :35485 / :52385 [v2.10.3.15] (the console state)

#include <QtTest/QtTest>

#include "core/StepAttenuatorController.h"
#include "core/TciProtocol.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "core/NbFamily.h"
#include "core/WdspTypes.h"

using namespace Longpath;

class TestTciExLongTail : public QObject {
    Q_OBJECT

private:
    // Two slices: slice 0 is Thetis's RX1 / VFO A, slice 1 its RX2 / VFO B.
    static void setupTwoSlices(RadioModel& model)
    {
        model.addSlice();
        model.addSlice();
    }

private slots:
    // ── Step attenuator ─────────────────────────────────────────────────────

    void step_att_enabled_roundtrips_through_controller()
    {
        RadioModel m;
        StepAttenuatorController ctrl;
        m.setStepAttController(&ctrl);

        QMetaObject::invokeMethod(&m, "setRxStepAttEnabled",
                                  Q_ARG(int, 0), Q_ARG(bool, true));
        QVERIFY(ctrl.stepAttEnabled());

        bool out = false;
        QMetaObject::invokeMethod(&m, "rxStepAttEnabled",
                                  Q_RETURN_ARG(bool, out), Q_ARG(int, 0));
        QVERIFY(out);

        QMetaObject::invokeMethod(&m, "setRxStepAttEnabled",
                                  Q_ARG(int, 0), Q_ARG(bool, false));
        QVERIFY(!ctrl.stepAttEnabled());
    }

    void step_att_without_controller_is_silent()
    {
        // A TCI client can probe before connectToRadio has installed the
        // controller.  The shim must answer, not crash.
        RadioModel m;
        QMetaObject::invokeMethod(&m, "setRxStepAttEnabled",
                                  Q_ARG(int, 0), Q_ARG(bool, true));
        bool out = true;
        QMetaObject::invokeMethod(&m, "rxStepAttEnabled",
                                  Q_RETURN_ARG(bool, out), Q_ARG(int, 0));
        QVERIFY(!out);

        int dB = -1;
        QMetaObject::invokeMethod(&m, "rxStepAttDb",
                                  Q_RETURN_ARG(int, dB), Q_ARG(int, 0));
        QCOMPARE(dB, 0);
    }

    void step_att_db_roundtrips_through_controller()
    {
        RadioModel m;
        StepAttenuatorController ctrl;
        ctrl.setMaxAttenuation(31);
        m.setStepAttController(&ctrl);

        QMetaObject::invokeMethod(&m, "setRxStepAttDb",
                                  Q_ARG(int, 0), Q_ARG(int, 12));
        QCOMPARE(ctrl.attenuatorDb(), 12);

        int out = 0;
        QMetaObject::invokeMethod(&m, "rxStepAttDb",
                                  Q_RETURN_ARG(int, out), Q_ARG(int, 0));
        QCOMPARE(out, 12);
    }

    void out_of_range_receiver_index_is_refused()
    {
        RadioModel m;
        StepAttenuatorController ctrl;
        ctrl.setMaxAttenuation(31);
        m.setStepAttController(&ctrl);

        QMetaObject::invokeMethod(&m, "setRxStepAttDb",
                                  Q_ARG(int, 0), Q_ARG(int, 7));
        QCOMPARE(ctrl.attenuatorDb(), 7);

        // Thetis checks rx < 0 || rx > 1 in every one of these handlers.
        QMetaObject::invokeMethod(&m, "setRxStepAttDb",
                                  Q_ARG(int, 2), Q_ARG(int, 21));
        QCOMPARE(ctrl.attenuatorDb(), 7);
    }

    // ── Preamp ──────────────────────────────────────────────────────────────

    void preamp_att_is_reported_negative()
    {
        RadioModel m;
        StepAttenuatorController ctrl;
        m.setStepAttController(&ctrl);

        // Thetis handleRxPreampAttEx refuses a positive argument and takes
        // Math.Abs of the rest; sendRxPreampAttEx negates on the way out.
        QMetaObject::invokeMethod(&m, "setRxPreampAttDb",
                                  Q_ARG(int, 0), Q_ARG(int, -20));
        QCOMPARE(ctrl.preampMode(), PreampMode::Minus20);

        int out = 0;
        QMetaObject::invokeMethod(&m, "rxPreampAttDb",
                                  Q_RETURN_ARG(int, out), Q_ARG(int, 0));
        QCOMPARE(out, -20);

        QMetaObject::invokeMethod(&m, "setRxPreampAttDb",
                                  Q_ARG(int, 0), Q_ARG(int, 0));
        QCOMPARE(ctrl.preampMode(), PreampMode::On);
        QMetaObject::invokeMethod(&m, "rxPreampAttDb",
                                  Q_RETURN_ARG(int, out), Q_ARG(int, 0));
        QCOMPARE(out, 0);
    }

    void preamp_off_step_value_is_refused()
    {
        RadioModel m;
        StepAttenuatorController ctrl;
        m.setStepAttController(&ctrl);

        QMetaObject::invokeMethod(&m, "setRxPreampAttDb",
                                  Q_ARG(int, 0), Q_ARG(int, -30));
        QCOMPARE(ctrl.preampMode(), PreampMode::Minus30);

        // 25 dB is not a preamp step on any supported board; refusing beats
        // rounding to a neighbouring gain setting behind the operator's back.
        QMetaObject::invokeMethod(&m, "setRxPreampAttDb",
                                  Q_ARG(int, 0), Q_ARG(int, -25));
        QCOMPARE(ctrl.preampMode(), PreampMode::Minus30);
    }

    // ── Noise blanker with index ────────────────────────────────────────────

    void nb_index_maps_onto_nb_mode()
    {
        RadioModel m;
        setupTwoSlices(m);
        SliceModel* const slice = m.sliceById(0);
        QVERIFY(slice);

        QMetaObject::invokeMethod(&m, "setRxNbIndexed",
                                  Q_ARG(int, 0), Q_ARG(bool, true), Q_ARG(int, 1));
        QCOMPARE(slice->nbMode(), NbMode::NB);
        int idx = 0;
        QMetaObject::invokeMethod(&m, "rxNbIndex", Q_RETURN_ARG(int, idx), Q_ARG(int, 0));
        QCOMPARE(idx, 1);

        QMetaObject::invokeMethod(&m, "setRxNbIndexed",
                                  Q_ARG(int, 0), Q_ARG(bool, true), Q_ARG(int, 2));
        QCOMPARE(slice->nbMode(), NbMode::NB2);
        QMetaObject::invokeMethod(&m, "rxNbIndex", Q_RETURN_ARG(int, idx), Q_ARG(int, 0));
        QCOMPARE(idx, 2);

        // enable=false wins over the index, per Thetis
        // SetSelectedNB(rx + 1, 0) at TCIServer.cs:4737 [v2.10.3.15].
        QMetaObject::invokeMethod(&m, "setRxNbIndexed",
                                  Q_ARG(int, 0), Q_ARG(bool, false), Q_ARG(int, 2));
        QCOMPARE(slice->nbMode(), NbMode::Off);
        QMetaObject::invokeMethod(&m, "rxNbIndex", Q_RETURN_ARG(int, idx), Q_ARG(int, 0));
        QCOMPARE(idx, 0);
    }

    // ── AGC auto ────────────────────────────────────────────────────────────

    void agc_auto_roundtrips_per_slice()
    {
        RadioModel m;
        setupTwoSlices(m);
        SliceModel* const a = m.sliceById(0);
        SliceModel* const b = m.sliceById(1);
        QVERIFY(a);
        QVERIFY(b);

        QMetaObject::invokeMethod(&m, "setRxAgcAuto",
                                  Q_ARG(int, 1), Q_ARG(bool, true));
        QVERIFY(b->autoAgcEnabled());
        QVERIFY(!a->autoAgcEnabled());   // per-slice, not radio-wide

        bool out = false;
        QMetaObject::invokeMethod(&m, "rxAgcAuto",
                                  Q_RETURN_ARG(bool, out), Q_ARG(int, 1));
        QVERIFY(out);
    }

    // ── VFO sync ────────────────────────────────────────────────────────────

    void vfo_sync_pulls_slice_one_and_then_follows()
    {
        RadioModel m;
        setupTwoSlices(m);
        SliceModel* const a = m.sliceById(0);
        SliceModel* const b = m.sliceById(1);
        QVERIFY(a);
        QVERIFY(b);

        a->setFrequency(14'250'000.0);
        b->setFrequency(7'100'000.0);

        QMetaObject::invokeMethod(&m, "setVfoSync", Q_ARG(bool, true));
        QCOMPARE(b->frequency(), 14'250'000.0);   // pulled at once

        a->setFrequency(14'300'000.0);
        QCOMPARE(b->frequency(), 14'300'000.0);   // and keeps following

        bool out = false;
        QMetaObject::invokeMethod(&m, "vfoSync", Q_RETURN_ARG(bool, out));
        QVERIFY(out);
    }

    void vfo_sync_off_stops_the_follower()
    {
        // The counter-check: without this the test above would also pass on
        // a build that never disconnects, and sync would be a one-way door.
        RadioModel m;
        setupTwoSlices(m);
        SliceModel* const a = m.sliceById(0);
        SliceModel* const b = m.sliceById(1);
        QVERIFY(a);
        QVERIFY(b);

        a->setFrequency(14'250'000.0);
        QMetaObject::invokeMethod(&m, "setVfoSync", Q_ARG(bool, true));
        QCOMPARE(b->frequency(), 14'250'000.0);

        QMetaObject::invokeMethod(&m, "setVfoSync", Q_ARG(bool, false));
        a->setFrequency(21'000'000.0);
        QCOMPARE(b->frequency(), 14'250'000.0);   // stayed where it was

        bool out = true;
        QMetaObject::invokeMethod(&m, "vfoSync", Q_RETURN_ARG(bool, out));
        QVERIFY(!out);
    }

    // ── VFO swap ────────────────────────────────────────────────────────────

    void vfo_swap_exchanges_frequency_and_mode()
    {
        RadioModel m;
        setupTwoSlices(m);
        SliceModel* const a = m.sliceById(0);
        SliceModel* const b = m.sliceById(1);
        QVERIFY(a);
        QVERIFY(b);

        a->setFrequency(14'250'000.0);
        a->setDspMode(DSPMode::USB);
        b->setFrequency(7'100'000.0);
        b->setDspMode(DSPMode::LSB);

        QMetaObject::invokeMethod(&m, "vfoSwap");

        QCOMPARE(a->frequency(), 7'100'000.0);
        QCOMPARE(a->dspMode(), DSPMode::LSB);
        QCOMPARE(b->frequency(), 14'250'000.0);
        QCOMPARE(b->dspMode(), DSPMode::USB);
    }

    void vfo_swap_with_one_slice_is_a_no_op()
    {
        RadioModel m;
        m.addSlice();
        SliceModel* const a = m.sliceById(0);
        QVERIFY(a);
        a->setFrequency(14'250'000.0);

        QMetaObject::invokeMethod(&m, "vfoSwap");
        QCOMPARE(a->frequency(), 14'250'000.0);
    }

    // ── TX filter band ──────────────────────────────────────────────────────

    void tx_filter_band_roundtrips_through_transmit_model()
    {
        RadioModel m;
        QMetaObject::invokeMethod(&m, "setTxFilterBand",
                                  Q_ARG(int, 300), Q_ARG(int, 2700));
        QCOMPARE(m.transmitModel().filterLow(), 300);
        QCOMPARE(m.transmitModel().filterHigh(), 2700);

        int low = 0;
        int high = 0;
        QMetaObject::invokeMethod(&m, "txFilterLowHz", Q_RETURN_ARG(int, low));
        QMetaObject::invokeMethod(&m, "txFilterHighHz", Q_RETURN_ARG(int, high));
        QCOMPARE(low, 300);
        QCOMPARE(high, 2700);
    }

    // ── Protocol-side refusals that must not reach the model ────────────────

    void refused_commands_leave_the_model_alone()
    {
        RadioModel m;
        StepAttenuatorController ctrl;
        ctrl.setMaxAttenuation(31);
        m.setStepAttController(&ctrl);
        setupTwoSlices(m);

        TciProtocol p(&m);
        p.handleCommand(QStringLiteral("rx_step_att_ex:0,9;"));
        QCOMPARE(ctrl.attenuatorDb(), 9);

        // Negative attenuation, out-of-range receiver, nb index 3, positive
        // preamp value: every one refused inside the handler.
        p.handleCommand(QStringLiteral("rx_step_att_ex:0,-5;"));
        p.handleCommand(QStringLiteral("rx_step_att_ex:7,21;"));
        QCOMPARE(ctrl.attenuatorDb(), 9);

        p.handleCommand(QStringLiteral("rx_nb_enable_ex:0,true,2;"));
        QCOMPARE(m.sliceById(0)->nbMode(), NbMode::NB2);
        p.handleCommand(QStringLiteral("rx_nb_enable_ex:0,true,3;"));
        QCOMPARE(m.sliceById(0)->nbMode(), NbMode::NB2);
    }

    void fm_deviation_and_run_cat_do_not_pretend()
    {
        RadioModel m;
        TciProtocol p(&m);

        // No FM deviation state exists: the command is refused outright
        // rather than answered with a plausible 2500/5000.
        QCOMPARE(p.handleCommand(QStringLiteral("fm_deviation_ex:0,5000;")), QString());
        QCOMPARE(p.handleCommand(QStringLiteral("fm_deviation_ex:0;")), QString());

        // run_cat_ex now really runs the command through CatParser
        // (phase 3K, same day). The command and the result are named
        // WITHOUT their terminators -- Thetis keeps them (":PS;,PS1;")
        // and the frame then splits in two at any conforming client,
        // which tools/tci_probe showed at the wire on 2026-10-11.
        // ID is the same whatever the radio is doing, so it pins the frame
        // shape; PS answers the real state (PS0 here -- this RadioModel is
        // not connected to anything), which is the point of running the
        // command instead of faking an answer.
        QCOMPARE(p.handleCommand(QStringLiteral("run_cat_ex:ID;")),
                 QStringLiteral("run_cat_ex:ID,ID019;"));
        QCOMPARE(p.handleCommand(QStringLiteral("run_cat_ex:PS;")),
                 QStringLiteral("run_cat_ex:PS,PS0;"));
        QVERIFY(!p.handleCommand(QStringLiteral("run_cat_ex:ID;"))
                     .mid(QStringLiteral("run_cat_ex:").size())
                     .chopped(1)
                     .contains(QLatin1Char(';')));

        // And the transmit gate of that parser is never opened: keying
        // through a TCI message is not a way around the CAT setting.
        QCOMPARE(p.handleCommand(QStringLiteral("run_cat_ex:TX;")),
                 QStringLiteral("run_cat_ex:TX,?;"));
    }
};

QTEST_GUILESS_MAIN(TestTciExLongTail)
#include "tst_tci_ex_long_tail.moc"
