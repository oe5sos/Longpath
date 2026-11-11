// =================================================================
// tests/tst_hot_key_dispatcher.cpp  (Longpath)
// =================================================================
//
// Der Verteiler gegen eine Attrappe des Fensters: welcher Empfaenger,
// welche Sperre, wie gehaltene Abstimmtasten gebuendelt werden, wie die
// Sprechtaste Buch fuehrt und wohin die Sprachspeicher-Tasten gehen.
//
// Der Verteiler ist aus dem Upstream NereusSDR uebernommen; sein dortiger
// Pruefstand (tst_hot_key_dispatcher.cpp) haengt an Kern-CW und fremden
// Geraeten. Dieser hier prueft, was Longpath davon behalten hat.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

const HotKeySource kKeyboard{HotKeySourceKind::Keyboard, QString()};

class FakeTargets : public HotKeyTargets {
public:
    SliceModel* slice{nullptr};
    QStringList calls;
    QStringList toasts;
    QString unavailable;           // fuer jede Handlung, wenn nicht leer
    bool keyed{false};
    bool tune{false};
    bool admitHold{true};
    bool takes{true};
    qint64 now{1000};
    QList<double> tunes;

    SliceModel* windowSlice() const override { return slice; }
    SliceModel* sliceByLetter(QChar letter) const override
    {
        return (slice && slice->sliceLetter() == letter) ? slice : nullptr;
    }
    QChar letterOf(const SliceModel* s) const override { return s ? s->sliceLetter() : QChar(); }
    bool tuneTo(SliceModel* s, double hz) override
    {
        tunes.append(hz);
        s->setFrequency(hz);
        return true;
    }
    void runSliceAction(const QString& id, SliceModel*, int steps) override
    {
        calls.append(QStringLiteral("slice:%1:%2").arg(id).arg(steps));
    }
    void runWindowAction(const QString& id) override { calls.append(QStringLiteral("window:") + id); }
    QString unavailableReason(const QString&) const override { return unavailable; }
    bool takesKey(const QString&) const override { return takes; }
    bool transmitting() const override { return keyed; }
    bool tuneOn() const override { return tune; }
    bool keyHold(bool down) override
    {
        calls.append(down ? QStringLiteral("hold:down") : QStringLiteral("hold:up"));
        if (down && !admitHold) { return false; }
        keyed = down;
        return true;
    }
    void runTransmitAction(const QString& id) override { calls.append(QStringLiteral("tx:") + id); }
    void runVoiceAction(const QString& id) override { calls.append(QStringLiteral("voice:") + id); }
    void toast(const QString& text) override { toasts.append(text); }
    qint64 nowMs() const override { return now; }
};

} // namespace

class TstHotKeyDispatcher : public QObject {
    Q_OBJECT

private:
    HotKeyBindings* m_bindings{nullptr};
    FakeTargets* m_targets{nullptr};
    HotKeyDispatcher* m_dispatcher{nullptr};
    SliceModel* m_slice{nullptr};

private slots:
    void init()
    {
        AppSettings::instance().clear();
        m_bindings = new HotKeyBindings;
        m_targets = new FakeTargets;
        m_slice = new SliceModel;
        m_slice->setFrequency(7100000.0);
        m_slice->setStepHz(100);
        m_targets->slice = m_slice;
        m_dispatcher = new HotKeyDispatcher(m_bindings, m_targets);
    }

    void cleanup()
    {
        delete m_dispatcher;
        delete m_targets;
        delete m_slice;
        delete m_bindings;
        AppSettings::instance().clear();
    }

    void tuneGoesThroughTheWindowsPath()
    {
        m_dispatcher->press(QStringLiteral("tune.up"), kKeyboard);
        QCOMPARE(m_targets->tunes.size(), 1);
        QCOMPARE(m_targets->tunes.first(), 7100100.0);
        m_targets->now += 100;
        m_dispatcher->press(QStringLiteral("tune.down10"), kKeyboard);
        QCOMPARE(m_targets->tunes.last(), 7099100.0);
    }

