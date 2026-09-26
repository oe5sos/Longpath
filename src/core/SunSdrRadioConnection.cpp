// no-port-check: Longpath/Longpath-original. See header for scope.

// =================================================================
// src/core/SunSdrRadioConnection.cpp  (Longpath)
// =================================================================
//
// Longpath/Longpath-original. Scope and rationale in the header.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "SunSdrRadioConnection.h"

#include <QLoggingCategory>
#include <QStringList>

#include <algorithm>
#include <QMutexLocker>
#include <QNetworkAddressEntry>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <cmath>

namespace Longpath {

namespace {
Q_LOGGING_CATEGORY(lcSunSdr, "longpath.sunsdr")

// Step 2 of the SunSDR2 QRP TX-chain plan — a distinct category from
// lcSunSdr above, same declare/define shape, so TX-gate log lines
// (setMox()/setTxArmed()) can be filtered independently of this file's
// existing RX/connection logging.
Q_LOGGING_CATEGORY(lcSunSdrTx, "longpath.sunsdr.tx")
}

// ── Bench-confirmed exact-byte frames, 2026-08-26 ───────────────────
//
// See the header's top-of-file comment for why these are literal
// captured bytes rather than built via SunSdrProtocol::buildControlHeader()
// (a real, observed discrepancy at header bytes 14-17 — not always
// zero the way that builder assumes). Design doc:
// docs/architecture/2026-08-24-sunsdr-native-driver-design.md,
// "BREAKTHROUGH, 2026-08-26" and the two findings immediately above it.

QByteArray SunSdrRadioConnection::discoveryFrameForTest()
{
    // The broadcast query ExpertSDR2 itself sends on a cold launch,
    // before ever addressing the QRP directly — design doc, "the
    // reachability gate is a broadcast discovery packet". Magic 0x03,
    // opcode 0x00 (a value not seen anywhere else in this protocol,
    // and only visible at all once a capture stops filtering to a
    // single host, since it's a broadcast).
    return QByteArray::fromHex(
        "03ff001a000000000000000000000000000000000000fbe6");
}

QByteArray SunSdrRadioConnection::stateSyncFrameForTest()
{
    // Opcode 0x01 — SUNSDR_OP_STATE_SYNC in ArtemisSDR's naming (there
    // a 68-byte frame in the DX boot macro; the QRP replies to a
    // smaller 30-byte version of the same opcode). Sending this exact
    // frame right after the beacon reply is what started a real,
    // sustained I/Q stream in the bench run this evening — design doc,
    // "BREAKTHROUGH, 2026-08-26". The 8-byte payload tail
    // (0c 08 04 03 02 02 02 02) has no attributed meaning yet; this is
    // a verbatim replay of an already-observed value, not a
    // synthesized one.
    return QByteArray::fromHex(
        "03ff01000c0000000000010000007648ea9e010000000c08040302020202");
}

QByteArray SunSdrRadioConnection::replayedFrequencyFrameForTest()
{
    // Opcode 0x08 — SUNSDR_OP_FREQ_COMP in ArtemisSDR's naming,
    // independently confirmed on the QRP via an isolated VFO-tuning
    // capture (design doc, "isolated-action capture attempt #2").
    // This exact payload tunes to whatever frequency ExpertSDR2 was
    // set to during that one capture — the encoding for an arbitrary
    // Hz value was not solved this evening (see the header's
    // top-of-file comment), so this is the one frequency this class
    // can currently request, not a general-purpose tune command.
    return QByteArray::fromHex(
        "03ff0800080000000000010000008ca31dd76ce0780800000000");
}

SunSdrRadioConnection::SunSdrRadioConnection(QObject* parent)
    : RadioConnection(parent)
{
}

SunSdrRadioConnection::~SunSdrRadioConnection() = default;

const SunSdr::Profile& SunSdrRadioConnection::resolveProfile(HPSDRHW board)
{
    // Only one row exists today. A future DX/PRO row would switch on
    // `board` here; the switch is written as an if-chain rather than a
    // real switch so adding a case later doesn't require touching this
    // function's control-flow shape, just adding a branch.
    if (board == HPSDRHW::SunSdr2Qrp) {
        return SunSdr::kProfileQrp;
    }
    // Falls back to the QRP profile rather than asserting: this
    // connection is only ever constructed for ProtocolVersion::SunSdr
    // (RadioConnection::create()), and the only board id that currently
    // maps there is SunSdr2Qrp. An unrecognized board here means a
    // future board id was added without updating this function, not a
    // wire-format ambiguity to guess at — that should be caught in
    // review, not papered over with a silent wrong-profile fallback
    // that only fails later, confusingly, on the wire.
    qCWarning(lcSunSdr) << "SunSdr: resolveProfile() called with an "
                           "unrecognized board id; defaulting to QRP profile";
    return SunSdr::kProfileQrp;
}

void SunSdrRadioConnection::init()
{
    m_controlSocket = new QUdpSocket(this);
    m_streamSocket  = new QUdpSocket(this);

    // Bind the control socket to the profile's own control port (50001),
    // not an ephemeral one — mirrors tools/sunsdr_probe.cpp's
    // runDiscoverMode() (the exact binding that received a real beacon
    // reply live, bench-confirmed 2026-08-26). sunsdr_probe.cpp's own
    // comment hedges that the QRP "replies to the request's sender port,
    // not necessarily 50001" — that hedge is what the ephemeral bind
    // below used to rely on, and a live in-app test the same evening
    // found it does NOT hold: an ephemeral-bound control socket sent the
    // discovery broadcast fine but never received the beacon, while the
    // fixed-port-bound probe did, on the same machine/network/radio
    // moments apart. ShareAddress|ReuseAddressHint lets this coexist
    // with ExpertSDR2 or another instance also holding the port, same
    // rationale as the probe's own comment. Falls back to an ephemeral
    // port only if the fixed-port bind itself fails.
    // setFixedPortBindingEnabledForTest(false) skips straight to the
    // ephemeral bind, no fixed-port attempt at all — see that setter's
    // comment for why: a real, well-known-port bind stays reachable by
    // unsolicited real traffic (confirmed the hard way, 2026-08-27, when
    // a test picked up 85 leftover I/Q packets a still-streaming QRP
    // sent to this exact port after the app itself had already closed).
    const quint16 sunSdrCtrlPort = SunSdr::kProfileQrp.defaultCtrlPort;
    bool controlBoundToFixedPort = false;
    if (m_fixedPortBindingEnabled) {
        controlBoundToFixedPort = m_controlSocket->bind(
            QHostAddress::AnyIPv4, sunSdrCtrlPort,
            QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
        if (!controlBoundToFixedPort) {
            qCWarning(lcSunSdr) << "SunSdr: failed to bind control socket to port"
                                << sunSdrCtrlPort << "- falling back to an ephemeral port";
        }
    }
    if (!controlBoundToFixedPort) {
        if (!m_controlSocket->bind(QHostAddress::AnyIPv4, 0)) {
            qCWarning(lcSunSdr) << "SunSdr: failed to bind control socket";
        }
    }
    // Same fixed-port reasoning as the control socket above, and now
    // doubly confirmed: a live in-app connect attempt the same evening
    // (2026-08-26) got its beacon reply correctly after the control-port
    // fix, sent the state-sync frame, but then timed out 3s later with
    // zero I/Q packets ever received — this ephemeral stream-socket bind
    // was still in place at the time. tools/sunsdr_probe.cpp's own
    // already-proven runListenMode() (the mode that streamed 15,336 real
    // packets bench-side) binds its receiving socket to the fixed stream
    // port (50002) first, ephemeral only as a fallback, with its own
    // comment noting the QRP "presumably keeps sending to exactly 50002"
    // even if the fallback path is taken.
    const quint16 sunSdrStreamPort = SunSdr::kProfileQrp.defaultStreamPort;
    bool streamBoundToFixedPort = false;
    if (m_fixedPortBindingEnabled) {
        streamBoundToFixedPort = m_streamSocket->bind(
            QHostAddress::AnyIPv4, sunSdrStreamPort,
            QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
        if (!streamBoundToFixedPort) {
            qCWarning(lcSunSdr) << "SunSdr: failed to bind stream socket to port"
                                << sunSdrStreamPort << "- falling back to an ephemeral port";
        }
    }
    if (!streamBoundToFixedPort) {
        if (!m_streamSocket->bind(QHostAddress::AnyIPv4, 0)) {
            qCWarning(lcSunSdr) << "SunSdr: failed to bind stream socket";
        }
    }

    connect(m_controlSocket, &QUdpSocket::readyRead,
            this, &SunSdrRadioConnection::onControlReadyRead);
    connect(m_streamSocket, &QUdpSocket::readyRead,
            this, &SunSdrRadioConnection::onStreamReadyRead);

    m_connectWatchdog = new QTimer(this);
    m_connectWatchdog->setSingleShot(true);
    connect(m_connectWatchdog, &QTimer::timeout,
            this, &SunSdrRadioConnection::onConnectTimeout);

    // Not started here — onControlReadyRead() starts it once a real
    // session opens, disconnect()/onConnectTimeout() stop it. See the
    // header's m_keepaliveTimer comment for why this exists at all.
    m_keepaliveTimer = new QTimer(this);
    connect(m_keepaliveTimer, &QTimer::timeout,
            this, &SunSdrRadioConnection::onKeepaliveTimeout);

    // Same start/stop lifecycle as m_keepaliveTimer — armed once the RX
    // gate opens (processControlDatagram()), stopped on disconnect()/
    // onConnectTimeout()/its own trip. See the header's m_dataWatchdog
    // comment for why this exists.
    m_dataWatchdog = new QTimer(this);
    connect(m_dataWatchdog, &QTimer::timeout,
            this, &SunSdrRadioConnection::onDataWatchdogTick);

    // Step 3 (SunSDR2 QRP TX-chain plan): same construction shape as the
    // two timers above (`new ...(this)`, constructed here, not started
    // here). Still zero wire reachability — see SunSdrTxPacer.h's own
    // top-of-file comment. setMox()/the teardown paths below start/stop
    // it; connectToRadio() keeps its profile in sync with m_profile.
    m_txPacer = new SunSdrTxPacer(this);

    qCDebug(lcSunSdr) << "SunSdr: init() control port"
                      << m_controlSocket->localPort() << "stream port"
                      << m_streamSocket->localPort();
}

void SunSdrRadioConnection::connectToRadio(const RadioInfo& info)
{
    if (m_running) {
        disconnect();
    }

    m_radioInfo = info;
    m_profile = &resolveProfile(info.boardType);
    m_rxLevelGain = static_cast<float>(std::pow(10.0, m_profile->rxLevelTrimDb / 20.0));
    m_singleChannelWarned = false;
    m_singleChannelSeen = false;
    m_iqConfirmed = false;
    m_streamStartTimer.invalidate();
    m_qCheckTimer.invalidate();
    m_qNonZeroInWindow = 0;
    m_qSamplesInWindow = 0;
    // Step 3: keep the pacer's own profile pointer (defaulted to
    // kProfileQrp at construction — see SunSdrTxPacer.h's own comment)
    // in sync with whatever this connection actually resolved, so the
    // two can never silently drift if a second profile is added later.
    if (m_txPacer) {
        m_txPacer->setProfile(*m_profile);
    }

    m_running = true;
    m_txSeq = 0;
    m_awaitingBeacon = true;
    m_radioAddr.clear();
    setRxReady(false);

    setState(ConnectionState::Connecting);

    // ── Minimal RX-start sequence (design doc, "BREAKTHROUGH,
    // 2026-08-26") — NOT ArtemisSDR's ~30-step DX boot macro. That
    // macro is still not ported (sunsdr_run_macro(), sunsdr.c:2778-2845)
    // because eight of the QRP's own boot-sequence opcodes still have
    // no attributed meaning at all (design doc, updated through this
    // evening) — sending them would be exactly the guess CLAUDE.md's
    // SOURCE-FIRST protocol exists to prevent.
    //
    // What runs here instead is the two-step sequence a live bench run
    // confirmed is sufficient, independent of that unresolved macro:
    // broadcast discovery, then (once the beacon replies, in
    // onControlReadyRead()) one replayed control frame. Deliberately
    // does NOT also send the frequency frame — that frame is a replay
    // of one specific, already-observed Hz value (see
    // replayedFrequencyFrameForTest()'s comment), and always sending it
    // on every connect would silently retune the radio to that one
    // fixed frequency regardless of what the operator actually wants,
    // which the bench run never needed to do (the state-sync frame
    // alone already started the stream). setReceiverFrequency() stays
    // a no-op until the Hz encoding itself is solved.
    qCInfo(lcSunSdr) << "SunSdr: connectToRadio() — sending discovery "
                        "broadcast (bench-confirmed 2026-08-26)";
    sendDiscoveryBroadcast();

    if (m_connectWatchdog) {
        m_connectWatchdog->start(kConnectTimeoutMs);
    }
}

void SunSdrRadioConnection::sendDiscoveryBroadcast()
{
    if (!m_controlSocket) { return; }
    if (!m_discoveryBroadcastEnabled) {
        qCDebug(lcSunSdr) << "SunSdr: discovery broadcast suppressed "
                             "(setDiscoveryBroadcastEnabledForTest(false))";
        return;
    }

    const QByteArray query = discoveryFrameForTest();
    const quint16 ctrlPort = m_profile ? m_profile->defaultCtrlPort
                                        : SunSdr::kProfileQrp.defaultCtrlPort;

    // One send per up interface's own broadcast address — mirrors what
    // a live capture showed ExpertSDR2 itself doing (loopback, WLAN,
    // wired, each with its own broadcast address), not a single guessed
    // 255.255.255.255. See the class header's top-of-file comment.
    // ── Zuerst geradeaus an das Geraet, das der Betreiber eingetragen
    //    hat (2026-09-23) ───────────────────────────────────────────────
    //
    // Die Rundsendung bleibt — sie ist der Weg, der am Geraet bewiesen
    // wurde. Aber sie ist auch der einzige, und das ist eine Schwaeche:
    // ein WLAN mit Client-Isolation, ein Router mit gefilterter
    // Rundsendung, ein anderes VLAN, ein Gast-Netz — in all diesen
    // Faellen kommt die Anfrage nie an, obwohl die Adresse des Geraets
    // im Eintrag steht und ein gewoehnliches Paket dorthin ankaeme. Ein
    // zusaetzliches Paket an genau diese Adresse kostet 24 Byte und
    // macht den Fall auf.
    //
    // Es ist dieselbe Anfrage (Opcode 0x00, eine reine Frage), also
    // kann sie auch nichts verstellen; das Geraet antwortet auf den
    // Absenderport, gleich ob es die Rundsendung oder dieses Paket
    // beantwortet. Eine Doppelantwort ist unschaedlich:
    // processControlDatagram() verwirft den zweiten Beacon, sobald der
    // Handschlag laeuft (eigener Prueffall in
    // tst_sunsdr_radio_connection).
    //
    // Nebenwirkung, die die Werkbank erst moeglich macht: ueber die
    // Rueckschleife gibt es keine Rundsendeadresse, also erreichte die
    // Anfrage ein Messgeraet auf 127.0.0.1 nie.
    if (!m_radioInfo.address.isNull()) {
        m_controlSocket->writeDatagram(query, m_radioInfo.address, ctrlPort);
        recordBytesSent(static_cast<qint64>(query.size()));
    }

    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) { continue; }
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            const QHostAddress bcast = entry.broadcast();
            if (bcast.isNull()) { continue; }
            m_controlSocket->writeDatagram(query, bcast, ctrlPort);
            recordBytesSent(static_cast<qint64>(query.size()));
        }
    }
}

