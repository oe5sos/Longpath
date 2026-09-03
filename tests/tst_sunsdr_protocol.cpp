// =================================================================
// tests/tst_sunsdr_protocol.cpp  (NereusSDR)
// =================================================================
//
// SunSDR2 wire framing — tested against REAL captured bytes, not just
// the encoder checked against itself.
//
// The control-header and IQ-header byte sequences below are quoted
// verbatim from docs/architecture/2026-08-24-sunsdr-native-driver-design.md
// "Confirmed from real QRP capture (2026-08-24)" — a genuine
// ExpertSDR2 <-> SunSDR2 QRP bench capture, not a synthetic fixture.
// This is the one place in this project's SunSDR work where something
// can be checked against REALITY without live hardware attached
// tonight: the bytes already came from reality once.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-25 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file. Fixture byte sequences
// are captured wire data (see file header), not ported code.

#include <QtTest>

#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>

#include "core/sunsdr/SunSdrProtocol.h"

using namespace Longpath::SunSdr;

namespace {

QByteArray hexBytes(const char* hex)
{
    return QByteArray::fromHex(QByteArray(hex).replace(' ', ""));
}

} // namespace

class TestSunSdrProtocol : public QObject
{
    Q_OBJECT

private slots:

    // ── Control header ───────────────────────────────────────────────

    // The exact H->R query-fixed (0x1a) packet from the real capture,
    // design doc "Confirmed: both header formats":
    //   03 ff 1a 00 04 00 00 00 00 00 01 00 00 00 00 00 00 00
    void buildControlHeaderMatchesRealCapturedBytes()
    {
        const QByteArray built =
            buildControlHeader(kProfileQrp, /*opcode=*/0x1a, /*sub=*/0,
                               /*declaredPayloadLen=*/4);
        const QByteArray expected =
            hexBytes("03 ff 1a 00 04 00 00 00 00 00 01 00 00 00 00 00 00 00");
        QCOMPARE(built.size(), kCtlHeaderSize);
        QCOMPARE(built, expected);
    }

    // The R->H reply to the same exchange (payload len 0 this time):
    //   03 ff 1a 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00
    void parseControlHeaderMatchesRealCapturedReply()
    {
        const QByteArray reply =
            hexBytes("03 ff 1a 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00");

        ControlHeader hdr;
        QVERIFY(parseControlHeader(
            reinterpret_cast<const quint8*>(reply.constData()),
            reply.size(), kProfileQrp, &hdr));

        QCOMPARE(hdr.magic0, quint8(0x03));
        QCOMPARE(hdr.opcode, quint8(0x1a));
        QCOMPARE(hdr.declaredPayloadLen, quint16(0));
        QCOMPARE(hdr.sub, quint16(0));
    }

    void buildThenParseRoundTrips()
    {
        const QByteArray built =
            buildControlHeader(kProfileDx, /*opcode=*/0x09, /*sub=*/1,
                               /*declaredPayloadLen=*/8);
        ControlHeader hdr;
        QVERIFY(parseControlHeader(
            reinterpret_cast<const quint8*>(built.constData()),
            built.size(), kProfileDx, &hdr));
        QCOMPARE(hdr.opcode, quint8(0x09));
        QCOMPARE(hdr.sub, quint16(1));
        QCOMPARE(hdr.declaredPayloadLen, quint16(8));
    }

    // A DX-magic packet handed to the QRP parser must be rejected, not
    // silently accepted with garbage fields — this is exactly the kind
    // of bug that would make a future multi-model implementation read
    // one radio's packets as another's.
    void wrongProfileMagicIsRejected()
    {
        const QByteArray dxPacket =
            buildControlHeader(kProfileDx, 0x09, 0, 8);
        ControlHeader hdr;
        QVERIFY2(!parseControlHeader(
            reinterpret_cast<const quint8*>(dxPacket.constData()),
            dxPacket.size(), kProfileQrp, &hdr),
            "a DX packet (magic0=0x32) must not parse as a QRP packet "
            "(magic0=0x03)");
    }

