// no-port-check: Longpath-eigener Prüfstand. Die Zitate nennen die Quelle
// der Formel; es ist keine Thetis-Logik portiert.
// =================================================================
// tests/tst_fm_squelch_abbildung.cpp  (Longpath)
// =================================================================
//
// Die Abbildung Reglerstellung -> WDSP-Schwelle der FM-Rauschsperre.
//
// From Thetis console.cs:47295-47304 [v2.10.3.15]:
//   //[2.10.3.5]MW0LGE convert to a 0-100 scale from a -160 to 0 scale
//   nValue = (int)(((ptbSquelch.Value + 160) / 160f) * 100f);
//   FMSquelchThreshold = (float)Math.Pow(10.0, -2.0 * nValue / 100.0);
//
// Zwei Dinge sichert dieser Lauf ab:
//  1. die Formel selbst, an ihren Enden und in der Mitte;
//  2. dass die Zahl, die bei WDSP ankommt, fuer jede Reglerstellung des
//     RX-Applets DIESELBE ist wie vor dem Umbau vom 2026-10-21. Vorher
//     rechnete das Applet dB = -0,4*n gegen eine kaputte Umrechnung in
//     RxChannel an (linear = 10^(dB/20)); zusammen kam Thetis' Wert
//     heraus, einzeln ergab keines von beiden Sinn. Wer eine der zwei
//     Haelften anfasst, muss diesen Lauf umwerfen.
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Neuer Prüfstand zur FM-Squelch-Abbildung.
//                Longpath, KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/RxChannel.h"
#include "gui/applets/RxApplet.h"

#include <cmath>

using namespace Longpath;

class TestFmSquelchAbbildung : public QObject {
    Q_OBJECT

private slots:
    void die_enden_der_skala()
    {
        //   0 dB  => n =   0 => 10^0  = 1,0  (Sperre oeffnet am leichtesten)
        QCOMPARE(RxChannel::fmsqScaleValue(0.0), 0);
        QCOMPARE(RxChannel::fmsqThresholdForWdsp(0.0), 1.0);

        // -40 dB => n = 100 => 10^-2 = 0,01 (dichtestes)
        QCOMPARE(RxChannel::fmsqScaleValue(-40.0), 100);
        QVERIFY(std::abs(RxChannel::fmsqThresholdForWdsp(-40.0) - 0.01) < 1e-7);
    }

    void jenseits_der_skala_wird_geklemmt()
    {
        // Die alte Vorgabe -150 lag weit hinter dem dichtesten Ende und
        // wurde ungeklemmt an WDSP gegeben (3,16e-8). Jetzt landet sie auf
        // dem Ende der Skala.
        QCOMPARE(RxChannel::fmsqScaleValue(-150.0), 100);
        QVERIFY(std::abs(RxChannel::fmsqThresholdForWdsp(-150.0) - 0.01) < 1e-7);
        QCOMPARE(RxChannel::fmsqScaleValue(5.0), 0);
        QCOMPARE(RxChannel::fmsqThresholdForWdsp(5.0), 1.0);
    }

    void die_mitte()
    {
        // -20 dB => n = 50 => 10^-1 = 0,1
        QCOMPARE(RxChannel::fmsqScaleValue(-20.0), 50);
        // Toleranz 1e-7, nicht 1e-9: die Zahl geht wie bei Thetis durch
        // float, und float(0,1) ist 0,100000001490116.
        QVERIFY(std::abs(RxChannel::fmsqThresholdForWdsp(-20.0) - 0.1) < 1e-7);
    }

    void zwischenwerte_rasten_auf_thetis_n()
    {
        // Thetis kennt nur ganze n. Ein Wert zwischen zwei Rasten landet
        // auf der naechsten, nicht irgendwo dazwischen.
        QCOMPARE(RxChannel::fmsqScaleValue(-20.1), 50);
        QCOMPARE(RxChannel::fmsqScaleValue(-20.3), 51);
        QCOMPARE(RxChannel::fmsqThresholdForWdsp(-20.1),
                 RxChannel::fmsqThresholdForWdsp(-20.0));
    }

    // Der eigentliche Waechter: fuer JEDE Reglerstellung des Applets kommt
    // bei WDSP dieselbe Zahl heraus wie mit der alten Doppelrechnung.
    void applet_regler_ergibt_unveraenderte_wdsp_zahl()
    {
        for (int n = 0; n <= 100; ++n) {
            const double dB    = RxApplet::fmSquelchDbFromSlider(n);
            const double jetzt = RxChannel::fmsqThresholdForWdsp(dB);
            // Die alte Kette: dB = -0,4*n in RxApplet, dann
            // linear = 10^(dB/20) in RxChannel.
            const double vorher = static_cast<double>(static_cast<float>(
                std::pow(10.0, (-0.4 * n) / 20.0)));
            QVERIFY2(std::abs(jetzt - vorher) < 1e-9,
                     qPrintable(QStringLiteral("n=%1: jetzt %2, vorher %3")
                                    .arg(n).arg(jetzt, 0, 'g', 12)
                                    .arg(vorher, 0, 'g', 12)));
            // und der Rueckweg trifft die Reglerstellung wieder.
            QCOMPARE(RxApplet::fmSquelchSliderFromDb(dB), n);
        }
    }

    // Gegenprobe zur Vorgabe: die ALTE Vorgabe -150 haette hier 3,16e-8
    // ergeben, also weit hinter dem dichtesten Ende.
    void die_alte_vorgabe_lag_hinter_der_skala()
    {
        const double alt = static_cast<double>(static_cast<float>(
            std::pow(10.0, -150.0 / 20.0)));
        QVERIFY(alt < 1e-7);
        QVERIFY(RxChannel::fmsqThresholdForWdsp(-150.0) > alt * 1e5);
    }
};

QTEST_MAIN(TestFmSquelchAbbildung)
#include "tst_fm_squelch_abbildung.moc"
