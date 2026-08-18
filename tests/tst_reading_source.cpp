// no-port-check: nennt MeterManager.cs nur als HERKUNFT der drei
// dBm-Stuetzstellen der S-Skala (S0/S9/S9+60). Der Port dieser Zahlen
// lebt in gui/instruments/ReadingSource.h und ist dort mit Zeile und
// Version zitiert; hier steht kein uebernommener Code, sondern die
// Pruefung, dass die Zahlen mit der Quelle uebereinstimmen.

// =================================================================
// tests/tst_reading_source.cpp  (NereusSDR)
// =================================================================
//
// Die Quellenliste der Instrumente und die Kennlinie der Empfangsskala.
//
// OE5SOS, 2026-08-17: „der Test zur S-Stauchung soll die Abbildung
// festnageln (S9 bei zwei Dritteln, +60 am Ende), nicht nur Monotonie
// prüfen."
//
// Das ist der Kern dieser Datei. Eine Monotonieprüfung hätte jede
// krumme Kennlinie durchgelassen, solange sie nur steigt — und eine
// Skala, auf der S9 nicht dort sitzt, wo der Betreiber es gezeichnet
// hat, ist eine andere Skala, auch wenn sie steigt.
//
// Geprüft werden deshalb Stützstellen, nicht Verläufe:
//   S0  → 0
//   S9  → 0,66          (die Zahl aus dem Entwurf, siehe unten)
//   +60 → 1
// und dazu, dass die Teilung unter S9 GLEICHMÄSSIG und darüber
// GESTAUCHT ist — was der eigentliche Inhalt der Aussage „bis S9
// gleichmässig und darüber gestaucht" ist.
//
// ── Zu „zwei Drittel" ────────────────────────────────────────────────
//
// Der Entwurf (~/Downloads/zeiger-verfeinert.html, sigFrac) rechnet mit
// 0.66, nicht mit 2/3 = 0,6667. Der Unterschied ist 0,7 % der
// Skalenlänge und auf dem Schirm nicht zu sehen — aber ein Test muss
// eine Zahl nennen, und die gezeichnete ist die Vorlage. Wer 2/3
// meint, ändert kS9Fraction; dieser Test folgt der Konstante nicht,
// sondern nennt die Zahl, damit die Änderung auffällt.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>

#include <cmath>

#include "gui/instruments/ReadingSource.h"
#include "gui/meters/MeterPoller.h"

using namespace NereusSDR;

class TestReadingSource : public QObject
{
    Q_OBJECT

private slots:

    // ── Die Stützstellen ─────────────────────────────────────────────

    void sScaleAnchorsAreNailedDown()
    {
        // S0 am Anfang.
        QVERIFY(qFuzzyIsNull(signalFraction(kS0Dbm)));

        // S9 bei 0,66 — die Zahl steht hier ausgeschrieben und nicht
        // als kS9Fraction, damit eine Änderung der Konstante diesen
        // Test umwirft statt ihn mitzuziehen.
        QVERIFY2(qAbs(signalFraction(kS9Dbm) - 0.66) < 1e-9,
                 qPrintable(QStringLiteral("S9 liegt bei %1 statt 0.66")
                                .arg(signalFraction(kS9Dbm), 0, 'f', 6)));

        // +60 am Ende.
        QVERIFY2(qAbs(signalFraction(kSMaxDbm) - 1.0) < 1e-9,
                 qPrintable(QStringLiteral("S9+60 liegt bei %1 statt 1.0")
                                .arg(signalFraction(kSMaxDbm), 0, 'f', 6)));
    }

    // Herkunft der dBm-Zahlen: Thetis MeterManager.cs:22870-22872
    // [v2.10.3.15-5-g852bf0e], die Kalibrierpunkte des S-Meter-Balkens.
    // Bis 2026-08-18 nannte dieser Kommentar SMeterWidget.h — unsere
    // eigene Zwischenstation, die die Zahlen ihrerseits aus AetherSDR
    // hatte und am selben Tag geloescht wurde.
    void dbmAnchorsMatchTheSMeterScale()
    {
        QCOMPARE(kS0Dbm,   -127.0);
        QCOMPARE(kS9Dbm,    -73.0);
        QCOMPARE(kSMaxDbm,  -13.0);
        // Sechs dB je S-Stufe: S0 bis S9 sind 54 dB.
        QCOMPARE(kS9Dbm - kS0Dbm, 54.0);
        // Darüber zehn dB je Stufe: S9 bis S9+60 sind 60 dB.
        QCOMPARE(kSMaxDbm - kS9Dbm, 60.0);
    }

