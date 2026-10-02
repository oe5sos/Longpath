// no-port-check: Longpath-original struct condensing Thetis TCIServer.cs:684-790
// [v2.10.3.13] field semantics into a Qt6-native layout.  The inline cites are
// traceability markers — the struct body is Longpath-original C++ code;
// Thetis threading/locking/queue primitives are replaced by Qt6 signal/slot.
// Copyright notice for the upstream field semantics is in the Upstream reference
// block below.

// src/core/TciClientSession.h  (Longpath)
// Longpath-original — per-client session state for the TCI WebSocket server.
//
// This struct condenses the 49-field TCPIPtciSocketListener class from Thetis
// to ~14 fields that Longpath's architecture actually requires.  See the
// divergence note below.
//
// Fields ported from Thetis TCIServer.cs:684-790 [v2.10.3.13] are cited
// inline.  Subsequent phases extend this struct:
//   - Phase 14: outbound send queues (m_outboundUrgentFrames etc.) replace
//               Thetis's per-client sender thread + AutoResetEvent.
//   - Phase 16: RX audio resampler state (one Resampler per DAX channel per
//               client, replacing the m_rxAudioResamplers Dictionary).
//
// Upstream reference: Thetis TCIServer.cs:684-790 [v2.10.3.13]
//   https://github.com/ramdor/Thetis
//   Copyright (C) 2020-2025 Richard Samphire MW0LGE
//
// Modification history (Longpath):
//   2026-05-10 — Phase 3J-1 Task 2.1 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.

#pragma once
#ifdef HAVE_WEBSOCKETS

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QString>

#include <array>

#include "TciSendQueue.h"
#include "core/audio/AudioRingSpsc.h"

class QWebSocket;

namespace Longpath {

// ── Architectural divergence: Thetis 49 fields → Longpath 14 fields ────────
//
// Thetis TCPIPtciSocketListener (TCIServer.cs:684-790 [v2.10.3.13]) holds:
//   - 4 per-client threads (listener / sender / VFO-drain / one-shot timers)
//   - 3 mutex-style lock objects (m_objStreamLock, m_objOutboundLock,
//     m_objRxAudioLock) + an AutoResetEvent (m_outboundFrameEvent)
//   - 5 outbound queues (urgent / binary / control / coalesced-order /
//     coalesced-frames)
//   - Stopwatch + Timer pairs for VFO throttle, centre throttle, TX freq
//   - Per-channel resampler state Dictionaries
//
// Longpath replaces all threading + locking + queue primitives with:
//   - Qt6 QWebSocket signal/slot — the event loop IS the listener thread
//   - Phase 14 TciSendQueue — a lock-free per-client output queue whose
//     drain runs on the TCI event loop, replacing the sender thread +
//     AutoResetEvent + three outbound queues
//   - Phase 15 VFO coalescer — a QTimer-based coalescer on the TCI thread,
//     replacing the m_swVFO Stopwatch + m_tmVFOtimer Timer pair
//   - Phase 16 per-client Resampler* QHash — a QHash<int, Resampler*> added
//     to this struct at that phase, replacing m_rxAudioResamplers
//
// Fields retained below are the subset that Longpath's Phase 2–13 logic
// actively reads or writes.  All other Thetis fields are either not-needed
// (their functionality disappears with the Qt6 architecture) or deferred to
// the phase that uses them.

struct TciClientSession {
    // ── Lifecycle / identity ─────────────────────────────────────────────────
    // From Thetis TCIServer.cs:739 [v2.10.3.13] (m_client / m_disconnected)

    QWebSocket* socket{nullptr};        // owning socket (not owned by this struct)
    QString     peer;                   // "ip:port" — for ClientChainApplet display
    QString     userAgent{QStringLiteral("(unknown)")};  // from WS upgrade-request header (best-effort)
    QElapsedTimer connectedAt;          // for connection-duration display

    // From Thetis TCIServer.cs:742 [v2.10.3.13] — m_disconnected.
    // Redundant with QWebSocket::state() but kept for parity with per-client
    // guards in Phases 11+.
    bool disconnected{false};

    // ── Stream subscriptions ─────────────────────────────────────────────────
    // From Thetis TCIServer.cs:766 [v2.10.3.13] — m_iqStreamEnabled HashSet<int>
    QSet<int> iqStreamEnabled;

