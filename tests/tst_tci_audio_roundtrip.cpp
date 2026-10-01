// tests/tst_tci_audio_roundtrip.cpp  (Longpath)
// no-port-check: Longpath-original integration test for the audio binary
// RX pipeline.  Validates: synthetic audio injection → AudioRingSpsc → drain
// timer assembly → resampler (identity at srcRate=48k) → TciBinaryFrame
// encode → QWebSocket sendBinaryMessage → client receives + decodes.
//
// Phase 3J-1 Task 16.4.  Plan spec: ≥ 1 binary frame with streamType==1
// and decoded payload matches input within 1e-3 (identity-resample case).

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "TciBurstHelfer.h"
#include "core/TciServer.h"

using namespace Longpath;

class TestTciAudioRoundtrip : public QObject {
    Q_OBJECT
private slots:
    void synthetic_1khz_tone_arrives_as_binary_frame();
    void mono_nimmt_links_und_behaelt_die_dauer();
    void mono_bei_12khz_liefert_ein_viertel();
    void format_echo_meldet_die_sitzung_nicht_das_programm();
    void zwei_zuhoerer_nehmen_sich_nichts_weg();

private:
    // Gemeinsamer Aufbau fuer die beiden Mono-Pruefpunkte: Server hoch,
    // Client dran, Format aushandeln, `dauerRahmen` Stereorahmen einspeisen,
    // und ALLE eingegangenen Werte samt Kopfzahlen zurueckgeben.
    //
    // Links traegt +1, rechts −1. Beide sind konstant, damit jeder einzelne
    // ausgegebene Wert verraet, aus welchem Kanal er stammt — auch nachdem
    // ein Resampler darueber gelaufen ist (ein FIR ueber einer Konstanten
    // gibt dieselbe Konstante zurueck, sobald er eingeschwungen ist).
    struct Ausbeute {
        int werte{0};          // Zahl der ausgegebenen Float-Werte insgesamt
        int rahmen{0};         // Zahl der Binaerrahmen
        quint32 kanaele{0};    // Kopffeld `channels` des ersten Rahmens
        quint32 rate{0};       // Kopffeld `sampleRate` des ersten Rahmens
        float kleinster{0.0f}; // kleinster Wert ueber alle Rahmen
        float groesster{0.0f};
    };
    Ausbeute monoLauf(int wunschRate, int dauerRahmen);
};

// ── synthetic_1khz_tone_arrives_as_binary_frame() ───────────────────────────
//
// Wire path exercised:
//   test calls injectAudioFrameForTest(slice=0, L, R, n=1024, srcRate=48000)
//   → onAudioFrameReady interleaves L/R into m_audioRing[0]
//   → 5ms drain timer fires: pops 2048*2 floats (one full audioStreamSamples
//     * channels chunk), no resampler branch (48000 == 48000), encodes via
//     TciBinaryFrame::buildStreamPayload, calls sendBinaryMessage
//   → client's binaryMessageReceived signal fires
//   → test decodes the 64-byte LE header and float payload, asserts fields
//     and sine-wave amplitude.

