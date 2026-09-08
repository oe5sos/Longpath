// SPDX-License-Identifier: GPL-3.0-or-later
//
// gui/applets/RttyDecoderApplet: the slice binding itself, not the DSP
// (that's tst_rtty_decoder.cpp). Regression for a real bug a code review
// found 2026-09-06: the applet never unbound when its bound slice was
// removed, leaving AudioEngine's tap routing whatever slice
// RadioModel::addSlice()'s lowest-free-index reuse handed the same index
// to next -- silently decoding the wrong slice's audio under this
// applet's title.

#include <QtTest>
#include <QLabel>

#include "gui/applets/RttyDecoderApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {
QLabel* markShiftLabelOf(RttyDecoderApplet& a)
{
    for (QLabel* l : a.findChildren<QLabel*>()) {
        if (l->text().contains(QStringLiteral("Mark"))) { return l; }
    }
    return nullptr;
}
} // namespace

class TstRttyDecoderApplet : public QObject
{
    Q_OBJECT

private slots:
    void unbindsWhenBoundSliceIsRemoved()
    {
        RadioModel radio;
        const int idA = radio.addSlice();
        const int idB = radio.addSlice();  // a second slice so removeSlice(idA) is allowed
        SliceModel* sliceA = radio.sliceById(idA);
        QVERIFY(sliceA);
        sliceA->setRttyMarkHz(2400);

        RttyDecoderApplet applet(&radio);
        applet.setSlice(sliceA);

        QLabel* label = markShiftLabelOf(applet);
        QVERIFY2(label, "expected a Mark/Shift info label in RttyDecoderApplet");
        QVERIFY2(label->text().contains(QStringLiteral("2400")),
                  qPrintable(QStringLiteral("expected bound slice's rttyMarkHz to show, got: ") + label->text()));

        radio.removeSlice(idA);

        QVERIFY2(label->text().contains(QChar(0x2014)),  // em dash, HAUSSTIL.md rule 7
                  qPrintable(QStringLiteral("expected the placeholder (em dash) after the bound slice was removed, got: ") + label->text()));

        Q_UNUSED(idB);
    }

    // Regression for a second, related bug found live 2026-09-07 against a
    // real ANAN 10e: MainWindow used to call setSlice() exactly once, at
    // slice-0-added time, and never again -- so when the ACTIVE slice's
    // IDENTITY changed later (a band click restoring a per-band mode, a
    // KiwiSDR profile), the applet kept showing the OLD slice's Mark/Shift
    // values and the old slice's rttyMarkHzChanged went on updating a
    // no-longer-active display. Fixed in MainWindow::
    // rebindRttyRadeAvailability(), which now re-calls setSlice() on every
    // RadioModel::activeSliceChanged, same as RxApplet and CommandBar
    // already did. This test exercises setSlice() itself: repeated calls
    // with a DIFFERENT slice must drop the old slice's connections, not
    // just add the new one's.
    void reboundSliceStopsFollowingThePreviousOne()
    {
        RadioModel radio;
        const int idA = radio.addSlice();
        const int idB = radio.addSlice();
        SliceModel* sliceA = radio.sliceById(idA);
        SliceModel* sliceB = radio.sliceById(idB);
        QVERIFY(sliceA);
        QVERIFY(sliceB);
        sliceA->setRttyMarkHz(2111);
        sliceB->setRttyMarkHz(2295);

        RttyDecoderApplet applet(&radio);
        applet.setSlice(sliceA);
        QLabel* label = markShiftLabelOf(applet);
        QVERIFY2(label, "expected a Mark/Shift info label in RttyDecoderApplet");
        QVERIFY2(label->text().contains(QStringLiteral("2111")),
                  qPrintable(QStringLiteral("expected slice A's rttyMarkHz to show, got: ") + label->text()));

        applet.setSlice(sliceB);
        QVERIFY2(label->text().contains(QStringLiteral("2295")),
                  qPrintable(QStringLiteral("expected slice B's rttyMarkHz after re-bind, got: ") + label->text()));

        // The old slice must no longer drive the display: changing it now
        // should leave the label showing slice B's value untouched.
        sliceA->setRttyMarkHz(9999);
        QVERIFY2(label->text().contains(QStringLiteral("2295")),
                  qPrintable(QStringLiteral("slice A should be unbound after re-bind to slice B, got: ") + label->text()));
        QVERIFY2(!label->text().contains(QStringLiteral("9999")),
                  qPrintable(QStringLiteral("stale slice A connection still updating the label: ") + label->text()));
    }
};

QTEST_MAIN(TstRttyDecoderApplet)
#include "tst_rtty_decoder_applet.moc"