void SunSdrRadioConnection::onConnectTimeout()
{
    if (state() != ConnectionState::Connecting) {
        return;  // already promoted to Connected, or already disconnected
    }
    // The message depends on how far the handshake actually got — found
    // live, 2026-08-26: a first fix (binding the control socket to its
    // fixed port instead of an ephemeral one) got a real beacon reply
    // every time, but the connection still timed out here 3s later,
    // because the stream socket had the identical ephemeral-bind bug —
    // "no beacon reply" was flatly wrong in that case; the beacon came
    // back fine, nothing after it did. m_awaitingBeacon is still true
    // here only when no beacon was ever seen at all.
    const bool gotBeacon = !m_awaitingBeacon;

    // Full teardown here, not just a state flip — mirrors
    // P1RadioConnection::onConnectTimeout()'s own "Issue #239" precedent
    // (P1RadioConnection.cpp, tear down to Disconnected so the UI does
    // not claim success while the radio session is actually unreachable
    // or incomplete) — adapted for a real, live-observed gap this class
    // had and P1's timeout path never could: a beacon CAN legitimately
    // arrive and open the RX gate (setRxReady(true) in
    // onControlReadyRead()) before this watchdog fires, if the I/Q
    // stream itself is what's slow to start — found live, 2026-08-26.
    // Without this, that already-open gate stays open: any I/Q packet
    // landing even a moment after this "failed" state is shown would
    // still be decoded and emitted straight into the DSP/audio/spectrum
    // pipeline (see processStreamDatagram()), while the UI insists the
    // connection failed. Closing both sockets makes that impossible —
    // nothing more can arrive on them at all, matching P1's own socket
    // close on this same path. connectToRadio()'s next attempt always
    // goes through a brand-new instance in production (RadioModel
    // creates one per connect via RadioConnection::create()), so there
    // is no "reopen after close" case this needs to also handle.
    m_running = false;
    m_awaitingBeacon = false;
    m_radioAddr.clear();
    setRxReady(false);
    // Step 2 TX gate: same reset as disconnect() (arming must never
    // survive a teardown, of any kind) — but the trace ring itself is
    // deliberately left alone here; see disconnect()'s own comment for
    // why a failure teardown keeps it while a deliberate disconnect()
    // clears it. TxTraceKind::Disarmed's own doc comment already covers
    // "a teardown path's reset" — record it before the silent stores
    // below, so an operator reading the trace tail after a connect
    // timeout sees WHY TX/arm went off, not just that it did.
    if (m_mox.load(std::memory_order_acquire)) {
        pushTxTrace(TxTraceKind::MoxAccepted,
                    QStringLiteral("accepted: mox -> false (forced by connect timeout)"));
    }
    if (m_txArmed.load(std::memory_order_acquire)) {
        pushTxTrace(TxTraceKind::Disarmed,
                    QStringLiteral("forced by connect timeout"));
    }
    m_txArmed.store(false, std::memory_order_release);
    m_mox.store(false, std::memory_order_release);

    // Der Ring der zuletzt angenommenen Folgenummern gehoert zur Sitzung:
    // eine neue faengt bei null an, sonst koennte eine Nummer aus der
    // alten Sitzung einen echten Block der neuen verwerfen.
    // Step 3: a pacer left running after this teardown fires would be a
    // "phantom pacer" ticking against a connection that just declared
    // itself timed out — same discipline as the socket closes right
    // below. Unconditional, same as this function's other resets above.
    if (m_txPacer) { m_txPacer->stop(); }
    if (m_keepaliveTimer) { m_keepaliveTimer->stop(); }
    if (m_dataWatchdog) { m_dataWatchdog->stop(); }
    m_lastStreamPacketAt.invalidate();
    if (m_controlSocket) { m_controlSocket->close(); }
    if (m_streamSocket) { m_streamSocket->close(); }

    setState(ConnectionState::Disconnected);
    emit connectFailed(ConnectFailure::Timeout,
                       gotBeacon
                           ? QStringLiteral("SunSDR: beacon replied but no "
                                            "I/Q stream followed")
                           : QStringLiteral("SunSDR: no beacon reply — radio "
                                            "unreachable or discovery blocked"));
}

