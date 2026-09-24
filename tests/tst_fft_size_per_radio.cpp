// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die FFT-Groesse des Panadapters gilt je Geraet.
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der Regler war nur global (DisplayFftSize). Eine feste Punktzahl ist
// aber je nach Abtastrate ein anderes Zeitfenster: 16 384 sind an der
// ANAN (192 kHz) 85 ms, an der SunSDR2 QRP (48 kHz) 341 ms -- dort
// bewegte sich das Spektrum traege. Betreiber 2026-09-24: "pro Geraet
// merken".
//
// =================================================================
// Modification history (Longpath):
//   2026-09-24 — Created for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "gui/MainWindow.h"

using namespace Longpath;

namespace {
const QString kQrp  = QStringLiteral("MANUAL:192.168.16.200:1024");
const QString kAnan = QStringLiteral("40:84:32:B1:2F:19");
}

class TstFftSizePerRadio : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        auto& s = AppSettings::instance();
        s.clearHardwareValues(kQrp);
        s.clearHardwareValues(kAnan);
        s.setValue(QStringLiteral("DisplayFftSize"), QStringLiteral("16384"));
    }

    void aRadioWithItsOwnValueGetsIt()
    {
        AppSettings::instance().setHardwareValue(
            kQrp, QStringLiteral("display/fftSize"), 4096);
        QCOMPARE(MainWindow::fftSizeForRadio(kQrp, 8192), 4096);
    }

    void anotherRadioKeepsTheGlobalValue()
    {
        // Das ist der Punkt: die QRP-Einstellung darf die ANAN nicht
        // mitnehmen.
        AppSettings::instance().setHardwareValue(
            kQrp, QStringLiteral("display/fftSize"), 4096);
        QCOMPARE(MainWindow::fftSizeForRadio(kAnan, 8192), 16384);
    }

    void anInvalidStoredValueIsIgnored()
    {
        AppSettings::instance().setHardwareValue(
            kQrp, QStringLiteral("display/fftSize"), 1234);
        QCOMPARE(MainWindow::fftSizeForRadio(kQrp, 8192), 16384);
    }

    void withoutAnythingTheFallbackHolds()
    {
        AppSettings::instance().remove(QStringLiteral("DisplayFftSize"));
        QCOMPARE(MainWindow::fftSizeForRadio(kQrp, 8192), 8192);
    }
};

QTEST_MAIN(TstFftSizePerRadio)
#include "tst_fft_size_per_radio.moc"
