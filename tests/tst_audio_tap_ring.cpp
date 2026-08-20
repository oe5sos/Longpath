// =================================================================
// tests/tst_audio_tap_ring.cpp  (NereusSDR)
// =================================================================
//
// Der Zwischenspeicher zwischen Audio-Faden und Aufnahme.
//
// Was hier geprueft wird, ist nicht „laeuft durch", sondern die drei
// Faelle, in denen so ein Speicher still falsch wird:
//
//   1. Er laeuft um. Wer beim Umlauf falsch rechnet, bekommt eine
//      Aufnahme, in der ein Stueck aus der Mitte an den Anfang
//      springt — und das faellt beim Hoeren auf, nie beim Lesen.
//   2. Er laeuft ueber. Dann muss NEUES wegfallen und die Zahl der
//      verlorenen Werte stehen. Eine stille Luecke ist das
//      Schlimmste, was hier passieren kann.
//   3. Voll und leer duerfen nicht derselbe Zustand sein.
//
// Und ein Lauf mit zwei Faeden, weil genau dafuer der ganze Aufwand
// betrieben wird. Er beweist nichts (Nebenlaeufigkeitsfehler zeigen
// sich nicht auf Bestellung), aber er faengt das grobe Versehen.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <thread>
#include <vector>

#include "core/audio/AudioTapRing.h"

using namespace Longpath;

class TestAudioTapRing : public QObject
{
    Q_OBJECT

private slots:

    void anEmptyRingSwallowsNothing()
    {
        AudioTapRing r(0);
        const float in[4] = {1, 2, 3, 4};
        QCOMPARE(r.write(in, 4), 0);
        QCOMPARE(r.available(), 0);
    }

    void whatGoesInComesOutInOrder()
    {
        AudioTapRing r(16);
        const float in[4] = {0.1f, 0.2f, 0.3f, 0.4f};
        QCOMPARE(r.write(in, 4), 4);
        QCOMPARE(r.available(), 4);

        float out[8] = {};
        QCOMPARE(r.read(out, 8), 4);
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(out[i], in[i]);
        }
        QCOMPARE(r.available(), 0);
    }

    // DER FALL, DER STILL FALSCH WIRD: mehrfach umlaufen. Wer sich beim
    // Umlauf verrechnet, bekommt eine Aufnahme mit einem Sprung darin.
    void itKeepsOrderAcrossManyWraps()
    {
        AudioTapRing r(8);
        int next = 0;
        int expect = 0;

        for (int round = 0; round < 50; ++round) {
            float in[5];
            for (float& v : in) { v = static_cast<float>(next++); }
            QCOMPARE(r.write(in, 5), 5);

            float out[5] = {};
            QCOMPARE(r.read(out, 5), 5);
            for (float v : out) {
                QCOMPARE(v, static_cast<float>(expect++));
            }
        }
        QCOMPARE(r.dropped(), 0LL);
    }

    // Ueberlauf: NEUES faellt weg, Altes bleibt. Andersherum waere die
    // Aufnahme am Anfang kaputt, wo es niemand sucht.
    void whenItOverflowsTheNewSamplesAreTheOnesThatGo()
    {
        AudioTapRing r(4);
        const float in[6] = {1, 2, 3, 4, 5, 6};

        QCOMPARE(r.write(in, 6), 4);
        QCOMPARE(r.dropped(), 2LL);

        float out[6] = {};
        QCOMPARE(r.read(out, 6), 4);
        QCOMPARE(out[0], 1.0f);
        QVERIFY2(out[3] == 4.0f,
                 "es muessen die ERSTEN vier drinstehen, nicht die letzten");
    }

    // Voll und leer duerfen sich nicht gleich anfuehlen.
    void aFullRingDoesNotLookEmpty()
    {
        AudioTapRing r(4);
        const float in[4] = {1, 2, 3, 4};
        QCOMPARE(r.write(in, 4), 4);
        QCOMPARE(r.available(), 4);
    }

    // ── Der Unterschied zwischen verloren und gewartet ───────────────
    //
    // dropped() traegt die Aussage „in der Aufnahme fehlt etwas", und die
    // Anzeige stellt sie dem Betreiber als Warnung hin. Ein Schreiber,
    // der nach einem Teilschreiben erneut anklopft, hat nichts verloren.
    // Wuerde beides gleich zaehlen, stuende die Warnung bei jeder vollen
    // Runde da und waere nach dem dritten Mal nichts mehr wert.
    void retryingIsNotLosing()
    {
        AudioTapRing r(4);
        const float in[6] = {1, 2, 3, 4, 5, 6};

        QCOMPARE(r.tryWrite(in, 6), 4);
        QVERIFY2(r.dropped() == 0LL,
                 "wer es gleich nochmal versucht, hat nichts verloren");

        float out[4] = {};
        r.read(out, 4);
        QCOMPARE(r.tryWrite(in + 4, 2), 2);
        QCOMPARE(r.dropped(), 0LL);
    }

    // Und umgekehrt: wer nicht warten kann, verliert wirklich.
    void theAudioThreadCannotWaitSoItsLossIsCounted()
    {
        AudioTapRing r(4);
        const float in[6] = {1, 2, 3, 4, 5, 6};

        QCOMPARE(r.write(in, 6), 4);
        QCOMPARE(r.dropped(), 2LL);
    }

    void resetEmptiesItAndForgetsTheLosses()
    {
        AudioTapRing r(2);
        const float in[4] = {1, 2, 3, 4};
        r.write(in, 4);
        QVERIFY(r.dropped() > 0);

        r.reset();
        QCOMPARE(r.available(), 0);
        QCOMPARE(r.dropped(), 0LL);
    }

    // Zwei Faeden. Beweist nichts, faengt aber das grobe Versehen —
    // und genau dafuer steht die Klasse ueberhaupt da.
    void oneWriterOneReaderKeepEveryValue()
    {
        AudioTapRing r(1024);
        constexpr int kTotal = 200000;

        std::thread writer([&r]() {
            int sent = 0;
            while (sent < kTotal) {
                float block[64];
                const int n = std::min(64, kTotal - sent);
                for (int i = 0; i < n; ++i) {
                    block[i] = static_cast<float>((sent + i) % 1000);
                }
                // tryWrite, nicht write: dieser Schreiber versucht es
                // gleich nochmal. write() wuerde jeden Teilschreibvorgang
                // als Verlust zaehlen, und die Anzeige stellt dropped()
                // dem Betreiber als „in der Aufnahme fehlt etwas" hin.
                // GENAU DAS hat dieser Test unter Last gefunden: 61 376
                // gemeldete Verluste, obwohl jeder Wert ankam.
                int done = 0;
                while (done < n) {
                    done += r.tryWrite(block + done, n - done);
                    std::this_thread::yield();
                }
                sent += n;
            }
        });

        int got = 0;
        bool orderHolds = true;
        while (got < kTotal) {
            float out[128];
            const int n = r.read(out, 128);
            for (int i = 0; i < n; ++i) {
                if (out[i] != static_cast<float>((got + i) % 1000)) {
                    orderHolds = false;
                }
            }
            got += n;
            if (n == 0) { std::this_thread::yield(); }
        }
        writer.join();

        QCOMPARE(got, kTotal);
        QVERIFY2(orderHolds, "die Reihenfolge ist unter zwei Faeden gerissen");
        QCOMPARE(r.dropped(), 0LL);
    }
};

QTEST_MAIN(TestAudioTapRing)
#include "tst_audio_tap_ring.moc"