void SunSdrRadioConnection::disconnect()
{
    m_running = false;
    m_awaitingBeacon = false;  // a late beacon reply after this must not
                               // reopen the RX gate — see onControlReadyRead()
    m_radioAddr.clear();
    setRxReady(false);

    // Der Ring der zuletzt angenommenen Folgenummern gehoert zur Sitzung:
    // eine neue faengt bei null an, sonst koennte eine Nummer aus der
    // alten Sitzung einen echten Block der neuen verwerfen. Dieselbe
    // Ueberlegung wie bei m_radioAddr eine Zeile darueber.

    // Step 2 TX gate: bench arming is per-session, deliberately not
    // sticky (setTxArmedForTest()'s own comment) — a disconnect() ends
    // the session, so both reset here, same discipline as m_radioAddr
    // above and the same reset this class's other two teardown paths
    // (onConnectTimeout(), onDataWatchdogTick()) also apply.
    m_txArmed.store(false, std::memory_order_release);
    m_mox.store(false, std::memory_order_release);

    // The TX trace ring is cleared HERE and only here — not in
    // onConnectTimeout() or onDataWatchdogTick(). Those two are failure
    // teardowns (a beacon never came, or the link went dead mid-session)
    // where the ring's most recent arm/gate-check/accept history is
    // exactly the diagnostic breadcrumb trail worth keeping past the
    // teardown; disconnect() is the deliberate, operator-initiated end
    // of a bench session, where a clean slate for the next session makes
    // more sense than carrying the prior one's trace forward. Only the
    // count/write-cursor reset — m_txTraceSeq is NOT touched, see its
    // own field comment in the header for why. Locked like every other
    // access to these fields (m_txTraceMutex's own comment) — this reset
    // runs on the connection thread, same as pushTxTrace(), but a GUI-
    // thread txTraceForTest() read must never observe it half-applied.
    {
        const QMutexLocker locker(&m_txTraceMutex);
        m_txTraceCount = 0;
        m_txTraceNext = 0;
    }

    if (m_connectWatchdog) {
        m_connectWatchdog->stop();
    }
    if (m_keepaliveTimer) {
        m_keepaliveTimer->stop();
    }
    if (m_dataWatchdog) {
        m_dataWatchdog->stop();
    }
    // Step 3: same "must never survive a teardown" discipline as
    // m_txArmed/m_mox above — a deliberate disconnect() ending the
    // bench session must also silence the pacer, not just disarm MOX.
    if (m_txPacer) {
        m_txPacer->stop();
    }
    m_lastStreamPacketAt.invalidate();

    setState(ConnectionState::Disconnected);
}

void SunSdrRadioConnection::setRxReady(bool ready)
{
    m_rxReady.store(ready, std::memory_order_release);
}

void SunSdrRadioConnection::setReceiverFrequency(int receiverIndex, quint64 frequencyHz)
{
    // receiverIndex is accepted but unused: the QRP profile's
    // RX-channel-count story is one of the items the boot-macro
    // research left unattributed, and this class only ever streams one
    // receiver.
    Q_UNUSED(receiverIndex);

    if (!m_controlSocket || !m_profile || m_radioAddr.isNull()) {
        // No open session to send this to yet — nothing meaningful to
        // retune until connectToRadio()'s handshake has actually
        // completed (m_radioAddr is only set once a real beacon replied
        // — see onControlReadyRead()).
        return;
    }

    // Bench-confirmed 2026-08-27: design doc "candidate frequency-
    // encoding formula found" was upgraded from hypothesis to confirmed
    // the same day, against a live, exact, known-frequency test —
    // ExpertSDR2 displayed 7,099,904 Hz, the candidate formula decoded
    // 7,099,204 Hz from the real captured frame, 700 Hz apart out of
    // 7.1 MHz (0.01%), consistent with VFO scroll-settling lag between
    // the last captured packet and the display's final resting value,
    // not a formula error. Payload:
    // SunSdr::encodeFrequencyPayload() (freqHz * 10, 8-byte
    // LE, from ArtemisSDR's real sunsdr_send_freq_pkt(),
    // sunsdr.c:2259-2277 [@f8b01d25c5]).
    //
    // Header caveat, still genuinely open: bytes 14-17 of the 18-byte
    // control header carry a varying, not-fully-understood value in
    // every real captured frame (SunSdrProtocol.h's own discrepancy
    // note — not zero padding, contrary to buildControlHeader()'s
    // assumption). This reuses the exact 18-byte header prefix from the
    // one frequency-set frame this project has bench-confirmed the
    // radio accepted, rather than guessing a new value for those bytes.
    // If retuning proves unreliable across repeated real-world use,
    // this header tail — not the now-confirmed payload formula — is the
    // next thing to investigate.
    // ── Erst die DDC (0x07), dann die VFO (0x08) ── 2026-09-25 ──────
    //
    // Nach dem Einschalten liefert die QRP nur EINEN reellen Kanal (Q = 0).
    // Am Geraet eingegrenzt (Martin schaltete zwischen jedem Versuch aus
    // und ein, Gruppen A/B/C, dann 0x07 allein): ein einziger 0x07-Rahmen
    // fuer Unterempfaenger 0 schaltet echtes I/Q ein -- Q ungleich 0 von
    // 0 % auf 21 %. ExpertSDR2 schickt ihn beim Verbinden mit seiner
    // VFO-Frequenz, 0x08 dagegen mit 0 Hz. Das deckt sich mit ArtemisSDRs
    // DX-Schema (Nummern dort um eins hoeher): 0x08 DX = "freq, DDC
    // companion, sub 0=RX1/1=RX2", 0x09 DX = "freq, primary/TX VFO"
    // (sunsdr.c:2381,2386,2656 [@f8b01d25c5]). Longpath stellte bisher nur
    // die VFO ein, nie die DDC.
    //
    // Nur fuer die QRP -- nur dort gemessen.
    if (m_profile->variant == SunSdr::Variant::Qrp) {
        const QByteArray ddc = ddcFrequencyFrame(0, frequencyHz);
        m_controlSocket->writeDatagram(ddc, m_radioAddr, m_profile->defaultCtrlPort);
        recordBytesSent(static_cast<qint64>(ddc.size()));
    }

    // Pruefsumme jetzt gerechnet (SunSdr::withControlFrameCrc) -- vorher
    // trug JEDE Frequenz das feste Ende 8ca31dd7 einer einzigen.
    QByteArray frame = QByteArray::fromHex(
        "03ff0800080000000000010000008ca31dd7");
    frame += SunSdr::encodeFrequencyPayload(frequencyHz);
    frame = SunSdr::withControlFrameCrc(frame);

    m_controlSocket->writeDatagram(frame, m_radioAddr, m_profile->defaultCtrlPort);
    recordBytesSent(static_cast<qint64>(frame.size()));
    qCInfo(lcSunSdr) << "SunSdr: setReceiverFrequency() ->" << frequencyHz << "Hz (DDC 0x07 + VFO 0x08)";
}

QByteArray SunSdrRadioConnection::ddcFrequencyFrame(int subReceiver, quint64 frequencyHz)
{
    // Kopf byte-genau aus ExpertSDR2s Start am 2026-09-25
    // (/tmp/qrp-att.pcap): [2] Opcode 0x07, [4] Laenge 8, [6] Unter-
    // empfaenger, [10] 0x01, [14..17] das Rahmenende, je Unterempfaenger
    // so, wie ExpertSDR2 es schickte. Nutzlast wie bei 0x08: u64 LE in
    // Zehntel-Hertz (encodeFrequencyPayload). Das Rahmenende wird unten
    // neu gerechnet (CRC-32, SunSdr::withControlFrameCrc).
    const char* head = subReceiver == 1
        ? "03ff0700080001000000010000001850a11e"
        : "03ff070008000000000001000000dabdabb7";
    QByteArray frame = QByteArray::fromHex(head);
    frame += SunSdr::encodeFrequencyPayload(frequencyHz);
    // Das Ende (Bytes 14..17) ist CRC-32 ueber den Rahmen -- fuer jede
    // Frequenz neu gerechnet. Mit dem alten Ende verwarf die QRP einen
    // Rahmen mit anderer Frequenz (Versuch "07x", 2026-09-25).
    return SunSdr::withControlFrameCrc(frame);
}