    // From Thetis TCIServer.cs:767 [v2.10.3.13] — m_audioStreamEnabled HashSet<int>
    QSet<int> audioStreamEnabled;

    // ── Eigener Tonvorrat je Sitzung (2026-09-30) ───────────────────────────
    //
    // Bis dahin lag der Ring beim SERVER, einer je Empfaenger, und jeder
    // Client popte daraus. Wer zuerst kam, nahm die Abtastwerte — der zweite
    // bekam, was uebrig war, also Stille mit Loechern. Das trifft jeden
    // Fall mit mehr als einem Zuhoerer: Handfunke am Telefon und am iPad,
    // oder Handfunke neben einem Digimode-Programm, das denselben Empfaenger
    // abonniert hat.
    //
    // Der Erzeugerring beim Server bleibt (der DSP-Faden darf keine
    // Sitzungen anfassen); der Hauptfaden verteilt daraus in diese Puffer,
    // und jeder Client liest danach seinen eigenen — mit seiner eigenen
    // Blockgroesse, seiner eigenen Rate und seinem eigenen Format.
    //
    // 65536 Byte sind 8192 Stereo-Rahmen bei 48 kHz, also gut 170 ms. Der
    // Abfluss laeuft alle 5 ms (960 Byte), das ist reichlich Luft — und
    // trotzdem halb so viel, wie ein Ring je Empfaenger kostete.
    std::array<AudioRingSpsc<65536>, 2> audioVorrat;

    // ── Spektrumstrom (Longpath-eigen, 2026-09-30) ──────────────────────────
    //
    // Fertig gerechnetes Spektrum statt rohem I/Q — siehe die Begründung am
    // TciStreamType::SpectrumStream. Der Client sagt beim Anfordern, wie viele
    // Bildpunkte er hat und wie oft er ein Bild will; der Server verdichtet
    // auf genau diese Punktzahl. Das ist der eigentliche Hebel: nicht die
    // Kompression, sondern gar nicht erst mehr zu schicken, als das Gerät
    // zeichnen kann.
    QSet<int> spectrumEnabled;

    // Bildpunkte je Bild. Vorgabe 256: ein Telefon quer hat rund 400
    // Bildpunkte, hochkant knapp 400 — mehr als 1024 kann kein Handy zeigen,
    // weniger als 64 wäre kein Spektrum mehr.
    int spectrumPoints{256};

    // Gewuenschte Bandbreite des Spektrumbildes in Hertz, 0 = alles.
    //
    // Ohne das zeigt ein Telefon die volle DDC-Breite: 192 kHz auf 373
    // Punkten sind 515 Hz je Bildpunkt. Ein Daumen trifft nie einen Punkt
    // genau, also springt die Frequenz beim Abstimmen in
    // Halbkilohertz-Schritten — der Betreiber am 2026-10-01: "frequenz kann
    // man zwar ändern, aber sehr schlecht".
    //
    // Der Ausschnitt wird am SERVER genommen, nicht im Browser. Nur so
    // steigt die Aufloesung wirklich: 24 kHz auf 373 Punkte sind 64 Hz je
    // Punkt. Schnitte der Browser selbst zu, haette er weiter 515er-Punkte
    // und wuerde sie nur breiter malen.
    int spectrumSpanHz{0};

    // Welche Spanne TATSAECHLICH im Bild steckt — und zuletzt gemeldet wurde.
    //
    // Nicht dasselbe wie der Wunsch darueber. Der Zuschnitt rechnet
    //     breite = max(punkte, lround(n * wunsch/abtastrate))
    // und die Untergrenze `punkte` greift, sobald der Wunsch schmaler ist,
    // als die Punktzahl an Bins hergibt: bei 373 Punkten und einer 2048er
    // FFT wird aus 6 kHz in Wahrheit knapp 8,8 kHz. Kennt der Server die
    // Abtastrate gar nicht, bleibt es bei der VOLLEN Breite.
    //
    // Der Client rechnet aber mit seinem Wunsch weiter: Abstimmstrich,
    // Durchlassband und das Schieben des Wasserfalls haengen alle daran.
    // Weicht die Wahrheit ab, sitzt alles davon falsch — und zwar still.
    // Darum wird jede Aenderung als `spectrum_span:<rx>,<hz>;` gemeldet;
    // 0 heisst "volle Breite, Spanne unbekannt".
    int spectrumSpanGemeldetHz{-1};   // -1 = noch nie gemeldet

