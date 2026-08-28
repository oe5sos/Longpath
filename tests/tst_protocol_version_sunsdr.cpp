// no-port-check: NereusSDR/Longpath-original test file.

// =================================================================
// tests/tst_protocol_version_sunsdr.cpp  (NereusSDR/Longpath)
// =================================================================
//
// Plan doc task A.1: docs/architecture/2026-08-26-sunsdr-connection-plan.md
// §2 Phase A — "enum round-trips through the one exhaustive switch and
// through AppSettings persistence without crashing". The exhaustive-switch
// half is covered by tst_sunsdr_radio_connection.cpp and
// tst_radio_connection_factory_sunsdr.cpp (a missing case there is a
// compile error, not a runtime bug this file could catch); what remains
// untested is the AppSettings round-trip — does a RadioInfo carrying
// ProtocolVersion::SunSdr survive save/load intact, the same guarantee
// tst_hl2_n2adr_persistence.cpp already proves for board-type persistence.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"

using namespace Longpath;

class TestProtocolVersionSunSdr : public QObject
{
    Q_OBJECT

private slots:

    // ProtocolVersion::SunSdr must not collide with Protocol1/Protocol2 —
    // the value the plan doc's §0.1 decision (a distinct third
    // RadioConnection subclass) actually depends on.
    void sunSdrIsDistinctFromP1AndP2()
    {
        QCOMPARE(static_cast<int>(ProtocolVersion::SunSdr), 3);
        QVERIFY(ProtocolVersion::SunSdr != ProtocolVersion::Protocol1);
        QVERIFY(ProtocolVersion::SunSdr != ProtocolVersion::Protocol2);
    }

    // AppSettings::saveRadio / savedRadios round-trip, same pattern as
    // tst_hl2_n2adr_persistence.cpp's saveTestRadio() helper.
    void protocolSurvivesAppSettingsRoundTrip()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        AppSettings s(tmp.filePath(QStringLiteral("NereusSDR.settings")));

        RadioInfo info;
        info.macAddress = QStringLiteral("MANUAL:192.0.2.1:50001");
        info.address    = QHostAddress(QStringLiteral("192.0.2.1"));
        info.port       = 50001;
        info.boardType  = HPSDRHW::SunSdr2Qrp;
        info.protocol   = ProtocolVersion::SunSdr;
        info.name       = QStringLiteral("Test SunSDR2 QRP");

        s.saveRadio(info, /*pinToMac=*/false);

        const QList<SavedRadio> radios = s.savedRadios();
        QCOMPARE(radios.size(), 1);
        QCOMPARE(radios.first().info.protocol, ProtocolVersion::SunSdr);
        QCOMPARE(radios.first().info.boardType, HPSDRHW::SunSdr2Qrp);
    }

    // A second AppSettings instance pointed at the same file must read
    // back the same value — proves this went through the XML serializer,
    // not just an in-memory cache. saveRadio() only touches the in-memory
    // map; an explicit save()/load() pair is required to actually cross
    // disk, same pattern as
    // tst_app_settings_arbitrary_key_persistence.cpp's two-cycle test.
    void protocolSurvivesReloadFromDisk()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        {
            AppSettings writer(path);
            RadioInfo info;
            info.macAddress = QStringLiteral("MANUAL:192.0.2.2:50001");
            info.address    = QHostAddress(QStringLiteral("192.0.2.2"));
            info.port       = 50001;
            info.boardType  = HPSDRHW::SunSdr2Qrp;
            info.protocol   = ProtocolVersion::SunSdr;
            info.name       = QStringLiteral("Test SunSDR2 QRP 2");
            writer.saveRadio(info, false);
            writer.save();
        }

        AppSettings reader(path);
        reader.load();
        const QList<SavedRadio> radios = reader.savedRadios();
        QCOMPARE(radios.size(), 1);
        QCOMPARE(radios.first().info.protocol, ProtocolVersion::SunSdr);
    }
};

QTEST_MAIN(TestProtocolVersionSunSdr)
#include "tst_protocol_version_sunsdr.moc"
