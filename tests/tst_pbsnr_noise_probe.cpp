// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original regression test.
//
// PB SNR auf reinem Rauschen (2026-09-27).
//
// Thetis' PB-SNR-Rechnung (console.cs spectralCalculations, siehe
// core/PassbandSnr.h) nimmt an, dass Rauschflur je FFT-Bin und
// gemitteltes Signal im Durchlass dieselbe Rauschleistung beschreiben:
// je Bin P * ENB / N, im Durchlass P * B / fs. Dieser Pruefstand
// schickt Gauss-Rauschen bekannter Leistung durch FFTEngine (Hamming,
// 4096, 48 kHz) und durch einen echten WDSP-Kanal (USB 50..3050) und
// prueft beide Seiten -- und dass PB SNR dann bei 0 dB steht.
//
// Anlass: am SunSDR2 QRP zeigte PB SNR auf Rauschen -7 dB. Die Rechnung
// war richtig (hier +0,1 dB); die Luecke kam von NB1, der das stark
// impulsartige QRP-Rauschen hinter dem Spektrum-Abgriff austastet (mit
// NB aus live +2,4 dB). Die verbleibenden +2,5 dB sind der
// NoiseFloorTracker: er mittelt dB-Werte, Thetis linear -- hier als
// "dB-Mittel" mitgemessen und nur protokolliert.

#include <QtTest/QtTest>
#include <QThread>
#include <cmath>
#include <random>

#include "core/FFTEngine.h"
#include "core/PassbandSnr.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"

using namespace Longpath;

class TstPbsnrNoiseProbe : public QObject {
    Q_OBJECT

private slots:
    void noiseReadsTheSameInBothPaths()
    {
#ifndef HAVE_WDSP
        QSKIP("ohne WDSP kein Kanal");
#else
        constexpr double kRate = 48000.0;
        constexpr int    kFft  = 4096;
        constexpr int    kChunk = bufferSizeForRate(48000);
        constexpr double kPower = 1.0e-8;            // -80 dBFS, komplexe Leistung
        const double sigma = std::sqrt(kPower / 2.0); // je I und Q

        std::mt19937 rng(12345u);
        std::normal_distribution<float> gauss(0.0f, static_cast<float>(sigma));

        // ── FFT ─────────────────────────────────────────────────────
        FFTEngine fft(0);
        fft.setSampleRate(kRate);
        fft.setFftSize(kFft);
        fft.setWindowFunction(WindowFunction::Hamming);
        double linSum = 0.0, dbSum = 0.0;
        long   nBins = 0;
        int    frames = 0;
        connect(&fft, &FFTEngine::fftReady, this,
                [&](int, const QVector<float>& bins) {
            ++frames;
            if (frames < 3) { return; }
            for (float b : bins) {
                linSum += std::pow(10.0, b / 10.0);
                dbSum  += b;
                ++nBins;
            }
        }, Qt::DirectConnection);
        QVector<float> iq(2 * kFft);
        for (int f = 0; f < 60; ++f) {
            for (int i = 0; i < kFft; ++i) { iq[2 * i] = gauss(rng); iq[2 * i + 1] = gauss(rng); }
            fft.feedIQ(iq);
            QThread::msleep(20);
            QCoreApplication::processEvents();
        }
        QVERIFY2(nBins > 0, "keine FFT-Rahmen");
        const double nfLin = 10.0 * std::log10(linSum / nBins);
        const double nfDb  = dbSum / nBins;
        const double enb   = fft.windowEnb();

        // ── WDSP-Kanal, USB 50..3050 ───────────────────────────────
        WdspEngine engine;
        engine.m_initialized = true;
        RxChannel* ch = engine.createRxChannel(0, kChunk, 4096, 48000, 48000, 48000);
        QVERIFY(ch);
        ch->setActive(true);
        ch->setMode(DSPMode::USB);
        ch->setFilterFreqs(50.0, 3050.0);
        std::vector<float> inI(kChunk), inQ(kChunk), outI(kChunk), outQ(kChunk);
        const int blocks = static_cast<int>(3.0 * kRate / kChunk);
        for (int b = 0; b < blocks; ++b) {
            for (int i = 0; i < kChunk; ++i) { inI[i] = gauss(rng); inQ[i] = gauss(rng); }
            ch->processIq(inI.data(), inQ.data(), outI.data(), outQ.data(), kChunk, kChunk);
            QThread::usleep(1000000 * kChunk / 48000);
        }
        const double sav = ch->getMeter(RxMeterType::SignalAvg);
        ch->setActive(false);
        engine.destroyRxChannel(0);

        const double pbExpected = 10.0 * std::log10(kPower) + 10.0 * std::log10(3000.0 / kRate);
        const PassbandSnrResult rLin = passbandSnr(sav, nfLin, kRate, kFft, enb, 50, 3050, 0.0, false);
        const PassbandSnrResult rDb  = passbandSnr(sav, nfDb,  kRate, kFft, enb, 50, 3050, 0.0, false);
        qInfo("Leistung %.2f dBFS | ENB %.3f | NF lin %.2f dB-Mittel %.2f (erwartet %.2f)",
              10.0 * std::log10(kPower), enb, nfLin, nfDb,
              10.0 * std::log10(kPower) + 10.0 * std::log10(enb / kFft));
        qInfo("S_AV %.2f (erwartet %.2f) | PB SNR mit NF lin %.2f, mit dB-Mittel %.2f",
              sav, pbExpected, rLin.estimatedSnr, rDb.estimatedSnr);
        QVERIFY(std::isfinite(sav));
        const double nfExpected = 10.0 * std::log10(kPower) + 10.0 * std::log10(enb / kFft);
        QVERIFY2(std::abs(nfLin - nfExpected) < 0.5, "FFT-Rauschdichte je Bin passt nicht zu P*ENB/N");
        QVERIFY2(std::abs(sav - pbExpected) < 1.0, "S_AV passt nicht zu P*B/fs");
        QVERIFY2(std::abs(rLin.estimatedSnr) < 1.0, "PB SNR auf Rauschen nicht bei 0 dB");
#endif
    }
};

QTEST_MAIN(TstPbsnrNoiseProbe)
#include "tst_pbsnr_noise_probe.moc"
