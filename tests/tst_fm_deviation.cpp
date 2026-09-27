// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original regression test (behaviour from Thetis,
// cited in the code under test).
//
// FM-Deviation 5 kHz / 2,5 kHz (2026-09-27). Die beiden Knoepfe im FM-Feld
// waren Attrappen, und die Deviation gab es in Longpath nirgends; der
// FM-Filter stand fest auf +/-8000. Geprueft wie Thetis
// (console.cs:40318-40395, radio.cs:1427-1444/1571/2882-2899):
//   - Filter = +/-(Deviation + 3000), Vorgabe und beim Umschalten,
//     nur fuer Slices in FM;
//   - die Wahl bleibt gespeichert;
//   - die Knoepfe der FM-Seite (Phone/CW-Applet) folgen dem Modell und
//     steuern es;
//   - der Frequenzspeicher nimmt die Deviation mit;
//   - ein Rebuild des Sendekanals behaelt Deviation und CTCSS.

#include <QtTest/QtTest>
#include <QFile>
#include <QPushButton>
#include <memory>

#include "core/AppSettings.h"
#include "core/TxChannel.h"
#include "core/dsp/TxChannelState.h"
#include "gui/applets/PhoneCwApplet.h"
#include "models/MemoryList.h"
#include "models/MemoryRecord.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

QPushButton* buttonWithText(QWidget& w, const QString& text)
{
    for (QPushButton* b : w.findChildren<QPushButton*>()) {
        if (b && b->text() == text) { return b; }
    }
    return nullptr;
}

std::pair<int, int> filterOf(const SliceModel* s)
{
    return {s->filterLow(), s->filterHigh()};
}

} // namespace

class TstFmDeviation : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void init()
    {
        AppSettings::instance().remove(QStringLiteral("FmDeviationHz"));
    }

    void theDefaultIsFiveKilohertzAndEightKilohertzFilter()
    {
        RadioModel model;
        QCOMPARE(model.fmDeviationHz(), 5000);
        QCOMPARE(SliceModel::fmHalfBandwidthHz(), 8000);
        QCOMPARE(SliceModel::defaultFilterForMode(DSPMode::FM),
                 std::make_pair(-8000, 8000));
    }

    void switchingRetunesOnlyTheFmSlicesAndIsKept()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int idFm = model.addSlice();
        const int idUsb = model.addSlice();
        SliceModel* fm = model.sliceById(idFm);
        SliceModel* usb = model.sliceById(idUsb);
        QVERIFY(fm);
        QVERIFY(usb);
        fm->setDspMode(DSPMode::FM);
        usb->setDspMode(DSPMode::USB);
        usb->setFilter(100, 2700);
        QSignalSpy changed(&model, &RadioModel::fmDeviationHzChanged);

        model.setFmDeviationHz(2500);

        QCOMPARE(changed.count(), 1);
        QCOMPARE(filterOf(fm), std::make_pair(-5500, 5500));
        QCOMPARE(filterOf(usb), std::make_pair(100, 2700));
        QCOMPARE(SliceModel::defaultFilterForMode(DSPMode::FM),
                 std::make_pair(-5500, 5500));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("FmDeviationHz")).toInt(), 2500);

        // Neustart: ein frisches Modell liest die Wahl zurueck.
        RadioModel again;
        QCOMPARE(again.fmDeviationHz(), 2500);

        // Gleicher Wert: nichts passiert.
        model.setFmDeviationHz(2500);
        QCOMPARE(changed.count(), 1);
        model.setFmDeviationHz(5000);
        QCOMPARE(filterOf(fm), std::make_pair(-8000, 8000));
    }

    void theAppletButtonsFollowAndDriveTheModel()
    {
        RadioModel model;
        // The FM page of the Phone/CW applet is what FM shows
        // (MainWindow: showPage(2)); FmApplet is never instantiated.
        PhoneCwApplet applet(&model);
        QPushButton* five = buttonWithText(applet, QStringLiteral("5.0k"));
        QPushButton* half = buttonWithText(applet, QStringLiteral("2.5k"));
        QVERIFY(five && half);
        QVERIFY(five->isChecked());
        QVERIFY(!half->isChecked());

        half->click();
        QCOMPARE(model.fmDeviationHz(), 2500);
        QVERIFY(half->isChecked());
        QVERIFY(!five->isChecked());

        // Den gewaehlten Knopf nochmals: er bleibt gewaehlt.
        half->click();
        QCOMPARE(model.fmDeviationHz(), 2500);
        QVERIFY(half->isChecked());

        // Aenderung von anderswo (Frequenzspeicher, ...): Knoepfe folgen.
        model.setFmDeviationHz(5000);
        QVERIFY(five->isChecked());
        QVERIFY(!half->isChecked());
    }

    void theFrequencyMemoryCarriesTheDeviation()
    {
        auto model = std::make_unique<RadioModel>();
        QFile::remove(MemoryList::filePath(model->memoriesDir()));
        model->memories()->clear();
        SliceModel* s = model->activeSlice();
        if (!s) { s = model->sliceById(model->addSlice()); }
        QVERIFY(s);
        s->setFrequency(145'600'000.0);
        s->setDspMode(DSPMode::FM);
        model->setFmDeviationHz(2500);

        MemoryRecord r = model->captureMemory();
        QCOMPARE(r.deviation, 2500);   // console.cs: TXFMDeviation into the record

        model->setFmDeviationHz(5000);
        model->recallMemory(r);        // console.cs:40540 FMDeviation_Hz = record.Deviation
        QCOMPARE(model->fmDeviationHz(), 2500);
        QFile::remove(MemoryList::filePath(model->memoriesDir()));
    }

    void aTxRebuildKeepsDeviationAndTone()
    {
        TxChannel before(1, 64, 64);
        before.setFmDeviation(2500.0);
        before.setCtcssFreq(88.5);
        before.setCtcssRun(true);
        const TxChannelState s = before.captureState();
        QCOMPARE(s.fmDeviationHz, 2500.0);
        QCOMPARE(s.ctcssFreqHz, 88.5);
        QVERIFY(s.ctcssRun);

        TxChannel after(1, 64, 64);
        QCOMPARE(after.fmDeviationForTest(), 5000.0);
        QVERIFY(!after.ctcssRunForTest());
        after.applyState(s);
        QCOMPARE(after.fmDeviationForTest(), 2500.0);
        QCOMPARE(after.ctcssFreqForTest(), 88.5);
        QVERIFY(after.ctcssRunForTest());
    }
};

QTEST_MAIN(TstFmDeviation)
#include "tst_fm_deviation.moc"
