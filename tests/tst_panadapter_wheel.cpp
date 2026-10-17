// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original regression test.
//
// Panadapter-Mausrad (2026-09-27). Jedes Radereignis zaehlte als voller
// Schritt -- ein Trackpad schickt Dutzende kleine je Wischen, und das
// Abstimmen oder Zoomen lief davon. Zoomen per Rad ging an
// setFrequencyRange vorbei (kein frequencyRangeChanged, dem die
// Zoom-Leiste folgt). Abstimmen rastete nicht auf das Schrittraster
// (Thetis SnapTune).

#include <QtTest>
#include <QSignalSpy>
#include <QWheelEvent>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

namespace {
QWheelEvent wheel(QPoint pixel, QPoint angle, Qt::KeyboardModifiers mods = Qt::NoModifier,
                  Qt::ScrollPhase phase = Qt::ScrollUpdate)
{
    return QWheelEvent(QPointF(200, 60), QPointF(200, 60), pixel, angle,
                       Qt::NoButton, mods, phase, false);
}
} // namespace

class TstPanadapterWheel : public QObject {
    Q_OBJECT

    static void setUp(SpectrumWidget& w)
    {
        w.resize(800, 400);
        w.setSampleRate(192000);
        w.setDdcCenterFrequency(14200000);
        w.setFrequencyRange(14200000, 96000);
        w.setVfoFrequency(14215050);
        w.setStepSize(100);
    }

private slots:
    void snapTuneLikeThetis()
    {
        QCOMPARE(SpectrumWidget::snapTuneHz(14215050, 100, 1),  14215100.0);
        QCOMPARE(SpectrumWidget::snapTuneHz(14215050, 100, -1), 14215000.0);
        QCOMPARE(SpectrumWidget::snapTuneHz(14215000, 100, -1), 14214900.0);
        QCOMPARE(SpectrumWidget::snapTuneHz(14215000, 100, 1),  14215100.0);
        QCOMPARE(SpectrumWidget::snapTuneHz(14215000, 1000, 3), 14218000.0);
    }

    // Ein Trackpad-Wischen aus zehn kleinen Ereignissen ist kein
    // Zehnfach-Schritt; der Nachlauf zaehlt nicht.
    void aTrackpadSwipeIsNotTenSteps()
    {
        SpectrumWidget w;
        setUp(w);
        QSignalSpy tune(&w, &SpectrumWidget::frequencyClicked);
        for (int i = 0; i < 10; ++i) {
            QWheelEvent ev = wheel(QPoint(0, 3), QPoint(0, 3));
            QCoreApplication::sendEvent(&w, &ev);
        }
        for (int i = 0; i < 10; ++i) {
            QWheelEvent ev = wheel(QPoint(0, 30), QPoint(0, 30), Qt::NoModifier, Qt::ScrollMomentum);
            QCoreApplication::sendEvent(&w, &ev);
        }
        QVERIFY2(tune.count() <= 2, qPrintable(QString::number(tune.count())));
        QVERIFY(tune.count() >= 1);
        QCOMPARE(tune.first().at(0).toDouble(), 14215100.0);   // eingerastet
    }

    // Ein Mausrad: jede Raste ein Schritt.
    void eachMouseNotchIsOneStep()
    {
        SpectrumWidget w;
        setUp(w);
        QSignalSpy tune(&w, &SpectrumWidget::frequencyClicked);
        for (int i = 0; i < 3; ++i) {
            QWheelEvent ev = wheel(QPoint(), QPoint(0, 120));
            QCoreApplication::sendEvent(&w, &ev);
            QTest::qWait(60);
        }
        QCOMPARE(tune.count(), 3);
        // Halbe Rasten (hochaufloesendes Rad) summieren sich.
        tune.clear();
        for (int i = 0; i < 2; ++i) {
            QWheelEvent ev = wheel(QPoint(), QPoint(0, 60));
            QCoreApplication::sendEvent(&w, &ev);
        }
        QCOMPARE(tune.count(), 1);
    }

    // Zoomen per Rad meldet den neuen Bereich wie jeder andere Zoom.
    void wheelZoomReportsTheNewRange()
    {
        SpectrumWidget w;
        setUp(w);
        QSignalSpy range(&w, &SpectrumWidget::frequencyRangeChanged);
        QSignalSpy bw(&w, &SpectrumWidget::bandwidthChangeRequested);
        QWheelEvent ev = wheel(QPoint(), QPoint(0, 120), Qt::ControlModifier);
        QCoreApplication::sendEvent(&w, &ev);
        QCOMPARE(bw.count(), 1);
        QVERIFY(range.count() >= 1);
        QVERIFY(w.bandwidth() < 96000.0);
        QCOMPARE(w.centerFrequency(), 14215050.0);   // wie bisher: auf den VFO
    }

    void gridStepFromSetup()
    {
        SpectrumWidget w;
        QCOMPARE(w.gridStepDb(), 0);
        w.setGridStepDb(6);
        QCOMPARE(w.gridStepDb(), 6);
        w.setGridStepDb(99);
        QCOMPARE(w.gridStepDb(), 40);
    }
};

QTEST_MAIN(TstPanadapterWheel)
#include "tst_panadapter_wheel.moc"
