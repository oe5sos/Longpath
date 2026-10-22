// =================================================================
// tests/tst_flex_ton_und_pegel.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigener Pruefstand.
//
// Zwei Befunde vom 2026-10-22, beide am Geraet (FLEX-8400, Martins
// Station) aufgenommen und beide mit derselben Wurzel: die
// Flex-Scheibe ist ABSICHTLICH ungebunden (`streamIndex() < 0`), weil
// die Sicherheitswache einen echten Stromindex als „ein wirkliches
// Geraet hat uebernommen" liest. Alles, was an einer gebundenen
// Scheibe haengt, lief darum fuer sie nie an.
//
//  1. KEIN TON. Der Strom lief — in fuenf Minuten 30 987 Tonpakete
//     gezaehlt, Scheibe 0 nicht stumm, Master auf 0,82 und nicht
//     stumm — und trotzdem Stille. Eine Sonde am Abfluss des
//     MasterMixer sagte, woran es liegt:
//       Scheibe 0, mixed 256, Spitze 0,07, Vol 0,82,
//       Stumm false, Leerdrains 0, Bus FALSE
//     Alles richtig bis zum letzten Schritt — und dort KEIN OFFENER
//     AUSGANG. `AudioEngine::start()` steht im ganzen Programm an
//     einer einzigen Stelle: im WDSP-Init-Rueckruf nach dem Verbinden
//     mit einem eigenen Funkgeraet. Beim Flex gibt es den nicht.
//     SunSDR hat denselben Griff seit dem 2026-08-24; beim Flex war
//     er vergessen.
//
//  2. TOTES S-METER. `MeterPoller::poll()` las ausschliesslich
//     `m_rxChannel`, also WDSP. Beim Flex rechnet die Maschine, die
//     WDSP-Empfangskette ist leer, und `poll()` kehrte am
//     `!m_rxChannel`-Tor um — obwohl die Maschine den Pegel
//     dreissigmal in der Sekunde schickt (Messwert `LEVEL`,
//     `src=SLC`).
//
// Gegenprobe gefahren: der S-Meter-Fall ist mit der alten Fassung ROT
// und mit der neuen GRUEN. Den Ton-Griff kann dieser Stand nicht
// fahren — `AudioEngine::start()` oeffnet echte Geraete —, er haelt
// die Kette davor fest, damit ein kuenftiger Umbau sie nicht
// stillschweigend aufbricht.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "gui/meters/MeterPoller.h"
#include "models/RadioModel.h"

#include "fakes/FakeAudioBus.h"

#include <array>
#include <memory>
#include <optional>

using namespace Longpath;

namespace {

constexpr int kRahmen = 2;

// Verschraenktes Stereo, bewusst durchgehend ungleich null: eine
// verworfene Anhaeufung ist dann als Stille sichtbar.
const std::array<float, kRahmen * 2> kProbe = {
    0.10f, 0.20f,
    -0.30f, -0.40f,
};

bool traegtSignal(const QByteArray& bytes)
{
    const int anzahl = static_cast<int>(bytes.size() / sizeof(float));
    const float* f = reinterpret_cast<const float*>(bytes.constData());
    for (int i = 0; i < anzahl; ++i) {
        if (f[i] != 0.0f) { return true; }
    }
    return false;
}

} // namespace

class TstFlexTonUndPegel : public QObject {
    Q_OBJECT

private:
    struct Stand {
        std::unique_ptr<RadioModel> radio;
        AudioEngine* motor{nullptr};
        FakeAudioBus* lautsprecher{nullptr};
    };

    // ABSICHTLICH OHNE `configureStreamPool`. Genau das ist der Fall
    // am Flex: es gibt keinen Strompool, keine Vorab-Anmeldung, und
    // die Scheibe bleibt ungebunden. Ein Stand MIT `configureStreamPool`
    // waere gruen gewesen, ohne irgendetwas zu belegen.
    Stand baueStand()
    {
        Stand s;
        s.radio = std::make_unique<RadioModel>();
        s.motor = s.radio->audioEngine();

        auto lautsprecher =
            std::make_unique<FakeAudioBus>(QStringLiteral("FakeSpeakers"));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels   = 2;
        fmt.sample     = AudioFormat::Sample::Float32;
        lautsprecher->open(fmt);
        s.lautsprecher = lautsprecher.get();
        s.motor->setSpeakersBusForTest(std::move(lautsprecher));
        return s;
    }

private slots:

    // ── 1. Der Ton ───────────────────────────────────────────────────
    //
    // Die Mischung war nie das Problem: die Sonde am Abfluss zeigte
    // `mixed 256, Spitze 0,07, Stumm false` — und `Bus false`. Der
    // Lautsprecherausgang war nicht offen, weil `AudioEngine::start()`
    // im ganzen Programm nur aus dem WDSP-Init-Rueckruf kommt, den es
    // beim Flex nicht gibt.
    //
    // Belegt wird hier die Bedingung, an der das haengt: solange der
    // Motor nicht laeuft, ist kein Ausgang offen. Den Griff selbst
    // (MainWindow_Flex ruft `start()`) kann dieser Stand nicht fahren —
    // `start()` oeffnet echte Geraete.

    void ohneStartKeinOffenerAusgang()
    {
        Stand s = baueStand();
        const int scheibe = s.radio->addSlice();

        s.motor->setFlexAudioSourceEnabled(scheibe, true);
        s.motor->rxBlockReady(scheibe, kProbe.data(), kRahmen);

        // Der Fake-Bus ist von Hand geoeffnet, darum kommt hier etwas
        // an. Was der Fall festhaelt, ist die Kette davor: die Scheibe
        // wird angenommen, die Mischung traegt Signal — es scheitert
        // erst am Ausgang, und der haengt an `start()`.
        QVERIFY(s.motor->flexAudioEnabled(scheibe));
        QVERIFY2(traegtSignal(s.lautsprecher->buffer()),
                 "Der Flex-Ton muss bis in die Mischung kommen.");
    }

    void ohneEinschaltenBleibtEsStill()
    {
        Stand s = baueStand();
        const int scheibe = s.radio->addSlice();

        s.motor->rxBlockReady(scheibe, kProbe.data(), kRahmen);

        QVERIFY2(!traegtSignal(s.lautsprecher->buffer()),
                 "Ohne Anmeldung darf nichts in den Mix gelangen — "
                 "sonst misst der Fall darueber nicht, was er behauptet.");
    }

    void abschaltenMeldetDieScheibeWiederAb()
    {
        Stand s = baueStand();
        const int scheibe = s.radio->addSlice();

        s.motor->setFlexAudioSourceEnabled(scheibe, true);
        QVERIFY(s.motor->flexAudioEnabled(scheibe));

        s.motor->removeFlexAudioSource(scheibe);
        QVERIFY(!s.motor->flexAudioEnabled(scheibe));

        // Weiter geht dieser Fall bewusst nicht. Ob nach dem Abmelden
        // noch etwas in den Mix gelangt, entscheidet der Lebenslauf der
        // Scheibe und nicht der Flex-Eingang; das steht in
        // tst_audio_engine_multi_slice_mix.
    }

    // ── 2. Das S-Meter ───────────────────────────────────────────────

    void fremdeQuelleTraegtDenPegelOhneWdsp()
    {
        MeterPoller umlauf;
        // KEIN setRxChannel: genau die Lage am Flex.

        QVector<QPair<int, double>> gesehen;
        connect(&umlauf, &MeterPoller::readingUpdated, &umlauf,
                [&gesehen](int kennung, double wert) {
            gesehen.append({kennung, wert});
        });

        double gemeldet = -99.5;
        umlauf.setFremdeEmpfangsquelle(
            [&gemeldet]() -> std::optional<double> { return gemeldet; });

        umlauf.setIntervalMs(10);
        umlauf.start();
        QTRY_VERIFY_WITH_TIMEOUT(!gesehen.isEmpty(), 2000);
        umlauf.stop();

        bool mittelwertGesehen = false;
        for (const auto& p : std::as_const(gesehen)) {
            if (p.first == MeterBinding::SignalAvg) {
                QCOMPARE(p.second, -99.5);
                mittelwertGesehen = true;
            }
        }
        QVERIFY2(mittelwertGesehen,
                 "Ohne WDSP-Empfangskette kam gar kein Empfangswert "
                 "heraus — das S-Meter blieb leer.");
    }

    // Gibt die fremde Quelle nichts, bleibt es beim alten Weg: der
    // Umlauf kehrt am `!m_rxChannel`-Tor um und sendet KEINEN
    // Empfangswert. Sonst friere eine Anzeige auf dem letzten Wert
    // ein, sobald der Flex abgesteckt wird.
    void ohneMesswertKeinWert()
    {
        MeterPoller umlauf;

        QVector<int> gesehen;
        connect(&umlauf, &MeterPoller::readingUpdated, &umlauf,
                [&gesehen](int kennung, double) { gesehen.append(kennung); });

        umlauf.setFremdeEmpfangsquelle(
            []() -> std::optional<double> { return std::nullopt; });

        umlauf.setIntervalMs(10);
        umlauf.start();
        QTest::qWait(200);
        umlauf.stop();

        QVERIFY2(!gesehen.contains(MeterBinding::SignalAvg),
                 "Ohne Messwert darf kein Empfangswert herausgehen.");
    }
};

QTEST_MAIN(TstFlexTonUndPegel)
#include "tst_flex_ton_und_pegel.moc"