QByteArray SunSdrRadioConnection::attenuatorFrameFor(int dB)
{
    // Opcode 0x04, Nutzlast = Stufenindex 00/01/02 = -20/-10/0 dB.
    // Gemessen 2026-09-25, siehe den Kommentar in setAttenuator().
    if (dB == 0) {
        return QByteArray::fromHex("03ff04000400000000000100000053ccd3b302000000");
    }
    if (dB == -10) {
        return QByteArray::fromHex("03ff040004000000000001000000bd6366a101000000");
    }
    if (dB == -20) {
        return QByteArray::fromHex("03ff040004000000000001000000d804da1900000000");
    }
    return {};
}

QByteArray SunSdrRadioConnection::preampFrameFor(int preampModeIdx)
{
    // Opcode 0x04, Nutzlast = Stufenindex. Mitgeschnitten 2026-09-25, als
    // Martin in ExpertSDR2 alle vier Stufen des Preamp-Knopfs der Reihe
    // nach durchgeschaltet hat (/tmp/qrp-att.pcap): 0 dB -> 02, +10 dB ->
    // 03, -20 dB -> 00, -10 dB -> 01. Dasselbe Schema wie ArtemisSDRs DX
    // (Index 0..3 = -20/-10/0/+10 dB, sunsdr.h:33-47 [@f8b01d25c5]), dort
    // unter Opcode 0x05 und mit dem 0x80-Bit.
    //
    // PreampMode-Indizes (StepAttenuatorController.h): 7 Plus10, 1 On
    // (0 dB), 2 Minus10, 3 Minus20; 0 Off heisst bei Thetis "-20 dB"
    // (HPSDR_OFF) und bekommt darum denselben Rahmen wie Minus20. Die
    // Anzeige-Korrektur (rxPreampOffsetDbFor) passt zu genau dieser
    // Zuordnung: On = 0 dB Bezug, +10 dB -> -10, -10 dB -> +10, -20 -> +20.
    char step = 0;
    switch (preampModeIdx) {
    case 7: step = 0x03; break;  // +10 dB
    case 1: step = 0x02; break;  //   0 dB, ExpertSDR2s Startzustand
    case 2: step = 0x01; break;  // -10 dB
    case 3:                      // -20 dB
    case 0: step = 0x00; break;  // Off = HPSDR_OFF = -20 dB
    default: return {};          // -30..-50 dB gibt es an der QRP nicht
    }
    QByteArray frame = QByteArray::fromHex("03ff04000400000000000100000000000000");
    frame.append(step);
    frame.append(3, '\0');
    // Das Rahmenende (Bytes 14..17) ist CRC-32 ueber den Rahmen; fuer die
    // vier Stufen ergibt das byte-genau die mitgeschnittenen Rahmen
    // (Test tst_sunsdr_radio_connection::preampFramesMatchExpertSdr2Bytes).
    return SunSdr::withControlFrameCrc(frame);
}

void SunSdrRadioConnection::setPreampModeIndex(int preampModeIdx)
{
    if (!m_controlSocket || !m_profile || m_radioAddr.isNull()) {
        return;
    }
    // Nur die QRP ist an diesem Rahmen gemessen; eine DX/PRO hat ihren
    // Preamp unter Opcode 0x05 (ArtemisSDR) und bekommt hier nichts.
    if (m_profile->variant != SunSdr::Variant::Qrp) {
        return;
    }
    const QByteArray frame = preampFrameFor(preampModeIdx);
    if (frame.isEmpty()) {
        qCInfo(lcSunSdr) << "SunSdr: setPreampModeIndex(" << preampModeIdx
                         << ") -- no such preamp step on the QRP, not sending";
        return;
    }
    m_controlSocket->writeDatagram(frame, m_radioAddr, m_profile->defaultCtrlPort);
    recordBytesSent(static_cast<qint64>(frame.size()));
    qCInfo(lcSunSdr).noquote() << "SunSdr: preamp step ->"
                               << frame.mid(18, 1).toHex() << "(mode index"
                               << preampModeIdx << ")";
}

void SunSdrRadioConnection::setAttenuator(int dB)
{
    // Bench-confirmed 2026-08-27, re-derived from the real capture
    // files still on disk (/tmp/sunsdr-action-preamp.pcap and
    // -preamp2.pcap, both from 2026-08-26), NOT from this evening's own
    // informal prose summary of them — that summary quoted an
    // apparently-correct 4-byte payload trailer by eye, but the exact
    // byte offset that quote came from was never independently
    // re-verified against the formal 18-byte header boundary the way
    // the frequency frame's quote turned out to be wrong by 4 bytes
    // earlier the same day. Re-parsing both pcaps directly (UDP payload
    // = full packet minus 20-byte IPv4 header minus 8-byte UDP header)
    // confirmed the quote WAS correctly aligned this time, but "was
    // right by luck last time" isn't a standard to build on, so this
    // reused the raw files rather than trusting the prose a second time.
    //
    // Opcode 0x04. Exactly two real states observed, tied to a specific
    // UI action (ExpertSDR2's own "-20dB" attenuator dropdown next to
    // RX2, design doc "isolated-action capture attempts #4-#6... #7
    // preamp/atten — clean hit"), both confirmed live via 6 identical
    // repeats in the first capture (0dB) and 2 identical repeats in the
    // second (-20dB): payload 00000000 = 0dB (off), payload 01000000 =
    // -20dB. No other attenuator value has ever been captured — this
    // deliberately does NOT interpolate or round an arbitrary requested
    // dB to the nearest known state, since that would silently apply a
    // different attenuation than what was asked for. Only these two
    // exact values are actionable; anything else is a no-op, logged so
    // the gap is visible rather than silently swallowed.
    //
    // Each state's header tail (bytes 14-17) differs from the other AND
    // from the frequency frame's own tail — direct confirmation this
    // field genuinely varies per capture session, not a fixed per-opcode
    // constant. Both frames below are still exact, real, previously-
    // radio-accepted bytes, same discipline as every other frame this
    // class sends, not a guess at what that tail should be for a new
    // session.
    if (!m_controlSocket || !m_profile || m_radioAddr.isNull()) {
        return;
    }

    // ── BERICHTIGT 2026-09-25 ─────────────────────────────────────
    //
    // Die Zuordnung oben (26.08.: "00000000 = 0 dB, 01000000 = -20 dB")
    // war FALSCH. Am 2026-09-25 in ExpertSDR2 alle vier Stufen des
    // Preamp-Knopfs der Reihe nach durchgeschaltet, mitgeschnitten
    // (/tmp/qrp-att.pcap, Zeitleiste /tmp/qrp-att-zeiten.txt):
    //   0 dB (Start) -> 02, +10 dB -> 03, -20 dB -> 00, -10 dB -> 01,
    //   0 dB -> 02
    // -- genau ArtemisSDRs DX-Schema (Index 0..3 = -20/-10/0/+10 dB,
    // sunsdr.h:33-47 [@f8b01d25c5]), nur ohne das 0x80-Bit und unter
    // Opcode 0x04 statt 0x05. Mit der alten Zuordnung haette
    // setAttenuator(0) die QRP auf -20 dB gestellt. Aufgerufen wurde es
    // nie (die QRP-Zeile hat keinen Stufenabschwaecher), gesendet also
    // nichts Falsches.
    //
    // Die Rahmen selbst stimmen byte-genau mit dem Mitschnitt vom
    // 2026-09-25 ueberein (die fuer 00 und 01 waren schon vorher
    // richtig, nur falsch benannt); 02 ist ExpertSDR2s eigener
    // Startrahmen. +10 dB (03, Tail 36ab6f0b) ist kein Abschwaecher und
    // gehoert zu setPreamp, nicht hierher.
    const QByteArray frame = attenuatorFrameFor(dB);
    if (frame.isEmpty()) {
        qCInfo(lcSunSdr) << "SunSdr: setAttenuator(" << dB
                         << ") — only 0, -10 and -20 dB exist on the QRP, "
                            "not sending anything for this value";
        return;
    }

    m_controlSocket->writeDatagram(frame, m_radioAddr, m_profile->defaultCtrlPort);
    recordBytesSent(static_cast<qint64>(frame.size()));
    qCInfo(lcSunSdr) << "SunSdr: setAttenuator() ->" << dB << "dB";
}

// ── Step 2 (SunSDR2 QRP TX-chain plan): bench-only TX gate scaffolding ──
//
// Design synthesis quote that authorizes exactly this scope, verbatim:
// "Gate scaffolding — still zero wire reachability. lcSunSdrTx category;
// m_txArmed/m_mox atomics; m_txCheckContext + setter; 50-entry trace
// ring; real setMox() body that calls BandPlanGuard::checkMoxAllowed()
// and — since no pacer/antenna/PA code exists yet — can only ever
// log-and-refuse or log-and-accept-with-no-wire-effect." Step 1's pure
// encoders (SunSdrProtocol::buildMoxFrame() and friends) are NOT called
// from here — that wiring, plus the socket send itself, is a later,
// separately-reviewed step. No QUdpSocket, no QTimer, nothing that
// sends a single byte lives in this function.

