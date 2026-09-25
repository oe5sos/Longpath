// no-port-check: Longpath-original test, no Thetis logic.
//
// coveredFraction() (gui/WindowPlacement): welcher Anteil eines Applets
// unter anderen Fenstern liegt. Betreiber 2026-09-25: das eingeschaltete
// RX-Applet lag in der Spalte unter Panadapter und Rotor/Log.

#include <QtTest/QtTest>

#include "gui/WindowPlacement.h"

using namespace Longpath;

class TestWindowCoverage : public QObject {
    Q_OBJECT
private slots:
    void nothingAboveIsZero()
    {
        QCOMPARE(coveredFraction(QRect(0, 0, 300, 200), {}), 0.0);
        QCOMPARE(coveredFraction(QRect(0, 0, 300, 200), {QRect(400, 0, 100, 100)}), 0.0);
    }

    void fullyUnderOneWindow()
    {
        QCOMPARE(coveredFraction(QRect(100, 100, 300, 200),
                                 {QRect(0, 0, 1000, 1000)}), 1.0);
    }

    void halfUnderOneWindow()
    {
        QCOMPARE(coveredFraction(QRect(0, 0, 320, 160),
                                 {QRect(160, -10, 500, 500)}), 0.5);
    }

    // Zwei Fenster, die sich selbst ueberlappen, zaehlen nicht doppelt.
    void overlappingCoversCountOnce()
    {
        const QRect target(0, 0, 320, 160);
        const QList<QRect> covers{QRect(0, 0, 160, 160), QRect(80, 0, 80, 160)};
        QCOMPARE(coveredFraction(target, covers), 0.5);
    }

    // Der Fall vom 2026-09-25: links der Panadapter, rechts Rotor/Log --
    // zusammen liegt das Applet fast ganz darunter.
    void appletBetweenPanadapterAndRotor()
    {
        const QRect rx(1022, 107, 418, 318);
        const QList<QRect> covers{QRect(8, 300, 1180, 320),    // Panadapter
                                  QRect(1186, 120, 284, 650),  // Rotor/Log
                                  QRect(0, 120, 1186, 170)};   // TX/Filter
        QVERIFY(coveredFraction(rx, covers) > 0.9);
    }

    void emptyTargetCountsAsHidden()
    {
        QCOMPARE(coveredFraction(QRect(), {}), 1.0);
    }
};

QTEST_GUILESS_MAIN(TestWindowCoverage)
#include "tst_window_coverage.moc"
