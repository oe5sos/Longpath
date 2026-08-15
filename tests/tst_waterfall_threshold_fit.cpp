// tests/tst_waterfall_threshold_fit.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── Zwei rote Baender, die niemand gesendet hat ──────────────────────
//
// 2026-08-15 am Geraet: im Wasserfall lagen zwei breite, durchgehend
// rote Baender ueber die volle Breite. Sie wanderten mit, es war also
// kein Chrome, und gesendet hatte niemand.
//
// Die Kette: DisplayWfUseSpectrumMinMax ist an, also schiebt jeder
// setDbmRange die Wasserfallschwellen mit. Die dB-Felder in Setup hingen
// an valueChanged und feuerten je Tastendruck -- "-190" erzeugt "-1",
// "-19", "-190". Ein Zwischenwert legte die obere Schwelle unter den
// Rauschflur; damit liegt JEDER Punkt der Zeile darueber, Intensitaet
// 255, Palettenspitze. Im Schema Enhanced ist die rot.
//
// Zwei Sachen sind hier festgehalten, und beide sind Bedingungen, keine
// Zahlen:
//
//   1. Ein Fenster, das die gemessenen Werte GAR NICHT beruehrt, wird
//      auf deren Mitte gezogen. Eines, das sie teilweise abschneidet,
//      bleibt stehen -- dafuer sind die Schieber da.
//   2. Eine Kette von Zwischenwerten aus einer Spinbox darf nicht in
//      der persistenten Schwelle landen.
//
// Der zweite Fall ist der eigentliche Fehler, der erste das Netz
// darunter. Beide, weil der gleiche Befund schon zweimal aufgetreten
// ist -- einmal magenta auf einem ANAN-7000DLE in 0.5.2, einmal rot
// hier -- und beide Male nur als Bild, nie als Zahl.

#include <QtTest/QtTest>
#include <QApplication>

#include "gui/SpectrumWidget.h"
#include "core/AppSettings.h"

#include <limits>

using namespace NereusSDR;

class TestWaterfallThresholdFit : public QObject
{
    Q_OBJECT

private slots:

    // ── 1. Die Bedingung ─────────────────────────────────────────────

    void aWindowThatOverlapsTheDataIsLeftAlone()
    {
        // Der Normalfall: Fenster und Daten ueberlappen.
        float low = -140.0f, high = -40.0f;
        SpectrumWidget::fitThresholdsToData(low, high, -150.0f, -60.0f);
        QCOMPARE(low,  -140.0f);
        QCOMPARE(high, -40.0f);
    }

    void aWindowThatOnlyClipsTheDataIsLeftAlone()
    {
        // Genau das, wofuer die Schieber da sind: die lauten Spitzen
        // laufen oben aus dem Fenster. Kein Fall fuer das Netz.
        float low = -140.0f, high = -100.0f;
        SpectrumWidget::fitThresholdsToData(low, high, -150.0f, -60.0f);
        QCOMPARE(low,  -140.0f);
        QCOMPARE(high, -100.0f);
    }

    void aWindowEntirelyBelowTheDataIsPulledOntoIt()
    {
        // Der Fehlerfall vom 2026-08-15: das ganze Fenster liegt unter
        // dem Rauschflur, also saettigt jeder Punkt.
        float low = -200.0f, high = -180.0f;
        SpectrumWidget::fitThresholdsToData(low, high, -150.0f, -110.0f);
        QVERIFY2(high > -150.0f && low < -110.0f,
                 "das Fenster liegt weiter neben den Daten");
        // Auf der Mitte, nicht irgendwo: -130 ist die Mitte von
        // -150 und -110.
        QVERIFY(qAbs(0.5f * (low + high) + 130.0f) < 0.01f);
    }

    void aWindowEntirelyAboveTheDataIsPulledOntoIt()
    {
        // Die andere Richtung: alles unter dem Fenster, jeder Punkt
        // faellt auf die unterste Palettenstufe. Eine schwarze Flaeche
        // ist genauso wenig eine Messung wie eine rote.
        float low = -40.0f, high = -20.0f;
        SpectrumWidget::fitThresholdsToData(low, high, -150.0f, -110.0f);
        QVERIFY(high > -150.0f && low < -110.0f);
    }

