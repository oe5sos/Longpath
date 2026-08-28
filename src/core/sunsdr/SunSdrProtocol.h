#pragma once

// This file ports content from TWO separate ArtemisSDR source files
// (sunsdr.c and sunsdr.h), each with its own distinct header. Per
// CLAUDE.md's multi-file attribution rule ("If a Longpath file ports
// from multiple [upstream] files, include every relevant header,
// separated by `// --- From [filename] ---` markers... do not
// paraphrase, summarize, or merge headers from different files"), both
// are reproduced verbatim below, separately. An earlier version of this
// file merged the two into one edited paraphrase and, in doing so,
// silently dropped real text from both AND spliced in a sentence that
// belongs to neither — it was lifted from a third, unrelated
// Thetis/MW0LGE-authored file in the same ArtemisSDR repo (tci.c) with
// its URL swapped to point at ArtemisSDR instead. Found and fixed
// 2026-08-26, during a from-scratch review of this file the same
// evening its port first ran live. Source: this project's own clone at
// `../ArtemisSDR/Project Files/Source/ChannelMaster/sunsdr.c` and
// `sunsdr.h`, commit f8b01d2.

// --- From sunsdr.c ---
/*  sunsdr.c

SunSDR2 native protocol implementation for Thetis.

All SunSDR-specific protocol logic is contained in this file.
This includes: discovery, bootstrap, power on/off, frequency,
mode, PTT, and IQ stream reception.

Protocol details derived from black-box reverse engineering — passive
observation of UDP traffic between a genuine ExpertSDR instance and an
owned SunSDR2 DX radio. No ExpertSDR code, binaries, firmware, or artwork
was referenced or used. The radio's firmware was not modified.

Copyright (C) 2026 Kosta Kanchev (K0KOZ)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

*/

// --- From sunsdr.h ---
/*  sunsdr.h

SunSDR2 native protocol support for Thetis.

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2026 Kosta Kanchev (K0KOZ)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

*/

// no-port-check: byte-for-byte header/constant PORT from ArtemisSDR
// sunsdr.c/sunsdr.h (magic bytes, packet layout, port numbers, NORM
// scale factor) — see docs/architecture/2026-08-24-sunsdr-native-driver-design.md
// for the QRP-confirmation gate this port sits behind. Upstream:
// https://github.com/kk68/ArtemisSDR (this project's own annotation,
// not part of either verbatim header above). THETIS-PROVENANCE.md-
// equivalent record: this is the first Longpath file to port ArtemisSDR
// code rather than merely cite ArtemisSDR facts; a PROVENANCE row should
// be added alongside this file in the same commit.

// =================================================================
// src/core/sunsdr/SunSdrProtocol.h  (NereusSDR/Longpath)
// =================================================================
//
// SunSDR2 native wire protocol — the CONFIRMED framing only.
//
// Scope, deliberately narrow (design doc "Phase 2: Connect + RX IQ +
// panadapter, receive-only, no PTT/drive code written at all yet"):
//   - Per-model profile (magic byte, ports) — DX/PRO from ArtemisSDR,
//     QRP confirmed against a real bench capture 2026-08-24.
//   - Control-channel 18-byte header: build + parse.
//   - IQ-stream 10-byte header: build + parse. Building this is NOT
//     TX/PTT logic — the host must send periodic silent 0xFE frames
//     just to keep the RX stream alive (design doc: "During RX idle,
//     the host must keep sending silent 0xFE packets... or the radio
//     disconnects the stream after roughly 8 seconds"). It carries no
//     audio and asserts no PTT state.
//   - IQ sample decode: 200 x 24-bit LE samples -> normalized float
//     I/Q pairs. This is the one piece that actually makes a
//     panadapter possible.
//
// Deliberately NOT here: the opcode table beyond what these functions
// need, the ~30-step boot macro, STATE_SYNC/CONFIG_BLOCK payload
// templates, PTT (0x06), drive byte (0x17), TX packet building. Those
// opcode MEANINGS are still DX-sourced and only partially
// cross-checked against the QRP (design doc "New finding: the QRP's
// real opcode set is richer than ArtemisSDR's documented DX subset").
// The framing implemented here — magic bytes, header layout, sample
// encoding — IS confirmed; trust that, not the rest, until it's
// separately confirmed.
//
// No networking, no threading, no RadioConnection dependency. Pure
// byte-level encode/decode, testable against the real captured packet
// bytes quoted in the design doc without any hardware attached.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-25 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork). Ported
//                 from ArtemisSDR sunsdr.c/sunsdr.h
//                 [@f8b01d25c5, 2026-07-08, kk68/ArtemisSDR main].
// =================================================================

#include <QByteArray>
#include <QVector>

