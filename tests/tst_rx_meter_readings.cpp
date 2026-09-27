// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original regression test (formula from Thetis,
// cited in core/PassbandSnr.cpp).
//
// RX-Messwerte (2026-09-27): PB SNR hatte eine Messgroesse, aber nichts
// rechnete einen Wert; die Zahl unter dem S-Meter schob die Zeile, und
// die Monospace-Schrift fiel ausserhalb des Macs auf eine proportionale
// zurueck.

#include <QtTest>
#include <QLabel>

#include "core/PassbandSnr.h"
#include "gui/StyleConstants.h"
#include "gui/instruments/InstrumentFooter.h"

using namespace Longpath;

class TstRxMeterReadings : public QObject {
    Q_OBJECT
private slots:
    // Thetis console.cs:21127-21148 von Hand nachgerechnet.
    void passbandSnrFollowsThetis()
    {
        // 48 kHz, 4096 Punkte, ENB 2.0: Bin 11.72 Hz, RBW 23.44 Hz.
        // Rauschflur -120 dBm je Bin -> NPSD -133.70 dBm/Hz;
        // 2700 Hz Durchlass -> PBNP -99.39 dBm; Signal -73 -> SNR 26.39 dB.
        const PassbandSnrResult r = passbandSnr(-73.0, -120.0, 48000.0, 4096, 2.0,
                                                300, 3000, 0.0, false);
        QCOMPARE(r.passbandBandwidth, 2700);
        QVERIFY(std::abs(r.binWidth - 11.71875) < 1e-9);
        QVERIFY(std::abs(r.rbw - 23.4375) < 1e-9);
        QVERIFY(std::abs(r.noiseFloorPowerSpectralDensity - (-133.6995)) < 1e-3);
        QVERIFY(std::abs(r.estimatedPassbandNoisePower - (-99.3861)) < 1e-3);
        QVERIFY(std::abs(r.estimatedSnr - 26.3861) < 1e-3);
        // Mit Verschiebung.
        QVERIFY(std::abs(passbandSnr(-73.0, -120.0, 48000.0, 4096, 2.0, 300, 3000, 3.0, false)
                             .estimatedSnr - 29.3861) < 1e-3);
        // LSB: Kanten negativ, Breite gleich.
        QCOMPARE(passbandSnr(-73.0, -120.0, 48000.0, 4096, 2.0, -3000, -300, 0.0, false)
                     .passbandBandwidth, 2700);
    }

    void whileTransmittingItIsMinus999()
    {
        QCOMPARE(passbandSnr(-73.0, -120.0, 48000.0, 4096, 2.0, 300, 3000, 0.0, true)
                     .estimatedSnr, -999.0);
    }

    void nonsenseInputsGiveZeroNotNaN()
    {
        QCOMPARE(passbandSnr(-73.0, -120.0, 48000.0, 0, 2.0, 300, 3000, 0.0, false).estimatedSnr, 0.0);
        QCOMPARE(passbandSnr(-73.0, -120.0, 48000.0, 4096, 2.0, 3000, 300, 0.0, false).estimatedSnr, 0.0);
    }

    void monoFontIsFixedPitchEverywhere()
    {
        const QFont f = Style::monoFont(QFont(), Style::kFontSmall);
        QVERIFY(f.fixedPitch());
        QCOMPARE(f.styleHint(), QFont::TypeWriter);
    }

    // Die Zahl unter dem S-Meter reserviert ihre groesste Breite.
    void theReadoutDoesNotShrinkAndJump()
    {
        InstrumentFooter footer;
        footer.setValueText(QStringLiteral("S9+20"));
        const int wide = footer.valueMinimumWidthForTest();
        QVERIFY(wide > 0);
        footer.setValueText(QStringLiteral("S7"));
        QCOMPARE(footer.valueMinimumWidthForTest(), wide);
        footer.setValueText(QStringLiteral("-105 dBm"));
        QVERIFY(footer.valueMinimumWidthForTest() >= wide);
    }
};

QTEST_MAIN(TstRxMeterReadings)
#include "tst_rx_meter_readings.moc"
