// no-port-check: Longpath-original test for the mic-PTT session gate
// (2026-10-12).
//
// The radio reports its own mic-PTT bit on every status frame, roughly
// 50-100 times a second, from the connection thread. The report reaches
// MoxController as a QUEUED call -- and Qt delivers a call that was
// already posted even after the connection behind it was disconnected
// or destroyed. So a mic_ptt=1 from a session that is being torn down
// can land in the session that replaces it and key the radio with
// nobody touching anything.
//
// Upstream NereusSDR fixed the same thing after its 2026.10.0 release
// ("Prevent a callback from an old radio session from keying a
// replacement session", #335). We inherited the wiring that allows it;
// RadioModel::handleMicPttFromRadio is the gate, and this file pins it.
//
// Counter-check built in: deliver_from_the_current_session_still_keys
// fails on a build where the gate drops everything, and
// a_report_from_a_retired_session_does_not_key fails on the build
// before the gate existed.

#include <QtTest/QtTest>

#include "core/MoxController.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TestMicPttSessionGate : public QObject {
    Q_OBJECT

private slots:
    void a_report_from_a_retired_session_does_not_key()
    {
        RadioModel radio;
        MoxController& mox = *radio.moxController();

        const quint64 session = radio.radioSession();
        QVERIFY(session != 0);

        // The teardown path retires the session. Anything still in flight
        // from it must be dropped.
        radio.retireRadioSession();
        QVERIFY(radio.radioSession() != session);

        QMetaObject::invokeMethod(&radio, "handleMicPttFromRadio",
                                  Q_ARG(quint64, session), Q_ARG(bool, true));
        QVERIFY2(!mox.isMox(), "a stale mic-PTT report keyed the radio");
    }

    void deliver_from_the_current_session_still_keys()
    {
        // The gate must not be a blanket block -- the live session's own
        // mic PTT has to go through, or the foot switch stops working.
        RadioModel radio;
        MoxController& mox = *radio.moxController();

        QMetaObject::invokeMethod(&radio, "handleMicPttFromRadio",
                                  Q_ARG(quint64, radio.radioSession()),
                                  Q_ARG(bool, true));
        QVERIFY2(mox.isMox(), "the live session's mic PTT did not key the radio");
        QCOMPARE(mox.pttMode(), PttMode::Mic);

        QMetaObject::invokeMethod(&radio, "handleMicPttFromRadio",
                                  Q_ARG(quint64, radio.radioSession()),
                                  Q_ARG(bool, false));
        // Release runs through the hold timer, so MOX stays up for now;
        // what matters here is that the report was accepted at all.
    }

    void a_report_from_a_future_session_does_not_key_either()
    {
        // Defensive: only the exact current number is acted on. A wrong
        // number in either direction means the report does not belong to
        // this session.
        RadioModel radio;
        MoxController& mox = *radio.moxController();

        QMetaObject::invokeMethod(&radio, "handleMicPttFromRadio",
                                  Q_ARG(quint64, radio.radioSession() + 99),
                                  Q_ARG(bool, true));
        QVERIFY(!mox.isMox());
    }

    void an_unkey_from_a_retired_session_is_dropped_too()
    {
        // Both directions are gated. An unkey from a dead session must
        // not drop a transmission the live session started -- that is the
        // same bug with the sign flipped.
        RadioModel radio;
        MoxController& mox = *radio.moxController();

        const quint64 stale = radio.radioSession();
        QMetaObject::invokeMethod(&radio, "handleMicPttFromRadio",
                                  Q_ARG(quint64, stale), Q_ARG(bool, true));
        QVERIFY(mox.isMox());

        radio.retireRadioSession();
        QMetaObject::invokeMethod(&radio, "handleMicPttFromRadio",
                                  Q_ARG(quint64, stale), Q_ARG(bool, false));
        QVERIFY2(mox.isMox(), "a stale release dropped the live transmission");
    }
};

QTEST_MAIN(TestMicPttSessionGate)
#include "tst_mic_ptt_session_gate.moc"