    void tooShortIsRejected()
    {
        const QByteArray built = buildControlHeader(kProfileQrp, 0x18, 0, 0);
        ControlHeader hdr;
        QVERIFY(!parseControlHeader(
            reinterpret_cast<const quint8*>(built.constData()),
            kCtlHeaderSize - 1, kProfileQrp, &hdr));
    }

    // ── IQ-stream header ─────────────────────────────────────────────

    // The exact RX-idle IQ-stream header from the real capture, design
    // doc "Confirmed: both header formats":
    //   03 ff fe ff b0 04 c4 e3 01 00
    // opcode 0xFE, payload size 0x04b0 = 1200, sequence 0xe3c4.
    void buildIqHeaderMatchesRealCapturedBytes()
    {
        const QByteArray built = buildIqHeader(
            kProfileQrp, kOpIqRxIdle, /*seq=*/0xe3c4, /*byte8=*/0x01,
            /*byte9=*/0x00);
        const QByteArray expected = hexBytes("03 ff fe ff b0 04 c4 e3 01 00");
        QCOMPARE(built.size(), kIqHeaderSize);
        QCOMPARE(built, expected);
    }

    void parseIqHeaderMatchesRealCapturedBytes()
    {
        const QByteArray real = hexBytes("03 ff fe ff b0 04 c4 e3 01 00");
        IqHeader hdr;
        QVERIFY(parseIqHeader(
            reinterpret_cast<const quint8*>(real.constData()),
            real.size(), kProfileQrp, &hdr));
        QCOMPARE(hdr.opcode, kOpIqRxIdle);
        QCOMPARE(hdr.payloadLen, quint16(kIqPayloadSize));
        QCOMPARE(hdr.seq, quint16(0xe3c4));
        QCOMPARE(hdr.byte8, quint8(0x01));
        QCOMPARE(hdr.byte9, quint8(0x00));
    }

    // Same discipline as wrongProfileMagicIsRejected() above, mirrored
    // for the IQ-stream header — found missing live, 2026-08-26: this
    // header's magic0/magic1 rejection had zero direct test coverage at
    // the protocol level (and, separately, the connection-level test
    // that claimed to cover it turned out to be vacuous — see
    // tst_sunsdr_radio_connection.cpp's wrongMagicPacketIsIgnored()).
    void wrongProfileMagicIsRejectedForIqHeader()
    {
        const QByteArray dxPacket =
            buildIqHeader(kProfileDx, kOpIqRxIdle, 0, 0, 0);
        IqHeader hdr;
        QVERIFY2(!parseIqHeader(
            reinterpret_cast<const quint8*>(dxPacket.constData()),
            dxPacket.size(), kProfileQrp, &hdr),
            "a DX IQ header (magic0=0x32) must not parse as a QRP header "
            "(magic0=0x03)");
    }

    void tooShortIqHeaderIsRejected()
    {
        const QByteArray built = buildIqHeader(kProfileQrp, kOpIqRxIdle, 0, 0, 0);
        IqHeader hdr;
        QVERIFY(!parseIqHeader(
            reinterpret_cast<const quint8*>(built.constData()),
            kIqHeaderSize - 1, kProfileQrp, &hdr));
    }

    // ── IQ sample decode ──────────────────────────────────────────────
    //
    // No full 1210-byte payload with real sample data is quoted in the
    // design doc (only the 10-byte header), so these build known
    // 24-bit patterns by hand and check the decode against hand-worked
    // arithmetic — same spirit as tst_wav_file.cpp's hand-built PCM16
    // fixtures, for the same reason: it exercises the decoder against
    // bytes the encoder didn't produce.