namespace Longpath {
namespace SunSdr {

// ── Per-model profile ────────────────────────────────────────────────
//
// From ArtemisSDR sunsdr.c:2728-2741 [@f8b01d25c5] (sunsdr_profile_dx /
// sunsdr_profile_pro) plus the QRP row confirmed against a real bench
// capture — design doc "Confirmed: ports and magic byte".
enum class Variant { Dx, Pro, Qrp };

struct Profile {
    Variant     variant;
    const char* name;
    quint16     defaultCtrlPort;
    quint16     defaultStreamPort;
    quint8      magic0;
    // Native RX rate. DX and PRO are both 312500 Hz on-wire
    // (ArtemisSDR sunsdr.c:2728-2741 [@f8b01d25c5]; the PRO row's
    // comment there records it was raised to match DX in v2.1.9).
    // The QRP value here is carried over from DX by assumption, NOT
    // independently confirmed — the design doc's capture analysis
    // established magic byte and header framing, not sample rate.
    double      rxNativeRateHz;
};

// From ArtemisSDR sunsdr.c:2728-2732 [@f8b01d25c5].
inline constexpr Profile kProfileDx{
    Variant::Dx, "SunSDR2 DX", 50001, 50002, 0x32, 312500.0};

// From ArtemisSDR sunsdr.c:2740-2741 [@f8b01d25c5].
inline constexpr Profile kProfilePro{
    Variant::Pro, "SunSDR2 PRO", 50002, 50003, 0x01, 312500.0};

// Ports and magic byte confirmed against a real bench capture,
// 2026-08-24 (21,720 packets, ExpertSDR2 <-> QRP) — design doc
// "Confirmed: ports and magic byte". rxNativeRateHz is the DX value,
// carried over unconfirmed (see Profile::rxNativeRateHz comment).
inline constexpr Profile kProfileQrp{
    Variant::Qrp, "SunSDR2 QRP", 50001, 50002, 0x03, 312500.0};

// From ArtemisSDR sunsdr.h:28 [@f8b01d25c5]. Second magic byte, fixed
// across every model/profile — only byte[0] varies.
inline constexpr quint8 kMagic1 = 0xFF;

// ── Control channel: 18-byte header ─────────────────────────────────
//
// From ArtemisSDR sunsdr_build_header, sunsdr.c:2242-2255 [@f8b01d25c5]
// (SUNSDR_CTL_HDR_SIZE = 18, sunsdr.h:136). Layout, confirmed
// byte-for-byte against a real QRP capture (design doc "Confirmed:
// both header formats"):
//   [0]      magic0 (per-profile)
//   [1]      magic1 (0xFF, universal)
//   [2]      opcode
//   [3]      0x00
//   [4:5]    declared payload length, u16 LE
//   [6:7]    sub-index / sub-opcode, u16 LE
//   [8:9]    0x00
//   [10]     0x01
//   [11:17]  zero padding
inline constexpr int kCtlHeaderSize = 18;

struct ControlHeader {
    quint8  magic0{0};
    quint8  magic1{0};
    quint8  opcode{0};
    quint16 declaredPayloadLen{0};
    quint16 sub{0};
};

// Builds an 18-byte control header for `profile`. Matches
// sunsdr_build_header's byte layout exactly, including the
// memset-to-zero-first semantics (bytes 3, 8, 9, 11-17 are zero;
// byte 10 is the fixed 0x01).
QByteArray buildControlHeader(const Profile& profile, quint8 opcode,
                              quint16 sub, quint16 declaredPayloadLen);

// Parses an 18-byte control header. Returns false (and leaves `out`
// untouched) if `data` is too short or magic0/magic1 don't match
// `profile` — a caller receiving bytes from the wrong model's socket,
// or noise, should not silently proceed as if it parsed correctly.
bool parseControlHeader(const quint8* data, int len, const Profile& profile,
                        ControlHeader* out);

// ── IQ stream: 10-byte header + 1200-byte payload ───────────────────
//
// From ArtemisSDR sunsdr_build_iq_header, sunsdr.c:1678-1690
// [@f8b01d25c5] (SUNSDR_IQ_HDR_SIZE = 10, SUNSDR_IQ_PAYLOAD_SIZE =
// 1200, SUNSDR_IQ_PKT_SIZE = 1210, sunsdr.h:85-87). Layout, confirmed
// against a real QRP capture (design doc "Confirmed: both header
// formats", the 1210-byte-packet example):
//   [0]   magic0 (per-profile)
//   [1]   magic1 (0xFF)
//   [2]   opcode (0xFE = RX-state/idle-TX, 0xFD = TX-active)
//   [3]   0xFF  (differs from the control header's byte[3] = 0x00)
//   [4:5] payload size, u16 LE (always 1200 in practice)
//   [6:7] sequence, u16 LE
//   [8:9] state bytes (caller-supplied; ArtemisSDR's TX-active path
//         uses byte8=0x02/byte9=0x01, sunsdr.c:1699-1701 [@f8b01d25c5])
inline constexpr int kIqHeaderSize      = 10;
inline constexpr int kIqPayloadSize     = 1200;
inline constexpr int kIqPacketSize      = kIqHeaderSize + kIqPayloadSize;
inline constexpr int kIqComplexPerPkt   = 200;  // I/Q pairs per packet
inline constexpr int kIqBytesPerComplex = 6;    // 3 bytes I + 3 bytes Q

// From ArtemisSDR sunsdr.h:79,82 [@f8b01d25c5].
inline constexpr quint8 kOpIqRxIdle = 0xFE;
inline constexpr quint8 kOpIqTxActive = 0xFD;

struct IqHeader {
    quint8  magic0{0};
    quint8  magic1{0};
    quint8  opcode{0};
    quint16 payloadLen{0};
    quint16 seq{0};
    quint8  byte8{0};
    quint8  byte9{0};
};

// Builds a 10-byte IQ-stream header. `payload` is NOT appended here —
// callers building the periodic silent RX-idle keepalive frame
// (design doc: "the host must keep sending silent 0xFE packets... or
// the radio disconnects the stream") append kIqPayloadSize zero bytes
// themselves; this function only builds the header ArtemisSDR itself
// builds separately from the payload (sunsdr_build_iq_header takes no
// payload pointer at all).
QByteArray buildIqHeader(const Profile& profile, quint8 opcode, quint16 seq,
                         quint8 byte8, quint8 byte9);

// Parses a 10-byte IQ-stream header. Same magic-byte validation
// discipline as parseControlHeader.
bool parseIqHeader(const quint8* data, int len, const Profile& profile,
                   IqHeader* out);

// Decodes the 1200-byte IQ payload (200 x 6-byte 24-bit-LE I/Q pairs)
// into normalized float32 I/Q pairs in [-1, +1), interleaved I,Q,I,Q...
// (kIqComplexPerPkt * 2 floats out).
//
// From ArtemisSDR's RX unpack loop, sunsdr.c:4653-4668 [@f8b01d25c5]:
// each 6-byte slot is Q-first-I-second on the wire (payload[k+0..2] =
// Q, payload[k+3..5] = I — design doc: "confirms Q-in-low-3-bytes /
// I-in-high-3-bytes per slot"), but the DEINTERLEAVED output ArtemisSDR
// itself produces is I-first (rxBuf[2*i+0] = I, rxBuf[2*i+1] = Q) —
// this function matches that output order exactly, not the wire order.
// ArtemisSDR's own comment warns swapping this mirrors the sideband on
// air (sunsdr.c:1711-1720 [@f8b01d25c5]).
//
// Normalization: each 24-bit sample is sign-extended by placing it in
// the top 3 bytes of a 32-bit word (i.e. scaled by 2^8) before
// dividing by 2^31 (ArtemisSDR's NORM constant, sunsdr.c:1306
// [@f8b01d25c5]: `1.0 / 2147483648.0`) — algebraically identical to
// dividing a signed 24-bit value by 2^23, just computed via a
// sign-extension-safe bit trick in C. This function reproduces the
// same arithmetic, not just the same result, so a future reader
// comparing against the original doesn't have to re-derive the
// equivalence.
void decodeIqSamples(const quint8* payload, int payloadLen,
                     QVector<float>* outInterleaved);

// ── Frequency-set payload (opcode 0x08/0x09): candidate encoding ────
//
// CONFIRMED 2026-08-27. Design doc, "candidate frequency-encoding
// formula found — source-grounded, band-plausible" (2026-08-26),
// upgraded from hypothesis to confirmed the next day against a live,
// exact, known-frequency bench test — see
// SunSdrRadioConnection::setReceiverFrequency()'s implementation
// comment for the numbers (ExpertSDR2 at 7,099,904 Hz vs. this
// formula's 7,099,204 Hz from the same real captured frame, 700 Hz
// apart out of 7.1 MHz, consistent with VFO scroll-settling lag, not a
// formula error). Derived from ArtemisSDR's real sunsdr_send_freq_pkt()
// (sunsdr.c:2259-2277 [@f8b01d25c5]) — `scaled = freqHz *
// SUNSDR_FREQ_SCALE` (SUNSDR_FREQ_SCALE=10, sunsdr.h:123
// [@f8b01d25c5]), an 8-byte little-endian integer at payload offset 0.
//
// Wired into SunSdrRadioConnection::setReceiverFrequency() since
// 2026-08-27 — that function reuses the exact 18-byte header prefix
// from the one bench-confirmed-accepted frequency-set frame and
// appends this payload for the requested Hz value. The one caveat
// that remains genuinely open (not this formula): the header's bytes
// 14-17 carry a varying, not-fully-understood value in every real
// captured frame — see that function's own comment.
//
// Only the low 4 bytes are populated (32-bit range, ~429 MHz headroom
// at this scale) because that's all any observed real frame ever used
// — ArtemisSDR's own code always writes the full 8 bytes, so
// encodeFrequencyPayload() does too, for exact byte-for-byte parity
// with what a real frame would contain.

// Encodes `freqHz` as an 8-byte little-endian `freqHz * 10` payload,
// matching sunsdr_send_freq_pkt()'s byte layout exactly.
QByteArray encodeFrequencyPayloadCandidate(quint64 freqHz);

// Inverse of the above: reads an 8-byte little-endian payload and
// returns `value / 10` as the candidate frequency in Hz. Returns 0 if
// `payload` is shorter than 8 bytes.
quint64 decodeFrequencyPayloadCandidate(const QByteArray& payload);

} // namespace SunSdr
} // namespace Longpath
