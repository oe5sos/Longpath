// Same two-file attribution as SunSdrProtocol.h — see that file's
// top-of-file comment for why both real headers are reproduced
// separately below rather than merged, and for the fabricated-sentence
// defect this replaces (found and fixed 2026-08-26).

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

// =================================================================
// src/core/sunsdr/SunSdrProtocol.cpp  (NereusSDR/Longpath)
// =================================================================
//
// NereusSDR/Longpath-original encode/decode logic; behaviour ported
// from ArtemisSDR as cited in the header. See SunSdrProtocol.h for
// scope and the QRP-confirmation gate this sits behind.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-25 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-09-02 — TX control-channel pure encoders (MOX 0x06, antenna
//                 select 0x15, drive byte 0x17, PA enable 0x24) added
//                 for Martin Fischer, AI-assisted via Anthropic Claude
//                 (Cowork). Step 1 of a 6-step operator-approved TX
//                 chain plan — see SunSdrProtocol.h's own modification
//                 history entry for the same date.
// =================================================================

#include "core/sunsdr/SunSdrProtocol.h"

#include <array>
#include <cstdint>
#include <cstring>

namespace Longpath {
namespace SunSdr {

namespace {

// Appends `value` as 4 little-endian bytes — the payload shape every
// `sunsdr_send_u32_cmd()`-issued opcode uses (ArtemisSDR
// sunsdr.c:2391-2403 [@f8b01d25c5]; see this file's header comment on
// the TX control-channel opcodes for the full citation).
void appendU32Le(QByteArray& out, quint32 value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 24) & 0xFF));
}

} // namespace

QByteArray buildControlHeader(const Profile& profile, quint8 opcode,
                              quint16 sub, quint16 declaredPayloadLen)
{
    // memset-to-zero-first, then overwrite specific bytes — mirrors
    // sunsdr_build_header, sunsdr.c:2242-2255 [@f8b01d25c5], byte for
    // byte. Bytes 3, 8, 9, 11-17 are left at the zero the memset gave
    // them; byte 10 is the one fixed non-zero padding byte.
    QByteArray out(kCtlHeaderSize, char(0));
    auto* buf = reinterpret_cast<uchar*>(out.data());

    buf[0] = profile.magic0;
    buf[1] = kMagic1;
    buf[2] = opcode;
    buf[3] = 0x00;
    buf[4] = static_cast<uchar>(declaredPayloadLen & 0xFF);
    buf[5] = static_cast<uchar>((declaredPayloadLen >> 8) & 0xFF);
    buf[6] = static_cast<uchar>(sub & 0xFF);
    buf[7] = static_cast<uchar>((sub >> 8) & 0xFF);
    buf[8] = 0x00;
    buf[9] = 0x00;
    buf[10] = 0x01;
    // buf[11..17] already zero from the QByteArray's fill-construction.

    return out;
}

bool parseControlHeader(const quint8* data, int len, const Profile& profile,
                        ControlHeader* out)
{
    if (data == nullptr || len < kCtlHeaderSize || out == nullptr) {
        return false;
    }
    if (data[0] != profile.magic0 || data[1] != kMagic1) {
        return false;
    }

    out->magic0 = data[0];
    out->magic1 = data[1];
    out->opcode = data[2];
    out->declaredPayloadLen =
        static_cast<quint16>(data[4]) | (static_cast<quint16>(data[5]) << 8);
    out->sub =
        static_cast<quint16>(data[6]) | (static_cast<quint16>(data[7]) << 8);
    return true;
}

QByteArray buildIqHeader(const Profile& profile, quint8 opcode, quint16 seq,
                         quint8 byte8, quint8 byte9)
{
    // Mirrors sunsdr_build_iq_header, sunsdr.c:1678-1690 [@f8b01d25c5].
    QByteArray out(kIqHeaderSize, char(0));
    auto* buf = reinterpret_cast<uchar*>(out.data());

    buf[0] = profile.magic0;
    buf[1] = kMagic1;
    buf[2] = opcode;
    buf[3] = 0xFF;
    buf[4] = static_cast<uchar>(kIqPayloadSize & 0xFF);
    buf[5] = static_cast<uchar>((kIqPayloadSize >> 8) & 0xFF);
    buf[6] = static_cast<uchar>(seq & 0xFF);
    buf[7] = static_cast<uchar>((seq >> 8) & 0xFF);
    buf[8] = byte8;
    buf[9] = byte9;

    return out;
}