    void aHeldKeyIsBundledAndLosesNoStep()
    {
        // Fuenf Drucke in derselben Millisekunde: einer geht sofort, die
        // vier anderen nach hoechstens 20 ms zusammen.
        for (int i = 0; i < 5; ++i) {
            m_dispatcher->press(QStringLiteral("tune.up"), kKeyboard);
        }
        QCOMPARE(m_targets->tunes.size(), 1);
        m_targets->now += TuneCoalescer::kMinIntervalMs;
        m_dispatcher->flushTune();
        QCOMPARE(m_targets->tunes.size(), 2);
        QCOMPARE(m_slice->frequency(), 7100500.0);
    }

    void aLockedSliceSaysSoAndStays()
    {
        m_slice->setLocked(true);
        m_dispatcher->press(QStringLiteral("tune.up"), kKeyboard);
        m_dispatcher->press(QStringLiteral("mode.next"), kKeyboard);
        QVERIFY(m_targets->tunes.isEmpty());
        QVERIFY(m_targets->calls.isEmpty());
        QCOMPARE(m_targets->toasts.size(), 2);
        QVERIFY(m_targets->toasts.first().contains(QStringLiteral("locked")));
        // Sperre, DSP und Ton gehen weiter.
        m_dispatcher->press(QStringLiteral("nr.toggle"), kKeyboard);
        m_dispatcher->press(QStringLiteral("slice.lock"), kKeyboard);
        QCOMPARE(m_targets->calls,
                 (QStringList{QStringLiteral("slice:nr.toggle:1"), QStringLiteral("slice:slice.lock:1")}));
    }

    void scopesReachTheirTargets()
    {
        m_dispatcher->press(QStringLiteral("filter.next"), kKeyboard);
        m_dispatcher->press(QStringLiteral("zoom.in"), kKeyboard);
        m_dispatcher->press(QStringLiteral("slice.next"), kKeyboard);
        m_dispatcher->press(QStringLiteral("tx.mox"), kKeyboard);
        m_dispatcher->press(QStringLiteral("tx.tune"), kKeyboard);
        QCOMPARE(m_targets->calls, (QStringList{
            QStringLiteral("slice:filter.next:1"), QStringLiteral("window:zoom.in"),
            QStringLiteral("window:slice.next"), QStringLiteral("tx:tx.mox"),
            QStringLiteral("tx:tx.tune")}));
    }

    void volumeStepsAddUpLikeTuning()
    {
        for (int i = 0; i < 3; ++i) {
            m_dispatcher->press(QStringLiteral("volume.down"), kKeyboard);
        }
        m_targets->now += TuneCoalescer::kMinIntervalMs;
        m_dispatcher->flushTune();
        QCOMPARE(m_targets->calls, (QStringList{QStringLiteral("slice:volume.down:1"),
                                                QStringLiteral("slice:volume.down:2")}));
    }

    void voiceKeysGoToTheVoiceKeyer()
    {
        m_dispatcher->press(QStringLiteral("voice.msg1"), kKeyboard);
        m_dispatcher->press(QStringLiteral("voice.msg10"), kKeyboard);
        m_dispatcher->press(QStringLiteral("voice.rec3"), kKeyboard);
        m_dispatcher->press(QStringLiteral("voice.stop"), kKeyboard);
        QCOMPARE(m_targets->calls, (QStringList{
            QStringLiteral("voice:voice.msg1"), QStringLiteral("voice:voice.msg10"),
            QStringLiteral("voice:voice.rec3"), QStringLiteral("voice:voice.stop")}));
    }

    void unavailableIsSaidAndNothingRuns()
    {
        m_targets->unavailable = QStringLiteral("Not connected to a radio.");
        m_dispatcher->press(QStringLiteral("voice.msg1"), kKeyboard);
        m_dispatcher->press(QStringLiteral("tx.hold"), kKeyboard);
        QVERIFY(m_targets->calls.isEmpty());
        QCOMPARE(m_targets->toasts.size(), 2);
        QVERIFY(!m_dispatcher->transmitHoldOn());
    }