    void sUnitsFromDbmMatchTheSixAndTenDbSteps()
    {
        QCOMPARE(sUnitsFromDbm(kS0Dbm),        0.0);
        QCOMPARE(sUnitsFromDbm(kS0Dbm + 6.0),  1.0);
        QCOMPARE(sUnitsFromDbm(kS0Dbm + 54.0), 9.0);
        QCOMPARE(sUnitsFromDbm(kS9Dbm + 10.0), 10.0);
        QCOMPARE(sUnitsFromDbm(kS9Dbm + 60.0), 15.0);
    }

    // ── Gleichmässig unten, gestaucht oben ───────────────────────────

    void belowS9TheDivisionIsEven()
    {
        // Drei gleich grosse Schritte in S-Stufen müssen drei gleich
        // grosse Schritte auf der Skala ergeben.
        const double f3 = signalFraction(kS0Dbm + 3 * 6.0);
        const double f6 = signalFraction(kS0Dbm + 6 * 6.0);
        const double f9 = signalFraction(kS9Dbm);
        QVERIFY(qAbs((f6 - f3) - f3) < 1e-9);
        QVERIFY(qAbs((f9 - f6) - f3) < 1e-9);
        // Und ein Drittel des Weges bis S9 ist ein Drittel von 0,66.
        QVERIFY(qAbs(f3 - 0.22) < 1e-9);
    }

    void aboveS9TheDivisionIsCompressed()
    {
        // Zehn dB unterhalb von S9 brauchen mehr Skala als zehn dB
        // oberhalb. Das IST die Stauchung; ohne diese Prüfung wäre
        // „gestaucht" ein Wort im Kommentar.
        const double below = signalFraction(kS9Dbm) - signalFraction(kS9Dbm - 10.0);
        const double above = signalFraction(kS9Dbm + 10.0) - signalFraction(kS9Dbm);
        QVERIFY2(above < below,
                 qPrintable(QStringLiteral("oben %1, unten %2 — nicht gestaucht")
                                .arg(above, 0, 'f', 6).arg(below, 0, 'f', 6)));
        // Und zwar um mehr als das Doppelte: 10 dB sind unten 1,67
        // S-Stufen zu je 0,0733, oben eine Stufe zu 0,0567.
        QVERIFY(below > 2.0 * above);
    }

    void aboveS9TheStepsAreEvenAmongThemselves()
    {
        // Die Stauchung ist eine EINMALIGE Änderung der Teilung bei S9,
        // keine fortlaufende Krümmung.
        const double a = signalFraction(kS9Dbm + 20.0) - signalFraction(kS9Dbm);
        const double b = signalFraction(kS9Dbm + 40.0) - signalFraction(kS9Dbm + 20.0);
        const double c = signalFraction(kS9Dbm + 60.0) - signalFraction(kS9Dbm + 40.0);
        QVERIFY(qAbs(a - b) < 1e-9);
        QVERIFY(qAbs(b - c) < 1e-9);
    }

    void outsideTheScaleItClamps()
    {
        QCOMPARE(signalFraction(kS0Dbm - 40.0), 0.0);
        QCOMPARE(signalFraction(kSMaxDbm + 40.0), 1.0);
    }

    // ── Die Liste ────────────────────────────────────────────────────

    void everyBindingAppearsExactlyOnce()
    {
        QSet<int> seen;
        for (const ReadingDescriptor& d : allReadings()) {
            QVERIFY2(!seen.contains(d.bindingId),
                     qPrintable(QStringLiteral("Kennung %1 steht zweimal")
                                    .arg(d.bindingId)));
            seen.insert(d.bindingId);
            QVERIFY2(!d.label.isEmpty(),
                     qPrintable(QStringLiteral("Kennung %1 ohne Beschriftung")
                                    .arg(d.bindingId)));
        }
        // 27 Größen aus der längeren der beiden Auswahllisten, aus
        // denen diese Tabelle hervorging (BaseItemEditor.cpp vor dem
        // 2026-08-17), plus zwei, die dort nie standen: Rauschflur und
        // RADE-SNR. Beide kommen nicht aus WDSP und lebten nur als
        // Beschriftung an der analogen S-Meter-Anzeige; sie sind am
        // 2026-08-18 Messgrössen mit eigener Kennung geworden.
        QCOMPARE(allReadings().size(), 29);
    }

    void lookupFindsWhatIsThereAndNothingElse()
    {
        QVERIFY(readingFor(MeterBinding::TxSwr) != nullptr);
        QVERIFY(readingFor(MeterBinding::HwTemperature) != nullptr);
        QVERIFY(readingFor(-1) == nullptr);
        QVERIFY(readingFor(9999) == nullptr);
    }

    // ── Skalen: nur, wo eine belegt ist ──────────────────────────────