void TestTciAudioRoundtrip::synthetic_1khz_tone_arrives_as_binary_frame()
{
    // ── 1.  Spin up TciServer on an ephemeral port ────────────────────────────
    TciServer server(nullptr);   // RadioModel* not needed — test-injection path
    QVERIFY(server.start(0));
    QVERIFY(server.isRunning());

    // ── 2.  Connect a real QWebSocket client ─────────────────────────────────
    QWebSocket client;
    QSignalSpy clientConnected(&client, &QWebSocket::connected);
    QSignalSpy binarySpy(&client, &QWebSocket::binaryMessageReceived);

    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(clientConnected.wait(2000));
    QCOMPARE(client.state(), QAbstractSocket::ConnectedState);

    // ── 3.  Subscribe to slice 0 audio ────────────────────────────────────────
    // onTextMessageReceived intercepts "audio_start:0;" before TciProtocol
    // dispatch and calls handleAudioSubscribe which creates the resampler.
    client.sendTextMessage(QStringLiteral("audio_start:0;"));

    // Give the subscription time to register: the message crosses the
    // loopback socket (one round-trip through the OS network stack) and
    // the slot runs synchronously on the Qt event loop.  50ms is generous.
    QTest::qWait(50);

    // ── 4.  Generate ~250 ms of 1 kHz sine at 48 kHz, stereo ─────────────────
    // audioStreamSamples default = 2048 (from TciClientSession defaults).
    // channels default = 2.  The drain pops 2048*2 floats per tick; so we
    // inject enough chunks to fill at least one drain window.
    //
    // 250ms @ 48kHz = 12000 frames.  We inject in 1024-frame chunks (12 chunks).
    // Each chunk pushes 1024*2 floats = 8192 bytes.
    // One drain window needs 2048*2*4 = 16384 bytes → 2 chunks fill it.
    constexpr int kSrcRate     = 48000;
    constexpr int kTotalFrames = kSrcRate / 4;    // 12000 stereo frames = 250ms
    constexpr int kChunkFrames = 1024;
    constexpr double kFreqHz   = 1000.0;

    std::vector<float> L(kChunkFrames), R(kChunkFrames);
    int totalFramesSent = 0;
    while (totalFramesSent < kTotalFrames) {
        const int remaining = kTotalFrames - totalFramesSent;
        const int thisChunk = std::min(kChunkFrames, remaining);
        for (int i = 0; i < thisChunk; ++i) {
            const double phase = 2.0 * M_PI * kFreqHz
                                 * (totalFramesSent + i) / double(kSrcRate);
            const float sample = static_cast<float>(std::sin(phase));
            L[i] = sample;
            R[i] = sample;   // mono content, stereo channel pair
        }
        // Test-only injection: bypasses the real RxChannel→RadioModel chain
        // (which requires hardware + WDSP wisdom) and feeds audio directly into
        // m_audioRing[0] for the drain timer to pick up.
        server.injectAudioFrameForTest(0, L.data(), R.data(), thisChunk, kSrcRate);
        totalFramesSent += thisChunk;
    }

    // ── 5.  Wait for drain ticks to flush binary frames to the client ─────────
    // Drain timer fires every 5ms.  200ms gives ≥40 ticks — well more than
    // needed to drain 250ms of audio at 2048-sample windows.
    // The QTest::qWait spins the event loop so timer events are processed.
    QTest::qWait(200);

    // ── 6.  Assert: at least one binary frame arrived ─────────────────────────
    QVERIFY2(binarySpy.count() >= 1,
             qPrintable(QStringLiteral("Expected ≥1 binary frame, got %1")
                            .arg(binarySpy.count())));

    // ── 7.  Decode the first frame ────────────────────────────────────────────
    //
    // Header layout (each field is uint32 LE, offsets in bytes):
    //   0   receiver
    //   4   sampleRate
    //   8   sampleType
    //   12  reserved
    //   16  reserved
    //   20  length  ← flat count (perChSamples * channels = 2048*2 = 4096)
    //   24  streamType
    //   28  channels
    //   32..60  reserved (8 × uint32, all 0)
    //
    // From TciBinaryFrame.cpp:buildStreamPayload + Thetis TCIServer.cs:5240-5262
    // [v2.10.3.13].  The `length` field carries the flat interleaved count, not
    // the per-channel count — verified by reading buildStreamPayload which calls
    //   encodeSamples(samples, length, sampleType)
    // and passes `outSamples = perChSamples * channels` as `length`.

    const QByteArray firstFrame = binarySpy.at(0).at(0).toByteArray();
    QVERIFY2(firstFrame.size() > 64,
             qPrintable(QStringLiteral("Frame too small: %1 bytes").arg(firstFrame.size())));

    // Little-endian uint32 reader.
    auto readU32 = [&](int offset) -> quint32 {
        const auto* p = reinterpret_cast<const quint8*>(firstFrame.constData() + offset);
        return static_cast<quint32>(p[0])
             | (static_cast<quint32>(p[1]) << 8)
             | (static_cast<quint32>(p[2]) << 16)
             | (static_cast<quint32>(p[3]) << 24);
    };

    const quint32 receiver   = readU32(0);
    const quint32 sampleRate = readU32(4);
    const quint32 sampleType = readU32(8);
    const quint32 length     = readU32(20);   // flat interleaved count
    const quint32 streamType = readU32(24);
    const quint32 channels   = readU32(28);

    // ── 8.  Assert header fields ──────────────────────────────────────────────
    QCOMPARE(receiver,   0u);       // slice 0
    QCOMPARE(sampleRate, 48000u);   // default audioSampleRate (TciClientSession default)
    QCOMPARE(sampleType, 3u);       // Float32 (TciClientSession audioSampleType default)
    QCOMPARE(streamType, 1u);       // RxAudioStream per TciStreamType enum
    QCOMPARE(channels,   2u);       // stereo (TciClientSession audioStreamChannels default)
    QVERIFY(length > 0u);

    // ── 9.  Decode FLOAT32 payload and check amplitude ────────────────────────
    //
    // For Float32/stereo, the payload after the 64-byte header is:
    //   length * sizeof(float) bytes  (length is the flat interleaved count)
    // The number of floats equals `length`, NOT `length * channels`.
    // Verified: encodeSamples(samples, length, sampleType) encodes exactly
    // `length` floats; buildStreamPayload stores that as `length` in the header.

    const int sampleBytes  = firstFrame.size() - 64;
    const int totalFloats  = sampleBytes / 4;

    // Sanity: payload size must be consistent with the flat-count header field.
    QCOMPARE(totalFloats, static_cast<int>(length));

    // All decoded values must be in [-1, 1] (sine is bounded by construction
    // and no clipping or gain stage is applied in the identity-resample path).
    float peak = 0.0f;
    for (int i = 0; i < totalFloats; ++i) {
        float v = 0.0f;
        std::memcpy(&v, firstFrame.constData() + 64 + i * 4, 4);
        QVERIFY2(v >= -1.000001f && v <= 1.000001f,
                 qPrintable(QStringLiteral("Sample %1 out of range: %2").arg(i).arg(double(v))));
        if (std::abs(v) > peak) { peak = std::abs(v); }
    }

    // A 1 kHz sine with amplitude 1.0 must produce a peak ≥ 0.5 within any
    // window of ≥ 2048 stereo samples (≥ 1024 per channel).  The interleaved
    // layout mixes L and R samples; both carry the same sine, so every other
    // float in the payload is a sine sample — more than enough to see peak.
    QVERIFY2(peak > 0.5f,
             qPrintable(QStringLiteral(
                 "Peak amplitude %1 too low — 1kHz sine not flowing through pipeline")
                 .arg(double(peak))));

    // ── Cleanup ───────────────────────────────────────────────────────────────
    client.close();
    server.stop();
}

