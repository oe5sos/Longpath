// =================================================================
// tests/tst_voice_signal_detector.cpp  (NereusSDR)
// =================================================================
//
// Der Signalerkenner hinter dem „S-Verlauf".
//
// Port aus AetherSDR src/core/VoiceSignalDetector.cpp [@0cd4559].
//
// Diese Datei ist der Grund, warum der Erkenner als Erstes drankam: er
// ist eine REINE FUNKTION ueber FFT-Werte. Kein Widget, kein Zeitgeber,
// kein Funkgeraet — die Werte lassen sich erfinden, und dann steht das
// Verhalten fest. Bei den vier Anzeigemerkmalen davor mussten die Tests
// sich auf den Zustand beschraenken; hier ist das VERHALTEN pruefbar.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>

#include "core/VoiceSignalDetector.h"

using namespace Longpath;

namespace {

// Ein Streifen Rauschen, in den sich Signale malen lassen.
// 4096 Werte ueber 192 kHz sind rund 47 Hz je Wert — die Groessenordnung,
// in der der Panadapter wirklich arbeitet.
constexpr int    kBins        = 4096;
constexpr double kCenterMhz   = 14.200;
constexpr double kBandwidthMhz = 0.192;
constexpr float  kNoiseDbm    = -120.0f;

QVector<float> flatNoise()
{
    return QVector<float>(kBins, kNoiseDbm);
}

double hzPerBin()
{
    return kBandwidthMhz * 1.0e6 / kBins;
}

int binForMhz(double mhz)
{
    const double startMhz = kCenterMhz - kBandwidthMhz / 2.0;
    return static_cast<int>((mhz - startMhz) / kBandwidthMhz * kBins);
}

// Malt ein Rechteck-Signal gegebener Breite und Hoehe.
void paintSignal(QVector<float>& bins, double centreMhz, double widthHz, float peakDbm)
{
    const int half = static_cast<int>(widthHz / hzPerBin() / 2.0);
    const int mid  = binForMhz(centreMhz);
    for (int i = mid - half; i <= mid + half; ++i) {
        if (i >= 0 && i < bins.size()) { bins[i] = peakDbm; }
    }
}

} // namespace

class TestVoiceSignalDetector : public QObject
{
    Q_OBJECT

private slots:

    // ── sLabel: die S-Stufen-Beschriftung ────────────────────────────
    // Massstab: S9 = -73 dBm, 6 dB je Stufe, AUFGERUNDET.
    //
    // ACHTUNG, FUND BEIM PORTIEREN: der Kommentar ueber sLabel() nennt
    // als Beispiel „-85 → S8". Das ist FALSCH, und zwar im Vorbild.
    // Nachgerechnet: S9 = -73, S8 = -79, S7 = -85 — der Code liefert
    // richtig S7, nur sein eigenes Beispiel widerspricht ihm. Der
    // Kommentar bleibt wortgleich stehen (Hausregel), diese Zeile haelt
    // das tatsaechliche Verhalten fest. Wer den Kommentar glaubt und den
    // Test aendert, dreht die S-Stufen um eine Stufe.
    void sLabelMatchesTheScale_data()
    {
        QTest::addColumn<float>("dbm");
        QTest::addColumn<QString>("label");

        QTest::newRow("S9 exakt")     << -73.0f << QStringLiteral("S9");
        QTest::newRow("ueber S9")     << -63.0f << QStringLiteral("S9+10");
        QTest::newRow("S8")           << -79.0f << QStringLiteral("S8");
        QTest::newRow("S7 (nicht S8)") << -85.0f << QStringLiteral("S7");
        QTest::newRow("ganz unten")   << -140.0f << QStringLiteral("S0");
    }

    void sLabelMatchesTheScale()
    {
        QFETCH(float, dbm);
        QFETCH(QString, label);
        QCOMPARE(sLabel(dbm), label);
    }

    // ── Bandplan-Etiketten ───────────────────────────────────────────
    void voiceSegmentLabelsAreRecognised()
    {
        QVERIFY(isVoiceSegmentLabel(QStringLiteral("PHONE")));
        QVERIFY(isVoiceSegmentLabel(QStringLiteral("SSB")));
        QVERIFY(isVoiceSegmentLabel(QStringLiteral("FM/RPT")));
        QVERIFY(!isVoiceSegmentLabel(QStringLiteral("CW")));
        QVERIFY(!isVoiceSegmentLabel(QStringLiteral("DIGITAL")));
    }