    void decodesFullScalePositiveAndNegative()
    {
        QByteArray payload(kIqPayloadSize, char(0));
        auto* buf = reinterpret_cast<uchar*>(payload.data());

        // Slot 0: Q = max positive 24-bit (0x7FFFFF, LE: FF FF 7F),
        //         I = max negative 24-bit (0x800000, LE: 00 00 80).
        buf[0] = 0xFF; buf[1] = 0xFF; buf[2] = 0x7F;   // Q
        buf[3] = 0x00; buf[4] = 0x00; buf[5] = 0x80;   // I

        QVector<float> out;
        decodeIqSamples(reinterpret_cast<const quint8*>(payload.constData()),
                        payload.size(), &out);

        QCOMPARE(out.size(), kIqComplexPerPkt * 2);
        // I (index 0) is the full-negative sample: exactly -1.0.
        QCOMPARE(out[0], -1.0f);
        // Q (index 1) is one LSB short of full-positive: right at the
        // edge, not clipped, not wrapped.
        QVERIFY2(out[1] > 0.9999f && out[1] < 1.0f,
                 "max-positive 24-bit sample should sit just under +1.0");
    }

    void decodesZeroAsZero()
    {
        QByteArray payload(kIqPayloadSize, char(0));
        QVector<float> out;
        decodeIqSamples(reinterpret_cast<const quint8*>(payload.constData()),
                        payload.size(), &out);
        QCOMPARE(out.size(), kIqComplexPerPkt * 2);
        for (float v : out) {
            QCOMPARE(v, 0.0f);
        }
    }

    // The wire order is Q-first/I-second per slot; the DECODED order is
    // I-first/Q-second. Swapping this mirrors the sideband on air
    // (design doc + ArtemisSDR sunsdr.c:1711-1720) — this test exists
    // specifically to catch that swap.
    void iAndQLandInTheCorrectOutputSlots()
    {
        QByteArray payload(kIqPayloadSize, char(0));
        auto* buf = reinterpret_cast<uchar*>(payload.data());

        // A small, unambiguous positive value on Q only (wire bytes
        // 0-2), nothing on I (wire bytes 3-5).
        buf[0] = 0x00; buf[1] = 0x00; buf[2] = 0x10;   // Q = 0x100000 region

        QVector<float> out;
        decodeIqSamples(reinterpret_cast<const quint8*>(payload.constData()),
                        payload.size(), &out);

        QCOMPARE(out[0], 0.0f);       // I (index 0): untouched wire bytes
        QVERIFY(out[1] > 0.0f);       // Q (index 1): carries the value
    }

    void tooShortPayloadYieldsEmptyOutput()
    {
        QByteArray payload(kIqPayloadSize - 1, char(0));
        QVector<float> out;
        decodeIqSamples(reinterpret_cast<const quint8*>(payload.constData()),
                        payload.size(), &out);
        QVERIFY2(out.isEmpty(),
                 "a truncated payload must not produce partial/garbage samples");
    }

    // ── Profile table ─────────────────────────────────────────────────

    void profilesMatchArtemisSdrAndTheConfirmedQrpCapture()
    {
        QCOMPARE(kProfileDx.magic0, quint8(0x32));
        QCOMPARE(kProfileDx.defaultCtrlPort, quint16(50001));
        QCOMPARE(kProfileDx.defaultStreamPort, quint16(50002));

        QCOMPARE(kProfilePro.magic0, quint8(0x01));
        QCOMPARE(kProfilePro.defaultCtrlPort, quint16(50002));
        QCOMPARE(kProfilePro.defaultStreamPort, quint16(50003));

        // The QRP row: confirmed against the real bench capture, NOT
        // just an ArtemisSDR source fact (ArtemisSDR has no QRP row at
        // all — see design doc "The one gate that comes before any code").
        QCOMPARE(kProfileQrp.magic0, quint8(0x03));
        QCOMPARE(kProfileQrp.defaultCtrlPort, quint16(50001));
        QCOMPARE(kProfileQrp.defaultStreamPort, quint16(50002));
    }

