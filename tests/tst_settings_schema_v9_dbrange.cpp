// =================================================================
// tests/tst_settings_schema_v9_dbrange.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// SettingsSchemaVersion v8 -> v9: der dargestellte dB-Bereich wird von
// -48/-116 auf -30/-190 geweitet.
//
// ── Worum es hier wirklich geht ──────────────────────────────────────
//
// Nicht um die neuen Zahlen, sondern um die alten. OE5SOS, 2026-08-15:
// "Mein gespeicherter Wert -73/-138 soll beim Zusammenfuehren nicht
// stillschweigend ueberschrieben werden."
//
// Eine Migration, die eine selbst eingestellte Anzeige wegwirft, ist
// schlimmer als gar keine: der Betreiber sucht den Fehler dann im
// Programm statt in einer Zeile, die jemand geaendert hat. Deshalb steht
// der Fall mit dem selbstgesetzten Wert zuerst.
//
// Und: die per-Band-Schluessel (DisplayGridMax_20m) sind Bandgedaechtnis,
// dort steht nie ein Vorgabewert. Sie sehen den Pan-Schluesseln zum
// Verwechseln aehnlich (DisplayGridMax_1) und duerfen trotzdem nicht
// angefasst werden.
// =================================================================

#include <QtTest/QtTest>
#include "core/AppSettings.h"

using namespace Longpath;

namespace {
QString get(const QString& k)
{
    return AppSettings::instance().value(k, QString()).toString();
}
void put(const QString& k, const QString& v)
{
    AppSettings::instance().setValue(k, v);
}
} // namespace

class TestSettingsSchemaV9DbRange : public QObject {
    Q_OBJECT
private slots:

    void aRangeTheOperatorChoseSurvivesUntouched()
    {
        auto& s = AppSettings::instance();
        put(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("8"));
        // Genau der Fall vom 2026-08-15.
        put(QStringLiteral("DisplayGridMax"), QStringLiteral("-73"));
        put(QStringLiteral("DisplayGridMin"), QStringLiteral("-138"));

        s.ensureSettingsAtVersion(9);

        QCOMPARE(get(QStringLiteral("DisplayGridMax")), QStringLiteral("-73"));
        QCOMPARE(get(QStringLiteral("DisplayGridMin")), QStringLiteral("-138"));
    }

    void onlyTheOldDefaultIsPulledForward()
    {
        auto& s = AppSettings::instance();
        put(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("8"));
        put(QStringLiteral("DisplayGridMax"), QStringLiteral("-48"));
        put(QStringLiteral("DisplayGridMin"), QStringLiteral("-116"));

        s.ensureSettingsAtVersion(9);

        QCOMPARE(get(QStringLiteral("DisplayGridMax")).toDouble(), -30.0);
        QCOMPARE(get(QStringLiteral("DisplayGridMin")).toDouble(), -190.0);
    }

    void aSecondPanadapterIsPulledToo()
    {
        auto& s = AppSettings::instance();
        put(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("8"));
        put(QStringLiteral("DisplayGridMax_1"), QStringLiteral("-48"));
        put(QStringLiteral("DisplayGridMin_1"), QStringLiteral("-116"));

        s.ensureSettingsAtVersion(9);

        QCOMPARE(get(QStringLiteral("DisplayGridMax_1")).toDouble(), -30.0);
        QCOMPARE(get(QStringLiteral("DisplayGridMin_1")).toDouble(), -190.0);
    }

    void theBandMemoryIsNotAPanadapter()
    {
        auto& s = AppSettings::instance();
        put(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("8"));
        // Sieht aus wie ein Pan-Schluessel, ist aber Bandgedaechtnis.
        // Selbst wenn dort zufaellig der alte Vorgabewert steht, ist er
        // dort gemessen oder gewaehlt worden und nicht geerbt.
        put(QStringLiteral("DisplayGridMax_20m"), QStringLiteral("-48"));
        put(QStringLiteral("DisplayGridMin_20m"), QStringLiteral("-116"));

        s.ensureSettingsAtVersion(9);

        QCOMPARE(get(QStringLiteral("DisplayGridMax_20m")), QStringLiteral("-48"));
        QCOMPARE(get(QStringLiteral("DisplayGridMin_20m")), QStringLiteral("-116"));
    }

    void theVersionKeyMovesOn()
    {
        auto& s = AppSettings::instance();
        put(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("8"));
        s.ensureSettingsAtVersion(9);
        QCOMPARE(get(QStringLiteral("SettingsSchemaVersion")), QStringLiteral("9"));
    }

    void runningItTwiceChangesNothingMore()
    {
        auto& s = AppSettings::instance();
        put(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("8"));
        put(QStringLiteral("DisplayGridMax"), QStringLiteral("-48"));
        s.ensureSettingsAtVersion(9);
        const QString once = get(QStringLiteral("DisplayGridMax"));
        s.ensureSettingsAtVersion(9);
        QCOMPARE(get(QStringLiteral("DisplayGridMax")), once);
    }
};

QTEST_MAIN(TestSettingsSchemaV9DbRange)
#include "tst_settings_schema_v9_dbrange.moc"
