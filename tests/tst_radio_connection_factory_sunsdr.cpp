// no-port-check: NereusSDR/Longpath-original test file.

// =================================================================
// tests/tst_radio_connection_factory_sunsdr.cpp  (NereusSDR/Longpath)
// =================================================================
//
// Plan doc task A.3: docs/architecture/2026-08-26-sunsdr-connection-plan.md
// §2 Phase A — "a RadioInfo{protocol=SunSdr} produces a non-null
// SunSdrRadioConnection*". RadioConnection::create()'s switch
// (RadioConnection.cpp:22-33) has no default: arm, so a missing case is a
// compile error, not a runtime bug — but a wrong case (e.g. SunSdr
// accidentally falling into the Protocol2 branch during a future edit)
// would compile fine and only be caught here. No existing test exercises
// this factory at all for any protocol, so this file also covers P1/P2 as
// a baseline, not just the new SunSdr case in isolation.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>

#include "core/RadioConnection.h"
#include "core/SunSdrRadioConnection.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"

using namespace Longpath;

namespace {
RadioInfo makeInfo(ProtocolVersion proto, HPSDRHW board)
{
    RadioInfo info;
    info.protocol   = proto;
    info.boardType  = board;
    info.address    = QHostAddress(QStringLiteral("192.0.2.1"));
    info.port       = 50001;
    info.macAddress = QStringLiteral("00:00:00:00:00:00");
    return info;
}
}  // namespace

class TestRadioConnectionFactorySunSdr : public QObject
{
    Q_OBJECT

private slots:

    void sunSdrProtocolProducesSunSdrRadioConnection()
    {
        const RadioInfo info = makeInfo(ProtocolVersion::SunSdr, HPSDRHW::SunSdr2Qrp);
        std::unique_ptr<RadioConnection> conn = RadioConnection::create(info);

        QVERIFY(conn != nullptr);
        QCOMPARE(conn->protocolVersion(), 3);

        auto* sunSdr = dynamic_cast<SunSdrRadioConnection*>(conn.get());
        QVERIFY2(sunSdr != nullptr,
                 "RadioInfo{protocol=SunSdr} must produce a SunSdrRadioConnection, "
                 "not some other RadioConnection subclass");
    }

    // Baseline: the same factory must still route P1/P2 correctly. No
    // existing test covered create() at all before this file.
    void protocol1ProducesP1RadioConnection()
    {
        const RadioInfo info = makeInfo(ProtocolVersion::Protocol1, HPSDRHW::Hermes);
        std::unique_ptr<RadioConnection> conn = RadioConnection::create(info);

        QVERIFY(conn != nullptr);
        QVERIFY(dynamic_cast<P1RadioConnection*>(conn.get()) != nullptr);
        QVERIFY(dynamic_cast<SunSdrRadioConnection*>(conn.get()) == nullptr);
    }

    void protocol2ProducesP2RadioConnection()
    {
        const RadioInfo info = makeInfo(ProtocolVersion::Protocol2, HPSDRHW::Orion);
        std::unique_ptr<RadioConnection> conn = RadioConnection::create(info);

        QVERIFY(conn != nullptr);
        QVERIFY(dynamic_cast<P2RadioConnection*>(conn.get()) != nullptr);
        QVERIFY(dynamic_cast<SunSdrRadioConnection*>(conn.get()) == nullptr);
    }
};

QTEST_MAIN(TestRadioConnectionFactorySunSdr)
#include "tst_radio_connection_factory_sunsdr.moc"
