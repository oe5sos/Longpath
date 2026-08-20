// =================================================================
// tests/tst_filter_width_rules.cpp  (NereusSDR)
// =================================================================
//
// Eine Breite eintippen und die Kanten setzen sich RICHTIG.
//
// Das ist der Teil, den man nicht sieht und ohne den ein Zahlenfeld
// nur ein zweiter Weg waere, dieselben zwei Kanten von Hand zu suchen.
// Je Betriebsart eine andere Regel — aus Thetis
// ptbFilterWidth_Scroll (console.cs:35318-35348) und der
// default_center-Rechnung in ptbFilterShift_Scroll
// (console.cs:35076-35097), v2.10.3.15-5-g852bf0e.
//
// ZWEI FALLEN, DIE HIER FESTGENAGELT WERDEN:
//
//   1. Bei AM, SAM, FM und DSB ist die ANGEZEIGTE Breite die HALBE.
//      Thetis rechnet vorher `bw /= 2` und setzt dann ±bw. Wer das
//      uebersieht, bekommt bei AM den doppelten Durchlass — und das
//      hoert man erst, wenn der Nachbarkanal mitkommt.
//
//   2. Bei CW muss die Mitte auf dem MITHOERTON bleiben. Ein
//      Durchlass, der beim Schmalerziehen wandert, laesst den Ton
//      wandern — der klassische Grund, warum eine Station beim
//      Einengen plötzlich weg ist.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>

#include "core/AppSettings.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestFilterWidthRules : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        // Feste Werte, damit der Test nicht davon abhaengt, was in der
        // Einstellungsdatei des Rechners steht.
        AppSettings::instance().setValue(QStringLiteral("CWPitch"),
                                         QStringLiteral("600"));
        AppSettings::instance().setValue(QStringLiteral("DefaultLowCut"),
                                         QStringLiteral("150"));
    }

    // ── SSB haengt an der Standardflanke ─────────────────────────────

    void upperSidebandGrowsUpwardsFromTheLowCut()
    {
        int low = 0, high = 0;
        SliceModel::widthToEdges(2400, DSPMode::USB, /*center=*/0, low, high);
        QCOMPARE(low,  150);
        QCOMPARE(high, 2550);
    }

    void lowerSidebandGrowsDownwardsFromTheLowCut()
    {
        int low = 0, high = 0;
        SliceModel::widthToEdges(2400, DSPMode::LSB, 0, low, high);
        QCOMPARE(high, -150);
        QCOMPARE(low,  -2550);
    }

    void theSsbAnchorDoesNotMoveWithTheWidth()
    {
        int lo1 = 0, hi1 = 0, lo2 = 0, hi2 = 0;
        SliceModel::widthToEdges(1800, DSPMode::USB, 0, lo1, hi1);
        SliceModel::widthToEdges(3200, DSPMode::USB, 0, lo2, hi2);
        QVERIFY2(lo1 == lo2,
                 "die untere Flanke ist der Anker — sie darf sich beim "
                 "Verbreitern nicht bewegen");
    }

    // ── CW bleibt auf dem Mithoerton ─────────────────────────────────

    void cwStaysCentredOnItsPitch()
    {
        // Mitte auf −600 (CWL bei 600 Hz Mithoerton).
        int low = 0, high = 0;
        SliceModel::widthToEdges(400, DSPMode::CWL, -600, low, high);
        QCOMPARE(low,  -800);
        QCOMPARE(high, -400);

        // Schmaler ziehen: die Mitte muss stehenbleiben.
        SliceModel::widthToEdges(100, DSPMode::CWL, -600, low, high);
        QCOMPARE((low + high) / 2, -600);
    }

    void theDefaultCentreForCwIsThePitch()
    {
        QCOMPARE(SliceModel::defaultFilterCenter(DSPMode::CWU, 500),  600);
        QCOMPARE(SliceModel::defaultFilterCenter(DSPMode::CWL, 500), -600);
    }

    void theDefaultCentreForSsbFollowsTheWidth()
    {
        // From Thetis: default_low_cut + bw/2.
        QCOMPARE(SliceModel::defaultFilterCenter(DSPMode::USB, 2400),
                 150 + 1200);
        QCOMPARE(SliceModel::defaultFilterCenter(DSPMode::LSB, 2400),
                 -150 - 1200);
    }

    void digitalModesSitOnTheirClickTuneOffset()
    {
        // From Thetis console.cs:14636 / :14671.
        QCOMPARE(SliceModel::defaultFilterCenter(DSPMode::DIGU, 500),  1500);
        QCOMPARE(SliceModel::defaultFilterCenter(DSPMode::DIGL, 500), -2210);
    }

    // ── DIE FALLE: bei AM ist die angezeigte Breite die halbe ────────

    void amTakesTheWidthAsAHalfWidth()
    {
        int low = 0, high = 0;
        SliceModel::widthToEdges(3000, DSPMode::AM, 0, low, high);
        QVERIFY2(low == -3000 && high == 3000,
                 "bei AM setzt Thetis ±Breite, nicht ±Breite/2 — die "
                 "angezeigte Zahl IST die halbe Bandbreite "
                 "(console.cs:35225)");
    }

    void ssbTakesTheWidthAsAFullWidth()
    {
        // Zum Vergleich, damit der Unterschied festgenagelt ist.
        int low = 0, high = 0;
        SliceModel::widthToEdges(3000, DSPMode::USB, 0, low, high);
        QCOMPARE(high - low, 3000);
    }

    // ── Und am Modell, durch die Begrenzung hindurch ─────────────────

    void settingTheWidthKeepsTheCentreInCw()
    {
        SliceModel s;
        s.setDspMode(DSPMode::CWL);
        s.setFilter(-850, -350);          // Mitte −600
        QCOMPARE(s.filterCenter(), -600);

        s.setFilterWidth(200);
        QCOMPARE(s.filterCenter(), -600);
        QCOMPARE(s.filterWidth(),  200);
    }

    void movingTheCentreKeepsTheWidth()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setFilter(150, 2550);           // 2 400 Hz breit

        s.setFilterCenter(2000);
        QCOMPARE(s.filterWidth(),  2400);
        QCOMPARE(s.filterCenter(), 2000);
    }

    // DER FALL, DER BEIM HOEREN AUFFAELLT: am Rand darf der Durchlass
    // nicht heimlich schmaler werden.
    void movingTheCentreAgainstTheSidebandKeepsTheWidth()
    {
        SliceModel s;
        s.setDspMode(DSPMode::USB);
        s.setLimitFiltersToSidebands(true);
        s.setFilter(150, 2550);

        s.setFilterCenter(300);           // wuerde unter null reichen
        QCOMPARE(s.filterLow(),   0);
        QVERIFY2(s.filterWidth() == 2400,
                 "beim Verschieben muss die Breite erhalten bleiben");
    }

    void resettingTheCentreGoesBackToTheModeDefault()
    {
        SliceModel s;
        s.setDspMode(DSPMode::CWU);
        s.setFilter(300, 700);            // Mitte 500, nicht auf dem Ton
        QCOMPARE(s.filterCenter(), 500);

        s.resetFilterCenter();
        QCOMPARE(s.filterCenter(), 600);  // Mithoerton
        QCOMPARE(s.filterWidth(),  400);
    }
};

QTEST_MAIN(TestFilterWidthRules)
#include "tst_filter_width_rules.moc"
