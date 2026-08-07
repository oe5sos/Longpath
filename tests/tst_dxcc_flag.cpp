// The flag table's contract is mostly about what it does NOT do: an
// entity it does not know gets no flag, never a guessed one.
// no-port-check: NereusSDR-original.

#include <QtTest/QtTest>
#include "core/DxccFlag.h"

using namespace NereusSDR;

class TstDxccFlag : public QObject {
    Q_OBJECT
private slots:
    void known_prefixes_map_to_iso_codes();
    void unknown_prefixes_yield_nothing();
    void lookup_is_case_and_whitespace_tolerant();
    void emoji_is_a_regional_indicator_pair();
    void entities_sharing_a_flag_agree();
};

void TstDxccFlag::known_prefixes_map_to_iso_codes()
{
    QCOMPARE(dxccIsoCode(QStringLiteral("OE")), QStringLiteral("AT"));
    QCOMPARE(dxccIsoCode(QStringLiteral("DL")), QStringLiteral("DE"));
    QCOMPARE(dxccIsoCode(QStringLiteral("JA")), QStringLiteral("JP"));
    QCOMPARE(dxccIsoCode(QStringLiteral("VK")), QStringLiteral("AU"));
    QCOMPARE(dxccIsoCode(QStringLiteral("K")),  QStringLiteral("US"));
}

void TstDxccFlag::unknown_prefixes_yield_nothing()
{
    // A wrong flag beside a callsign is worse than a blank space, so an
    // unlisted entity must produce an empty string, not a fallback.
    QVERIFY(dxccIsoCode(QStringLiteral("1A")).isEmpty());   // SMOM, no flag
    QVERIFY(dxccIsoCode(QStringLiteral("ZZZ")).isEmpty());
    QVERIFY(dxccIsoCode(QString{}).isEmpty());
    QVERIFY(dxccFlagEmoji(QStringLiteral("1A")).isEmpty());
    QVERIFY(dxccFlagEmoji(QString{}).isEmpty());
}

void TstDxccFlag::lookup_is_case_and_whitespace_tolerant()
{
    QCOMPARE(dxccIsoCode(QStringLiteral("oe")),   QStringLiteral("AT"));
    QCOMPARE(dxccIsoCode(QStringLiteral(" OE ")), QStringLiteral("AT"));
}

void TstDxccFlag::emoji_is_a_regional_indicator_pair()
{
    const QString flag = dxccFlagEmoji(QStringLiteral("OE"));
    const QList<uint> cps = flag.toUcs4();
    QCOMPARE(cps.size(), 2);
    // A = U+1F1E6, so AT is U+1F1E6 U+1F1F9.
    QCOMPARE(cps.at(0), 0x1F1E6u);           // A
    QCOMPARE(cps.at(1), 0x1F1E6u + 19u);     // T
    // Two code points outside the BMP means four UTF-16 units.
    QCOMPARE(flag.size(), 4);
}

void TstDxccFlag::entities_sharing_a_flag_agree()
{
    // Alaska and Hawaii are separate DXCC entities under one flag; the
    // table must not invent distinct ones for them.
    QCOMPARE(dxccIsoCode(QStringLiteral("KL")),  QStringLiteral("US"));
    QCOMPARE(dxccIsoCode(QStringLiteral("KH6")), QStringLiteral("US"));
    // Scotland and Wales likewise.
    QCOMPARE(dxccIsoCode(QStringLiteral("GM")), QStringLiteral("GB"));
    QCOMPARE(dxccIsoCode(QStringLiteral("GW")), QStringLiteral("GB"));
}

QTEST_APPLESS_MAIN(TstDxccFlag)
#include "tst_dxcc_flag.moc"
