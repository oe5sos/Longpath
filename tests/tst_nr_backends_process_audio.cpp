// =================================================================
// tests/tst_nr_backends_process_audio.cpp  (NereusSDR)
// =================================================================
//
// Jede Rauschminderung im NR-Menue laeuft hier wirklich mit Audio
// durch einen echten WDSP-Kanal — nicht nur die Fahne wird gesetzt.
//
// Anlass, 2026-09-17: "bitte kontrollieren nicht nur nr1, nr2 und nr3
// sondern auch die anderen optionen im offenen menü." Davor am selben
// Tag ein Absturzbericht der Auslieferung 0.5.2 (10:25:55): SIGTRAP in
// __memcpy_chk unter rnn_compute_generic_conv1d <- rnnoise_process_frame
// <- xrnnr — also NR3 mitten in der Verarbeitung. Ein Prüfstand, der
// die Run-Fahne prueft (tst_rxchannel_emnr), haette das nie gesehen:
// die Fahne war richtig, der Puffer nicht.
//
// Was hier je Minderung passiert: echter Kanal (WdspEngine +
// createRxChannel, m_initialized ueber Freundschaft wie in
// tst_notch_tune_frequency), Modell laden, wo eines gebraucht wird
// (NR3: rnnoise Default_large.bin, NNR: wdsp_nnr_0/1.bin), Minderung
// einschalten, 400 Bloecke zu 238 Abtastwerten Rauschen+Ton
// hindurchschieben (rund zwei Sekunden bei 48 kHz), dann: nichts
// abgestuerzt, Ausgabe endlich (kein NaN/Inf), Ausgabe nicht ueberall
// null.
//
// DFNR und BNR stehen NICHT hier: DFNR ist in diesem Bau nicht
// enthalten (Bibliothek fehlt, ./setup-deepfilter.sh nie gelaufen),
// BNR gibt es nur unter Windows mit NVIDIA. Beide sind im Menue trotzdem
// anklickbar und tun dann nichts — das ist ein eigener Befund, kein
// Fall fuer diesen Pruefstand.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-17 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QDir>
#include <QFileInfo>
#include <QThread>

#include <cmath>
#include <vector>

#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "core/ModelPaths.h"
#ifdef HAVE_WDSP
#include "core/wdsp_api.h"
#endif

using namespace Longpath;

class TstNrBackendsProcessAudio : public QObject {
    Q_OBJECT

private:
    static constexpr int kChunk      = 238;     // ein P2-Paket
    static constexpr int kIterations = 300;     // ~1,5 s bei 48 kHz
    static constexpr int kChannelId  = 0;

    // ── Im Takt der Abtastrate, nicht so schnell wie moeglich ────────
    //
    // WDSPs fexchange2 ist asynchron: der Aufrufer legt einen Block in
    // den Eingangsring, der wdspmain-Thread rechnet, der Aufrufer holt
    // vom Ausgangsring. Der Eingangsring hat eine feste Tiefe und KEINE
    // Ueberlaufpruefung (iobuffs.c: "add check with *error += -1; for
    // case when r1 is full and an overwrite occurs" — ein Kommentar,
    // kein Code). Die erste Fassung dieses Pruefstands schob 400
    // Bloecke in einer engen Schleife hinein und bekam unter Last
    // (ctest -j4) ab Block ~290 NaN zurueck — in NR1 UND NR2, also
    // nicht die Minderung, sondern der ueberholte Ring. Im Betrieb
    // taktet das Funkgeraet den Aufrufer auf 48 kHz; genau das tut die
    // Pause hier: 238 Abtastwerte sind 4,96 ms.
    static constexpr int kBlockPeriodUs = 1000000 * kChunk / 48000;

    // Modelle aus dem Quellbaum, unabhaengig davon, wo das Testprogramm
    // liegt: erst der Pfad, den die App selbst nimmt (ModelPaths, sucht
    // relativ zur Binary), sonst __FILE__-relativ.
    static QString modelPath(const QString& viaApp, const char* rel)
    {
        if (!viaApp.isEmpty() && QFileInfo::exists(viaApp)) { return viaApp; }
        const QDir tests(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath());
        const QString p = QDir::cleanPath(tests.filePath(QString::fromLatin1(rel)));
        return QFileInfo::exists(p) ? p : QString();
    }

    // Wiederholbares Rauschen (LCG) plus ein 1-kHz-Ton, damit die
    // Minderung etwas zu tun hat.
    struct Source {
        uint32_t seed{0x1234567u};
        double   phase{0.0};
        float next(bool q)
        {
            seed = seed * 1664525u + 1013904223u;
            const float noise = (static_cast<float>(seed >> 8) / 16777216.0f - 0.5f) * 0.02f;
            if (!q) { phase += 2.0 * 3.14159265358979323846 * 1000.0 / 48000.0; }
            const float tone = 0.01f * static_cast<float>(q ? std::sin(phase) : std::cos(phase));
            return noise + tone;
        }
    };