    // ── Der leere Fall ───────────────────────────────────────────────
    void flatNoiseFindsNothing()
    {
        const auto found = detectVoiceSignals(flatNoise(), kCenterMhz, kBandwidthMhz,
                                              {}, kNoiseDbm);
        QVERIFY2(found.isEmpty(),
                 "auf glattem Rauschen darf nichts gefunden werden");
    }

    void tooFewBinsIsRefused()
    {
        QVector<float> tiny(10, kNoiseDbm);
        QVERIFY(detectVoiceSignals(tiny, kCenterMhz, kBandwidthMhz, {}, kNoiseDbm).isEmpty());
    }

    void zeroBandwidthIsRefused()
    {
        QVERIFY(detectVoiceSignals(flatNoise(), kCenterMhz, 0.0, {}, kNoiseDbm).isEmpty());
    }

    // ── Ein Sprachsignal ─────────────────────────────────────────────
    void oneVoiceSignalIsFound()
    {
        QVector<float> bins = flatNoise();
        // 2,4 kHz breit, 30 dB ueber dem Boden — eine gewoehnliche SSB-Station.
        paintSignal(bins, 14.205, 2400.0, kNoiseDbm + 30.0f);

        const auto found = detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz,
                                              {}, kNoiseDbm, QStringLiteral("USB"));
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().mode, QStringLiteral("USB"));
        QCOMPARE(found.first().peakDbm, kNoiseDbm + 30.0f);

        // Die Marke sitzt bei USB auf der TRAEGERSEITE, also LINKS vom
        // Signal, und wird auf volle kHz abgerundet — Funkbetrieb laeuft
        // auf runden kHz. Unser gemaltes Rechteck reicht von 14.2038 bis
        // 14.2062, die Marke gehoert also auf 14.203.
        //
        // Das ist der Grund, warum hier nicht auf die Mitte geprueft
        // wird: die Marke SOLL nicht in der Mitte sitzen.
        QVERIFY2(found.first().freqMhz <= 14.205,
                 "USB-Marke gehoert auf die Traegerseite, nicht in die Mitte");
        QVERIFY2(std::abs(found.first().freqMhz - 14.2038) < 0.0015,
                 "die Marke muss an der linken Signalkante liegen");
    }

    // Die Betriebsart des Betreibers schlaegt die Heuristik — sonst
    // widerspraeche die Marke dem, was im VFO steht.
    void thecallersModeWins()
    {
        QVector<float> bins = flatNoise();
        paintSignal(bins, 14.205, 2400.0, kNoiseDbm + 30.0f);

        const auto usb = detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz,
                                            {}, kNoiseDbm, QStringLiteral("USB"));
        const auto lsb = detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz,
                                            {}, kNoiseDbm, QStringLiteral("LSB"));
        QCOMPARE(usb.first().mode, QStringLiteral("USB"));
        QCOMPARE(lsb.first().mode, QStringLiteral("LSB"));
    }

    // ── Die Schwelle ─────────────────────────────────────────────────
    // Ein Signal muss 13 dB ueber dem Boden liegen, um gemeldet zu
    // werden (kMinPeakAboveFloorDb). 8 dB reichen nicht: das faengt
    // Rauschbuckel ab, die die 6-dB-Kante gerade eben ueberschreiten.
    void aWeakBumpIsNotReported()
    {
        QVector<float> bins = flatNoise();
        paintSignal(bins, 14.205, 2400.0, kNoiseDbm + 8.0f);

        const auto found = detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz,
                                              {}, kNoiseDbm, QStringLiteral("USB"));
        QVERIFY2(found.isEmpty(),
                 "8 dB ueber dem Boden ist ein Buckel, kein Signal");
    }

    void aStrongSignalIsReported()
    {
        QVector<float> bins = flatNoise();
        paintSignal(bins, 14.205, 2400.0, kNoiseDbm + 20.0f);

        QCOMPARE(detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz,
                                    {}, kNoiseDbm, QStringLiteral("USB")).size(), 1);
    }

    // ── Schmalband bleibt Schmalband ─────────────────────────────────
    // Ein CW-Traeger (rund 50 Hz) darf nicht als Sprache durchgehen. Die
    // Fuellung verbreitert ihn um hoechstens 2x400 Hz und bleibt damit
    // unter den 1,8 kHz — die Marke entsteht trotzdem, aber als
    // schmales Signal, nicht als Station.
    void aCarrierStaysNarrow()
    {
        QVector<float> bins = flatNoise();
        paintSignal(bins, 14.205, 50.0, kNoiseDbm + 30.0f);

        const auto found = detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz,
                                              {}, kNoiseDbm, QStringLiteral("USB"));
        QCOMPARE(found.size(), 1);
        QVERIFY2(found.first().widthHz < 1800.0,
                 "ein Traeger darf nicht auf Sprachbreite anwachsen");
    }

    // ── Der Bandplan-Filter ──────────────────────────────────────────
    // Sprachbreite Signale ausserhalb der Fonie-Bereiche werden
    // uebergangen; Schmalband-Stoerungen NICHT, weil Stoerungen sich
    // nicht an den Bandplan halten.
    void voiceOutsideThePhoneSegmentIsSkipped()
    {
        QVector<float> bins = flatNoise();
        paintSignal(bins, 14.205, 2400.0, kNoiseDbm + 30.0f);

        // Fonie erst ab 14.210 — das Signal liegt darunter.
        const QVector<QPair<double, double>> phoneOnly{{14.210, 14.350}};
        const auto found = detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz,
                                              phoneOnly, kNoiseDbm, QStringLiteral("USB"));
        QVERIFY2(found.isEmpty(),
                 "sprachbreite Signale ausserhalb der Fonie-Bereiche zaehlen nicht");
    }

    void voiceInsideThePhoneSegmentIsKept()
    {
        QVector<float> bins = flatNoise();
        paintSignal(bins, 14.240, 2400.0, kNoiseDbm + 30.0f);

        const QVector<QPair<double, double>> phoneOnly{{14.210, 14.350}};
        QCOMPARE(detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz,
                                    phoneOnly, kNoiseDbm, QStringLiteral("USB")).size(), 1);
    }

    // ── Der Rauschboden von aussen ───────────────────────────────────
    // Gibt der Aufrufer keinen Boden (< -500), rechnet die Funktion das
    // 10. Perzentil des Bildes. Beides muss dasselbe Signal finden.
    void itFallsBackToItsOwnFloorEstimate()
    {
        QVector<float> bins = flatNoise();
        paintSignal(bins, 14.205, 2400.0, kNoiseDbm + 30.0f);

        const auto withFloor = detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz,
                                                  {}, kNoiseDbm, QStringLiteral("USB"));
        const auto without   = detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz,
                                                  {}, -1000.0f, QStringLiteral("USB"));
        QCOMPARE(withFloor.size(), without.size());
        QCOMPARE(withFloor.first().freqMhz, without.first().freqMhz);
    }

    // ── Zwei Stationen nebeneinander ─────────────────────────────────
    // Zwei getrennte Signale muessen zwei Marken ergeben, nicht eine
    // breite. Das ist der Fall, den die Talsuche im Vorbild abdeckt.
    void twoSeparateStationsGiveTwoMarkers()
    {
        QVector<float> bins = flatNoise();
        paintSignal(bins, 14.195, 2400.0, kNoiseDbm + 30.0f);
        paintSignal(bins, 14.215, 2400.0, kNoiseDbm + 30.0f);

        const auto found = detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz,
                                              {}, kNoiseDbm, QStringLiteral("USB"));
        QCOMPARE(found.size(), 2);
    }

    // ── Breitband ist eine Stoerung, keine Station ───────────────────
    // Ueber 8 kHz kann keine Sprache sein (OTH-Radar, Splatter). Das
    // Vorbild meldet dafuer EINE Marke in der Mitte, statt das Bild mit
    // einem Dutzend Sprachschnipseln zuzupflastern.
    void widebandInterferenceGivesOneMarker()
    {
        QVector<float> bins = flatNoise();
        paintSignal(bins, 14.205, 20000.0, kNoiseDbm + 30.0f);

        const auto found = detectVoiceSignals(bins, kCenterMhz, kBandwidthMhz,
                                              {}, kNoiseDbm, QStringLiteral("USB"));
        QCOMPARE(found.size(), 1);
        QVERIFY2(found.first().widthHz > 8000.0,
                 "die Breite muss als Stoerbreite durchgereicht werden");
    }
};

QTEST_MAIN(TestVoiceSignalDetector)
#include "tst_voice_signal_detector.moc"