    // ── Candidate frequency-payload encoding (UNCONFIRMED) ─────────────
    //
    // Design doc, "candidate frequency-encoding formula found"
    // (2026-08-27). These tests pin the MATH this project derived from
    // ArtemisSDR's real sunsdr_send_freq_pkt() — they do NOT claim the
    // formula itself is bench-confirmed for the QRP. See
    // SunSdrProtocol.h's comment on these functions and the design doc
    // section for the full derivation and what would actually confirm
    // or kill this hypothesis.

    void encodeFrequencyPayloadCandidateMatchesArtemisSdrScaleFormula()
    {
        // 14,213,950 Hz * 10 = 142,139,500 as an 8-byte LE integer.
        const QByteArray encoded = encodeFrequencyPayloadCandidate(14213950);
        QCOMPARE(encoded.size(), 8);
        QCOMPARE(encoded, hexBytes("6c e0 78 08 00 00 00 00"));
    }

    void decodeFrequencyPayloadCandidateIsTheExactInverseOfEncode()
    {
        for (quint64 freq : {1'800'000ULL, 7'074'000ULL, 14'074'000ULL,
                              21'074'000ULL, 50'313'000ULL}) {
            const QByteArray encoded = encodeFrequencyPayloadCandidate(freq);
            QCOMPARE(decodeFrequencyPayloadCandidate(encoded), freq);
        }
    }

    void decodeFrequencyPayloadCandidateTooShortReturnsZero()
    {
        QCOMPARE(decodeFrequencyPayloadCandidate(QByteArray()), quint64(0));
        QCOMPARE(decodeFrequencyPayloadCandidate(hexBytes("6c e0 78 08 00 00 00")),
                  quint64(0));
    }

    // The one exact real capture this project has in code
    // (SunSdrRadioConnection::replayedFrequencyFrameForTest(), design
    // doc "the exact frame in code"). This is the whole basis for the
    // 2026-08-27 hypothesis — decoding it is expected to land at
    // 14,213,950 Hz (14.213950 MHz, inside the 20m amateur band) if the
    // candidate formula is right. This test does NOT prove the formula
    // correct (no independent ground truth exists yet — see the design
    // doc), it only pins that this project's own code reproduces its
    // own derivation consistently, so a future refactor can't silently
    // drift from the value the design doc's analysis actually used.
    void decodesTheOneRealCapturedFrequencyFrameToTheDerivedCandidateValue()
    {
        const QByteArray fullFrame = hexBytes(
            "03 ff 08 00 08 00 00 00 00 00 01 00 00 00 8c a3 1d d7 "
            "6c e0 78 08 00 00 00 00");
        QCOMPARE(fullFrame.size(), 26);
        // Payload starts at the formal 18-byte header boundary — NOT at
        // the design doc's earlier informally-eyeballed offset (see
        // that section's "real byte-alignment bug in this document's
        // own earlier notes" for why the two differ by 4 bytes).
        const QByteArray payload = fullFrame.mid(18, 8);
        QCOMPARE(payload, hexBytes("6c e0 78 08 00 00 00 00"));
        QCOMPARE(decodeFrequencyPayloadCandidate(payload), quint64(14213950));
    }

    // ── TX control-channel pure encoders (Step 1 of 6) ──────────────────
    //
    // All four share the same header shape: buildControlHeader() with
    // sub=0, declaredPayloadLen=4, followed by a 4-byte little-endian
    // u32 payload — directly confirmed by ArtemisSDR's
    // sunsdr_send_u32_cmd() (sunsdr.c:2391-2403 [@f8b01d25c5]; see
    // SunSdrProtocol.h's comment on these functions). Every expected
    // byte string below is that fixed 18-byte QRP-profile header
    // prefix (magic0=0x03, magic1=0xff, opcode, sub=0, len=4) plus the
    // 4-byte payload — not just "non-empty" or "starts with the right
    // opcode".

    // ── MOX/PTT (opcode 0x06) ────────────────────────────────────────

    void buildMoxFrameEncodesOnAsOne()
    {
        const QByteArray built = buildMoxFrame(kProfileQrp, /*on=*/true);
        const QByteArray expected = hexBytes(
            "03 ff 06 00 04 00 00 00 00 00 01 00 00 00 00 00 00 00 "
            "01 00 00 00");
        QCOMPARE(built, expected);
    }