    void runSlot(NrSlot slot, const char* label)
    {
#ifndef HAVE_WDSP
        Q_UNUSED(slot); Q_UNUSED(label);
        QSKIP("ohne WDSP kein Kanal");
#else
        WdspEngine engine;
        engine.m_initialized = true;   // Freundschaft, siehe WdspEngine.h
        RxChannel* ch = engine.createRxChannel(kChannelId, kChunk, 4096,
                                               48000, 48000, 48000);
        QVERIFY2(ch, "createRxChannel lieferte keinen Kanal");
        ch->setActive(true);
        ch->setActiveNr(slot);
        QCOMPARE(ch->activeNr(), slot);

        std::vector<float> inI(kChunk), inQ(kChunk), outI(kChunk), outQ(kChunk);
        Source src;
        double energy = 0.0;
        bool finite = true;
        int firstBadIter = -1, badSamples = 0;
        for (int it = 0; it < kIterations; ++it) {
            for (int i = 0; i < kChunk; ++i) {
                inI[i] = src.next(false);
                inQ[i] = src.next(true);
            }
            ch->processIq(inI.data(), inQ.data(), outI.data(), outQ.data(),
                          kChunk, kChunk);
            QThread::usleep(kBlockPeriodUs);
            for (int i = 0; i < kChunk; ++i) {
                if (!std::isfinite(outI[i]) || !std::isfinite(outQ[i])) {
                    finite = false;
                    ++badSamples;
                    if (firstBadIter < 0) { firstBadIter = it; }
                } else {
                    energy += double(outI[i]) * outI[i] + double(outQ[i]) * outQ[i];
                }
            }
        }
        if (!finite) {
            qWarning().noquote() << label << "- erste NaN/Inf in Block"
                                 << firstBadIter << "," << badSamples
                                 << "betroffene Abtastwerte von"
                                 << (kIterations * kChunk);
        }
        ch->setActiveNr(NrSlot::Off);
        ch->setActive(false);
        engine.destroyRxChannel(kChannelId);

        qInfo().noquote() << label << "- Ausgangsenergie ueber"
                          << kIterations << "Bloecke:" << energy;
        QVERIFY2(finite, qPrintable(QStringLiteral(
            "%1: NaN oder Inf in der Ausgabe").arg(QLatin1String(label))));
        QVERIFY2(energy > 0.0, qPrintable(QStringLiteral(
            "%1: Ausgabe ueberall null — die Minderung frisst alles oder der "
            "Kanal laeuft nicht").arg(QLatin1String(label))));
#endif
    }

private slots:
    void nr1_anr()  { runSlot(NrSlot::NR1, "NR1 (ANR)"); }
    void nr2_emnr() { runSlot(NrSlot::NR2, "NR2 (EMNR)"); }

    void nr3_rnnoise()
    {
#ifdef HAVE_WDSP
        const QString model = modelPath(ModelPaths::rnnoiseDefaultLargeBin(),
                                        "../third_party/rnnoise/models/Default_large.bin");
        if (model.isEmpty()) { QSKIP("rnnoise-Modell Default_large.bin nicht gefunden"); }
        qInfo().noquote() << "NR3-Modell:" << model;
        RNNRloadModel(model.toStdString().c_str());
#endif
        runSlot(NrSlot::NR3, "NR3 (rnnoise)");
    }

    void nr4_specbleach() { runSlot(NrSlot::NR4, "NR4 (libspecbleach)"); }

    void nnr_wdsp210()
    {
#ifdef HAVE_WDSP
        const QString m0 = modelPath(ModelPaths::nnrModel0Bin(),
                                     "../third_party/wdsp/models/wdsp_nnr_0.bin");
        const QString m1 = modelPath(ModelPaths::nnrModel1Bin(),
                                     "../third_party/wdsp/models/wdsp_nnr_1.bin");
        if (m0.isEmpty() || m1.isEmpty()) { QSKIP("NNR-Modelle wdsp_nnr_0/1.bin nicht gefunden"); }
        qInfo().noquote() << "NNR-Modelle:" << m0 << m1;
        SetNNRModelPathSlot(0, m0.toStdString().c_str());
        SetNNRModelPathSlot(1, m1.toStdString().c_str());
#endif
        runSlot(NrSlot::NNR, "NNR (WDSP 2.10)");
    }

    void mnr_accelerate()
    {
#ifndef HAVE_MNR
        QSKIP("MNR nur unter macOS");
#else
        runSlot(NrSlot::MNR, "MNR (Apple Accelerate)");
#endif
    }
};

QTEST_MAIN(TstNrBackendsProcessAudio)
#include "tst_nr_backends_process_audio.moc"
