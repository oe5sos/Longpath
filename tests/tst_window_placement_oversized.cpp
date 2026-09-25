// =================================================================
// tests/tst_window_placement_oversized.cpp  (Longpath)
// =================================================================
//
// ensureOnVisibleScreen() mit einem Fenster, das groesser ist als die
// verfuegbare Bildschirmflaeche.
//
// Befund 2026-09-25 (voller Debug/ASAN-Lauf): qBound(min, x, max) bekam
// max < min, sobald die Mindestgroesse die Flaeche uebersteigt -- Qts
// Zusicherung "!(max < min)" brach tst_pan_floating_window,
// tst_panadapter_stack_layouts und tst_real_pan_float_state ab. Im
// Release-Bau kam still die linke/obere Kante heraus; das ist jetzt das
// ausdrueckliche Verhalten und wird hier festgehalten. Im Debug-Bau
// bricht dieser Test ohne die Behebung ab.
// =================================================================

#include <QtTest/QtTest>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

#include "gui/WindowPlacement.h"

using namespace Longpath;

class TstWindowPlacementOversized : public QObject {
    Q_OBJECT

private slots:
    void aWindowLargerThanTheScreenStartsAtItsTopLeft()
    {
        QScreen* screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect avail = screen->availableGeometry();

        QWidget w;
        const QSize tooBig(avail.width() + 400, avail.height() + 300);
        ensureOnVisibleScreen(&w, nullptr, tooBig);

        qDebug() << "Flaeche" << avail << "Fenster" << w.geometry();
        QCOMPARE(w.geometry().topLeft(), avail.topLeft());
        QVERIFY(w.width()  >= tooBig.width());
        QVERIFY(w.height() >= tooBig.height());
    }

    // Nur zu breit, nicht zu hoch: waagrecht an die Kante, senkrecht
    // weiter mittig -- die Achsen sind unabhaengig.
    void onlyTooWideKeepsTheVerticalCentring()
    {
        QScreen* screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect avail = screen->availableGeometry();

        QWidget w;
        const QSize wide(avail.width() + 200, avail.height() / 2);
        ensureOnVisibleScreen(&w, nullptr, wide);

        QCOMPARE(w.geometry().left(), avail.left());
        QCOMPARE(w.geometry().top(), avail.y() + (avail.height() - w.height()) / 2);
    }
};

QTEST_MAIN(TstWindowPlacementOversized)
#include "tst_window_placement_oversized.moc"
