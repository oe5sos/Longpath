// no-port-check: Longpath-original test for the tuning-step binding
// (2026-10-13).
//
// Die Rasterung des Panadapters -- Rad, Ziehen, Klicken -- laeuft ueber
// SpectrumWidget::m_stepHz (SpectrumWidget.cpp:9309, :9757, :10011).
// Gesetzt wurde der Wert genau einmal, beim Anlegen der Scheibe;
// SliceModel::stepHzChanged hatte im ganzen Baum keinen Zuhoerer. Wer
// also im RX-Applet die Schrittweite umstellte, drehte danach im
// Panadapter weiter in der alten.
//
// Upstream NereusSDR hat denselben Mangel nach 2026.10.0 behoben ("Keep
// each pan's displayed and wheel-tuning STEP bound to its selected
// receiver, including selection changes and replacement receivers").
//
// WAS DIESER LAUF NICHT PRUEFT: die Verdrahtung in MainWindow selbst.
// MainWindow ist in diesem Rahmen nicht konstruierbar -- es startet
// echte Discovery-Broadcasts, braucht rund 9 s fuer den ConnectionPanel
// und bricht beim Abbau ab; tst_pan_menu_routing.cpp,
// tst_mainwindow_status_bar_safety.cpp und tst_pan_active_slice_sync.cpp
// halten dieselbe Feststellung fest. Geprueft wird daher die Regel, die
// dort verdrahtet ist, an denselben echten Objekten (SliceModel,
// SpectrumWidget): dass das Signal kommt, dass der Empfaenger es
// annimmt, und dass eine NICHT aktive Scheibe nicht durchgreift.

#include <QtTest/QtTest>
#include <QtTest/QSignalSpy>

#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TestStepFollowsSlice : public QObject {
    Q_OBJECT

private:
    // Dieselbe Regel wie in MainWindow::wireSliceToSpectrum: nur die
    // aktive Scheibe schreibt die Rasterung.
    static void wireLikeMainWindow(RadioModel& radio, SliceModel* slice,
                                   SpectrumWidget* spectrum)
    {
        QObject::connect(slice, &SliceModel::stepHzChanged, spectrum,
                         [&radio, slice, spectrum](int hz) {
            if (slice != radio.activeSlice()) { return; }
            spectrum->setStepSize(hz);
        });
    }

private slots:
    void the_slice_announces_a_step_change()
    {
        // Ohne dieses Signal kann niemand folgen -- es war da, es hatte
        // nur keinen Zuhoerer.
        RadioModel radio;
        radio.addSlice();
        SliceModel* const slice = radio.sliceById(0);
        QVERIFY(slice);

        QSignalSpy steps(slice, &SliceModel::stepHzChanged);
        slice->setStepHz(500);
        QCOMPARE(steps.count(), 1);
        QCOMPARE(steps.last().at(0).toInt(), 500);
        QCOMPARE(slice->stepHz(), 500);

        // Derselbe Wert noch einmal darf nicht erneut feuern.
        slice->setStepHz(500);
        QCOMPARE(steps.count(), 1);
    }

    void the_spectrum_takes_the_new_step()
    {
        RadioModel radio;
        radio.addSlice();
        SliceModel* const slice = radio.sliceById(0);
        QVERIFY(slice);

        SpectrumWidget spectrum;
        spectrum.setStepSize(slice->stepHz());
        wireLikeMainWindow(radio, slice, &spectrum);

        slice->setStepHz(10);
        QCOMPARE(spectrum.stepSize(), 10);
        slice->setStepHz(1000);
        QCOMPARE(spectrum.stepSize(), 1000);
    }

    void a_background_slice_does_not_touch_the_rastering()
    {
        // Das Panadapter zeigt eine Scheibe. Eine zweite, die im
        // Hintergrund verstellt wird, darf ihr nicht in die Rasterung
        // greifen -- sonst rastet das Rad plötzlich in einer Schrittweite,
        // die zu nichts auf dem Schirm gehoert.
        RadioModel radio;
        radio.addSlice();
        radio.addSlice();
        SliceModel* const shown  = radio.sliceById(0);
        SliceModel* const hidden = radio.sliceById(1);
        QVERIFY(shown);
        QVERIFY(hidden);
        QCOMPARE(radio.activeSlice(), shown);

        SpectrumWidget spectrum;
        spectrum.setStepSize(100);
        wireLikeMainWindow(radio, shown, &spectrum);
        wireLikeMainWindow(radio, hidden, &spectrum);

        hidden->setStepHz(9000);
        QCOMPARE(spectrum.stepSize(), 100);   // unberuehrt

        shown->setStepHz(250);
        QCOMPARE(spectrum.stepSize(), 250);
    }

    void every_slice_carries_its_own_step()
    {
        // Grundlage fuer das Nachziehen beim Wechsel: die Schrittweite
        // gehoert der Scheibe, nicht dem Fenster.
        RadioModel radio;
        radio.addSlice();
        radio.addSlice();
        SliceModel* const a = radio.sliceById(0);
        SliceModel* const b = radio.sliceById(1);
        QVERIFY(a);
        QVERIFY(b);

        a->setStepHz(50);
        b->setStepHz(2500);
        QCOMPARE(a->stepHz(), 50);
        QCOMPARE(b->stepHz(), 2500);
    }
};

QTEST_MAIN(TestStepFollowsSlice)
#include "tst_step_follows_slice.moc"
