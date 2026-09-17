// SPDX-License-Identifier: GPL-3.0-or-later
//
// gui/applets/CwDecoderApplet: the slice binding itself, not the DSP
// (that's tst_cw_decoder.cpp). Same two regressions the RTTY decoder's
// applet test pins (2026-09-06/07): the applet must unbind its audio tap
// when the bound slice is removed, and a re-bind to another slice must
// move the tap, not add to it. Checked through AudioEngine's
// cwTapSliceForTest() seam.
// no-port-check: Longpath-original applet test.

#include <QtTest>

#include "core/AudioEngine.h"
#include "core/CwDecoder.h"
#include "gui/applets/CwDecoderApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstCwDecoderApplet : public QObject
{
    Q_OBJECT
private slots:
    void bindsTheTapToTheSliceAndUnbindsWhenItIsRemoved()
    {
        RadioModel radio;
        const int idA = radio.addSlice();
        const int idB = radio.addSlice();   // removeSlice() keeps a lone slice
        SliceModel* sliceA = radio.sliceById(idA);
        QVERIFY(sliceA);
        QVERIFY(radio.audioEngine());

        // (AppletWidget syncs to the active slice on construction, so the
        // tap may already point at slice A here -- setSlice() must agree.)
        CwDecoderApplet applet(&radio);
        applet.setSlice(sliceA);
        QCOMPARE(radio.audioEngine()->cwTapSliceForTest(), sliceA->sliceIndex());

        radio.removeSlice(idA);
        QCOMPARE(radio.audioEngine()->cwTapSliceForTest(), -1);
        Q_UNUSED(idB);
    }

    void reboundSliceMovesTheTap()
    {
        RadioModel radio;
        const int idA = radio.addSlice();
        const int idB = radio.addSlice();
        SliceModel* sliceA = radio.sliceById(idA);
        SliceModel* sliceB = radio.sliceById(idB);
        QVERIFY(sliceA);
        QVERIFY(sliceB);
        QVERIFY(sliceA->sliceIndex() != sliceB->sliceIndex());

        CwDecoderApplet applet(&radio);
        applet.setSlice(sliceA);
        QCOMPARE(radio.audioEngine()->cwTapSliceForTest(), sliceA->sliceIndex());
        applet.setSlice(sliceB);
        QCOMPARE(radio.audioEngine()->cwTapSliceForTest(), sliceB->sliceIndex());

        // Removing the OLD slice must not disturb the new binding.
        radio.removeSlice(idA);
        QCOMPARE(radio.audioEngine()->cwTapSliceForTest(), sliceB->sliceIndex());
    }

    void thePitchStartsOnTheCwPitchSettingAndIsClamped()
    {
        RadioModel radio;
        CwDecoderApplet applet(&radio);
        CwDecoder* dec = applet.decoderForTest();
        QVERIFY(dec);
        QVERIFY(dec->isRunning());
        QVERIFY(dec->pitchHz() >= CwDecoder::kMinPitchHz);
        QVERIFY(dec->pitchHz() <= CwDecoder::kMaxPitchHz);
    }
};

QTEST_MAIN(TstCwDecoderApplet)
#include "tst_cw_decoder_applet.moc"