void SunSdrRadioConnection::setTxArmed(bool armed)
{
    m_txArmed.store(armed, std::memory_order_release);
    qCInfo(lcSunSdrTx) << "SunSdr: TX" << (armed ? "armed" : "disarmed")
                       << "(bench-only — setTxArmedForTest)";
    pushTxTrace(armed ? TxTraceKind::Armed : TxTraceKind::Disarmed, QString());

    // Disarming mid-transmission must actually stop the transmission, not
    // just block future setMox(true) calls — the same "a kill switch that
    // could be refused isn't one" reasoning that made setMox(false)
    // unconditional (2026-09-02) applies here: revoking the arm gate is
    // itself a kill request whenever TX is currently on. Found during
    // Step 3's review (a running pacer survived setTxArmedForTest(false)
    // alone, since only the four teardown paths and setMox(false) itself
    // stopped it) — routed through setMox(false) rather than duplicating
    // its release logic, so there is exactly one place that turns TX off.
    if (!armed && m_mox.load(std::memory_order_acquire)) {
        setMox(false);
    }
}

void SunSdrRadioConnection::pushTxTrace(TxTraceKind kind, const QString& reason)
{
    const QMutexLocker locker(&m_txTraceMutex);
    m_txTrace[static_cast<std::size_t>(m_txTraceNext)] =
        TxTraceEntry{m_txTraceSeq++, kind, reason};
    m_txTraceNext = (m_txTraceNext + 1) % kTxTraceRingSize;
    if (m_txTraceCount < kTxTraceRingSize) {
        ++m_txTraceCount;
    }
}

QVector<TxTraceEntry> SunSdrRadioConnection::txTraceForTest() const
{
    const QMutexLocker locker(&m_txTraceMutex);
    QVector<TxTraceEntry> out;
    out.reserve(m_txTraceCount);
    // Oldest-first. Before the ring has ever wrapped, that's simply
    // index 0..count-1; once it has wrapped, the oldest surviving entry
    // sits at m_txTraceNext (the slot the next push will overwrite).
    const int start = (m_txTraceCount < kTxTraceRingSize) ? 0 : m_txTraceNext;
    for (int i = 0; i < m_txTraceCount; ++i) {
        const int idx = (start + i) % kTxTraceRingSize;
        out.append(m_txTrace[static_cast<std::size_t>(idx)]);
    }
    return out;
}

// Step 4 (SunSDR2 QRP TX-chain plan) — see this method's own header
// comment. Built on top of txTraceForTest() rather than walking m_txTrace
// a second way: that method already produces the oldest-first snapshot
// this one just trims to its last `maxEntries`.
QVector<TxTraceEntry> SunSdrRadioConnection::txTraceTail(int maxEntries) const
{
    const QVector<TxTraceEntry> all = txTraceForTest();
    if (maxEntries <= 0 || all.size() <= maxEntries) {
        return all;
    }
    return all.mid(all.size() - maxEntries);
}

// setMox() — see the file-section comment above for the exact scope
// this implements. m_txArmed gates everything: it is a bench-only arm
// switch that is never set by any AppSettings-persisted value, GUI
// control, or default (setTxArmedForTest()'s own header comment), so in
// every real, non-test build today this always refuses. m_txCheckContext
// is explicitly test/bench-settable state for now — this class has no
// SliceModel/RadioModel of its own to source region/mode/band/frequency
// from the way RadioModel::installBandPlanMoxCheck()'s real lambda does
// (RadioModel.cpp:9318-9374, the shape m_txCheckContext's fields
// mirror). Full integration with MoxController's single-authority MOX
// flow (MoxController.h's own K.2 setMoxCheck() callback mechanism) is
// future work, out of scope here.
void SunSdrRadioConnection::setMox(bool enabled)
{
    // Disabling MOX is an unconditional release, never gated — same
    // precedent as MoxController::setMox()'s own K.2 BandPlanGuard check
    // and its Task 87 TxInterlockPolicy check, both written as
    // `if (on && ...)`: the guard exists only on the path that turns TX
    // ON, never on the path that turns it off. A kill switch that could
    // itself be refused is not a kill switch. Found and fixed 2026-09-02,
    // before this class ever had a caller that could hit it, precisely
    // because the "armed" gate above this comment used to run for both
    // directions.
    if (!enabled) {
        qCInfo(lcSunSdrTx) << "SunSdr: setMox(false) — unconditional release, "
                              "no gate applies to turning TX off";
        pushTxTrace(TxTraceKind::MoxAccepted, QStringLiteral("accepted: mox -> false (unconditional release)"));
        m_mox.store(false, std::memory_order_release);
        // Step 3: the pacer's stop() gets the exact same "no gate,
        // always works" guarantee as the m_mox release right above —
        // matching this whole branch's own unconditional-release
        // discipline, not a second, separately-gated shutdown path.
        if (m_txPacer) {
            m_txPacer->stop();
        }
        return;
    }

    if (!m_txArmed.load(std::memory_order_acquire)) {
        qCWarning(lcSunSdrTx) << "SunSdr: setMox(true) refused: not armed "
                                 "(setTxArmedForTest(true) was never "
                                 "called, or has since been disarmed)";
        pushTxTrace(TxTraceKind::MoxRefused, QStringLiteral("refused: not armed"));
        return;  // m_mox unchanged
    }

    const safety::BandPlanGuard::MoxCheckResult verdict = m_bandPlan.checkMoxAllowed(
        m_txCheckContext.region, m_txCheckContext.txFreqHz, m_txCheckContext.mode,
        m_txCheckContext.rxBand, m_txCheckContext.txBand,
        m_txCheckContext.preventDifferentBand, m_txCheckContext.extended);

    if (!verdict.ok) {
        qCWarning(lcSunSdrTx) << "SunSdr: setMox(true) refused by BandPlanGuard:"
                              << verdict.reason;
        pushTxTrace(TxTraceKind::MoxRefused, verdict.reason);
        return;  // m_mox unchanged
    }

    qCInfo(lcSunSdrTx) << "SunSdr: setMox(true) accepted — Step 3's pacer starts "
                          "ticking, but still builds no socket send (still zero "
                          "wire reachability; no antenna/PA wiring exists yet)";
    pushTxTrace(TxTraceKind::MoxAccepted, QStringLiteral("accepted: mox -> true"));
    m_mox.store(true, std::memory_order_release);
    // Step 3: "on every PTT-on" (design doc) — this accepted-true path
    // IS the PTT-on event this class recognizes, so it's what actually
    // calls resetSeq(), not SunSdrTxPacer itself (see that class's own
    // resetSeq() comment).
    if (m_txPacer) {
        m_txPacer->resetSeq();
        m_txPacer->start();
    }
}

void SunSdrRadioConnection::setActiveReceiverCount(int count)
{
    Q_UNUSED(count);
}

void SunSdrRadioConnection::setSampleRate(int sampleRate)
{
    // The QRP's native rate is fixed at 312,500 Hz (design doc "IQ
    // stream" section) — not negotiated. A caller requesting a
    // different rate isn't wrong to ask (Longpath's resampling
    // infrastructure could in principle adapt), but this connection
    // has nothing to send to change it on the wire, so the request is
    // simply not actionable here.
    Q_UNUSED(sampleRate);
}

void SunSdrRadioConnection::onControlReadyRead()
{
    if (!m_controlSocket) { return; }

    while (m_controlSocket->hasPendingDatagrams()) {
        const QNetworkDatagram dgram = m_controlSocket->receiveDatagram();
        recordBytesReceived(static_cast<qint64>(dgram.data().size()));
        processControlDatagram(dgram.data(), dgram.senderAddress());
    }
}

void SunSdrRadioConnection::feedControlDatagramForTest(
    const QByteArray& datagram, const QHostAddress& sender)
{
    processControlDatagram(datagram, sender);
}

void SunSdrRadioConnection::processControlDatagram(const QByteArray& data,
                                                     const QHostAddress& sender)
{
    if (!m_awaitingBeacon || !m_profile) {
        return;  // handshake already done, or no session pending — drain only
    }

    // Beacon-reply shape (header comment; design doc "the
    // reachability gate is a broadcast discovery packet"): magic0/
    // magic1 match the profile, opcode (byte[2]) is 0x01. This is
    // NOT the general 18-byte ControlHeader layout — the one
    // captured beacon has 0x1a at byte[3], where parseControlHeader()
    // requires 0x00 — so detection here is a direct byte check, the
    // same treatment as the exact-byte frame constants above rather
    // than a run through that parser.
    if (data.size() < 3
        || quint8(data[0]) != m_profile->magic0
        || quint8(data[1]) != SunSdr::kMagic1
        || quint8(data[2]) != 0x01) {
        return;
    }

    qCInfo(lcSunSdr) << "SunSdr: beacon reply from" << sender
                      << "- replaying state-sync frame "
                         "(bench-confirmed 2026-08-26)";
    m_radioAddr = sender;
    m_awaitingBeacon = false;

    if (m_controlSocket) {
        // ExpertSDR2 schickt den Zustandsrahmen nicht als erstes, sondern
        // als vorletztes -- davor liegen rund zwanzig andere. Ob die
        // Reihenfolge zaehlt, laesst sich nur ausprobieren, also gibt es
        // beide Seiten: PRE davor, EXTRA danach.
        sendBenchFrames(QStringLiteral("LONGPATH_SUNSDR_PRE"));

        const QByteArray stateSync = stateSyncFrameForTest();
        m_controlSocket->writeDatagram(stateSync, m_radioAddr,
                                        m_profile->defaultCtrlPort);
        recordBytesSent(static_cast<qint64>(stateSync.size()));

        sendBenchFrames(QStringLiteral("LONGPATH_SUNSDR_EXTRA"));
    }

    // No downstream DSP-readiness signal exists yet to gate this on
    // (plan doc §Phase C.3/D — grep-confirmed no other RadioConnection
    // subclass wires an equivalent gate externally either, so there's
    // nothing to wait for). Opening it here, right after the one
    // frame the bench run showed is sufficient to start the stream,
    // is a pragmatic call: the alternative is that this class's gate
    // never opens at all. processStreamDatagram() still does its own
    // promotion of ConnectionState to Connected on the first
    // successfully decoded packet, not here.
    setRxReady(true);

    // Start the periodic keepalive now, not on the first decoded I/Q
    // packet — the radio's own ~8s stream-drop clock (SunSdrProtocol.h
    // citation) starts counting from whenever it considers the
    // session live, which is at latest right after this state-sync
    // reply, not after Longpath happens to have decoded something.
    if (m_keepaliveTimer && !blockReplyEnabled()) {
        m_keepaliveTimer->start(kKeepaliveIntervalMs);
    }

    // Baseline the silence clock here too, same reasoning as the
    // keepalive above — arm it from when the session is considered
    // live, not from whenever the first I/Q packet happens to land, so
    // a genuinely slow stream start doesn't eat into the silence
    // budget it hasn't earned yet.
    m_lastStreamPacketAt.restart();
    if (m_dataWatchdog) {
        m_dataWatchdog->start(kDataWatchdogTickMs);
    }
}