    void onlyGroundedScalesAreOfferedToInstruments()
    {
        const auto scaled = readingsWithScale();
        QSet<int> ids;
        for (const ReadingDescriptor* d : scaled) { ids.insert(d->bindingId); }

        // Belegt: SWR (TxApplet.h:175), Vorlaufleistung
        // (ItemGroup.cpp:680 + TxApplet.h:174), Signal
        // (InstrumentApplet „SignalInstrument", MainWindow.cpp:5001).
        //
        // Hier stand „Signal (SMeterWidget)". Das war seit 300e8d48 falsch:
        // die Klasse ist geloescht, und wer ihr nachging, um zu sehen, WER
        // die Kennung belegt, fand nichts. Die Skalen haengen jetzt an der
        // InstrumentApplet, die setPrimary(MeterBinding::SignalAvg) setzt.
        QVERIFY(ids.contains(MeterBinding::TxSwr));
        QVERIFY(ids.contains(MeterBinding::TxPower));
        QVERIFY(ids.contains(MeterBinding::SignalAvg));
        QVERIFY(ids.contains(MeterBinding::SignalPeak));

        // NICHT belegt, und darum bewusst NICHT angeboten: die
        // PA-Temperatur. Es gibt im Baum weder Bereich noch Grenze für
        // sie, und ../Thetis ist auf diesem Rechner nicht geklont. Eine
        // erfundene Skala sähe aus wie eine Messung.
        //
        // Diese Zeile ist die Erinnerung: sobald eine belastbare Skala
        // vorliegt, fällt sie um, und das ist der Zeitpunkt, sie
        // einzutragen.
        QVERIFY2(!ids.contains(MeterBinding::HwTemperature),
                 "Temperatur hat eine Skala bekommen — Herkunft eintragen "
                 "und diese Pruefung umdrehen");
    }

    void swrScaleMatchesTheDrawing()
    {
        const ReadingDescriptor* d = readingFor(MeterBinding::TxSwr);
        QVERIFY(d);
        QVERIFY(d->hasScale);
        QCOMPARE(d->min, 1.0);
        QCOMPARE(d->max, 3.0);
        QVERIFY(d->threshold.has_value());
        QCOMPARE(d->threshold.value(), 2.5);
        // Linear: die Mitte des Bereichs ist die Mitte der Skala.
        QVERIFY(qAbs(d->fraction(2.0) - 0.5) < 1e-9);
        QCOMPARE(d->text(1.42), QStringLiteral("1.42"));
    }

    // Auf der Empfangsskala gibt es keine Schwelle — „Wo es keine
    // Schwelle gibt, gibt es keinen roten Abschnitt."
    void theReceiveScaleHasNoThreshold()
    {
        for (int id : {MeterBinding::SignalAvg, MeterBinding::SignalPeak}) {
            const ReadingDescriptor* d = readingFor(id);
            QVERIFY(d);
            QVERIFY2(!d->threshold.has_value(),
                     "die Empfangsskala hat eine Schwelle bekommen");
            QVERIFY(d->fractionOf != nullptr);   // gestaucht, nicht linear
        }
    }

    void signalTextReadsAsSUnits()
    {
        const ReadingDescriptor* d = readingFor(MeterBinding::SignalAvg);
        QVERIFY(d);
        QCOMPARE(d->text(kS9Dbm),         QStringLiteral("S9"));
        QCOMPARE(d->text(kS0Dbm + 42.0),  QStringLiteral("S7"));
        QCOMPARE(d->text(kS9Dbm + 20.0),  QStringLiteral("S9+20"));
        QCOMPARE(d->text(kSMaxDbm),       QStringLiteral("S9+60"));
    }

    // Der Deskriptor benennt sich über readingName() — den wörtlichen
    // Thetis-Port —, statt eine zweite Namensliste zu führen.
    void thetisNamesComeFromTheVerbatimPort()
    {
        const ReadingDescriptor* d = readingFor(MeterBinding::SignalAvg);
        QVERIFY(d);
        QCOMPARE(d->thetisName(), QStringLiteral("Signal Average"));
    }

    // ── Die zwei Quellen, die nicht aus WDSP kommen ──────────────────
    //
    // OE5SOS, 2026-08-18: Rauschflur und RADE-SNR „als Quelle ins
    // Instrument, nicht als eigene Zeile irgendwo". Beide standen
    // vorher nur als Beschriftung an der analogen S-Meter-Anzeige.