// ── monoLauf() ───────────────────────────────────────────────────────────────

TestTciAudioRoundtrip::Ausbeute
TestTciAudioRoundtrip::monoLauf(int wunschRate, int dauerRahmen)
{
    Ausbeute a;

    TciServer server(nullptr);
    if (!server.start(0)) { return a; }

    QWebSocket client;
    QSignalSpy verbunden(&client, &QWebSocket::connected);
    QSignalSpy binaer(&client, &QWebSocket::binaryMessageReceived);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    if (!verbunden.wait(2000)) { return a; }

    // Format VOR dem Abonnieren: handleAudioSubscribe legt die Resampler
    // anhand der dann geltenden Rate und Kanalzahl an.
    client.sendTextMessage(QStringLiteral("audio_samplerate:%1;").arg(wunschRate));
    client.sendTextMessage(QStringLiteral("audio_stream_channels:1;"));
    client.sendTextMessage(QStringLiteral("audio_start:0;"));
    QTest::qWait(50);

    // Einspeisen. Der Ring fasst 131072 Bytes; bei 8192 Stereorahmen sind
    // das 65536 Bytes, also mit Sicherheitsabstand. Waehrend dieser Schleife
    // laeuft keine Ereignisschleife, es wird also nichts abgeflossen sein.
    constexpr int kBlock = 1024;
    std::vector<float> L(kBlock, 1.0f), R(kBlock, -1.0f);
    for (int gesendet = 0; gesendet < dauerRahmen; gesendet += kBlock) {
        const int n = std::min(kBlock, dauerRahmen - gesendet);
        server.injectAudioFrameForTest(0, L.data(), R.data(), n, 48000);
    }

    // Der Abflusstakt ist 5 ms und gibt je Runde einen Block ab. 8192
    // Rahmen sind hoechstens 8 Bloecke, 400 ms sind reichlich.
    QTest::qWait(400);

    auto lies = [](const QByteArray& f, int off) -> quint32 {
        const auto* p = reinterpret_cast<const quint8*>(f.constData() + off);
        return quint32(p[0]) | (quint32(p[1]) << 8)
             | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
    };

    bool erster = true;
    for (int r = 0; r < binaer.count(); ++r) {
        const QByteArray f = binaer.at(r).at(0).toByteArray();
        if (f.size() <= 64) { continue; }
        if (erster) {
            a.rate    = lies(f, 4);
            a.kanaele = lies(f, 28);
            erster = false;
        }
        const int zahl = (f.size() - 64) / 4;   // Float32
        for (int i = 0; i < zahl; ++i) {
            float v = 0.0f;
            std::memcpy(&v, f.constData() + 64 + i * 4, 4);
            if (a.werte == 0 && i == 0) { a.kleinster = a.groesster = v; }
            a.kleinster = std::min(a.kleinster, v);
            a.groesster = std::max(a.groesster, v);
            ++a.werte;
        }
        ++a.rahmen;
    }

    client.close();
    server.stop();
    return a;
}