    void buildMoxFrameEncodesOffAsZero()
    {
        const QByteArray built = buildMoxFrame(kProfileQrp, /*on=*/false);
        const QByteArray expected = hexBytes(
            "03 ff 06 00 04 00 00 00 00 00 01 00 00 00 00 00 00 00 "
            "00 00 00 00");
        QCOMPARE(built, expected);
    }

    // ── Antenna select (opcode 0x15) ─────────────────────────────────
    //
    // THE TRAP: antenna A3 is one physical port and one opcode, but
    // the selector byte differs by direction (design doc line 983 —
    // "RX A3 wire 0x03, TX A3 wire 0x02 (differs!)"). These two tests
    // exist specifically to catch a future edit that collapses the
    // lookup table back into a single shared literal.

    void antennaA3SelectorByteIs0x03OnRx()
    {
        QByteArray built;
        QVERIFY(buildAntennaSelectFrame(kProfileQrp, AntennaPort::A3,
                                        /*forTx=*/false, &built));
        const QByteArray expected = hexBytes(
            "03 ff 15 00 04 00 00 00 00 00 01 00 00 00 00 00 00 00 "
            "03 00 00 00");
        QCOMPARE(built, expected);
    }

    void antennaA3SelectorByteIs0x02OnTxNotTheSameAsRx()
    {
        QByteArray built;
        QVERIFY(buildAntennaSelectFrame(kProfileQrp, AntennaPort::A3,
                                        /*forTx=*/true, &built));
        const QByteArray expected = hexBytes(
            "03 ff 15 00 04 00 00 00 00 00 01 00 00 00 00 00 00 00 "
            "02 00 00 00");
        QCOMPARE(built, expected);

        // Restated as a direct byte comparison, independent of the
        // full-frame QCOMPARE above, so this specific fact survives
        // even if the header shape ever changes: A3's RX byte (0x03)
        // and TX byte (0x02) at the same payload offset must differ.
        QByteArray rxBuilt;
        QVERIFY(buildAntennaSelectFrame(kProfileQrp, AntennaPort::A3,
                                        /*forTx=*/false, &rxBuilt));
        QVERIFY2(built.at(18) != rxBuilt.at(18),
                 "antenna A3's TX selector byte must differ from its RX "
                 "selector byte -- this is the one-byte trap the lookup "
                 "table exists to guard");
    }

    // A1/A2 selector bytes are not stated in the design doc's own prose
    // (only A3's are) -- sourced directly from ArtemisSDR instead
    // (HPSDR/SunSdrAntenna.cs:10-30,81-95 + sunsdr.c:2278-2293, both
    // agreeing independently): RX=TX=0x01 for both ports, no
    // direction-dependent split (that trap is A3-only). See
    // kAntennaByteTable's comment in SunSdrProtocol.cpp.
    void antennaA1AndA2AreBothConfirmedAsByte0x01WithNoRxTxSplit()
    {
        QByteArray a1Rx, a1Tx, a2Rx, a2Tx;
        QVERIFY(buildAntennaSelectFrame(kProfileQrp, AntennaPort::A1,
                                        /*forTx=*/false, &a1Rx));
        QVERIFY(buildAntennaSelectFrame(kProfileQrp, AntennaPort::A1,
                                        /*forTx=*/true, &a1Tx));
        QVERIFY(buildAntennaSelectFrame(kProfileQrp, AntennaPort::A2,
                                        /*forTx=*/false, &a2Rx));
        QVERIFY(buildAntennaSelectFrame(kProfileQrp, AntennaPort::A2,
                                        /*forTx=*/true, &a2Tx));

        const QByteArray expected = hexBytes(
            "03 ff 15 00 04 00 00 00 00 00 01 00 00 00 00 00 00 00 "
            "01 00 00 00");
        QCOMPARE(a1Rx, expected);
        QCOMPARE(a1Tx, expected);
        QCOMPARE(a2Rx, expected);
        QCOMPARE(a2Tx, expected);
    }

