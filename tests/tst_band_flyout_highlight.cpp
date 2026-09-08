// SPDX-License-Identifier: GPL-3.0-or-later
//
// Von einer AetherSDR-Sichtung angestossen (2026-09-06, "highlight
// active band"): das Band-Flyout im Display-Overlay markierte nirgends,
// welches Band gerade gehoert wird. Betreiber 2026-09-05 waehlte
// Entwurf A (gefuellt, wie der WNB-Knopf im selben Panel); die
// eigentliche Verdrahtung -- SliceModel::bandChanged() -> welcher
// Knopf im Flyout steht auf "checked" -- deckt dieser Test ab.
//
// Erster Anlauf zielte auf PanadapterModel::bandChanged() -- die Klasse
// wird aber nirgends instanziiert (RadioModel::addPanadapter() hat im
// ganzen Baum nur EINEN Aufrufer, und der steckt selbst in einem Test --
// tst_oc_outputs_live_pins.cpp -- nicht im ausgelieferten Code). SliceModel
// ist die tatsaechlich lebendige Quelle, wie der Bandklick selbst schon
// zeigt (MainWindow::ensureOverlayPanels()).
//
// Bare RadioModel + addSlice() statt einer vollen MainWindow -- wie
// tst_oc_outputs_live_pins.cpp es fuer PanadapterModel vormacht, nur mit
// dem tatsaechlich lebendigen Weg.

#include <QtTest/QtTest>
#include <QPushButton>
#include <QWidget>

#include "gui/SpectrumOverlayPanel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {
QPushButton* findBandButton(QWidget* root, const QString& label)
{
    for (QPushButton* b : root->findChildren<QPushButton*>()) {
        if (b->isCheckable() && b->text() == label) { return b; }
    }
    return nullptr;
}
} // namespace

class TstBandFlyoutHighlight : public QObject
{
    Q_OBJECT

private slots:
    void aktivesBandWirdMarkiert()
    {
        RadioModel model;
        const int sliceId = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* slice0 = model.sliceById(sliceId);
        QVERIFY2(slice0, "addSlice() hat keinen Slice geliefert");

        // m_bandFlyout is built as a sibling under parentWidget(), not as a
        // child of the panel itself (SpectrumOverlayPanel.cpp:701, "new
        // QWidget(parentWidget())") -- a real parent is needed here, or the
        // flyout's buttons never show up under panel.findChildren().
        QWidget host;
        SpectrumOverlayPanel panel(&host);
        panel.setPanId(QStringLiteral("pan-0"));
        panel.setRadioModel(&model);
        panel.setSliceResolver([slice0]() { return slice0; });
        panel.bindToPanSlice();

        auto* btn20m = findBandButton(&host, QStringLiteral("20"));
        auto* btn40m = findBandButton(&host, QStringLiteral("40"));
        QVERIFY2(btn20m, "Knopf '20' (20m) nicht gefunden");
        QVERIFY2(btn40m, "Knopf '40' (40m) nicht gefunden");

        slice0->setFrequency(14.0e6);   // 20m
        QVERIFY2(btn20m->isChecked(), "20m-Knopf sollte markiert sein");
        QVERIFY2(!btn40m->isChecked(), "40m-Knopf sollte NICHT markiert sein");

        slice0->setFrequency(7.0e6);    // 40m
        QVERIFY2(btn40m->isChecked(), "40m-Knopf sollte nach dem Bandwechsel markiert sein");
        QVERIFY2(!btn20m->isChecked(), "20m-Knopf sollte nach dem Bandwechsel NICHT mehr markiert sein");
    }
};

QTEST_MAIN(TstBandFlyoutHighlight)
#include "tst_band_flyout_highlight.moc"
