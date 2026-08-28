#include "SetupHelpers.h"

#include <cmath>

#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QWidget>

namespace Longpath {

// Spinbox widths tuned to fit the widest expected value + suffix without
// clipping. Int spinboxes handle up to "-200 dBm" in ~80px; double spinboxes
// reserve an extra 10px for the decimal point and fractional digit.
// Spinboxes inherit the dark-theme stylesheet from the parent Setup page
// (see applyDarkStyle() in DisplaySetupPages.cpp) via Qt QSS inheritance,
// so no per-widget styling is needed in the factory.
static constexpr int kSetupSpinWidth  = 80;
static constexpr int kSetupSpinWidthD = 90;

SliderRow makeSliderRow(int min, int max, int initial,
                        const QString& suffix,
                        QWidget* parent)
{
    auto* container = new QWidget(parent);
    auto* layout    = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(min, max);
    slider->setValue(initial);

    auto* spin = new QSpinBox(container);
    spin->setRange(min, max);
    spin->setValue(initial);
    spin->setSuffix(suffix);
    spin->setFixedWidth(kSetupSpinWidth);
    spin->setAlignment(Qt::AlignRight);

    // Bidirectional sync. Consumers of the row connect to slider->valueChanged
    // as the single source of truth for model updates -- so a spin->slider
    // edit must let slider->setValue() re-emit valueChanged (NOT blocked)
    // or that edit never reaches the model at all. The slider->spin
    // direction below still blocks spin, which is what actually prevents
    // the infinite loop: spin's own setValue call in that handler is a
    // silent no-op besides (Qt skips the signal when the value doesn't
    // change), and even if it weren't, blocking it there stops it from
    // bouncing straight back into this handler.
    //
    // Found 2026-08-26 (OE5SOS bench report): the Setup > Display > Spectrum
    // Defaults "Line Width" spinbox arrows visibly moved the slider and its
    // own readout, but the persisted/rendered value never changed --
    // because the old code blocked the slider's valueChanged on every
    // spin-originated edit, so the model-update connection wired to
    // slider->valueChanged (DisplaySetupPages.cpp) never fired. Same defect
    // for every other makeSliderRow/makeDoubleSliderRow control in Setup,
    // any time the operator uses the spinbox instead of dragging the
    // slider -- fixed here at the root rather than per call site.
    QObject::connect(slider, &QSlider::valueChanged, spin, [spin](int v) {
        QSignalBlocker block(spin);
        spin->setValue(v);
    });
    QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged),
                     slider, [slider](int v) {
        slider->setValue(v);
    });

    layout->addWidget(slider, /*stretch=*/1);
    layout->addWidget(spin,   /*stretch=*/0);

    return { slider, spin, container };
}

SliderRowD makeDoubleSliderRow(double min, double max, double initial,
                               int decimals,
                               const QString& suffix,
                               QWidget* parent)
{
    const int scale = static_cast<int>(std::pow(10, decimals));

    auto* container = new QWidget(parent);
    auto* layout    = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(static_cast<int>(min * scale),
                     static_cast<int>(max * scale));
    slider->setValue(static_cast<int>(initial * scale));

    auto* spin = new QDoubleSpinBox(container);
    spin->setDecimals(decimals);
    spin->setRange(min, max);
    spin->setValue(initial);
    spin->setSuffix(suffix);
    spin->setSingleStep(1.0 / scale);
    spin->setFixedWidth(kSetupSpinWidthD);
    spin->setAlignment(Qt::AlignRight);

    // See makeSliderRow's comment above -- same defect, same fix: the
    // spin->slider direction must NOT block the slider, or a spinbox-
    // originated edit never re-emits slider->valueChanged and the model
    // update wired to it never fires.
    QObject::connect(slider, &QSlider::valueChanged, spin, [spin, scale](int v) {
        QSignalBlocker block(spin);
        spin->setValue(static_cast<double>(v) / scale);
    });
    QObject::connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                     slider, [slider, scale](double v) {
        slider->setValue(static_cast<int>(v * scale));
    });

    layout->addWidget(slider, 1);
    layout->addWidget(spin,   0);

    return { slider, spin, container };
}

} // namespace Longpath
