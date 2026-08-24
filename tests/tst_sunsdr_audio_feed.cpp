// SPDX-License-Identifier: GPL-3.0-or-later
//
// AudioEngine::feedSunSdrAudioData — der Ton-Haken fuer den SunSDR2 QRP
// (TCI-Client), Schritt 2a.
//
// Anlass, 2026-08-24: derselbe Zuschnitt wie KiwiSDR-Ton (feedKiwiSdrAudioData
// -> rxBlockReady, siehe AudioEngine.h), aber mit EINEM Unterschied — TCI
// erlaubt die Quellrate jederzeit umzustellen (audio_samplerate), waehrend
// Kiwi-Ton fest bei 24 kHz liegt. Das ist genau die Stelle, an der ein
// naives Kopieren des Kiwi-Musters schiefgehen wuerde: ein Wandler, der fuer
// 48 kHz gebaut wurde, aber weiter benutzt wird, nachdem die Quelle auf
// 96 kHz umgestellt hat, liefert Unsinn oder ein Knacken, still.
//
// Es gibt in diesem Projekt bisher KEINEN Test, der feedKiwiSdrAudioData
// selbst durchspielt (nur den Ein/Aus-Zustand drumherum, siehe
// tst_kiwi_tx_mute.cpp) — kein Vorbild zum Abschreiben also. Diese Pruefung
// geht darum direkt auf das, was neu ist: die Ratenumstellung mitten im
// Betrieb darf nicht abstuerzen und darf den Wandler nicht unbemerkt mit der
// falschen Rate weiterlaufen lassen.

#include <QtTest>

#include <vector>

#include "core/AudioEngine.h"
#include "gui/MainWindow.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

// 4096 interleaved-Werte = 2048 Rahmen je Kanal -- groessenordnungsmaessig
// das, was ein TCI-Tonrahmen tatsaechlich traegt (gemessen: Feld20=8192 bei
// 48 kHz, siehe docs/TCI-SunSDR-gemessen.md; hier kleiner gehalten, damit
// der Test schnell bleibt -- die Groesse selbst ist fuer das Verhalten
// nicht wichtig).
std::vector<float> makeInterleavedStereo(int frames, float value)
{
    return std::vector<float>(static_cast<size_t>(frames) * 2, value);
}

} // namespace

class TstSunSdrAudioFeed : public QObject
{
    Q_OBJECT

private slots:
    void tonFliesstNurWennEingeschaltet()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        AudioEngine* audio = model->audioEngine();
        QVERIFY(audio);
        SliceModel* slice = model->activeSlice();
        QVERIFY2(slice, "keine aktive Scheibe -- der Rest der Pruefung prueft nichts");
        const int sliceId = slice->sliceIndex();

        QVERIFY2(!audio->sunSdrAudioEnabled(sliceId),
                 "frisch gestartet sollte SunSDR-Ton fuer keine Scheibe an sein");

        const std::vector<float> pcm = makeInterleavedStereo(512, 0.1f);

        // Vor dem Einschalten: darf nichts tun, darf nicht abstuerzen.
        audio->feedSunSdrAudioData(sliceId, pcm.data(), 512, 48000);

        audio->setSunSdrAudioSourceEnabled(sliceId, true);
        QVERIFY(audio->sunSdrAudioEnabled(sliceId));

        // Jetzt darf es Werte annehmen -- der eigentliche Beweis waere ein
        // Blick in MasterMixer, der keinen oeffentlichen Lesezugriff dafuer
        // bietet (wie schon bei Kiwi-Ton). Gemessen wird darum wie beim
        // Kiwi-Vorbild am Zustand aussen: kein Absturz, Zustand bleibt an.
        audio->feedSunSdrAudioData(sliceId, pcm.data(), 512, 48000);
        QVERIFY(audio->sunSdrAudioEnabled(sliceId));

        audio->setSunSdrAudioSourceEnabled(sliceId, false);
        QVERIFY(!audio->sunSdrAudioEnabled(sliceId));