void SunSdrRadioConnection::onStreamReadyRead()
{
    if (!m_streamSocket) { return; }

    while (m_streamSocket->hasPendingDatagrams()) {
        const QNetworkDatagram dgram = m_streamSocket->receiveDatagram();
        processStreamDatagram(dgram.data(), dgram.senderAddress());
    }
}

void SunSdrRadioConnection::feedStreamDatagramForTest(const QByteArray& datagram)
{
    // Simulates a packet from the connected radio itself — the sender
    // check in processStreamDatagram() passes trivially. Existing tests
    // built on this hook are exercising "the radio sent us this," which
    // is what they always meant; see
    // feedStreamDatagramFromSenderForTest() for the foreign-sender case.
    processStreamDatagram(datagram, m_radioAddr);
}

void SunSdrRadioConnection::feedStreamDatagramFromSenderForTest(
    const QByteArray& datagram, const QHostAddress& sender)
{
    processStreamDatagram(datagram, sender);
}

void SunSdrRadioConnection::processStreamDatagram(const QByteArray& data,
                                                    const QHostAddress& sender)
{
    // The stream socket binds the protocol's fixed, well-known port
    // (50002) with ShareAddress — deliberately, so a still-streaming
    // QRP from a just-ended prior session stays reachable (see init()'s
    // own comment, and setFixedPortBindingEnabledForTest()'s comment,
    // which records exactly that happening: 85 real leftover I/Q
    // packets from a QRP that kept streaming after the app had already
    // closed). Without this sender check, that stale traffic would both
    // get decoded as this session's I/Q and keep the dead-link
    // watchdog's silence clock alive, defeating the point of that
    // watchdog entirely. Found in review, 2026-08-28.
    if (sender != m_radioAddr) {
        return;
    }

    // Any datagram at all from the connected radio proves the link is
    // still there and talking to us — restart the silence clock ahead
    // of the content checks below, so onDataWatchdogTick() reflects
    // real link liveness rather than only "decoded valid I/Q" liveness.
    m_lastStreamPacketAt.restart();
    recordBytesReceived(static_cast<qint64>(data.size()));

    if (!m_rxReady.load(std::memory_order_acquire)) {
        return;  // discarded, not buffered — see header rationale
    }
    if (!m_profile || data.size() < SunSdr::kIqPacketSize) {
        return;
    }

    SunSdr::IqHeader hdr;
    if (!SunSdr::parseIqHeader(
            reinterpret_cast<const quint8*>(data.constData()),
            data.size(), *m_profile, &hdr)) {
        return;
    }
    if (hdr.opcode != SunSdr::kOpIqRxIdle) {
        return;  // TX-active frames don't apply to a receive-only connection
    }

    if (blockReplyEnabled()) {
        replyToBlock(hdr.seq);
    }

    if (!m_probeChecked) {
        m_probeChecked = true;
        m_probeOn = qEnvironmentVariableIsSet("LONGPATH_SUNSDR_PROBE");
        if (m_probeOn) {
            m_probeTimer.start();
            qCInfo(lcSunSdr) << "SunSdr: Messgeraet an (LONGPATH_SUNSDR_PROBE)";
        }
    }
    if (m_probeOn) {
        probeFeed(hdr.seq, data.mid(SunSdr::kIqHeaderSize));
    }

    // ── Hier stand bis zum 2026-09-23 ein Wiederholungsfilter ───────────
    //
    // Er ist wieder draussen. Nicht weil die Messung falsch war -- sie
    // stimmt, am Geraet nachgemessen und heute noch einmal bestaetigt:
    //
    //   Pakete/s = 1922 | Folgenummer wiederholt: 1682 | bytegleich: 1682
    //                   | VERSCHIEDEN: 0
    //
    // Jede Wiederholung traegt wirklich dieselben Bytes, und die 240
    // uebrig bleibenden Bloecke je Sekunde kommen lueckenlos aufsteigend
    // (eigene Messung: 241 angenommen/s, 0 Spruenge, 0 rueckwaerts).
    //
    // Der Filter ist trotzdem raus, weil er am Geraet nicht funktioniert
    // hat. Der Betreiber hoert mit ihm ein Rauschen, das "nicht typisch"
    // klingt -- mit der alten Fassung ohne Filter klingt dasselbe Geraet
    // richtig. Das gilt sogar dann, wenn man zusaetzlich die
    // Abtastrate auf die gemessenen 48 kHz stellt, also die Kombination,
    // die rechnerisch stimmen MUESSTE.
    //
    // Was daraus folgt: irgendetwas an diesem Strom verstehen wir noch
    // nicht. Solange das so ist, hat das Ohr am echten Geraet Vorrang vor
    // meiner Paketzaehlung -- ein Zustand, der nachweislich funktioniert,
    // ist mehr wert als einer, der nachweislich zaehlbar ist.
    //
    // STAND 2026-09-23 abends, nach einem Tag Messen am Geraet. Was
    // jetzt BEWIESEN ist -- nicht vermutet:
    //
    //   * Die Achtfachung stimmt, und zwar ueber die ganze Nutzlast.
    //     Im Treiber selbst gemessen (LONGPATH_SUNSDR_PROBE=1, Fenster
    //     ueber 64 Folgenummern, volle 1200 Byte):
    //         1921 Pakete/s, 247 Folgenummern/s,
    //         8 Pakete bei 232 der 247 Nummern,
    //         1683 Wiederholungen, davon GANZ bytegleich 1683,
    //         verschieden 0.
    //
    //   * ExpertSDR2 bekommt an DEMSELBEN Geraet, in derselben Stunde,
    //     etwas anderes: 480 Pakete/s, 240 Folgenummern/s, ZWEI Pakete
    //     je Nummer, und die beiden sind VERSCHIEDEN (4797 von 4797
    //     Gruppen). Die Rahmenrate ist bei beiden 240/s.
    //
    //   Es ist also keine Eigenart des Geraets und kein Messfehler: die
    //   QRP legt uns achtmal dasselbe in acht Plaetze, waehrend sie
    //   ExpertSDR zwei Plaetze mit echten Daten fuellt. Der Unterschied
    //   liegt in dem, was beim Verbinden gesagt wird -- ExpertSDR2
    //   schickt rund zwei Dutzend Steuerrahmen, dieser Treiber einen.
    //
    // Was AUSGESCHLOSSEN ist:
    //   * Die Abtastrate allein. Mit 48 000 in den Kenndaten UND
    //     gefiltertem Strom stimmt die Rechnung von vorne bis hinten
    //     (Protokoll: sampleRate=48000, 240 Bloecke/s, 48 007 Proben/s)
    //     -- und am Geraet klingt genau das am schlechtesten.
    //   * Die empfangsseitigen Steuerrahmen von ExpertSDR2, verbatim
    //     nachgeschickt (0x03 0x04 0x0f 0x10 0x11 0x13 0x15 0x16 0x18
    //     0x1a 0x1c, alle mit den Originalbytes aus einem Mitschnitt
    //     desselben Abends): der Strom bleibt Paket fuer Paket derselbe.
    //     Ausprobiert ueber LONGPATH_SUNSDR_EXTRA.
    //
    // Was als naechstes zu versuchen waere: dieselben Rahmen in
    // ExpertSDRs REIHENFOLGE, also groesstenteils VOR dem
    // Zustandsrahmen 0x01 statt danach. Der Mitschnitt liegt in
    // ~/Longpath/werkzeug/mitschnitte; auslesen mit
    // tools/sunsdr_opcode_watch.py --pcap <datei> --full.

    QVector<float> samples;
    SunSdr::decodeIqSamples(
        reinterpret_cast<const quint8*>(data.constData()) + SunSdr::kIqHeaderSize,
        data.size() - SunSdr::kIqHeaderSize, &samples);
    if (samples.isEmpty()) { return; }

    // TEMPORARY diagnostic, 2026-09-03 (bench session, real antenna,
    // ExpertSDR2 shows the same "waterfall but no station audio" symptom
    // -- ruling out a Longpath-specific decode bug, but not yet ruling
    // out whether the QRP's ADC/RF front end is genuinely live at all.
    // Two checks, once a second: (a) the payload's raw bytes vs the
    // previous packet's -- identical would mean frozen/stuck data, not
    // real antenna noise; (b) peak decoded sample magnitude, as a coarse
    // "is anything moving" gauge. Remove once this question is settled.
    {
        static QElapsedTimer diagTimer;
        static bool diagStarted = false;
        static QByteArray lastPayload;
        static quint64 packetsSinceLog = 0;
        static quint64 identicalToPrevSinceLog = 0;
        if (!diagStarted) { diagTimer.start(); diagStarted = true; }

        const QByteArray payload = data.mid(SunSdr::kIqHeaderSize);
        ++packetsSinceLog;
        if (!lastPayload.isEmpty() && payload == lastPayload) {
            ++identicalToPrevSinceLog;
        }
        lastPayload = payload;

        if (diagTimer.elapsed() > 1000) {
            diagTimer.restart();
            float peakAbs = 0.0f;
            for (float v : samples) {
                const float a = v < 0.0f ? -v : v;
                if (a > peakAbs) { peakAbs = a; }
            }
            qCInfo(lcSunSdr) << "SunSdr: [DIAG] peak |sample| =" << peakAbs
                             << "(full scale 1.0) --" << identicalToPrevSinceLog
                             << "of" << packetsSinceLog
                             << "packets this second were byte-identical "
                                "to the one before them -- Q ungleich 0:"
                             << m_qNonZeroPercent << "%";
            packetsSinceLog = 0;
            identicalToPrevSinceLog = 0;
        }
    }

    // ── Traegt der Q-Kanal Daten? ─────────────────────────────────
    //
    // Nach dem Einschalten liefert die QRP nur EINEN reellen Kanal -- Q
    // ist dann zu 100 % exakt 0 (2026-09-25, 1,7 Mio. Proben), die
    // Seitenbaender liegen gespiegelt uebereinander. Erst ExpertSDR2
    // schaltet echtes I/Q ein, und das bleibt bis zum Ausschalten.
    // Welcher Befehl das ist, ist noch offen; diese Zaehlung zeigt es an
    // (je Verbindung, sekundenweise).
    for (int i = 1; i < samples.size(); i += 2) {
        if (samples[i] != 0.0f) { ++m_qNonZeroInWindow; }
    }
    m_qSamplesInWindow += static_cast<quint64>(samples.size() / 2);
    if (!m_qCheckTimer.isValid()) { m_qCheckTimer.start(); }
    if (m_qCheckTimer.elapsed() > 1000 && m_qSamplesInWindow > 0) {
        m_qCheckTimer.restart();
        m_qNonZeroPercent = 100.0 * double(m_qNonZeroInWindow) / double(m_qSamplesInWindow);
        m_singleChannelSeen = (m_qNonZeroInWindow == 0);
        m_qNonZeroInWindow = 0;
        m_qSamplesInWindow = 0;
        if (m_singleChannelSeen && !m_singleChannelWarned) {
            m_singleChannelWarned = true;
            qCWarning(lcSunSdr) << "SunSdr: die QRP liefert nur EINEN Kanal (Q = 0) --"
                                   " echtes I/Q ist nicht eingeschaltet, die Seitenbaender"
                                   " liegen uebereinander. Uebergang: ExpertSDR2 einmal"
                                   " verbinden lassen, dann Longpath.";
        }
    }

    if (state() == ConnectionState::Connecting) {
        setState(ConnectionState::Connected);
        if (m_connectWatchdog) { m_connectWatchdog->stop(); }
    }

    // Pegelabgleich je Geraet (SunSdr::Profile::rxLevelTrimDb) -- erst
    // hier, nach der DIAG-Messung, damit deren Werte weiter den rohen
    // Vollausschlag zeigen und mit den Messungen vor dem Abgleich
    // vergleichbar bleiben.
    if (m_rxLevelGain != 1.0f) {
        for (float& v : samples) { v *= m_rxLevelGain; }
    }

    // ── Einschaltstoss stumm ── 2026-09-26 ──────────────────────────
    //
    // Frisch eingeschaltet liefert die QRP nur I (Q = 0), bis
    // setReceiverFrequency() mit 0x07 echtes I/Q einschaltet. Dazwischen
    // liegt rund eine Sekunde (Protokoll 26.09.: verbunden 58,436, 0x07
    // 58,581, Q ungleich 0 erst in der naechsten DIAG-Sekunde), in der
    // die Seitenbaender uebereinanderliegen und der Pegel ~20 dB zu hoch
    // ist -- Betreiber: "war kurz ganz laut". Solange ein Block keinen
    // einzigen Q-Wert traegt, geht Stille weiter (das Spektrum zeigt dann
    // den Thetis-Boden -200 dBm), hoechstens m_singleChannelHoldMs ab dem
    // ersten Block. Bleibt die QRP laenger einkanalig, laeuft das Signal
    // wie bisher durch und die Warnung oben sagt, warum.
    //
    // Echtes I/Q ohne Antenne hat ~17 % Q-Werte ungleich 0; ein Block
    // (~200 Paare) ist dann praktisch nie ganz leer. Ein einziger Block
    // mit Q schaltet die Sperre fuer diese Verbindung ab.
    if (!m_iqConfirmed) {
        bool anyQ = false;
        for (int i = 1; i < samples.size(); i += 2) {
            if (samples[i] != 0.0f) { anyQ = true; break; }
        }
        if (anyQ) {
            m_iqConfirmed = true;
        } else {
            if (!m_streamStartTimer.isValid()) { m_streamStartTimer.start(); }
            if (m_streamStartTimer.elapsed() < m_singleChannelHoldMs) {
                samples.fill(0.0f);
            }
        }
    }

    emit iqDataReceived(/*hwReceiverIndex=*/0, samples);
    emit frameReceived();
}