    // ── Drive byte (opcode 0x17) ─────────────────────────────────────

    void buildDriveFrameIsABarePassthrough()
    {
        QCOMPARE(buildDriveFrame(kProfileQrp, 0x00),
                 hexBytes("03 ff 17 00 04 00 00 00 00 00 01 00 00 00 "
                           "00 00 00 00 00 00 00 00"));
        QCOMPARE(buildDriveFrame(kProfileQrp, 0x80),
                 hexBytes("03 ff 17 00 04 00 00 00 00 00 01 00 00 00 "
                           "00 00 00 00 80 00 00 00"));
        QCOMPARE(buildDriveFrame(kProfileQrp, 0xFF),
                 hexBytes("03 ff 17 00 04 00 00 00 00 00 01 00 00 00 "
                           "00 00 00 00 ff 00 00 00"));
    }

    // Step 1 scope guard: this byte must never reach a real caller
    // without a QRP-specific bench power-calibration table, which does
    // not exist yet (design doc "TX drive / power scaling" -- the only
    // known table is DX/PRO hardware, 40 m-only, "very likely wrong
    // for a QRP"). Grep-scans src/ (production code only, not this
    // test file) for any call to buildDriveFrame(...) outside its own
    // declaration/definition in SunSdrProtocol.h/.cpp. Same grep-scan
    // idea as tst_popup_style_coverage.cpp's kPopupMenu invariant,
    // enforcing an absence instead of a presence.
    void buildDriveFrameHasNoProductionCallSites()
    {
        const QString root =
            QString::fromLatin1(NEREUS_SOURCE_ROOT) + QStringLiteral("/src");
        QDirIterator it(root,
                        QStringList{QStringLiteral("*.cpp"), QStringLiteral("*.h"),
                                     QStringLiteral("*.cc"), QStringLiteral("*.hpp"),
                                     QStringLiteral("*.mm")},
                        QDir::Files, QDirIterator::Subdirectories);

        const QRegularExpression callSite(
            QStringLiteral(R"(\bbuildDriveFrame\s*\()"));
        QStringList offenders;

        while (it.hasNext()) {
            const QString path = it.next();
            if (path.endsWith(QStringLiteral(
                    "src/core/sunsdr/SunSdrProtocol.h")) ||
                path.endsWith(QStringLiteral(
                    "src/core/sunsdr/SunSdrProtocol.cpp"))) {
                continue;  // the function's own declaration/definition
            }
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            const QString src = QString::fromUtf8(f.readAll());
            if (callSite.match(src).hasMatch()) {
                offenders << path;
            }
        }

        QVERIFY2(offenders.isEmpty(),
                 qPrintable(QStringLiteral(
                     "buildDriveFrame() must have zero production call "
                     "sites in Step 1 (no QRP bench power-calibration "
                     "table exists yet) -- found references in: ") +
                     offenders.join(QStringLiteral(", "))));
    }

    // ── PA enable (opcode 0x24) ──────────────────────────────────────

    void buildPaEnableFrameEncodesEnabledAsOne()
    {
        const QByteArray built = buildPaEnableFrame(kProfileQrp, /*enabled=*/true);
        const QByteArray expected = hexBytes(
            "03 ff 24 00 04 00 00 00 00 00 01 00 00 00 00 00 00 00 "
            "01 00 00 00");
        QCOMPARE(built, expected);
    }

    void buildPaEnableFrameEncodesDisabledAsZero()
    {
        const QByteArray built = buildPaEnableFrame(kProfileQrp, /*enabled=*/false);
        const QByteArray expected = hexBytes(
            "03 ff 24 00 04 00 00 00 00 00 01 00 00 00 00 00 00 00 "
            "00 00 00 00");
        QCOMPARE(built, expected);
    }
};

QTEST_MAIN(TestSunSdrProtocol)
#include "tst_sunsdr_protocol.moc"
