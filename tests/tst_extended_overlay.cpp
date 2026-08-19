// =================================================================
// tests/tst_extended_overlay.cpp  (NereusSDR)
// =================================================================
//
// Die Verlaengerung von Mittellinie, Filterkanten und Durchlassflaeche
// in den Wasserfall — ab 2026-08-19 abschaltbar.
//
// Port aus AetherSDR setExtendedFrequencyLine + setExtendedPassband
// (SpectrumWidget.cpp:4094-4130 [@0cd4559]).
//
// Der Kern dieser Faelle ist die VORGABE: bei AetherSDR stehen beide
// Schalter auf aus, bei uns auf ein. Wir malen die Verlaengerung seit je
// unbedingt, und eine Vorgabe darf niemandem das Bild umstellen. Faellt
// einer dieser Faelle, hat jemand die Vorgabe gedreht — und das ist
// genau der Fehler, den er fangen soll.
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

class TestExtendedOverlay : public QObject
{
    Q_OBJECT

private slots:

    // AetherSDR: beide aus. Wir: beide ein, weil das der Istzustand ist,
    // den Betreiber kennen.
    void bothOnByDefault()
    {
        SpectrumWidget w;
        QVERIFY2(w.extendedFrequencyLine(),
                 "die Mittellinie lief seit je durch den Wasserfall");
        QVERIFY2(w.extendedPassband(),
                 "die Durchlassflaeche lief seit je durch den Wasserfall");
    }

    void bothCanBeSwitchedOff()
    {
        SpectrumWidget w;
        w.setExtendedFrequencyLine(false);
        w.setExtendedPassband(false);
        QVERIFY(!w.extendedFrequencyLine());
        QVERIFY(!w.extendedPassband());
    }

    // Zwei Schalter, zwei Merkmale: die Linie abzuschalten darf die
    // Flaeche nicht mitnehmen. Bei AetherSDR sind es ebenfalls zwei.
    void theTwoAreIndependent()
    {
        SpectrumWidget w;
        w.setExtendedFrequencyLine(false);
        QVERIFY2(w.extendedPassband(),
                 "die Flaeche haengt nicht an der Linie");
        w.setExtendedFrequencyLine(true);
        w.setExtendedPassband(false);
        QVERIFY2(w.extendedFrequencyLine(),
                 "die Linie haengt nicht an der Flaeche");
    }

    void settingTheSameValueTwiceIsHarmless()
    {
        SpectrumWidget w;
        w.setExtendedPassband(false);
        w.setExtendedPassband(false);
        QVERIFY(!w.extendedPassband());
    }
};

QTEST_MAIN(TestExtendedOverlay)
#include "tst_extended_overlay.moc"