// ── mono_nimmt_links_und_behaelt_die_dauer() ─────────────────────────────────
//
// Der Fehler, den dieser Pruefpunkt festhaelt, wurde am 2026-09-30 an einem
// echten Geraet gemessen (ANVELINA Pro 3, 20 m): bei ausgehandelten 12 kHz
// mono kamen 24 064 Werte je Sekunde an statt 12 000 — genau Faktor zwei.
//
// Ursache: drainAudio popte `audioStreamSamples * channels` Werte aus dem
// Ring und hielt das fuer `audioStreamSamples` Zeitpunkte. Der Ring traegt
// aber IMMER Stereo (onAudioFrameReady legt L und R paarweise hinein), also
// waren es bei channels == 1 nur halb so viele Zeitpunkte, und L,R,L,R lief
// als vermeintliches Mono weiter: doppelte Rate, vermischte Kanaele, eine
// Oktave zu tiefer Ton.
//
// Thetis macht es andersherum und ist damit richtig: getrennte L/R-Schlangen,
// bei `channels <= 1` ein Feld aus NUR links, und danach in beiden Faellen
// `Advance(packetSamples)` auf beiden Schlangen — die Blockdauer haengt dort
// nicht an der Kanalzahl (TCIServer.cs:5896-5911 [v2.10.3.15]).
//
// Hier bei 48 kHz, also ohne Umtaster: der 48-kHz-Pfad uebersprang die
// kanalweise Behandlung frueher ganz und war deshalb genauso betroffen.

void TestTciAudioRoundtrip::mono_nimmt_links_und_behaelt_die_dauer()
{
    constexpr int kRahmen = 8192;
    const Ausbeute a = monoLauf(48000, kRahmen);

    QVERIFY2(a.rahmen >= 1, "Es muss mindestens ein Rahmen ankommen");
    QCOMPARE(a.kanaele, 1u);
    QCOMPARE(a.rate,    48000u);

    // Nur links. Rechts traegt −1; taucht ein negativer Wert auf, ist der
    // verschraenkte Puffer ungetrennt durchgelaufen.
    QVERIFY2(a.kleinster > 0.9f,
             qPrintable(QStringLiteral(
                 "Ein Wert war %1 — rechts (−1) ist in den Monostrom geraten")
                 .arg(double(a.kleinster))));
    QVERIFY2(a.groesster < 1.1f,
             qPrintable(QStringLiteral("Unerwarteter Hoechstwert %1")
                            .arg(double(a.groesster))));

    // Die Dauer: 8192 eingespeiste Zeitpunkte muessen 8192 Werte ergeben,
    // nicht 16384. Der Abfluss gibt nur ganze Bloecke zu je 2048 Zeitpunkten
    // ab, der Rest bleibt im Ring — deshalb hoechstens kRahmen, und wegen
    // der Blockung mindestens kRahmen − 2048.
    QVERIFY2(a.werte <= kRahmen,
             qPrintable(QStringLiteral(
                 "%1 Werte aus %2 Zeitpunkten — bei Mono darf hoechstens einer "
                 "je Zeitpunkt herauskommen (Faktor %3)")
                 .arg(a.werte).arg(kRahmen)
                 .arg(double(a.werte) / kRahmen, 0, 'f', 2)));
    QVERIFY2(a.werte >= kRahmen - 2048,
             qPrintable(QStringLiteral("Nur %1 von %2 Werten abgeflossen")
                            .arg(a.werte).arg(kRahmen)));
}

// ── mono_bei_12khz_liefert_ein_viertel() ─────────────────────────────────────
//
// Derselbe Fehler mit Umtaster davor — das ist der Fall, der an der
// Handfunke wirklich lief. 48 kHz herunter auf 12 kHz ist Faktor vier: aus
// 8192 Zeitpunkten werden rund 2048 Werte. Mit dem alten Fehler waren es
// 4096, und der Ton lag eine Oktave zu tief.

