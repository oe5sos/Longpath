// no-port-check: NereusSDR/Longpath-original. See header for scope.

// =================================================================
// src/core/SunSdrRadioConnection.cpp  (NereusSDR/Longpath)
// =================================================================
//
// NereusSDR/Longpath-original. Scope and rationale in the header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "SunSdrRadioConnection.h"

#include <QLoggingCategory>
#include <QNetworkAddressEntry>
#include <QNetworkDatagram>
#include <QNetworkInterface>

namespace Longpath {

namespace {
Q_LOGGING_CATEGORY(lcSunSdr, "longpath.sunsdr")
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
    m_caps = m_hardwareProfile.caps
             ? m_hardwareProfile.caps
             : &BoardCapsTable::forBoard(info.boardType);

    m_intentionalDisconnect = false;
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
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) { continue; }
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            const QHostAddress bcast = entry.broadcast();
            if (bcast.isNull()) { continue; }
            m_controlSocket->writeDatagram(query, bcast, ctrlPort);
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
    m_intentionalDisconnect = true;
    m_awaitingBeacon = false;
    setRxReady(false);
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
    m_intentionalDisconnect = true;
    m_running = false;
    m_awaitingBeacon = false;  // a late beacon reply after this must not
                               // reopen the RX gate — see onControlReadyRead()
    m_radioAddr.clear();
    setRxReady(false);

    if (m_connectWatchdog) {
        m_connectWatchdog->stop();
    }
    if (m_keepaliveTimer) {
        m_keepaliveTimer->stop();
    }
    if (m_dataWatchdog) {
        m_dataWatchdog->stop();
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
    // SunSdr::encodeFrequencyPayloadCandidate() (freqHz * 10, 8-byte
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
    QByteArray frame = QByteArray::fromHex(
        "03ff0800080000000000010000008ca31dd7");
    frame += SunSdr::encodeFrequencyPayloadCandidate(frequencyHz);

    m_controlSocket->writeDatagram(frame, m_radioAddr, m_profile->defaultCtrlPort);
    qCInfo(lcSunSdr) << "SunSdr: setReceiverFrequency() ->" << frequencyHz << "Hz";
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

    QByteArray frame;
    if (dB == 0) {
        frame = QByteArray::fromHex(
            "03ff040004000000000001000000d804da1900000000");
    } else if (dB == -20) {
        frame = QByteArray::fromHex(
            "03ff040004000000000001000000bd6366a101000000");
    } else {
        qCInfo(lcSunSdr) << "SunSdr: setAttenuator(" << dB
                         << ") — only 0 and -20 dB are bench-confirmed, "
                            "not sending anything for this value";
        return;
    }

    m_controlSocket->writeDatagram(frame, m_radioAddr, m_profile->defaultCtrlPort);
    qCInfo(lcSunSdr) << "SunSdr: setAttenuator() ->" << dB << "dB";
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
        m_controlSocket->writeDatagram(stateSyncFrameForTest(), m_radioAddr,
                                        m_profile->defaultCtrlPort);
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
    if (m_keepaliveTimer) {
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
        // Any datagram at all on this socket proves the radio is still
        // there and talking to us — restart the silence clock before
        // processStreamDatagram()'s own content checks (rxReady gate,
        // opcode filter), so onDataWatchdogTick() reflects real link
        // liveness rather than only "decoded valid I/Q" liveness.
        m_lastStreamPacketAt.restart();
        processStreamDatagram(dgram.data());
    }
}

void SunSdrRadioConnection::feedStreamDatagramForTest(const QByteArray& datagram)
{
    processStreamDatagram(datagram);
}

void SunSdrRadioConnection::processStreamDatagram(const QByteArray& data)
{
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

    QVector<float> samples;
    SunSdr::decodeIqSamples(
        reinterpret_cast<const quint8*>(data.constData()) + SunSdr::kIqHeaderSize,
        data.size() - SunSdr::kIqHeaderSize, &samples);
    if (samples.isEmpty()) { return; }

    if (state() == ConnectionState::Connecting) {
        setState(ConnectionState::Connected);
        if (m_connectWatchdog) { m_connectWatchdog->stop(); }
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
    setRxReady(false);
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

void SunSdrRadioConnection::setTxFrequency(quint64) {}
void SunSdrRadioConnection::setPreamp(bool) {}
void SunSdrRadioConnection::setTxDrive(int) {}
void SunSdrRadioConnection::setMox(bool) {}
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

} // namespace Longpath