    void theNoiseFloorUsesThePanadaptersOwnRange()
    {
        const ReadingDescriptor* d = readingFor(MeterBinding::NoiseFloor);
        QVERIFY(d);
        QVERIFY2(d->hasScale, "ohne Skala bietet kein Instrument sie an");
        // PanadapterModel.h:186-187 — dieselben Grenzen, in denen der
        // Wert entsteht. Eine eigene Zahl hier waere eine zweite
        // Behauptung ueber denselben Bereich.
        QCOMPARE(d->min, -140.0);
        QCOMPARE(d->max,  -40.0);
        QVERIFY2(!d->threshold.has_value(),
                 "ein hoher Rauschflur ist ein schlechter Standort, "
                 "keine Gefahr");
    }

    // Auf der dBm-ACHSE, nicht auf der S-Skala. Ein Rauschflur ist kein
    // Signal; die Stauchung ueber S9 saehe hier aus wie eine
    // Eigenschaft des Rauschens.
    void theNoiseFloorIsLinearNotSCompressed()
    {
        const ReadingDescriptor* d = readingFor(MeterBinding::NoiseFloor);
        QVERIFY(d);
        // Die Mitte des Bereichs liegt bei der Haelfte der Skala —
        // das tut sie bei der S-Kennlinie gerade NICHT.
        const double mid = (d->min + d->max) / 2.0;
        QVERIFY(qAbs(d->fraction(mid) - 0.5) < 1e-9);

        const ReadingDescriptor* sig = readingFor(MeterBinding::SignalAvg);
        QVERIFY(sig);
        const double sigMid = (sig->min + sig->max) / 2.0;
        QVERIFY2(qAbs(sig->fraction(sigMid) - 0.5) > 0.01,
                 "die Empfangsskala ist nicht mehr gestaucht — dann sagt "
                 "der Vergleich oben nichts mehr aus");
    }

    // Der Bereich ist AUS DEM RADE-QUELLTEXT GERECHNET, nicht geraten:
    // rade_ofdm.c:412-424 [@b289102] mit den Konstanten aus
    // rade_dsp.h:59-65. Die Klemme snr_est <= 0 -> 0.1 in Zeile 413 ist
    // der kleinste Wert, den der Schaetzer melden kann.
    void theRadeSnrFloorComesFromTheEstimatorsOwnClamp()
    {
        const ReadingDescriptor* d = readingFor(MeterBinding::RadeSnr);
        QVERIFY(d);
        QVERIFY(d->hasScale);

        // Die Rechnung nachvollzogen, damit der Test faellt, wenn
        // jemand die Zahl ohne die Quelle aendert.
        constexpr double kFs = 8000.0, kNc = 30.0, kM = 160.0, kNcp = 32.0;
        const double snrEstFloor = 0.1;                     // Zeile 413
        double dB = 10.0 * std::log10(snrEstFloor);
        dB = (dB - 4.1343) / 0.7650;                        // Zeile 417-419
        const double Rs = kFs / kM;
        dB += 10.0 * std::log10(Rs * kNc / 3000.0)
            + 10.0 * std::log10((kM + kNcp) / kM);          // Zeile 422-424

        QVERIFY2(qAbs(dB - (-20.69)) < 0.02,
                 qPrintable(QStringLiteral("die RADE-Rechnung ergibt %1 dB, "
                                           "nicht -20.69").arg(dB)));
        // Abgerundet, damit die Skala nicht bei einer krummen Zahl
        // beginnt — aber NICHT darunter: ein Instrument, das mehr
        // Skala zeigt, als die Quelle je erreicht, zeigt Bereich, den
        // es nicht gibt.
        QCOMPARE(d->min, -21.0);
        QVERIFY(d->min <= dB);
    }

    // Die Grenze bei 5 dB (VfoWidget.cpp:930) ist UMGEKEHRT — klein ist
    // schlecht —, und `threshold` heisst „darueber ist es eine
    // Warnung". Sie hier einzutragen waere ein zweiter Begriff mit
    // demselben Namen.
    void theRadeSnrCarriesNoInvertedThreshold()
    {
        const ReadingDescriptor* d = readingFor(MeterBinding::RadeSnr);
        QVERIFY(d);
        QVERIFY(!d->threshold.has_value());
    }

    // PB SNR ist etwas anderes: Spitze-zu-Grundlinie aus dem Spektrum,
    // nicht die Schaetzung des RADE-Decoders. Es bleibt getrennt.
    void peakToBaselineSnrStaysSeparateFromRadeSnr()
    {
        const ReadingDescriptor* pb = readingFor(MeterBinding::PbSnr);
        const ReadingDescriptor* rade = readingFor(MeterBinding::RadeSnr);
        QVERIFY(pb);
        QVERIFY(rade);
        QVERIFY(pb->bindingId != rade->bindingId);
        QVERIFY(pb->label != rade->label);
    }
};

QTEST_MAIN(TestReadingSource)
#include "tst_reading_source.moc"