        mw->close();
    }

    void ratenumstellungMittenImBetriebStuerztNichtAb()
    {
        // Der eigentliche Anlass dieser Pruefung: TCI kann audio_samplerate
        // jederzeit aendern. Der Wandler fuer die alte Rate muss verworfen
        // werden, nicht mit falscher Rate weiterlaufen.
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        AudioEngine* audio = model->audioEngine();
        QVERIFY(audio);
        SliceModel* slice = model->activeSlice();
        QVERIFY(slice);
        const int sliceId = slice->sliceIndex();

        audio->setSunSdrAudioSourceEnabled(sliceId, true);

        const std::vector<float> at48k = makeInterleavedStereo(512, 0.2f);
        const std::vector<float> at96k = makeInterleavedStereo(1024, 0.3f);
        const std::vector<float> backAt48k = makeInterleavedStereo(512, 0.1f);

        // Mehrere Bloecke bei 48 kHz -- baut den Wandler auf.
        for (int i = 0; i < 3; ++i) {
            audio->feedSunSdrAudioData(sliceId, at48k.data(), 512, 48000);
        }
        // Umstellung auf 96 kHz mitten im Betrieb -- der alte Wandler (fuer
        // 48 kHz) muss verworfen und durch einen neuen ersetzt werden.
        for (int i = 0; i < 3; ++i) {
            audio->feedSunSdrAudioData(sliceId, at96k.data(), 1024, 96000);
        }
        // Und zurueck auf 48 kHz -- derselbe Wechsel in die andere Richtung.
        for (int i = 0; i < 3; ++i) {
            audio->feedSunSdrAudioData(sliceId, backAt48k.data(), 512, 48000);
        }

        // Kein Absturz bis hierher ist der Kern dieser Pruefung. Der
        // Zustand bleibt unveraendert an -- eine Ratenumstellung ist kein
        // Abschalten.
        QVERIFY(audio->sunSdrAudioEnabled(sliceId));

        mw->close();
    }

    void ungueltigeEingabenWerdenVerworfenNichtAbgestuerzt()
    {
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        AudioEngine* audio = model->audioEngine();
        QVERIFY(audio);
        SliceModel* slice = model->activeSlice();
        QVERIFY(slice);
        const int sliceId = slice->sliceIndex();

        audio->setSunSdrAudioSourceEnabled(sliceId, true);

        const std::vector<float> pcm = makeInterleavedStereo(64, 0.1f);

        // rate <= 0: der Aufrufer kennt die wahre Rate noch nicht
        // (TciClient::audioSampleRate() liest 0 vor der Selbstauskunft).
        audio->feedSunSdrAudioData(sliceId, pcm.data(), 64, 0);
        audio->feedSunSdrAudioData(sliceId, pcm.data(), 64, -48000);
        // frames <= 0
        audio->feedSunSdrAudioData(sliceId, pcm.data(), 0, 48000);
        // nullptr
        audio->feedSunSdrAudioData(sliceId, nullptr, 64, 48000);

        // Keines davon darf abstuerzen, und der Ein-Zustand bleibt
        // unberuehrt -- eine verworfene Eingabe ist kein Abschalten.
        QVERIFY(audio->sunSdrAudioEnabled(sliceId));

        mw->close();
    }

    void abschaltenWirftDenWandlerWeg()
    {
        // removeSunSdrAudioSource() muss den Wandler wirklich entfernen,
        // nicht nur den Ein-Zustand loeschen -- sonst traegt ein neuer
        // Empfaenger auf derselben Scheibe den Filterzustand des alten
        // mit sich (siehe Kommentar an removeSunSdrAudioSource). Von
        // aussen ist das nur indirekt zu pruefen: nach dem Entfernen und
        // erneutem Einschalten darf ein Block bei einer ANDEREN Rate
        // sofort ohne Absturz ankommen -- ein liegen gebliebener Wandler
        // fuer die alte Rate waere hier die Fehlerquelle.
        auto* mw = new MainWindow();
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, 15000));

        RadioModel* model = mw->radioModelForTest();
        QVERIFY(model);
        AudioEngine* audio = model->audioEngine();
        QVERIFY(audio);
        SliceModel* slice = model->activeSlice();
        QVERIFY(slice);
        const int sliceId = slice->sliceIndex();

        audio->setSunSdrAudioSourceEnabled(sliceId, true);
        const std::vector<float> at48k = makeInterleavedStereo(512, 0.2f);
        audio->feedSunSdrAudioData(sliceId, at48k.data(), 512, 48000);

        audio->removeSunSdrAudioSource(sliceId);
        QVERIFY2(!audio->sunSdrAudioEnabled(sliceId),
                 "removeSunSdrAudioSource haette auch den Ein-Zustand loeschen muessen");

        audio->setSunSdrAudioSourceEnabled(sliceId, true);
        const std::vector<float> at192k = makeInterleavedStereo(2048, 0.4f);
        audio->feedSunSdrAudioData(sliceId, at192k.data(), 2048, 192000);
        QVERIFY(audio->sunSdrAudioEnabled(sliceId));

        mw->close();
    }
};

QTEST_MAIN(TstSunSdrAudioFeed)
#include "tst_sunsdr_audio_feed.moc"
