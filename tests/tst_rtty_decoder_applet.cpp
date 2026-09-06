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
};

QTEST_MAIN(TstRttyDecoderApplet)
#include "tst_rtty_decoder_applet.moc"