void SunSdrRadioConnection::onKeepaliveTimeout()
{
    if (!m_streamSocket || !m_profile || m_radioAddr.isNull()) { return; }

    // Silent RX-idle frame, opcode 0xFE (kOpIqRxIdle) with an
    // all-zero payload — the same shape a genuine idle-RX packet from
    // the radio itself has, just host-to-radio instead of the reverse.
    // Header-building only, not TX/PTT logic: SunSdrProtocol.h's own
    // scope comment says exactly this ("the host must keep sending
    // periodic silent 0xFE frames just to keep the RX stream alive...
    // It carries no audio and asserts no PTT state").
    QByteArray pkt = SunSdr::buildIqHeader(*m_profile, SunSdr::kOpIqRxIdle,
                                           m_txSeq++, /*byte8=*/0, /*byte9=*/0);
    pkt.append(SunSdr::kIqPayloadSize, char(0));
    m_streamSocket->writeDatagram(pkt, m_radioAddr, m_profile->defaultStreamPort);
    recordBytesSent(static_cast<qint64>(pkt.size()));
}

void SunSdrRadioConnection::onDataWatchdogTick()
{
    if (!m_running || state() != ConnectionState::Connected) { return; }
    if (!m_lastStreamPacketAt.isValid()) { return; }
    if (m_lastStreamPacketAt.elapsed() <= kDataSilenceTimeoutMs) { return; }

    // Full teardown, not just a state flip — same discipline as
    // onConnectTimeout()'s own precedent in this file: this class has
    // no reconnect timer to hand off to (see that function's comment),
    // so leaving the sockets bound and rxReady open after declaring
    // the link lost would let a late, spurious packet keep flowing
    // into the DSP/audio/spectrum pipeline while the UI says the link
    // is down. The operator reconnects via a brand-new instance
    // (RadioModel's normal connect path), same as after any other
    // disconnect.
    qCWarning(lcSunSdr) << "SunSdr: no I/Q data for"
                        << m_lastStreamPacketAt.elapsed()
                        << "ms - radio unreachable, powered off, or "
                           "network path lost; declaring link lost";
    m_running = false;
    m_awaitingBeacon = false;
    m_radioAddr.clear();
    setRxReady(false);
    // Step 2 TX gate: same reset+leave-the-trace-ring-alone rationale as
    // onConnectTimeout() above — a dead-link trip is exactly when the
    // ring's recent history is most useful to a diagnosing operator.
    // Recorded before the stores, same as onConnectTimeout(), so that
    // history includes WHY TX/arm went off, not just that it did.
    if (m_mox.load(std::memory_order_acquire)) {
        pushTxTrace(TxTraceKind::MoxAccepted,
                    QStringLiteral("accepted: mox -> false (forced by dead-link watchdog)"));
    }
    if (m_txArmed.load(std::memory_order_acquire)) {
        pushTxTrace(TxTraceKind::Disarmed,
                    QStringLiteral("forced by dead-link watchdog"));
    }
    m_txArmed.store(false, std::memory_order_release);
    m_mox.store(false, std::memory_order_release);
    // Step 3: same rationale as onConnectTimeout()'s identical line — a
    // dead-link trip must silence the pacer along with everything else
    // this block already tears down.
    if (m_txPacer) { m_txPacer->stop(); }
    if (m_keepaliveTimer) { m_keepaliveTimer->stop(); }
    if (m_dataWatchdog) { m_dataWatchdog->stop(); }
    m_lastStreamPacketAt.invalidate();
    if (m_controlSocket) { m_controlSocket->close(); }
    if (m_streamSocket) { m_streamSocket->close(); }

    setState(ConnectionState::LinkLost);
    emit errorOccurred(RadioConnectionError::NoDataTimeout,
                       QStringLiteral("SunSDR: radio stopped responding"));
}

