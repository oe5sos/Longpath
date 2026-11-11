// =================================================================
// tests/tst_voice_keyer_player.cpp  (Longpath)
// =================================================================
//
// Der Sprachspeicher im Betrieb: Senden, Nachlauf, Loslassen,
// Wiederholen mit Empfangspause, Abbruch von aussen, Ablehnung -- gegen
// Attrappen fuer Router und MoxController. Ein Fehler hier sendet
// nichts.
//
// Der Sendefaden wird von Hand gespielt: pullSamples() so oft rufen,
// bis die Ansage durch ist. Genau so holt TxWorkerThread die Proben,
// wenn eigeneTonquelleAktiv() wahr ist.
//
// Dazu der Router: MicSource::Dvk wird geholt wie FT8, ist aber KEIN
// Datensignal -- die Stimme geht durch den Sprach-Kanalzug.
//
// Und die Flex (2026-11-11): ohne Sendefaden nimmt der Spieler das
// Mikrofon direkt ab und verwirft, was vor dem Tastendruck im Eingang lag.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-11-11 — Aufnahme ohne Sendefaden (Flex). KI-gestuetzt ueber
//                 Anthropic Claude.
// =================================================================

#include <QtTest>
#include <QSignalSpy>
#include <QFile>
#include <QTemporaryDir>

#include "core/audio/CompositeTxMicRouter.h"
#include "core/audio/VoiceKeyer.h"
#include "core/audio/VoiceKeyerPlayer.h"

#include <algorithm>
#include <cmath>

using namespace Longpath;

namespace {

QVector<float> tone(int n, float amp = 0.5f)
{
    QVector<float> v(n);
    for (int i = 0; i < n; ++i) {
        v[i] = amp * static_cast<float>(std::sin(2.0 * M_PI * 700.0 * i / 48000.0));
    }
    return v;
}

// Was die Attrappen gesehen haben.
struct Rig {
    bool keyed{false};
    bool routed{false};
    int keyOn{0};
    int keyOff{0};
    int routes{0};
    int restores{0};
    QString refuse;      // nicht leer: das naechste key(true) wird abgelehnt
    bool haveRouter{true};
};

VoiceKeyerPlayer::Hooks hooksFor(Rig& rig)
{
    VoiceKeyerPlayer::Hooks h;
    h.routeToVoiceKeyer = [&rig]() {
        if (!rig.haveRouter) { return false; }
        rig.routed = true;
        ++rig.routes;
        return true;
    };
    h.restoreRoute = [&rig]() {
        rig.routed = false;
        ++rig.restores;
    };
    h.key = [&rig](bool on) {
        if (on && !rig.refuse.isEmpty()) {
            return rig.refuse;
        }
        rig.keyed = on;
        ++(on ? rig.keyOn : rig.keyOff);
        return QString();
    };
    h.keyed = [&rig]() { return rig.keyed; };
    h.txWorker = []() -> TxWorkerThread* { return nullptr; };
    return h;
}

// Den Sendefaden spielen, bis die Ansage durch ist.
void drain(WavTxSource* src)
{
    float buf[1024];
    for (int guard = 0; guard < 2000 && src->isPlaying(); ++guard) {
        src->pullSamples(buf, 1024);
    }
}

} // namespace

