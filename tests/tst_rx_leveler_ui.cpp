// =================================================================
// tests/tst_rx_leveler_ui.cpp  (Longpath)
// =================================================================
//
// The RX leveler's two operator surfaces:
//
//   * the LVL toggle in the RxApplet's volume row follows the slice
//     both ways (click -> model, model -> checked), and persists
//   * the right-click profile (DspQuickPopup::showRxLeveler) shows the
//     Auto/Eigene choice and five sliders whose ranges are the Zeus
//     profile limits; moving one writes the slice; Reset restores the
//     defaults
//   * the slice's leveler settings survive a save/restore round trip
//
// Longpath-original test. no-port-check: UI glue over a cited Zeus port.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>

#include "core/AppSettings.h"
#include "core/audio/RxAudioLeveler.h"
#include "gui/applets/RxApplet.h"
#include "gui/widgets/DspParamPopup.h"
#include "gui/widgets/DspQuickPopups.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <memory>

using namespace Longpath;

namespace {

QPushButton* button(QWidget& w, const QString& text)
{
    for (QPushButton* b : w.findChildren<QPushButton*>()) {
        if (b && b->text() == text) { return b; }
    }
    return nullptr;
}

DspParamPopup* openPopup()
{
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (auto* p = qobject_cast<DspParamPopup*>(w)) {
            if (p->isVisible()) { return p; }
        }
    }
    return nullptr;
}

} // namespace

class TestRxLevelerUi : public QObject {
    Q_OBJECT

private:
    struct Harness {
        std::unique_ptr<RadioModel> radio;
        std::unique_ptr<RxApplet>   applet;
        SliceModel* slice{nullptr};
    };

    Harness make()
    {
        Harness h;
        h.radio = std::make_unique<RadioModel>();
        h.slice = h.radio->sliceById(0);
        if (!h.slice) {
            const int id = h.radio->addSlice();
            h.slice = h.radio->sliceById(id);
        }
        h.applet = std::make_unique<RxApplet>(h.slice, h.radio.get(), nullptr);
        return h;
    }

private slots:
    void lvlButtonFollowsTheSliceBothWays()
    {
        Harness h = make();
        QPushButton* lvl = button(*h.applet, QStringLiteral("LVL"));
        QVERIFY2(lvl, "kein LVL-Knopf in der Lautstaerke-Reihe");
        QVERIFY(lvl->isCheckable());
        QVERIFY(!lvl->isChecked());
        QVERIFY(!h.slice->levelerEnabled());

        lvl->click();
        QVERIFY(h.slice->levelerEnabled());

        h.slice->setLevelerEnabled(false);
        QVERIFY(!lvl->isChecked());
        h.slice->setLevelerEnabled(true);
        QVERIFY(lvl->isChecked());
    }

    void rightClickProfileShowsTheZeusRanges()
    {
        SliceModel slice(0);
        DspQuickPopup::showRxLeveler(nullptr, &slice, QPoint(100, 100));
        DspParamPopup* p = openPopup();
        QVERIFY2(p, "das Profil-Popup ist nicht aufgegangen");

        const QList<QRadioButton*> radios = p->findChildren<QRadioButton*>();
        QCOMPARE(radios.size(), 2);
        QCOMPARE(radios[0]->text(), QStringLiteral("Auto"));
        QCOMPARE(radios[1]->text(), QStringLiteral("Eigene"));
        QVERIFY(radios[0]->isChecked());

        const QList<QSlider*> sliders = p->findChildren<QSlider*>();
        QCOMPARE(sliders.size(), 5);
        // Ziel, Anhebung, Attack, Release, Hang -- the Dtos.cs limits.
        QCOMPARE(sliders[0]->minimum(), static_cast<int>(RxLevelerConfig::kMinTargetRmsDb));
        QCOMPARE(sliders[0]->maximum(), static_cast<int>(RxLevelerConfig::kMaxTargetRmsDb));
        QCOMPARE(sliders[0]->value(),   static_cast<int>(RxLevelerConfig::kDefaultTargetRmsDb));
        QCOMPARE(sliders[1]->maximum(), static_cast<int>(RxLevelerConfig::kMaxBoostLimitDb));
        QCOMPARE(sliders[2]->minimum(), RxLevelerConfig::kMinAttackMs);
        QCOMPARE(sliders[2]->maximum(), RxLevelerConfig::kMaxAttackMs);
        QCOMPARE(sliders[3]->maximum(), RxLevelerConfig::kMaxReleaseMs);
        QCOMPARE(sliders[4]->maximum(), RxLevelerConfig::kMaxHangMs);

        // Moving a slider and picking Eigene writes the slice.
        radios[1]->click();
        QVERIFY(slice.levelerCustom());
        sliders[0]->setValue(-24);
        QCOMPARE(slice.levelerTargetDb(), -24.0);
        sliders[2]->setValue(150);
        QCOMPARE(slice.levelerAttackMs(), 150);

        // Reset restores the upstream defaults on the model.
        QPushButton* reset = button(*p, QStringLiteral("Reset"));
        QVERIFY2(reset, "kein Reset im Profil-Popup");
        reset->click();
        QCOMPARE(slice.levelerTargetDb(), RxLevelerConfig::kDefaultTargetRmsDb);
        QCOMPARE(slice.levelerAttackMs(), RxLevelerConfig::kDefaultAttackMs);
        p->close();
    }

    void settingsRoundTrip()
    {
        {
            SliceModel slice(3);
            slice.setLevelerEnabled(true);
            slice.setLevelerCustom(true);
            slice.setLevelerTargetDb(-22.0);
            slice.setLevelerMaxBoostDb(12.0);
            slice.setLevelerAttackMs(250);
            slice.setLevelerReleaseMs(300);
            slice.setLevelerHangMs(900);
            slice.saveToSettings(Band::Band20m);
        }
        SliceModel again(3);
        again.restoreFromSettings(Band::Band20m);
        QVERIFY(again.levelerEnabled());
        QVERIFY(again.levelerCustom());
        QCOMPARE(again.levelerTargetDb(), -22.0);
        QCOMPARE(again.levelerMaxBoostDb(), 12.0);
        QCOMPARE(again.levelerAttackMs(), 250);
        QCOMPARE(again.levelerReleaseMs(), 300);
        QCOMPARE(again.levelerHangMs(), 900);

        // Out-of-range values are normalised on the way in.
        again.setLevelerTargetDb(-99.0);
        QCOMPARE(again.levelerTargetDb(), RxLevelerConfig::kMinTargetRmsDb);
    }
};

QTEST_MAIN(TestRxLevelerUi)
#include "tst_rx_leveler_ui.moc"
