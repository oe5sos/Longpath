// no-port-check: Longpath-original test for the DEXP threshold marker
// (2026-10-12).
//
// The marker on the DEXP peak meter is drawn from the applet's slider.
// That slider sat at its build-time value (-50 dB) and knew nothing
// about the real threshold, which lives in TransmitModel::voxThresholdDb
// and is saved in the mic profile as "Dexp_Threshold"
// (MicProfileManager.cpp:95). So the real value survived every restart
// and the marker beside it did not -- after a start, and after any
// change in Setup, the line on the meter pointed at the wrong place.
//
// Upstream NereusSDR fixed the same thing after 2026.10.0 ("Keep the
// DEXP threshold marker across restarts", #332).
//
// The slider stays display-only: dragging it still moves the marker and
// nothing else. Setting the threshold belongs in Setup.

#include <QtTest/QtTest>
#include <QtWidgets/QSlider>

#include "gui/applets/PhoneCwApplet.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

class TestDexpThresholdMarker : public QObject {
    Q_OBJECT

private:
    static QSlider* markerSlider(PhoneCwApplet& applet)
    {
        return applet.findChild<QSlider*>(
            QStringLiteral("PhoneCwDexpThresholdSlider"));
    }

private slots:
    void the_marker_starts_at_the_saved_threshold()
    {
        // A restart is this: the model carries the saved value, the
        // applet is built fresh. The marker has to come up on the saved
        // value, not on the slider's build-time default.
        RadioModel radio;
        radio.transmitModel().setVoxThresholdDb(-23);

        PhoneCwApplet applet(&radio);
        QSlider* const slider = markerSlider(applet);
        QVERIFY(slider);
        QCOMPARE(slider->value(), -23);
    }

    void the_marker_follows_a_change_from_setup()
    {
        RadioModel radio;
        radio.transmitModel().setVoxThresholdDb(-40);

        PhoneCwApplet applet(&radio);
        QSlider* const slider = markerSlider(applet);
        QVERIFY(slider);
        QCOMPARE(slider->value(), -40);

        // Setup moves the real threshold while the applet is open.
        radio.transmitModel().setVoxThresholdDb(-12);
        QCOMPARE(slider->value(), -12);
    }

    void dragging_the_marker_does_not_move_the_real_threshold()
    {
        // Deliberate: the slider is the marker, not the control. Thetis
        // does the same (console.cs:28972 draws from ptbNoiseGate and
        // never pushes it into WDSP), and changing that would be a
        // change to how the radio is operated, not a fix.
        RadioModel radio;
        radio.transmitModel().setVoxThresholdDb(-40);

        PhoneCwApplet applet(&radio);
        QSlider* const slider = markerSlider(applet);
        QVERIFY(slider);

        slider->setValue(-70);
        QCOMPARE(radio.transmitModel().voxThresholdDb(), -40);
    }

    void no_feedback_loop_between_model_and_marker()
    {
        // The model signal moves the slider, the slider's own handler
        // redraws the marker -- that must not bounce back into the model
        // or ping-pong between the two.
        RadioModel radio;
        PhoneCwApplet applet(&radio);
        QSlider* const slider = markerSlider(applet);
        QVERIFY(slider);

        QSignalSpy modelChanges(&radio.transmitModel(),
                                &TransmitModel::voxThresholdDbChanged);
        radio.transmitModel().setVoxThresholdDb(-33);
        QCOMPARE(slider->value(), -33);
        QCOMPARE(modelChanges.count(), 1);   // exactly one, no echo
    }
};

QTEST_MAIN(TestDexpThresholdMarker)
#include "tst_dexp_threshold_marker.moc"
