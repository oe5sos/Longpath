// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_flex_zweiton_tor.cpp  (Longpath)
// =================================================================
//
// Die IMD-Messung am Flex konnte gar nicht stattfinden, und zwar aus
// einem Grund, den man am Geraet nie gesehen haette:
//
//   `SpectrumWidget::drawImdOverlay` haengt an vier Toren — mox,
//   testIMD, showImd, duplex. `testIMD` kam bisher ausschliesslich
//   von `TwoToneController` (WDSP), `mox` von `MoxController`. Am
//   Flex rechnet die MASCHINE: beide Steuerwerke fahren dort leer
//   mit, also blieben beide Tore zu.
//
// Die Rechnung dahinter waere bereitgewesen: das Flex-Bild laeuft
// ueber `updateKiwiSpectrumDbm` nach `updateSpectrumLinear`, und dort
// rechnet `analyseImd()`.
//
// Dieser Prueflauf nagelt den Weg fest, den die Zustandszeile der
// Maschine nimmt — mit den Zeilen, die am 2026-11-01 an der
// FLEX-8400 mitgeschrieben wurden (Firmware 4.2.20.41343):
//
//   transmit tx_rf_power_changes_allowed=1 tune=0 tune_mode=single_tone …
//   transmit set tune_mode=two_tone      -> code 0, Zustand: tune_mode=two_tone
//   transmit set tune_mode=single_tone   -> code 0, Zustand: tune_mode=single_tone
// =================================================================

#include <QtTest/QtTest>

#include <QSignalSpy>

#include "core/FlexRadioConnection.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

/// Zerlegt eine Statuszeile wie `parseLine`: Objektgrenze vor dem
/// ersten '='.
void speise(RadioModel& rm, const QString& rumpf)
{
    const int firstEq = rumpf.indexOf(QLatin1Char('='));
    const int kvStart = rumpf.lastIndexOf(QLatin1Char(' '), firstEq);
    QMap<QString, QString> kvs;
    const QStringList paare =
        rumpf.mid(kvStart + 1).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& paar : paare) {
        const int gleich = paar.indexOf(QLatin1Char('='));
        if (gleich > 0) { kvs.insert(paar.left(gleich), paar.mid(gleich + 1)); }
    }
    rm.flexApdStatusForTest(rumpf.left(kvStart).trimmed(), kvs);
}

}  // namespace

class TstFlexZweitonTor : public QObject
{
    Q_OBJECT

private slots:
    // Der Befehl stellt den Modus — und tastet nicht.
    void derBefehlStelltNurDenModus()
    {
        QCOMPARE(FlexCommandWire::setTuneModeCommand(true),
                 QStringLiteral("transmit set tune_mode=two_tone"));
        QCOMPARE(FlexCommandWire::setTuneModeCommand(false),
                 QStringLiteral("transmit set tune_mode=single_tone"));

        // Was taesten wuerde, steht woanders und heisst anders.
        QCOMPARE(FlexCommandWire::tuneCommand(true),
                 QStringLiteral("transmit tune 1"));
    }

    // Beides muss stimmen: Modus UND Traeger. Ein Zweiton, der nicht
    // ausgesendet wird, ist keine Messgelegenheit — und ein
    // Abstimmtraeger mit EINEM Ton auch nicht.
    void nurBeidesZusammenOeffnetDasTor()
    {
        RadioModel rm;
        QVERIFY(!rm.flexZweitonAktiv());

        speise(rm, QStringLiteral("transmit tune=0 tune_mode=single_tone"));
        QVERIFY(!rm.flexZweitonAktiv());

        speise(rm, QStringLiteral("transmit tune=0 tune_mode=two_tone"));
        QVERIFY2(!rm.flexZweitonAktiv(), "Modus allein ist noch keine Sendung");

        speise(rm, QStringLiteral("transmit tune=1 tune_mode=single_tone"));
        QVERIFY2(!rm.flexZweitonAktiv(), "Ein Ton ist kein Zweiton");

        speise(rm, QStringLiteral("transmit tune=1 tune_mode=two_tone"));
        QVERIFY(rm.flexZweitonAktiv());
    }

    // Gemeldet wird nur, wenn sich das TOR aendert — nicht bei jeder
    // Zeile. Die Maschine schickt den Sendezustand mehrfach.
    void gemeldetWirdNurDieAenderung()
    {
        RadioModel rm;
        QSignalSpy spy(&rm, &RadioModel::flexZweitonGeaendert);

        speise(rm, QStringLiteral("transmit tune=1 tune_mode=two_tone"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), true);

        speise(rm, QStringLiteral("transmit tune=1 tune_mode=two_tone"));
        QCOMPARE(spy.count(), 0);

        speise(rm, QStringLiteral("transmit tune=0 tune_mode=two_tone"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), false);
    }

    // Die Falle, die schon bei `apd` zugeschlagen hat: die Maschine
    // schickt Teilmeldungen der Sendeebene. Was nicht dasteht, hat
    // sich nicht geaendert — sonst faellt der Traeger bei jeder
    // Lautstaerkemeldung aus dem Zustand.
    void dieTeilmeldungLoeschtDenZustandNicht()
    {
        RadioModel rm;
        speise(rm, QStringLiteral("transmit tune=1 tune_mode=two_tone"));
        QVERIFY(rm.flexZweitonAktiv());

        speise(rm, QStringLiteral("transmit dax=1"));
        QVERIFY2(rm.flexZweitonAktiv(), "die Teilmeldung hat den Traeger geloescht");

        speise(rm, QStringLiteral("transmit mon_gain_sb=80 mic_level=0"));
        QVERIFY(rm.flexZweitonAktiv());
    }

    // Die echte, vollstaendige Zeile der 8400 — so, wie sie beim
    // Anmelden kommt.
    void dieEchteZeileDerMaschine()
    {
        RadioModel rm;
        speise(rm, QStringLiteral(
            "transmit freq=7.133162 rfpower=0 tunepower=0 tx_slice_mode=LSB "
            "hwalc_enabled=0 inhibit=0 dax=0 sb_monitor=0 mon_gain_sb=80 "
            "mon_pan_sb=50 met_in_rx=0 am_carrier_level=100 mic_selection=MIC "
            "mic_level=0 lo=100 hi=3600 tx_antenna=INVALID"));
        QVERIFY2(!rm.flexZweitonAktiv(),
                 "diese Zeile traegt weder tune noch tune_mode");

        speise(rm, QStringLiteral(
            "transmit tx_rf_power_changes_allowed=1 tune=1 "
            "tune_mode=two_tone mon_available=1 max_power_level=100"));
        QVERIFY(rm.flexZweitonAktiv());
    }
};

QTEST_MAIN(TstFlexZweitonTor)
#include "tst_flex_zweiton_tor.moc"
