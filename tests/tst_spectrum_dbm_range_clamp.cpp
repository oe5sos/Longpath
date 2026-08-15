// tests/tst_spectrum_dbm_range_clamp.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── Eine Spanne, die auf null zulaufen konnte ────────────────────────
//
// m_dynamicRange ist der Nenner jeder Umrechnung von dBm nach Bildpunkt
// (dbmToY, dbmToYf). Null teilt durch null; negativ stellt die Skala auf
// den Kopf, und dann sitzen Kurve, Marken und Gitterlinien alle falsch.
//
// Erreichbar war das über die NF-Gitternachführung. onNoiseFloorChanged
// schiebt das Minimum auf den Rauschflur und lässt das Maximum stehen,
// solange maintainNFAdjustDelta aus ist (Vorgabe). Auf einem lauten Band
// steigt der Flur, das Minimum wandert nach oben — und nichts hielt es
// am Maximum auf.
//
// Der umgekehrte Fall, nach dem beim Befund gefragt wurde, ist harmlos:
// ohne Antenne oder auf einem ruhigen Band sinkt der Flur, das Minimum
// wandert nach unten, die Spanne wird groesser. Deshalb steht er hier
// als eigener Fall — damit niemand den Schutz spaeter fuer beide
// Richtungen haelt und die falsche Seite anzieht.

#include <QtTest/QtTest>
#include <QApplication>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestSpectrumDbmRangeClamp : public QObject
{
    Q_OBJECT

private slots:

    void anOrdinaryRangeIsLeftAlone()
    {
        SpectrumWidget w;
        w.setDbmRange(-190.0f, -30.0f);
        QCOMPARE(w.refLevel(), -30.0f);
        QCOMPARE(w.dynamicRange(), 160.0f);
    }

    void aCollapsedRangeIsHeldOpen()
    {
        SpectrumWidget w;
        // Der Fall aus der NF-Nachfuehrung: das Minimum ist bis auf das
        // Maximum hochgewandert.
        w.setDbmRange(-40.0f, -40.0f);
        QVERIFY2(w.dynamicRange() >= SpectrumWidget::kMinSpanDb,
                 "die Spanne lief auf null zu");
        QCOMPARE(w.refLevel(), -40.0f);   // der Bezugspegel bleibt stehen
    }

    void anInvertedRangeIsNeverNegative()
    {
        SpectrumWidget w;
        // Minimum ueber dem Maximum — die Skala stuende auf dem Kopf.
        w.setDbmRange(-20.0f, -120.0f);
        QVERIFY2(w.dynamicRange() > 0.0f,
                 "m_dynamicRange wurde negativ");
        QVERIFY(w.dynamicRange() >= SpectrumWidget::kMinSpanDb);
    }

    void aQuietBandWidensRatherThanNarrows()
    {
        SpectrumWidget w;
        w.setDbmRange(-190.0f, -30.0f);
        // Ohne Antenne sinkt der Rauschflur, die Nachfuehrung schiebt das
        // Minimum nach unten. Das ist kein Fall fuer den Schutz, und er
        // darf hier nichts anfassen.
        w.setDbmRange(-200.0f, -30.0f);
        QCOMPARE(w.dynamicRange(), 170.0f);
    }

    void theClampLeavesTheTopWhereItWas()
    {
        SpectrumWidget w;
        // Aufgeweitet wird nach unten, nicht nach oben: das Maximum ist
        // der Bezugspegel, und ihn zu verschieben hiesse, die Anzeige
        // ueber den Kopf des Betreibers hinweg umzuskalieren.
        w.setDbmRange(-42.0f, -40.0f);
        QCOMPARE(w.refLevel(), -40.0f);
        QCOMPARE(w.dynamicRange(), SpectrumWidget::kMinSpanDb);
    }
};

QTEST_MAIN(TestSpectrumDbmRangeClamp)
#include "tst_spectrum_dbm_range_clamp.moc"