    void holdToTalkKeysAndLetsGo()
    {
        m_dispatcher->press(QStringLiteral("tx.hold"), kKeyboard);
        QVERIFY(m_targets->keyed);
        QVERIFY(m_dispatcher->transmitHoldOn());
        // Tastenwiederholung: nichts Neues.
        m_dispatcher->press(QStringLiteral("tx.hold"), kKeyboard);
        QCOMPARE(m_targets->calls, QStringList{QStringLiteral("hold:down")});
        m_dispatcher->release(QStringLiteral("tx.hold"), kKeyboard);
        QVERIFY(!m_targets->keyed);
        QVERIFY(!m_dispatcher->transmitHoldOn());
        QCOMPARE(m_targets->calls, (QStringList{QStringLiteral("hold:down"), QStringLiteral("hold:up")}));
    }

    void aLatchedMoxSurvivesTheHold()
    {
        // MOX war schon an: die Sprechtaste beansprucht nichts und
        // schaltet beim Loslassen nichts ab.
        m_targets->keyed = true;
        m_dispatcher->press(QStringLiteral("tx.hold"), kKeyboard);
        m_dispatcher->release(QStringLiteral("tx.hold"), kKeyboard);
        QVERIFY(m_targets->calls.isEmpty());
        QVERIFY(m_targets->keyed);
    }

    void tuneTakingOverStaysOn()
    {
        m_dispatcher->press(QStringLiteral("tx.hold"), kKeyboard);
        m_targets->tune = true;
        m_dispatcher->release(QStringLiteral("tx.hold"), kKeyboard);
        QCOMPARE(m_targets->calls, QStringList{QStringLiteral("hold:down")});
    }

    void aRefusedHoldClaimsNothing()
    {
        m_targets->admitHold = false;
        m_dispatcher->press(QStringLiteral("tx.hold"), kKeyboard);
        QVERIFY(!m_dispatcher->transmitHoldOn());
        m_dispatcher->release(QStringLiteral("tx.hold"), kKeyboard);
        QCOMPARE(m_targets->calls, QStringList{QStringLiteral("hold:down")});
    }

    void transmitWentOffDropsTheClaim()
    {
        m_dispatcher->press(QStringLiteral("tx.hold"), kKeyboard);
        m_targets->keyed = false;
        m_dispatcher->onTransmitWentOff();
        QVERIFY(!m_dispatcher->transmitHoldOn());
        m_dispatcher->release(QStringLiteral("tx.hold"), kKeyboard);
        // Kein zweites Loslassen eines Senders, der schon aus ist.
        QCOMPARE(m_targets->calls, QStringList{QStringLiteral("hold:down")});
    }

    void theOffSwitchStopsEverythingAndLetsGo()
    {
        m_dispatcher->press(QStringLiteral("tx.hold"), kKeyboard);
        m_bindings->setEnabled(false);
        QVERIFY(!m_targets->keyed);
        QVERIFY(!m_dispatcher->takesKey(QStringLiteral("tune.up")));
        m_dispatcher->press(QStringLiteral("tune.up"), kKeyboard);
        m_dispatcher->press(QStringLiteral("voice.msg1"), kKeyboard);
        QVERIFY(m_targets->tunes.isEmpty());
        QCOMPARE(m_targets->calls, (QStringList{QStringLiteral("hold:down"), QStringLiteral("hold:up")}));
    }

    void takesKeyAsksTheWindow()
    {
        QVERIFY(m_dispatcher->takesKey(QStringLiteral("voice.stop")));
        m_targets->takes = false;
        QVERIFY(!m_dispatcher->takesKey(QStringLiteral("voice.stop")));
    }

    void aSleepDropsTheHold()
    {
        qint64 wall = 0;
        qint64 awake = 0;
        m_dispatcher->setSleepClocksForTest([&wall] { return wall; }, [&awake] { return awake; });
        m_dispatcher->press(QStringLiteral("tx.hold"), kKeyboard);
        QVERIFY(m_dispatcher->sleepWatchRunning());
        wall += 60000;   // eine Minute Schlaf
        awake += 1000;
        m_dispatcher->checkForSleep();
        QVERIFY(!m_targets->keyed);
        QVERIFY(!m_dispatcher->sleepWatchRunning());
    }
};

QTEST_GUILESS_MAIN(TstHotKeyDispatcher)
#include "tst_hot_key_dispatcher.moc"