bool parseIqHeader(const quint8* data, int len, const Profile& profile,
                   IqHeader* out)
{
    if (data == nullptr || len < kIqHeaderSize || out == nullptr) {
        return false;
    }
    if (data[0] != profile.magic0 || data[1] != kMagic1) {
        return false;
    }

    out->magic0 = data[0];
    out->magic1 = data[1];
    out->opcode = data[2];
    out->payloadLen =
        static_cast<quint16>(data[4]) | (static_cast<quint16>(data[5]) << 8);
    out->seq =
        static_cast<quint16>(data[6]) | (static_cast<quint16>(data[7]) << 8);
    out->byte8 = data[8];
    out->byte9 = data[9];
    return true;
}

void decodeIqSamples(const quint8* payload, int payloadLen,
                     QVector<float>* outInterleaved)
{
    if (outInterleaved == nullptr) { return; }
    outInterleaved->clear();
    if (payload == nullptr || payloadLen < kIqPayloadSize) { return; }

    // ArtemisSDR's NORM, sunsdr.c:1306 [@f8b01d25c5]: 1.0 / 2147483648.0
    // (1 / 2^31). Placing a 24-bit sample into the top 3 bytes of a
    // 32-bit word scales it by 2^8, so dividing by 2^31 is
    // algebraically dividing the original 24-bit value by 2^23 — the
    // ordinary signed-24-bit-to-float normalization — but computed via
    // this specific shift so the C `int` cast sign-extends correctly.
    static constexpr double kNorm = 1.0 / 2147483648.0;

    outInterleaved->resize(kIqComplexPerPkt * 2);

    for (int i = 0; i < kIqComplexPerPkt; ++i) {
        const int k = i * kIqBytesPerComplex;

        // Wire order: payload[k+0..2] = Q, payload[k+3..5] = I
        // (sunsdr.c:4653-4668 [@f8b01d25c5]: "RX wire order on the
        // observed DX/PRO stream is Q first, I second").
        const std::int32_t iRaw =
            (static_cast<std::int32_t>(payload[k + 5]) << 24) |
            (static_cast<std::int32_t>(payload[k + 4]) << 16) |
            (static_cast<std::int32_t>(payload[k + 3]) << 8);
        const std::int32_t qRaw =
            (static_cast<std::int32_t>(payload[k + 2]) << 24) |
            (static_cast<std::int32_t>(payload[k + 1]) << 16) |
            (static_cast<std::int32_t>(payload[k + 0]) << 8);

        // Output order: I first, Q second — matches ArtemisSDR's own
        // deinterleaved rxBuf (sdr.rxBuf[2*i+0] = I, [2*i+1] = Q), NOT
        // the wire order above. Swapping this mirrors the sideband on
        // air (sunsdr.c:1711-1720 [@f8b01d25c5]).
        (*outInterleaved)[2 * i + 0] = static_cast<float>(iRaw * kNorm);
        (*outInterleaved)[2 * i + 1] = static_cast<float>(qRaw * kNorm);
    }
}

// From ArtemisSDR sunsdr.h:123 [@f8b01d25c5]: #define SUNSDR_FREQ_SCALE 10.
// See the header's comment: candidate only, not bench-confirmed.
namespace {
constexpr quint64 kFreqScaleCandidate = 10;
}

QByteArray encodeFrequencyPayloadCandidate(quint64 freqHz)
{
    const quint64 scaled = freqHz * kFreqScaleCandidate;
    QByteArray out(8, char(0));
    for (int i = 0; i < 8; ++i) {
        out[i] = static_cast<char>((scaled >> (8 * i)) & 0xFF);
    }
    return out;
}

quint64 decodeFrequencyPayloadCandidate(const QByteArray& payload)
{
    if (payload.size() < 8) { return 0; }
    quint64 scaled = 0;
    for (int i = 7; i >= 0; --i) {
        scaled = (scaled << 8) | static_cast<quint8>(payload[i]);
    }
    return scaled / kFreqScaleCandidate;
}

// ── TX control-channel opcodes: PURE ENCODERS ───────────────────────
//
// See SunSdrProtocol.h's own comment above these four declarations for
// the full citation and the "zero wire reachability" scope this Step 1
// sits behind.

QByteArray buildMoxFrame(const Profile& profile, bool on)
{
    QByteArray out = buildControlHeader(profile, kOpMoxPtt, /*sub=*/0,
                                        /*declaredPayloadLen=*/4);
    appendU32Le(out, on ? 1u : 0u);
    return out;
}

