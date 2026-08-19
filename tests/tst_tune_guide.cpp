// =================================================================
// tests/tst_tune_guide.cpp  (NereusSDR)
// =================================================================
//
// Die Abstimmhilfe im Panadapter: senkrechte Linie am Zeiger plus
// Frequenz auf das Hertz.
//
// Port aus AetherSDR (SpectrumWidget.cpp:15046-15074 [@0cd4559]);
// zweites der sieben Merkmale aus der Merkmalsliste vom 2026-08-19.
//
// Geprueft wird der ZUSTAND, nicht das gemalte Bild — dieselbe
// Entscheidung wie bei tst_squelch_line, und aus demselben Grund: ein
// Pixelvergleich haengt an Bezugspegel, Spanne und Kantenglaettung und
// braeche beim naechsten Feinschliff, ohne dass die Regel verletzt
// waere.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestTuneGuide : public QObject
{
    Q_OBJECT

private slots:

    // Aus als Vorgabe, wie m_showTuneGuides{false} bei AetherSDR: eine
    // Linie, die am Zeiger klebt, gehoert nicht ungefragt ins Bild.
    void offUntilAsked()
    {
        SpectrumWidget w;
        QVERIFY2(!w.tuneGuideEnabled(),
                 "die Abstimmhilfe darf sich nicht selbst einschalten");
        QVERIFY(!w.tuneGuideShowing());
    }

    // Der Schalter allein zeigt nichts. Sichtbar wird die Hilfe erst
    // durch Zeigerbewegung — das ist der Unterschied zwischen „erlaubt"
    // und „zu sehen", und die zwei Zustaende sind getrennt, damit das
    // Ausblenden den Schalter nicht umlegt.
    void enablingIsNotShowing()
    {
        SpectrumWidget w;
        w.setTuneGuideEnabled(true);
        QVERIFY(w.tuneGuideEnabled());
        QVERIFY2(!w.tuneGuideShowing(),
                 "ohne Zeigerbewegung ist nichts zu zeigen");
    }

    // Ausschalten loescht beides, und zwar sofort: bliebe m_showing
    // stehen, laege die Linie bis zum Ablauf der vier Sekunden weiter
    // im Bild, obwohl das Haekchen schon aus ist.
    void switchingOffClearsBoth()
    {
        SpectrumWidget w;
        w.setTuneGuideEnabled(true);
        w.setTuneGuideEnabled(false);
        QVERIFY(!w.tuneGuideEnabled());
        QVERIFY(!w.tuneGuideShowing());
    }

    // Zweimal derselbe Wert darf nicht zweimal speichern und malen —
    // der Waechter am Anfang des Setters. Dieselbe Form wie
    // setCursorFreqVisible daneben.
    void settingTheSameValueTwiceIsHarmless()
    {
        SpectrumWidget w;
        w.setTuneGuideEnabled(true);
        w.setTuneGuideEnabled(true);
        QVERIFY(w.tuneGuideEnabled());
    }
};

QTEST_MAIN(TestTuneGuide)
#include "tst_tune_guide.moc"
