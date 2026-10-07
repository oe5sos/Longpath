// Ton im Programm, nichts aus den Lautsprechern (2026-10-04)
//
// Am Mac des Betreibers hatte Longpath Ton -- die Handy-App bekam ihn ueber
// TCI --, aus den Lautsprechern kam nichts. Im Log:
//
//   07:24:21  PortAudioBus: output via [Core Audio] on "MacBook Air-..."
//   07:57:45  AudioEngine started ( speakers bus open )
//
// Der Ausgang war 33 Minuten VOR dem Verbinden geoeffnet worden.
// AudioEngine::ensureSpeakersOpen() prueft mit isOpen(), und das ist bei
// PortAudioBus ein Zeigervergleich (m_stream != nullptr). Stirbt der Strom
// darunter -- Geraet gewechselt, Rate umgestellt (auf dem Rechner laufen
// BoomAudio und DeskFx als virtuelle Treiber), Ruhezustand --, bleibt der
// Zeiger stehen. ensureSpeakersOpen kehrt sofort zurueck, und Longpath
// schreibt in einen toten Strom, ohne dass irgendwo etwas auffaellt.

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "models/RadioModel.h"

#include "fakes/FakeAudioBus.h"

#include <memory>

using namespace Longpath;

class TstAudioEngineDeadSpeakers : public QObject {
    Q_OBJECT

private slots:
    // Ein lebender Bus bleibt in Ruhe -- sonst wuerde die Behebung bei
    // jedem Start unnoetig neu oeffnen und es klickte.
    void lebenderAusgangBleibtStehen()
    {
        RadioModel radio;
        AudioEngine* engine = radio.audioEngine();
        QVERIFY(engine != nullptr);

        auto bus = std::make_unique<FakeAudioBus>();
        FakeAudioBus* roh = bus.get();
        roh->open(AudioFormat{48000, 2, AudioFormat::Sample::Float32});
        engine->setSpeakersBusForTest(std::move(bus));
        QCOMPARE(engine->speakersBusForTest(), static_cast<const IAudioBus*>(roh));

        engine->ensureSpeakersOpenForTest();
        QCOMPARE(engine->speakersBusForTest(), static_cast<const IAudioBus*>(roh));
    }

    // Geoeffnet, aber tot: der Bus muss weg. Vor der Behebung blieb er
    // stehen, weil isOpen() weiter "ja" sagte.
    void toterAusgangWirdWeggeraeumt()
    {
        RadioModel radio;
        AudioEngine* engine = radio.audioEngine();
        QVERIFY(engine != nullptr);

        auto bus = std::make_unique<FakeAudioBus>();
        FakeAudioBus* roh = bus.get();
        roh->open(AudioFormat{48000, 2, AudioFormat::Sample::Float32});
        roh->setLebendig(false);          // offen, aber tot
        QVERIFY(roh->isOpen());
        QVERIFY(!roh->isAlive());
        engine->setSpeakersBusForTest(std::move(bus));

        engine->ensureSpeakersOpenForTest();

        // Entweder steht dort jetzt ein neuer Bus oder gar keiner (wenn das
        // Geraet im Pruefstand nicht aufgeht). Beides ist richtig -- nur der
        // tote darf nicht stehenbleiben.
        QVERIFY(engine->speakersBusForTest() != static_cast<const IAudioBus*>(roh));
    }
};

QTEST_MAIN(TstAudioEngineDeadSpeakers)
#include "tst_audio_engine_dead_speakers.moc"
