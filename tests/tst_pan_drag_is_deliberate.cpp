// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Ansicht verschiebt sich nur noch auf eine BEWUSSTE Geste.
//
// Live gesehen am 2026-08-22: der Betreiber rutschte mit einem
// unabsichtlichen Zug von 7,07 auf 6,38 MHz, der VFO blieb ausserhalb
// des Bilds zurueck. AetherSDRs Regel — jeder Druck ins Spektrum wird
// zum Verschieben — ist fuer den Funkbetrieb die falsche: die Flaeche,
// auf die man staendig zeigt und klickt, darf nicht gleichzeitig der
// Griff sein, der die Ansicht wegtraegt.
//
// Neu gilt: Verschieben NUR an der Frequenzskala. Im Spektrum bleibt
// der kurze Klick das Abstimmen; eine Bewegung dort verpufft.

#include <QtTest>
#include "gui/SpectrumWidget.h"

#include <QMouseEvent>

namespace {
// QTest::mouseMove liefert KEINE Move-Ereignisse mit gedrueckter
// Taste — ein "Zug" aus press/move/release prueft deshalb nichts.
// Daran ist die erste Fassung dieses Tests gescheitert (beide
// Richtungen unentscheidbar). Also die Ereignisse selbst bauen.
void drag(QWidget* w, QPoint from, QPoint to)
{
    QMouseEvent press(QEvent::MouseButtonPress, from, w->mapToGlobal(from),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &press);
    const int steps = 8;
    for (int i = 1; i <= steps; ++i) {
        const QPoint p = from + (to - from) * i / steps;
        QMouseEvent move(QEvent::MouseMove, p, w->mapToGlobal(p),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(w, &move);
    }
    QMouseEvent release(QEvent::MouseButtonRelease, to, w->mapToGlobal(to),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &release);
    QCoreApplication::processEvents();
}
} // namespace

using namespace Longpath;

class TstPanDragIsDeliberate : public QObject
{
    Q_OBJECT

private:
    static SpectrumWidget* make()
    {
        auto* w = new SpectrumWidget;
        w->resize(1000, 600);
        // Verbunden stellen: getrennt schluckt der Waechter jeden
        // Linksklick (Phase 3Q-8) und dieser Test prueft gar nichts.
        // Genau daran sind die ersten zwei Laeufe gescheitert.
        w->setConnectionState(ConnectionState::Connected);
        w->setDdcCenterFrequency(7'100'000.0);
        w->setSampleRate(192'000.0);
        w->setFrequencyRange(7'100'000.0, 192'000.0);
        w->show();
        return w;
    }

private slots:
    /// Ein Zug quer durchs Spektrum laesst die Mitte stehen.
    void aDragAcrossTheSpectrumDoesNotPan()
    {
        QScopedPointer<SpectrumWidget> w(make());
        QVERIFY(QTest::qWaitForWindowExposed(w.data()));

        const double before = w->centerFrequency();
        drag(w.data(), QPoint(500, 120), QPoint(200, 120));

        QCOMPARE(w->centerFrequency(), before);
    }

    /// Ein Zug an der FREQUENZSKALA verschiebt — die bewusste Geste.
    void aDragOnTheFrequencyScaleStillPans()
    {
        QScopedPointer<SpectrumWidget> w(make());
        QVERIFY(QTest::qWaitForWindowExposed(w.data()));

        const double before = w->centerFrequency();
        // Die Skala liegt zwischen Spektrum und Wasserfall.
        const int scaleY = w->freqScaleYForTest() + 8;
        drag(w.data(), QPoint(500, scaleY), QPoint(300, scaleY));

        QVERIFY2(w->centerFrequency() != before,
                 "Auch die Skala verschiebt nicht mehr — dann gibt es "
                 "gar keinen Weg mehr, die Ansicht zu bewegen");
    }

    /// Und der kurze Klick im Spektrum stimmt weiter ab.
    void aShortClickStillTunes()
    {
        QScopedPointer<SpectrumWidget> w(make());
        QVERIFY(QTest::qWaitForWindowExposed(w.data()));

        QSignalSpy tuned(w.data(), &SpectrumWidget::frequencyClicked);
        // Weit weg vom Durchlassbereich: mitten darin waere der Druck
        // ein VFO-Zug, kein Abstimmklick — auch das hat die erste
        // Fassung uebersehen.
        QTest::mouseClick(w.data(), Qt::LeftButton, Qt::NoModifier,
                          QPoint(150, 120));
        QCoreApplication::processEvents();

        QCOMPARE(tuned.count(), 1);
    }
};

QTEST_MAIN(TstPanDragIsDeliberate)
#include "tst_pan_drag_is_deliberate.moc"