class TestVoiceKeyerPlayer : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    VoiceKeyerStore* m_store{nullptr};

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    void init()
    {
        delete m_store;
        m_store = new VoiceKeyerStore;
        m_store->setFolder(m_dir.path() + QStringLiteral("/vk"));
        // Platz 1 und 2 belegt, je 0,2 s. Platz 3 bleibt leer.
        QString err;
        QVERIFY2(m_store->setRecording(0, tone(9600), 48000, &err), qPrintable(err));
        QVERIFY2(m_store->setRecording(1, tone(9600, 0.3f), 48000, &err), qPrintable(err));
        m_store->clearSlot(2);
    }

    void cleanupTestCase()
    {
        delete m_store;
        m_store = nullptr;
    }

    // ── Ablehnungen ──────────────────────────────────────────────────

    void emptySlotIsRefusedAndKeysNothing()
    {
        Rig rig;
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        const QString r = p.play(2);
        QVERIFY2(r.contains(QStringLiteral("empty")), qPrintable(r));
        QCOMPARE(rig.keyOn, 0);
        QCOMPARE(rig.routes, 0);
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Idle);
    }

    void withoutHooksNothingIsSent()
    {
        VoiceKeyerPlayer p(m_store);
        QVERIFY(!p.play(0).isEmpty());
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Idle);
    }

    void withoutRouterNothingIsKeyed()
    {
        Rig rig;
        rig.haveRouter = false;
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        const QString r = p.play(0);
        QVERIFY2(r.contains(QStringLiteral("this radio")), qPrintable(r));
        QCOMPARE(rig.keyOn, 0);
        QVERIFY(!p.source()->isPlaying());
    }

    void bandPlanRefusalIsSaidAndUndone()
    {
        Rig rig;
        rig.refuse = QStringLiteral("frequency outside the band");
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        const QString r = p.play(0);
        QVERIFY2(r.contains(QStringLiteral("outside the band")), qPrintable(r));
        QCOMPARE(rig.routes, 1);
        QCOMPARE(rig.restores, 1);       // der Weg ist wieder zurueck
        QVERIFY(!rig.routed);
        QVERIFY(!p.source()->isPlaying());
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Idle);
    }

    void aTransmitterSomeoneElseKeyedIsLeftAlone()
    {
        // Gehaltene Sprechtaste oder TUNE: das Ende der Ansage wuerde
        // diesen Sender sonst abschalten.
        Rig rig;
        rig.keyed = true;
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        const QString r = p.play(0);
        QVERIFY2(r.contains(QStringLiteral("already on")), qPrintable(r));
        QCOMPARE(rig.routes, 0);
        QCOMPARE(rig.keyOff, 0);
        QVERIFY(rig.keyed);
    }

    void recordingWithoutTransmitPathIsRefused()
    {
        Rig rig;
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        const QString r = p.record(0);
        QVERIFY2(r.contains(QStringLiteral("connect")), qPrintable(r));
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Idle);
        QCOMPARE(rig.keyOn, 0);
    }

    // Flex (2026-11-11): kein Sendefaden, aber ein offenes Mikrofon.
    // Die Aufnahme holt es direkt ab -- und wirft weg, was schon vor dem
    // Tastendruck im Eingang lag.
    void recordingWithoutWorkerPullsTheMic()
    {
        Rig rig;
        VoiceKeyerPlayer p(m_store);
        VoiceKeyerPlayer::Hooks h = hooksFor(rig);
        QVector<float> eingang(4800, 0.9f);   // alt: 0,1 s laut, vor dem Druck
        h.micPullable = []() { return true; };
        h.pullMic = [&eingang](float* dst, int n) {
            const int k = std::min<int>(n, int(eingang.size()));
            std::copy_n(eingang.constData(), k, dst);
            eingang.remove(0, k);
            return k;
        };
        p.setHooks(std::move(h));

        m_store->clearSlot(2);
        QVERIFY(p.record(2).isEmpty());
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Recording);
        QVERIFY(eingang.isEmpty());           // das Alte ist weg

        // Jetzt spricht der Betreiber: 0,5 s, abgeholt im Takt.
        eingang = tone(24000, 0.25f);
        QTRY_VERIFY_WITH_TIMEOUT(eingang.isEmpty(), 2000);
        QVERIFY(p.takeRecordPeak() > 0.2f);
        QVERIFY(p.record(2).isEmpty());       // dieselbe Taste beendet
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Idle);

        const VoiceKeyerSlot& s = m_store->slot(2);
        QVERIFY(!s.isEmpty());
        QVERIFY(QFile::exists(s.wavPath));
        // Genau das Gesprochene, ohne die laute Vorgeschichte.
        WavTxSource src;
        QString err;
        QVERIFY2(src.load(s.wavPath, VoiceKeyerPlayer::kRateHz, &err), qPrintable(err));
        QVERIFY(std::abs(src.seconds() - 0.5) < 0.02);
    }

    void withoutWorkerAndWithoutMicNothingIsRecorded()
    {
        Rig rig;
        VoiceKeyerPlayer p(m_store);
        VoiceKeyerPlayer::Hooks h = hooksFor(rig);
        h.micPullable = []() { return false; };   // keine Flex verbunden
        h.pullMic = [](float*, int) { return 0; };
        p.setHooks(std::move(h));
        const QString r = p.record(2);
        QVERIFY2(r.contains(QStringLiteral("connect")), qPrintable(r));
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Idle);
    }

    // ── Senden ───────────────────────────────────────────────────────

    void aMessageKeysPlaysAndLetsGoAfterTheTail()
    {
        Rig rig;
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        QSignalSpy states(&p, &VoiceKeyerPlayer::stateChanged);

        QCOMPARE(p.play(0), QString());
        // Erst der Weg, dann die Taste.
        QVERIFY(rig.routed);
        QVERIFY(rig.keyed);
        QCOMPARE(rig.keyOn, 1);
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Playing);
        QCOMPARE(p.activeSlot(), 0);
        QVERIFY(p.source()->isPlaying());

        drain(p.source());
        QVERIFY(!p.source()->isPlaying());

        // Nachlauf: nicht sofort los.
        QTest::qWait(VoiceKeyerPlayer::kPollMs * 3);
        QVERIFY(rig.keyed);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.keyed, 2000);
        QCOMPARE(rig.keyOff, 1);
        QVERIFY(!rig.routed);
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Idle);
        QVERIFY(states.count() >= 2);
    }

    void theSourceDeliversTheRecording()
    {
        Rig rig;
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        QCOMPARE(p.play(0), QString());
        float buf[256];
        QCOMPARE(p.source()->pullSamples(buf, 256), 256);
        float peak = 0.0f;
        for (float v : buf) { peak = std::max(peak, std::fabs(v)); }
        QVERIFY2(peak > 0.3f, "die Ansage kommt an, nicht Stille");
        p.stop();
    }

    void aSecondMessageReplacesTheFirstWithoutDroppingTheKey()
    {
        Rig rig;
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        QCOMPARE(p.play(0), QString());
        float buf[512];
        p.source()->pullSamples(buf, 512);

        QCOMPARE(p.play(1), QString());
        QCOMPARE(rig.keyOn, 1);           // nicht neu getastet
        QCOMPARE(rig.keyOff, 0);          // und nicht losgelassen
        QCOMPARE(rig.routes, 1);
        QCOMPARE(p.activeSlot(), 1);
        QVERIFY(p.source()->isPlaying());
        p.stop();
        QCOMPARE(rig.keyOff, 1);
        QVERIFY(!rig.routed);
    }

    void stopLetsGoAtOnce()
    {
        Rig rig;
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        QCOMPARE(p.play(0), QString());
        p.stop();
        QVERIFY(!rig.keyed);
        QVERIFY(!rig.routed);
        QVERIFY(!p.source()->isPlaying());
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Idle);
        QVERIFY(!p.busy());
    }

    void keyedOffFromOutsideEndsTheMessage()
    {
        // MOX-Knopf, Bandplan, Schutzabschaltung: der Spieler laesst den
        // Sender nicht ein zweites Mal los und gibt den Weg zurueck.
        Rig rig;
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        QCOMPARE(p.play(0), QString());
        rig.keyed = false;
        p.onKeyedChanged(false);
        QCOMPARE(rig.keyOff, 0);
        QVERIFY(!rig.routed);
        QVERIFY(!p.source()->isPlaying());
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Idle);
    }

    void itsOwnKeyEchoIsNotAnInterruption()
    {
        Rig rig;
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        QCOMPARE(p.play(0), QString());
        // moxStateChanged(true) kommt nach dem Zeitablauf des
        // MoxControllers -- es ist das eigene Tasten.
        p.onKeyedChanged(true);
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Playing);
        p.stop();
    }

    // ── Wiederholen ──────────────────────────────────────────────────

    void repeatListensBetweenCalls()
    {
        Rig rig;
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        p.setRepeatSeconds(1);
        p.setRepeat(true);

        QCOMPARE(p.play(0), QString());
        drain(p.source());
        // Nach dem Nachlauf: Sender AUS, Pause ist Empfang.
        QTRY_COMPARE_WITH_TIMEOUT(p.state(), VoiceKeyerPlayer::State::Gap, 2000);
        QVERIFY(!rig.keyed);
        QVERIFY(!rig.routed);
        QCOMPARE(p.activeSlot(), 0);
        QVERIFY(p.busy());

        // Nach der Pause derselbe Ruf noch einmal.
        QTRY_COMPARE_WITH_TIMEOUT(p.state(), VoiceKeyerPlayer::State::Playing, 3000);
        QVERIFY(rig.keyed);
        QCOMPARE(rig.keyOn, 2);
        QCOMPARE(p.activeSlot(), 0);
        p.stop();
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Idle);
        QVERIFY(!rig.keyed);
    }

    void keyingDuringTheGapEndsTheRepeat()
    {
        Rig rig;
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        p.setRepeatSeconds(1);
        p.setRepeat(true);
        QCOMPARE(p.play(0), QString());
        drain(p.source());
        QTRY_COMPARE_WITH_TIMEOUT(p.state(), VoiceKeyerPlayer::State::Gap, 2000);

        // Der Betreiber spricht selbst.
        rig.keyed = true;
        p.onKeyedChanged(true);
        QCOMPARE(p.state(), VoiceKeyerPlayer::State::Idle);
        QTest::qWait(1500);
        QCOMPARE(rig.keyOn, 1);   // kein zweiter Ruf
    }

    void repeatSecondsAreClampedAndKept()
    {
        {
            VoiceKeyerPlayer p(m_store);
            p.setRepeatSeconds(0);
            QCOMPARE(p.repeatSeconds(), 1);
            p.setRepeatSeconds(500);
            QCOMPARE(p.repeatSeconds(), 60);
            p.setRepeatSeconds(7);
        }
        VoiceKeyerPlayer again(m_store);
        QCOMPARE(again.repeatSeconds(), 7);
        QVERIFY(!again.repeat());   // Wiederholen ist nach dem Start aus
    }

    // ── Der Router ───────────────────────────────────────────────────

    void theRouterFetchesTheVoiceButNotAsData()
    {
        CompositeTxMicRouter router(nullptr, nullptr, false);
        WavTxSource src;
        router.setDvkSource(&src);

        router.setActiveSource(MicSource::Dvk);
        QVERIFY(router.eigeneTonquelleAktiv());
        QVERIFY(!router.eigenesDatensignalAktiv());

        // FT8 bleibt ein Datensignal.
        WavTxSource ft8;
        router.setFt8Source(&ft8);
        router.setActiveSource(MicSource::Ft8);
        QVERIFY(router.eigeneTonquelleAktiv());
        QVERIFY(router.eigenesDatensignalAktiv());

        // Ohne angemeldete Quelle: Mikrofon, kein eigener Ton.
        router.setDvkSource(nullptr);
        router.setActiveSource(MicSource::Dvk);
        QVERIFY(!router.eigeneTonquelleAktiv());
        QVERIFY(!router.eigenesDatensignalAktiv());
    }

    void theRouterPullsFromTheVoiceKeyer()
    {
        Rig rig;
        VoiceKeyerPlayer p(m_store);
        p.setHooks(hooksFor(rig));
        CompositeTxMicRouter router(nullptr, nullptr, false);
        router.setDvkSource(p.source());
        router.setActiveSource(MicSource::Dvk);
        QCOMPARE(p.play(0), QString());

        float buf[512];
        QCOMPARE(router.pullSamples(buf, 512), 512);
        float peak = 0.0f;
        for (float v : buf) { peak = std::max(peak, std::fabs(v)); }
        QVERIFY(peak > 0.3f);
        p.stop();
    }
};

QTEST_GUILESS_MAIN(TestVoiceKeyerPlayer)
#include "tst_voice_keyer_player.moc"