// ── Safe no-ops: receive-only, see header ───────────────────────────
//
// setMox() is NOT here any more — Step 2 of the SunSDR2 QRP TX-chain
// plan gave it a real body (see its own definition, next to
// setAttenuator() above), same reasoning that already pulled
// setAttenuator() out of this block.

void SunSdrRadioConnection::setTxFrequency(quint64) {}
void SunSdrRadioConnection::setPreamp(bool) {}
void SunSdrRadioConnection::setTxDrive(int) {}
void SunSdrRadioConnection::setAntennaRouting(AntennaRouting) {}
void SunSdrRadioConnection::sendTxIq(const float*, int) {}
void SunSdrRadioConnection::setTrxRelay(bool) {}
void SunSdrRadioConnection::setMicBoost(bool) {}
void SunSdrRadioConnection::setLineIn(bool) {}
void SunSdrRadioConnection::setMicTipRing(bool) {}
void SunSdrRadioConnection::setMicBias(bool) {}
void SunSdrRadioConnection::setLineInGain(int) {}
void SunSdrRadioConnection::setUserDigOut(quint8) {}
void SunSdrRadioConnection::setPuresignalRun(bool) {}
void SunSdrRadioConnection::setMicPTTDisabled(bool) {}
void SunSdrRadioConnection::setMicXlr(bool) {}
void SunSdrRadioConnection::setWatchdogEnabled(bool) {}


// ---------------------------------------------------------------------------
// probeFeed / probeCloseFrame / probeReportIfDue — der Strom von innen
//
// Siehe den Kommentar an den Mitgliedern im Kopf. Kurz: die Frage, ob
// die Pakete einer Folgenummer dieselben Daten tragen, ist am
// 2026-09-23 zweimal von aussen beantwortet worden, beide Male mit
// abgeschnittener Nutzlast. Hier wird sie ueber alle 1200 Byte
// beantwortet.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// sendBenchFrames — Steuerrahmen aus einer Umgebungsvariablen schicken
//
// LONGPATH_SUNSDR_PRE geht VOR dem Zustandsrahmen hinaus,
// LONGPATH_SUNSDR_EXTRA danach; beide tragen Rahmen als Hexziffern,
// durch Komma getrennt.
//
// Wozu: am 2026-09-23 ist gemessen, dass die QRP an Longpath jeden
// Block ACHTMAL schickt (bytegleich, im Treiber ueber alle 1200 Byte
// geprueft), an ExpertSDR2 dagegen zwei VERSCHIEDENE. Der einzige
// Unterschied ist, was beim Verbinden gesagt wird: ExpertSDR schickt
// rund zwei Dutzend Rahmen, Longpath einen. Damit laesst sich das
// ausprobieren, ohne fuer jeden Versuch neu zu bauen -- und auch die
// REIHENFOLGE, denn ExpertSDR schickt den Zustandsrahmen zuletzt.
//
// Nur fuer die Werkbank: ohne die Variable geht nichts hinaus, und was
// hineingeschrieben wird, entscheidet der Mensch davor.
// ---------------------------------------------------------------------------
void SunSdrRadioConnection::sendBenchFrames(const QString& envName)
{
    if (!m_controlSocket || !m_profile) { return; }
    const QString list = qEnvironmentVariable(envName.toLatin1().constData());
    if (list.isEmpty()) { return; }

    const QStringList parts = list.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& hex : parts) {
        const QByteArray frame = QByteArray::fromHex(hex.trimmed().toLatin1());
        if (frame.isEmpty()) { continue; }
        m_controlSocket->writeDatagram(frame, m_radioAddr,
                                        m_profile->defaultCtrlPort);
        recordBytesSent(static_cast<qint64>(frame.size()));
        qCInfo(lcSunSdr) << "SunSdr: Werkbank-Rahmen" << envName << "Opcode"
                         << (frame.size() > 2 ? quint8(frame[2]) : 0)
                         << "-" << frame.size() << "Byte";
    }
}

bool SunSdrRadioConnection::blockReplyEnabled()
{
    if (!m_blockReplyChecked) {
        if (!m_profile) { return false; }
        m_blockReplyChecked = true;
        // Fuer die QRP der Normalfall, am Geraet bestaetigt (2026-09-24:
        // 240 Pakete/s, 1x je Folgenummer, und am Ohr "sollte passen").
        // DX/PRO sind nie gegen ein echtes Geraet gelaufen; dort bleibt
        // der bisherige Keepalive, bis jemand es dort misst.
        // LONGPATH_SUNSDR_BLOCKANTWORT=0/1 ueberstimmt beides.
        const QByteArray env = qgetenv("LONGPATH_SUNSDR_BLOCKANTWORT");
        m_blockReplyOn = env.isEmpty() ? m_profile->variant == SunSdr::Variant::Qrp
                                       : env != "0";
        qCInfo(lcSunSdr) << "SunSdr: Blockantwort" << (m_blockReplyOn ? "an" : "aus")
                         << "-- jeder neue Block wird mit derselben Folgenummer"
                            " still beantwortet, statt alle 2 s ein Keepalive";
    }
    return m_blockReplyOn;
}

void SunSdrRadioConnection::replyToBlock(quint16 seq)
{
    if (!m_streamSocket || !m_profile || m_radioAddr.isNull()) { return; }

    for (int i = 0; i < m_blockReplyFill; ++i) {
        if (m_blockReplyRing[i] == seq) { return; }
    }
    m_blockReplyRing[m_blockReplyPos] = seq;
    m_blockReplyPos = (m_blockReplyPos + 1) % int(m_blockReplyRing.size());
    m_blockReplyFill = std::min(m_blockReplyFill + 1, int(m_blockReplyRing.size()));

    // Kopf wie ExpertSDR2 im Leerlauf und wie ArtemisSDRs
    // sunsdr_build_tx_silence(), sunsdr.c:4105-4115 [@f8b01d25c5]:
    // op=0xFE, byte8=0x01, byte9=0x00, Nutzlast Null (Stille).
    QByteArray pkt = SunSdr::buildIqHeader(*m_profile, SunSdr::kOpIqRxIdle,
                                           seq, /*byte8=*/0x01, /*byte9=*/0x00);
    pkt.append(SunSdr::kIqPayloadSize, char(0));
    m_streamSocket->writeDatagram(pkt, m_radioAddr, m_profile->defaultStreamPort);
    recordBytesSent(static_cast<qint64>(pkt.size()));
    ++m_blockRepliesSent;
    m_lastBlockReplySeq = seq;
}

void SunSdrRadioConnection::probeFeed(quint16 seq, const QByteArray& payload)
{
    ++m_probePackets;

    // Gegen ALLE Pakete derselben Folgenummer im Fenster vergleichen,
    // ueber die ganze Nutzlast -- nicht nur gegen das vorige Paket und
    // nicht nur ueber die ersten Bytes.
    bool isRepeat = false;
    for (const auto& prev : std::as_const(m_probeSeen)) {
        if (prev.first != seq) { continue; }
        isRepeat = true;
        if (prev.second == payload) {
            ++m_probeSame;
        } else {
            ++m_probeDiffer;
            if (m_probeFirstDiff < 0) {
                const int n = std::min(prev.second.size(), payload.size());
                for (int k = 0; k < n; ++k) {
                    if (prev.second[k] != payload[k]) { m_probeFirstDiff = k; break; }
                }
            }
        }
        break;
    }
    if (isRepeat) { ++m_probeRepeats; }

    m_probeCount[seq] += 1;
    m_probeSeen.prepend(qMakePair(seq, payload));
    while (m_probeSeen.size() > kProbeWindow) { m_probeSeen.removeLast(); }

    probeReportIfDue();
}

void SunSdrRadioConnection::probeReportIfDue()
{
    if (m_probeTimer.elapsed() <= 1000) { return; }
    const double secs = double(m_probeTimer.elapsed()) / 1000.0;

    m_probeSizes.clear();
    for (auto it = m_probeCount.cbegin(); it != m_probeCount.cend(); ++it) {
        m_probeSizes[it.value()] += 1;
    }
    QStringList sizes;
    for (auto it = m_probeSizes.cbegin(); it != m_probeSizes.cend(); ++it) {
        sizes << QStringLiteral("%1x bei %2 Nummern").arg(it.key()).arg(it.value());
    }

    qCInfo(lcSunSdr).nospace()
        << "SunSdr: [MESSUNG] " << quint64(double(m_probePackets) / secs)
        << " Pakete/s, " << quint64(double(m_probeCount.size()) / secs)
        << " Folgenummern/s | Pakete je Nummer: " << sizes.join(QStringLiteral(", "))
        << " | Wiederholungen: " << m_probeRepeats
        << ", davon GANZ bytegleich: " << m_probeSame
        << ", verschieden: " << m_probeDiffer
        << (m_probeFirstDiff >= 0
                ? QStringLiteral(" (erste Abweichung bei Byte %1)").arg(m_probeFirstDiff)
                : QString());

    m_probeTimer.restart();
    m_probePackets = 0;
    m_probeRepeats = 0;
    m_probeSame = 0;
    m_probeDiffer = 0;
    m_probeFirstDiff = -1;
    m_probeCount.clear();
}

} // namespace Longpath