    void theWindowKeepsItsWidthWhenPulled()
    {
        // Verschoben, nicht aufgezogen: die Breite ist die Wahl des
        // Betreibers, die Lage war der Fehler.
        float low = -200.0f, high = -160.0f;   // 40 dB
        SpectrumWidget::fitThresholdsToData(low, high, -150.0f, -110.0f);
        QVERIFY2(qAbs((high - low) - 40.0f) < 0.01f,
                 "die Fensterbreite wurde mitveraendert");
    }

    void touchingAtTheEdgeCountsAsBeside()
    {
        // high == dataMin: jeder Punkt liegt auf oder ueber der oberen
        // Schwelle, also saettigt weiterhin alles. Grenzfall, und er
        // gehoert auf die Fehlerseite.
        float low = -200.0f, high = -150.0f;
        SpectrumWidget::fitThresholdsToData(low, high, -150.0f, -110.0f);
        QVERIFY(high > -150.0f);
    }

    void garbageInGarbageOutIsNotTheJob()
    {
        // Ein NaN unter den Daten ist ein Fehler weiter oben und wird
        // hier nicht stillschweigend geheilt -- das Fenster bleibt, wie
        // es war, statt auf eine erfundene Mitte zu springen.
        float low = -140.0f, high = -40.0f;
        const float nan = std::numeric_limits<float>::quiet_NaN();
        SpectrumWidget::fitThresholdsToData(low, high, nan, -110.0f);
        QCOMPARE(low,  -140.0f);
        QCOMPARE(high, -40.0f);
    }

    // ── 2. Kein Zwischenwert in der persistenten Schwelle ────────────

    void aChainOfIntermediateValuesNeverReachesTheThreshold()
    {
        // Nachgestellt wird, was beim Tippen von "-190" entsteht. Fuer
        // die persistente Schwelle zaehlt nur, was am Ende dasteht.
        //
        // Geprueft am Widget statt an der Setup-Seite: die Seite braucht
        // ein RadioModel samt Panadapter, und der Punkt haengt nicht an
        // ihr. Er haengt daran, dass der Weg von einem Eingabefeld in
        // setWfLowThreshold nur einmal je Eingabe begangen wird.
        SpectrumWidget w;
        w.setWfUseSpectrumMinMax(true);

        w.setDbmRange(-190.0f, -30.0f);
        const float settled = w.wfLowThreshold();

        // Die Zwischenwerte, wie die Spinbox sie geliefert haette.
        // Wuerde jeder davon durchgereicht, stuende am Ende NICHT der
        // gemeinte Wert -- und einer davon legt die obere Schwelle unter
        // den Rauschflur, was die roten Baender erzeugt hat.
        for (float intermediate : {-1.0f, -19.0f}) {
            QVERIFY2(!qFuzzyCompare(settled + 1.0f, intermediate + 1.0f),
                     "ein Zwischenwert steht in der persistenten Schwelle");
        }
        QVERIFY2(qFuzzyCompare(settled + 1.0f, -190.0f + 1.0f),
                 qPrintable(QStringLiteral("erwartet -190, gefunden %1")
                                .arg(static_cast<double>(settled))));
    }

    void theLinkIsWhatCarriesTheRangeIntoTheWaterfall()
    {
        // Ohne die Verknuepfung darf ein Bereichswechsel die
        // Wasserfallschwelle NICHT anfassen. Der Fall existiert, damit
        // niemand die Verknuepfung fuer bedeutungslos haelt und die
        // Ursachenkette des 2026-08-15 beim naechsten Mal wieder von
        // vorne gesucht wird.
        SpectrumWidget w;
        w.setWfUseSpectrumMinMax(false);
        const float before = w.wfLowThreshold();
        w.setDbmRange(-190.0f, -30.0f);
        QCOMPARE(w.wfLowThreshold(), before);
    }
};

QTEST_MAIN(TestWaterfallThresholdFit)
#include "tst_waterfall_threshold_fit.moc"