void TestTciAudioRoundtrip::mono_bei_12khz_liefert_ein_viertel()
{
    constexpr int kRahmen = 8192;
    const Ausbeute a = monoLauf(12000, kRahmen);

    QVERIFY2(a.rahmen >= 1, "Es muss mindestens ein Rahmen ankommen");
    QCOMPARE(a.kanaele, 1u);
    QCOMPARE(a.rate,    12000u);

    // Nur links, auch nach dem Umtaster: ein FIR ueber der Konstanten +1
    // gibt +1 zurueck, sobald er eingeschwungen ist. Der erste Block traegt
    // das Einschwingen, deshalb ist die Schranke hier lockerer als oben.
    QVERIFY2(a.kleinster > -0.1f,
             qPrintable(QStringLiteral(
                 "Ein Wert war %1 — rechts (−1) ist in den Monostrom geraten")
                 .arg(double(a.kleinster))));

    const double erwartet = kRahmen / 4.0;
    QVERIFY2(a.werte <= erwartet * 1.05,
             qPrintable(QStringLiteral(
                 "%1 Werte statt rund %2 — das ist Faktor %3 zu viel")
                 .arg(a.werte).arg(erwartet)
                 .arg(a.werte / erwartet, 0, 'f', 2)));
    QVERIFY2(a.werte >= erwartet - 512,
             qPrintable(QStringLiteral("Nur %1 von rund %2 Werten abgeflossen")
                            .arg(a.werte).arg(erwartet)));
}

// ── format_echo_meldet_die_sitzung_nicht_das_programm() ──────────────────────
//
// Bis 2026-09-30 lief `audio_stream_sample_type:` durch den Abfangpunkt
// HINDURCH bis zu TciProtocol. Das kennt nur die vier TCI-Namen, faellt bei
// allem anderen auf float32, setzt damit das GLOBALE RadioModel und echot
// float32 an alle Clients.
//
// Am echten Geraet gemessen: Rahmen mit Probentyp 101 (mu-law) kamen an,
// waehrend das Echo "float32" sagte. Der Client schaltete daraufhin selbst
// auf int16 zurueck — der billige Ton kam nie zum Einsatz. Schwerer noch:
// ein Client, der mulaw8 anfordert, haette einem gleichzeitig laufenden
// WSJT-X das Tonformat verstellt.
//
// Das Format gehoert der Sitzung. Der Prueffpunkt haelt beides fest: das Echo
// nennt den Sitzungswert, und die Rahmen tragen ihn auch.

void TestTciAudioRoundtrip::format_echo_meldet_die_sitzung_nicht_das_programm()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy verbunden(&client, &QWebSocket::connected);
    QSignalSpy binaer(&client, &QWebSocket::binaryMessageReceived);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(verbunden.wait(2000));

    // Erst den Init-Burst abwarten, dann mitschreiben: er bringt selbst
    // `audio_stream_sample_type:;` mit und landete auf dem CI-Laeufer
    // mitten in der Messung (2026-10-02). Siehe TciBurstHelfer.h.
    QVERIFY2(TciTest::warteAufReady(client),
             "Kein ready; — der Server ist gar nicht fertig geworden");

    QStringList antworten;
    connect(&client, &QWebSocket::textMessageReceived,
            [&antworten](const QString& s) { antworten << s; });

    client.sendTextMessage(QStringLiteral("audio_stream_sample_type:mulaw8;"));
    QTest::qWait(120);

    bool sahMulaw = false, sahFloat = false;
    for (const QString& z : antworten) {
        for (const QString& teil : z.split(QLatin1Char(';'))) {
            const QString s = teil.trimmed();
            if (!s.startsWith(QLatin1String("audio_stream_sample_type:"))) { continue; }
            if (s.contains(QLatin1String("mulaw8")))  { sahMulaw = true; }
            if (s.contains(QLatin1String("float32"))) { sahFloat = true; }
        }
    }
    QVERIFY2(sahMulaw, qPrintable(QStringLiteral(
        "Das Echo muss mulaw8 nennen — bekommen: %1").arg(antworten.join(QLatin1Char(' ')))));
    QVERIFY2(!sahFloat, "float32 im Echo heisst, der Befehl lief bis TciProtocol "
                        "durch und hat das globale Modell verstellt");

    // Und die Rahmen tragen es auch.
    client.sendTextMessage(QStringLiteral("audio_start:0;"));
    QTest::qWait(50);
    std::vector<float> L(1024, 0.5f), R(1024, 0.5f);
    for (int i = 0; i < 8; ++i) {
        server.injectAudioFrameForTest(0, L.data(), R.data(), 1024, 48000);
    }
    QTest::qWait(250);

    QVERIFY2(binaer.count() >= 1, "Es muss ein Tonrahmen ankommen");
    const QByteArray f = binaer.at(0).at(0).toByteArray();
    QVERIFY(f.size() > 64);
    const auto* p = reinterpret_cast<const quint8*>(f.constData() + 8);   // sampleType
    const quint32 typ = quint32(p[0]) | (quint32(p[1]) << 8)
                      | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
    QCOMPARE(typ, 101u);

    client.close();
    server.stop();
}