    // Bilder je Sekunde. Vorgabe 10 — darunter ruckelt der Wasserfall
    // sichtbar, darüber sieht das Auge am Telefon nichts mehr dazu.
    int spectrumFps{10};

    // Zeitpunkt des letzten gesendeten Spektrums (ms seit Epoche), für die
    // Drossel. Ohne sie ginge jedes FFT-Bild der Engine raus (rund 30/s),
    // also das Dreifache des Verlangten.
    qint64 lastSpectrumMs{0};

    // Phase 16 Task 16.3 (sub-commit b): per-slice WDSP RESAMPLEF instance.
    // Created lazily on audio_start, destroyed on audio_stop + disconnect.
    // Key = rx index (slice).  void* avoids pulling WDSP resample.h into
    // TciClientSession.h; TciServer manages create/destroy via
    // handleAudioSubscribe / handleAudioUnsubscribe / cleanupResamplers.
    //
    // From Thetis TCIServer.cs:789 [v2.10.3.13] — m_rxAudioResamplers
    // Dictionary<int, Resampler> replaced by QHash<int, void*> (opaque ptr
    // to RESAMPLEF struct allocated via create_resampleF / create_resampleFV).
    // Schlüssel ist NICHT der Empfänger allein, sondern (rx, Kanal) —
    // resamplerKey() unten. Grund, gefunden 2026-09-30:
    //
    // WDSPs RESAMPLEF rechnet einkanalig und reell (third_party/wdsp/src/
    // resample.c: ein Ringpuffer, ein Wert je Abtastung, kein Kanalbegriff).
    // Bis dahin lief der VERSCHRÄNKTE Stereopuffer (L,R,L,R,…) als EIN Strom
    // hindurch. Das hatte zwei Folgen, beide bei jeder Rate ausser 48000 (nur
    // dort überspringt der Abfluss den Umtaster ganz):
    //   1. L und R liefen durch denselben FIR und vermischten sich.
    //   2. Die Tonhöhe stimmte nicht — der Resampler sah doppelt so viele
    //      Werte wie es Abtastungen gibt, rechnete also faktisch von 96 kHz
    //      herunter.
    // Belegt an WDSP selbst in tests/tst_tci_audio_resample_channels.cpp.
    //
    // Also je Kanal ein eigener Resampler, und der Abfluss trennt vor dem
    // Umtasten auf und verschränkt danach wieder.
    QHash<int, void*> audioResamplers;

    // (rx, Kanal) -> Schlüssel. Zwei Kanäle sind das Maximum: TCI kennt Mono
    // und Stereo, nichts dazwischen (audio_stream_channels: 1 oder 2).
    static constexpr int resamplerKey(int rx, int channel) { return rx * 2 + channel; }

    // ── Herkunft und Anmeldung (2026-09-30) ─────────────────────────────────
    //
    // TCI kennt weder Anmeldung noch Verschlüsselung — in der 41-seitigen
    // Spezifikation kommen auth, password, token und TLS kein einziges Mal
    // vor. Solange der Server auf 127.0.0.1 lauscht, ist das vertretbar: wer
    // dort verbinden kann, sitzt ohnehin am Rechner. Sobald er ins Netz geht,
    // ist es das nicht mehr — ein Handy kann dann tasten, und jedes andere
    // Gerät im WLAN auch.
    //
    // Deshalb der Schnitt entlang der HERKUNFT, nicht entlang eines globalen
    // Schalters: was von Loopback kommt, läuft unverändert weiter (WSJT-X,
    // JTDX, N1MM+, Log4OM, Hamlib — die kennen kein auth: und sollen es nicht
    // lernen müssen). Was aus dem Netz kommt, muss sich anmelden und darf
    // erst senden, wenn der Betreiber das ausdrücklich erlaubt hat.
    //
    // Ein Server, der auf Loopback gebunden ist, sieht ohnehin nur
    // Loopback-Gegenstellen — dort ist beides also wirkungslos, und genau so
    // soll es sein.
    bool fromLoopback{true};

    // Angemeldet? Auf Loopback von vornherein true. Aus dem Netz erst, wenn
    // ein `auth:<token>` mit dem richtigen Token kam. Bis dahin beantwortet
    // der Server ausschließlich auth: und hält auch den Init-Burst zurück —
    // der verrät sonst Rufzeichen, Gerät und Frequenz an jeden, der den Port
    // findet.
    bool authenticated{true};