namespace {

// Confirmed-selector-byte table for opcode 0x15
// (buildAntennaSelectFrame()). A3's RX/TX split is stated directly in
// the design doc (docs/architecture/2026-08-24-sunsdr-native-driver-design.md:983:
// "RX A3 wire 0x03, TX A3 wire 0x02 (differs!)") — the exact trap this
// table exists to guard against: a copy-pasted `0x03`/`0x02` literal
// at more than one call site could silently diverge, where a table
// lookup that is wrong once is wrong everywhere the same way.
//
// A1/A2 are not stated in the design doc's own prose (its "Antenna
// model" section, lines 1090-1099, names all three physical ports but
// only spells out a byte for A3). Per the design doc's own closing
// instruction ("re-derive it by re-reading the same ArtemisSDR files
// rather than trusting a paraphrase, the same standard this project
// holds Thetis ports to"), and per CLAUDE.md's Reference Repositories
// entry for ArtemisSDR (the citable primary source for undocumented
// SunSDR wire facts), the values below are read directly from
// ArtemisSDR, not guessed:
//   - HPSDR/SunSdrAntenna.cs:10-30,81-95 — "HF RX selector: A3 -> 0x03.
//     HF TX selector: A3 -> 0x02"; A1/A2 RX=TX (no split).
//   - sunsdr.c:2278-2293 (`sunsdr_map_ant_selector` /
//     `sunsdr_map_tx_ant_selector`) — the same values in code form,
//     confirming the .cs comment isn't stale.
// Both sources agree independently, so these carry the same
// confirmation tier as the A3 rows below — "confirmed" here means
// "traced to a citable source", not "bench-verified against the real
// QRP in a live TX session", a distinction that applies equally to
// every row in this table (A3 included) until this driver's TX path
// is actually armed and tested on real hardware. See
// docs/architecture/2026-08-24-sunsdr-native-driver-design.md's own
// caveat: opcodes/sequences need bench confirmation against the actual
// QRP before any session-opening code may send them.
struct AntennaByteEntry {
    AntennaPort port;
    bool        forTx;
    bool        confirmed;
    quint8      byteValue;  // meaningful only when confirmed is true
};

constexpr std::array<AntennaByteEntry, 6> kAntennaByteTable{{
    // ArtemisSDR HPSDR/SunSdrAntenna.cs:10-30,81-95 + sunsdr.c:2278-2293 —
    // A1 has no RX/TX split (VHF-only port).
    {AntennaPort::A1, /*forTx=*/false, /*confirmed=*/true,  /*byteValue=*/0x01},
    {AntennaPort::A1, /*forTx=*/true,  /*confirmed=*/true,  /*byteValue=*/0x01},
    // Same source — A2 has no RX/TX split either.
    {AntennaPort::A2, /*forTx=*/false, /*confirmed=*/true,  /*byteValue=*/0x01},
    {AntennaPort::A2, /*forTx=*/true,  /*confirmed=*/true,  /*byteValue=*/0x01},
    // Design doc line 983 (RX side of the trap).
    {AntennaPort::A3, /*forTx=*/false, /*confirmed=*/true,  /*byteValue=*/0x03},
    // Design doc line 983 (TX side of the trap) — THE VALUE THAT
    // DIFFERS from the RX row above for the same physical port.
    {AntennaPort::A3, /*forTx=*/true,  /*confirmed=*/true,  /*byteValue=*/0x02},
}};

} // namespace

bool buildAntennaSelectFrame(const Profile& profile, AntennaPort port,
                              bool forTx, QByteArray* out)
{
    if (out == nullptr) { return false; }

    for (const AntennaByteEntry& entry : kAntennaByteTable) {
        if (entry.port != port || entry.forTx != forTx) {
            continue;
        }
        if (!entry.confirmed) {
            // No row in kAntennaByteTable is currently unconfirmed (A1/A2
            // were filled in from ArtemisSDR, see the table's own
            // comment) — this branch is defensive, not dead: it's what
            // protects any future row added without a real source
            // citation from silently sending a guessed byte instead of
            // refusing.
            return false;
        }
        QByteArray frame = buildControlHeader(profile, kOpAntennaSelect,
                                              /*sub=*/0,
                                              /*declaredPayloadLen=*/4);
        appendU32Le(frame, entry.byteValue);
        *out = frame;
        return true;
    }
    return false;
}

QByteArray buildDriveFrame(const Profile& profile, quint8 raw0to255)
{
    QByteArray out = buildControlHeader(profile, kOpDrive, /*sub=*/0,
                                        /*declaredPayloadLen=*/4);
    appendU32Le(out, static_cast<quint32>(raw0to255));
    return out;
}

QByteArray buildPaEnableFrame(const Profile& profile, bool enabled)
{
    QByteArray out = buildControlHeader(profile, kOpPaEnable, /*sub=*/0,
                                        /*declaredPayloadLen=*/4);
    appendU32Le(out, enabled ? 1u : 0u);
    return out;
}

} // namespace SunSdr
} // namespace Longpath
