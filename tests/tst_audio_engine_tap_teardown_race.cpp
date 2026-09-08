// =================================================================
// tests/tst_audio_engine_tap_teardown_race.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test file.
//
// DER FALL, DER STILL FALSCH WIRD (2026-09-06 code review, gefunden waehrend
// des Baus des nativen RTTY-Decoders): AudioEngine::setQsoTap / setAsrTap /
// setWavRecordTap / setRttyTap bestanden beim ABSCHALTEN (ring == nullptr)
// nur aus zwei ungesicherten std::atomic-Speicheroperationen. Kein Warten
// darauf, dass ein bereits laufender AudioEngine::rxBlockReady()-Aufruf auf
// dem echten Audio-Faden seinen tap->write() schon fertig hat.
//
// Ein Aufrufer wie QsoRecorderController::~QsoRecorderController() ruft
// setQsoTap(nullptr, -1) und zerstoert unmittelbar danach den
// AudioTapRing als Elementobjekt ("Erst den Abgriff loesen, dann
// sterben" — QsoRecorderController.cpp). Wenn der Audio-Faden zu diesem
// Zeitpunkt noch mitten in tap->write() steckt, ist das ein echter
// Use-after-free auf dem Ring.
//
// Der Fix (AudioEngine.cpp: writeToTapIfCurrent / waitForTapQuiescence)
// gibt jedem der vier Abgriffe einen eigenen Belegt-Zaehler: der
// Audio-Faden meldet sich VOR dem Lesen des Zeigers an und wieder ab,
// NACHDEM write() zurueckgekehrt ist; setXxxTap(nullptr, ...) veroeffent-
// licht erst den Null-Zeiger und wartet dann, bis der Zaehler auf null
// steht, bevor es zurueckkehrt.
//
// Dieser Test haelt den simulierten Audio-Faden ueber den
// NEREUS_BUILD_TESTS-Testhaken setTapWriteDelayHookForTest() gezielt in
// genau diesem Fenster fest (Zaehler schon erhoeht, Zeiger schon
// gelesen, write() noch nicht aufgerufen) und misst, wie lange
// setXxxTap(nullptr, ...) braucht. Vor dem Fix waere das im Bereich
// weniger Mikrosekunden gewesen, unabhaengig vom kuenstlichen Verzug im
// Haken — ein starkes, deterministisches Signal dafuer, dass die Wartung
// tatsaechlich wirkt.
//
// Ein echter Use-after-free waere ein noch staerkerer Nachweis, haette
// aber im alten (kaputten) Code den ganzen Testlauf abstuerzen lassen
// statt einen einzelnen QVERIFY fehlschlagen zu lassen — dafuer bewusst
// nicht entschieden. Die Zeitmessung unten ist in beide Richtungen
// deterministisch und CI-tauglich.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/audio/AudioTapRing.h"
#include "models/RadioModel.h"

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

using namespace Longpath;

namespace {

constexpr int kTestFrames = 2;
constexpr int kTestStereoFloats = kTestFrames * 2;

const std::array<float, kTestStereoFloats> kTestSamples = {
    0.10f, 0.20f,   // frame 0  L,R
    -0.30f, -0.40f, // frame 1  L,R
};

// Long enough that scheduling jitter can never make the "waited" branch
// look like the "returned immediately" branch, short enough that four of
// these in one test binary stay fast.
constexpr auto kHookDelay = std::chrono::milliseconds(200);

} // namespace

class TstAudioEngineTapTeardownRace : public QObject {
    Q_OBJECT

private:
    // Shared body for all four taps. enableTap/disableTap let each test
    // method plug in its own setXxxTap pair while reusing the exact same
    // race-inducing harness.
    void checkTapWaitsForInFlightWrite(
        const std::function<void(AudioEngine*, AudioTapRing*, int)>& enableTap,
        const std::function<void(AudioEngine*)>& disableTap)
    {
        auto radio = std::make_unique<RadioModel>();
        AudioEngine* engine = radio->audioEngine();
        const int sliceId = radio->addSlice();

        AudioTapRing ring(64);
        enableTap(engine, &ring, sliceId);

        std::atomic<bool> writeStarted{false};

        // Fires on the (simulated) audio thread, inside
        // writeToTapIfCurrent, exactly after the busy counter has been
        // incremented and the tap pointer loaded — i.e. inside the
        // window the fix closes.
        engine->setTapWriteDelayHookForTest([&]() {
            writeStarted.store(true, std::memory_order_release);
            std::this_thread::sleep_for(kHookDelay);
        });

        std::thread audioThread([&]() {
            engine->rxBlockReady(sliceId, kTestSamples.data(), kTestFrames);
        });

        while (!writeStarted.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // audioThread is now inside the hook's sleep, holding the tap's
        // busy counter at 1. disableTap() (setXxxTap(nullptr, -1)) MUST
        // NOT return before that write() has finished — otherwise a real
        // caller's very next line (freeing the ring, as QsoRecorderController's
        // dtor does) would race a write() still in flight.
        const auto before = std::chrono::steady_clock::now();
        disableTap(engine);
        const auto elapsed = std::chrono::steady_clock::now() - before;

        audioThread.join();
        engine->setTapWriteDelayHookForTest(nullptr);

        QVERIFY2(elapsed >= kHookDelay / 2,
                 "setXxxTap(nullptr, ...) returned before the in-flight "
                 "write() finished -- the 2026-09-06 race is back");
    }

private slots:

    void qsoTapWaitsForInFlightWrite()
    {
        checkTapWaitsForInFlightWrite(
            [](AudioEngine* e, AudioTapRing* r, int s) { e->setQsoTap(r, s); },
            [](AudioEngine* e) { e->setQsoTap(nullptr, -1); });
    }

    void asrTapWaitsForInFlightWrite()
    {
        checkTapWaitsForInFlightWrite(
            [](AudioEngine* e, AudioTapRing* r, int s) { e->setAsrTap(r, s); },
            [](AudioEngine* e) { e->setAsrTap(nullptr, -1); });
    }

    void wavRecordTapWaitsForInFlightWrite()
    {
        checkTapWaitsForInFlightWrite(
            [](AudioEngine* e, AudioTapRing* r, int s) { e->setWavRecordTap(r, s); },
            [](AudioEngine* e) { e->setWavRecordTap(nullptr, -1); });
    }

    void rttyTapWaitsForInFlightWrite()
    {
        checkTapWaitsForInFlightWrite(
            [](AudioEngine* e, AudioTapRing* r, int s) { e->setRttyTap(r, s); },
            [](AudioEngine* e) { e->setRttyTap(nullptr, -1); });
    }
};

QTEST_MAIN(TstAudioEngineTapTeardownRace)
#include "tst_audio_engine_tap_teardown_race.moc"