    // Zahl der Fehlversuche. Nach kMaxAuthAttempts wird die Verbindung
    // geschlossen; ohne das könnte jemand Token für Token durchprobieren.
    int authAttempts{0};

    // ── Audio stream configuration ───────────────────────────────────────────
    // From Thetis TCIServer.cs:779 [v2.10.3.13] — m_audioSampleRate = 48000
    int audioSampleRate{48000};

    // From Thetis TCIServer.cs:780 [v2.10.3.13] — m_audioSampleType = FLOAT32
    // Encoded as int: 0=int16, 3=float32 (matches TCI binary frame header format field).
    int audioSampleType{3};

    // From Thetis TCIServer.cs:781 [v2.10.3.13] — m_audioStreamChannels = 2
    int audioStreamChannels{2};

    // From Thetis TCIServer.cs:782 [v2.10.3.13] — m_audioStreamSamples = 2048 (range 100..2048)
    int audioStreamSamples{2048};

    // From Thetis TCIServer.cs:783 [v2.10.3.13] — m_audioStreamSamplesExplicitlySet
    bool audioStreamSamplesExplicitlySet{false};

    // ── TX audio negotiation ─────────────────────────────────────────────────
    // From Thetis TCIServer.cs:788 [v2.10.3.13] — m_seenModernTxAudioNegotiation
    bool seenModernTxAudioNegotiation{false};

    // ── Outbound priority send queue (Phase 14) ──────────────────────────────
    // Per-client TciSendQueue replaces direct QWebSocket::sendTextMessage
    // calls in TciServer's broadcast/unicast paths. The per-client drain
    // timer in TciServer pumps frames in priority order (Urgent > Binary >
    // Control), capped at 64 frames per tick, matching the Thetis sender
    // thread + AutoResetEvent at TCIServer.cs:1754-1795 [v2.10.3.13].
    // Coalesced-key map (Thetis m_outboundCoalescedFrames) is Phase 15.
    TciSendQueue sendQueue{1024};   // 1024 frames per priority bucket

    // ── ClientChainApplet display state (Longpath-original) ────────────────
    // No Thetis equivalent — drives the per-client row in the future
    // ClientChainApplet (Phase 13).
    QString lastCommand;
    qint64  lastCommandAt{0};   // QDateTime::currentMSecsSinceEpoch() at last command

    // Backpressure drop counter. Synced from sendQueue.dropCount() by the
    // drain timer so Phase 22 ClientChainApplet can read it without touching
    // the queue directly.
    int     framesDropped{0};

    // Phase 17: inbound TX audio drop counter.
    // Incremented when this client sends a binary TX_AUDIO_STREAM frame but
    // does not hold the TX audio mutex (m_txAudioActiveClient != this).
    // Separate from framesDropped (outbound) to keep semantics clean.
    // Phase 22 ClientChainApplet reads both: "outbound: N" + "TX: M dropped".
    int     txFramesDropped{0};

    // ── Phase 19: per-client sensor subscriptions ────────────────────────────
    // From Thetis TCIServer.cs:684-790 [v2.10.3.13] — per-listener
    // m_sensorManager.RxSensorsEnabled / TxSensorsEnabled flags.
    //
    // Longpath flattens clsTCISensorManager's per-listener state into two
    // bools + interval fields on the session struct. TciServer intercepts
    // rx_sensors_enable:true[,intervalMs]; / tx_sensors_enable:true|false[,intervalMs];
    // commands to toggle these flags before passing to TciProtocol dispatch.
    //
    // From Thetis handleRxSensorsEnable (TCIServer.cs:4449-4459 [v2.10.3.13]).
    bool rxSensorsEnabled{false};

    // From Thetis handleTxSensorsEnable (TCIServer.cs:4460-4469 [v2.10.3.13]).
    bool txSensorsEnabled{false};

    // From Thetis clsTCISensorManager._rxIntervalMs default 200 ms
    // (TCIServer.cs:491 [v2.10.3.13]).
    int rxSensorIntervalMs{200};

    // From Thetis clsTCISensorManager._txIntervalMs default 200 ms
    // (TCIServer.cs:492 [v2.10.3.13]).
    int txSensorIntervalMs{200};
};

} // namespace Longpath

#endif // HAVE_WEBSOCKETS