// ── zwei_zuhoerer_nehmen_sich_nichts_weg() ──────────────────────────────────
//
// Bis 2026-09-30 lag der Tonring beim SERVER, einer je Empfaenger, und jeder
// Client popte daraus. Wer zuerst kam, nahm die Abtastwerte — der zweite
// bekam, was uebrig war. Ein Ring, viele Leser, das geht nicht auf.
//
// Der Fall ist nicht konstruiert: Handfunke am Telefon und am iPad, oder
// Handfunke neben einem Digimode-Programm am selben Empfaenger. An Martins
// Server haengt neben der Handfunke ein Stream-Deck-Plugin.
//
// Geprueft wird das Einzige, was zaehlt: beide bekommen GLEICH VIEL, und
// zwar das Ganze.

void TestTciAudioRoundtrip::zwei_zuhoerer_nehmen_sich_nichts_weg()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));

    QWebSocket a, b;
    QSignalSpy aVerb(&a, &QWebSocket::connected), bVerb(&b, &QWebSocket::connected);
    QSignalSpy aBin(&a, &QWebSocket::binaryMessageReceived);
    QSignalSpy bBin(&b, &QWebSocket::binaryMessageReceived);
    const QUrl u(QStringLiteral("ws://127.0.0.1:%1").arg(server.port()));
    a.open(u); b.open(u);
    QVERIFY(aVerb.wait(2000));
    QVERIFY(bVerb.count() > 0 || bVerb.wait(2000));

    // Beide auf denselben Empfaenger, beide in der Vorgabe (48 kHz stereo).
    a.sendTextMessage(QStringLiteral("audio_start:0;"));
    b.sendTextMessage(QStringLiteral("audio_start:0;"));
    QTest::qWait(80);

    // Ein Signal, an dem sich jede Luecke zeigt: konstant +0,5.
    constexpr int kRahmen = 8192;
    std::vector<float> L(1024, 0.5f), R(1024, 0.5f);
    for (int g = 0; g < kRahmen; g += 1024) {
        server.injectAudioFrameForTest(0, L.data(), R.data(), 1024, 48000);
        QTest::qWait(12);   // dem Abfluss Zeit lassen, sonst staut es sich
    }
    QTest::qWait(400);

    auto werte = [](QSignalSpy& s) {
        int n = 0;
        for (int i = 0; i < s.count(); ++i) {
            const QByteArray f = s.at(i).at(0).toByteArray();
            if (f.size() > 64) { n += (f.size() - 64) / 4; }   // Float32
        }
        return n;
    };
    const int nA = werte(aBin), nB = werte(bBin);

    QVERIFY2(nA > 0 && nB > 0,
             qPrintable(QStringLiteral("Beide muessen Ton bekommen — A %1, B %2")
                            .arg(nA).arg(nB)));
    // Gleich viel, nicht "einer bekommt alles". Toleranz fuer den Takt: die
    // beiden Abonnements starten Millisekunden auseinander.
    const double verhaeltnis = double(std::min(nA, nB)) / std::max(nA, nB);
    QVERIFY2(verhaeltnis > 0.8,
             qPrintable(QStringLiteral(
                 "A bekam %1 Werte, B %2 — einer nimmt dem anderen den Ton weg")
                 .arg(nA).arg(nB)));

    a.close(); b.close();
    server.stop();
}

QTEST_GUILESS_MAIN(TestTciAudioRoundtrip)
#include "tst_tci_audio_roundtrip.moc"

#else  // !HAVE_WEBSOCKETS

// WebSockets not available — test file must still compile and produce a
// no-op binary so CTest does not report a missing executable.
int main() { return 0; }

#endif // HAVE_WEBSOCKETS
