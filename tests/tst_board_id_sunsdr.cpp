// no-port-check: NereusSDR/Longpath-original test file.

// =================================================================
// tests/tst_board_id_sunsdr.cpp  (NereusSDR/Longpath)
// =================================================================
//
// Plan doc task A.2: docs/architecture/2026-08-26-sunsdr-connection-plan.md
// §2 Phase A — "new id is distinct, doesn't collide with reserved slots".
// HpsdrModel.h:141 documents 14..19 as reserved for future NereusSDR-
// original SKU slots and HermesC10=20 was deliberately relocated off of
// 20 on 2026-05-21 to keep clear of the Thetis wire range (0-11) — see
// that file's own comments. SunSdr2Qrp=13 sits below the reserved gap by
// design (it was the next free NereusSDR-original slot after
// HermesLiteRxOnly=12, not inside 14..19). This file pins that fact so a
// future SKU addition into 14..19 can't silently collide with it, and
// confirms the value also reaches BoardCapsTable::forBoard() correctly
// rather than falling through to the kUnknown fallback row.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>

#include "core/HpsdrModel.h"
#include "core/BoardCapabilities.h"
#include "core/RadioDiscovery.h"

using namespace Longpath;

class TestBoardIdSunSdr : public QObject
{
    Q_OBJECT

private slots:

    void sunSdr2QrpIsThirteenAndDistinctFromEveryOtherBoard()
    {
        QCOMPARE(static_cast<int>(HPSDRHW::SunSdr2Qrp), 13);

        const HPSDRHW others[] = {
            HPSDRHW::Atlas, HPSDRHW::Hermes, HPSDRHW::HermesII,
            HPSDRHW::Angelia, HPSDRHW::Orion, HPSDRHW::OrionMKII,
            HPSDRHW::HermesLite, HPSDRHW::Saturn, HPSDRHW::SaturnMKII,
            HPSDRHW::HermesLiteRxOnly, HPSDRHW::HermesC10,
            HPSDRHW::Andromeda, HPSDRHW::Unknown,
        };
        for (HPSDRHW other : others) {
            QVERIFY2(HPSDRHW::SunSdr2Qrp != other,
                     "SunSdr2Qrp must not collide with any existing board id");
        }
    }

    // The 14..19 reserved-slot comment (HpsdrModel.h:141) must stay a real
    // gap, not something SunSdr2Qrp silently ate into.
    void sunSdr2QrpSitsBelowTheReservedGap()
    {
        QVERIFY(static_cast<int>(HPSDRHW::SunSdr2Qrp) < 14);
    }

    void boardCodeNameIsSet()
    {
        QCOMPARE(QString(boardCodeName(HPSDRHW::SunSdr2Qrp)),
                 QStringLiteral("SunSDR2 QRP"));
    }

    // BoardCapsTable::forBoard() must resolve to the real row, not the
    // kUnknown fallback every other unrecognized id silently returns.
    void boardCapsTableResolvesTheRealRow()
    {
        const BoardCapabilities& caps = BoardCapsTable::forBoard(HPSDRHW::SunSdr2Qrp);
        QCOMPARE(caps.board, HPSDRHW::SunSdr2Qrp);
        QCOMPARE(caps.protocol, ProtocolVersion::SunSdr);
        QVERIFY(caps.board != HPSDRHW::Unknown);
    }
};

QTEST_MAIN(TestBoardIdSunSdr)
#include "tst_board_id_sunsdr.moc"
