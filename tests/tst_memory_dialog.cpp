// =================================================================
// tests/tst_memory_dialog.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis source (tests for the port of):
//   Project Files/Source/Console/Memory/MemoryForm.cs
//   Project Files/Source/Console/console.cs (RecallMemory, quick memory)
//   original licences from Thetis source are included in the ported
//   files (src/gui/MemoryDialog.h, src/models/RadioModel.h)
//
// RadioModel::captureMemory / recallMemory and the memory window:
//   * capture reads the active slice (frequency, mode, step, filter,
//     AGC, power) and names the preset when the bounds match one
//   * recall applies a record: frequency, mode, step, filter bounds,
//     AGC mode + AGC-T, power; FM records set the repeater set; auto-AGC
//     is switched off only when AGC-T differs (MW0LGE_21k8 rule)
//   * quick save / restore round-trips frequency, mode and filter
//   * the dialog: Add appends a row from the slice, Copy duplicates it,
//     Select recalls it, Delete asks and removes, closing hides and
//     saves memory.xml
//
// no-port-check: tests a Thetis port (see gui/MemoryDialog.h, models/RadioModel.h); registered in THETIS-PROVENANCE.md
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================
//=================================================================
// MemoryForm.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2003-2013  FlexRadio Systems
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
//    USA
//=================================================================
// --- From console.cs ---
//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Final modifictions by MW0LGE Richard Samphire - 19th April 2026
// Nothing further added by him after this date, and his repo is now in archive https://github.com/ramdor/Thetis
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <QtTest/QtTest>
#include <QPushButton>
#include <QTableView>

#include "core/AppSettings.h"
#include "gui/MemoryDialog.h"
#include "models/MemoryList.h"
#include "models/MemoryRecord.h"
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

struct Rig {
    std::unique_ptr<RadioModel> radio;
    SliceModel* slice{nullptr};

    Rig()
    {
        radio = std::make_unique<RadioModel>();
        radio->memories()->clear();
        slice = radio->activeSlice();
        if (!slice) {
            const int id = radio->addSlice();
            slice = radio->sliceById(id);
        }
    }
};

} // namespace

class TestMemoryDialog : public QObject {
    Q_OBJECT

private slots:
    void captureReadsTheActiveSlice()
    {
        Rig rig;
        QVERIFY(rig.slice);
        rig.slice->setFrequency(14'205'000.0);
        rig.slice->setDspMode(DSPMode::USB);
        rig.slice->setStepHz(500);
        rig.slice->setFilter(100, 2700);     // USB F6 in Thetis's preset table
        rig.slice->setAgcMode(AGCMode::Fast);
        rig.slice->setAgcThreshold(-33);
        rig.radio->transmitModel().setPower(42);

        const MemoryRecord r = rig.radio->captureMemory();
        QCOMPARE(r.rxFreqMHz, 14.205);
        QCOMPARE(r.name, QStringLiteral("14.205000"));
        QCOMPARE(r.dspMode, DSPMode::USB);
        QCOMPARE(r.tuneStep, QStringLiteral("500Hz"));
        QCOMPARE(r.rxFilterLow, 100);
        QCOMPARE(r.rxFilterHigh, 2700);
        QCOMPARE(r.agcMode, AGCMode::Fast);
        QCOMPARE(r.agcT, -33);
        QCOMPARE(r.power, 42);
        QVERIFY(!r.split);
        QCOMPARE(r.txFreqMHz, 14.205);
        QVERIFY(r.scan);
        // 100..2700 is USB F6 in Thetis's table (console.cs:5233-5273), so it
        // gets that name; an odd pair is VAR1.
        QCOMPARE(r.rxFilter, QStringLiteral("F6"));
        rig.slice->setFilter(123, 2345);
        QCOMPARE(rig.radio->captureMemory().rxFilter, QStringLiteral("VAR1"));
    }

    void recallAppliesTheRecord()
    {
        Rig rig;
        rig.slice->setFrequency(7'100'000.0);
        rig.slice->setDspMode(DSPMode::LSB);
        rig.slice->setAgcThreshold(-20);
        rig.slice->setAutoAgcEnabled(true);

        MemoryRecord r;
        r.rxFreqMHz = 21.250;
        r.dspMode = DSPMode::USB;
        r.tuneStep = QStringLiteral("1kHz");
        r.rxFilter = QStringLiteral("VAR2");
        r.rxFilterLow = 200;
        r.rxFilterHigh = 2400;
        r.agcMode = AGCMode::Slow;
        r.agcT = -40;
        r.power = 77;

        rig.radio->recallMemory(r);
        QCOMPARE(rig.slice->frequency(), 21'250'000.0);
        QCOMPARE(rig.slice->dspMode(), DSPMode::USB);
        QCOMPARE(rig.slice->stepHz(), 1000);
        QCOMPARE(rig.slice->filterLow(), 200);
        QCOMPARE(rig.slice->filterHigh(), 2400);
        QCOMPARE(rig.slice->agcMode(), AGCMode::Slow);
        QCOMPARE(rig.slice->agcThreshold(), -40);
        // AGC-T differed, so auto-AGC went off (MW0LGE_21k8).
        QVERIFY(!rig.slice->autoAgcEnabled());
        QCOMPARE(rig.radio->transmitModel().power(), 77);

        // Same AGC-T again with auto-AGC on: auto-AGC survives.
        rig.slice->setAutoAgcEnabled(true);
        rig.radio->recallMemory(r);
        QVERIFY(rig.slice->autoAgcEnabled());

        // An unknown tune-step name leaves the step alone (TuneStepLookup -1).
        r.tuneStep = QStringLiteral("7Hz");
        rig.radio->recallMemory(r);
        QCOMPARE(rig.slice->stepHz(), 1000);
    }

    void fmRecordSetsTheRepeaterSet()
    {
        Rig rig;
        MemoryRecord r;
        r.rxFreqMHz = 145.600;
        r.dspMode = DSPMode::FM;
        r.rptr = FmTxMode::Low;
        r.rptrOffsetMHz = 0.6;
        r.ctcssOn = true;
        r.ctcssFreq = 88.5;
        r.rxFilterLow = -5000;
        r.rxFilterHigh = 5000;
        rig.slice->setFilter(150, 2850);

        rig.radio->recallMemory(r);
        QCOMPARE(rig.slice->dspMode(), DSPMode::FM);
        QCOMPARE(rig.slice->fmTxMode(), FmTxMode::Low);
        QCOMPARE(rig.slice->fmOffsetHz(), 600000);
        QVERIFY(rig.slice->fmCtcssMode() != 0);
        QCOMPARE(rig.slice->fmCtcssValueHz(), 88.5);
    }

    void quickMemoryRoundTrips()
    {
        Rig rig;
        rig.slice->setFrequency(3'700'000.0);
        rig.slice->setDspMode(DSPMode::LSB);
        rig.slice->setFilter(-2800, -200);
        rig.radio->memoryQuickSave();
        QVERIFY(rig.radio->hasQuickMemory());

        rig.slice->setFrequency(14'100'000.0);
        rig.slice->setDspMode(DSPMode::USB);
        rig.slice->setFilter(200, 2800);
        rig.radio->memoryQuickRestore();
        QCOMPARE(rig.slice->frequency(), 3'700'000.0);
        QCOMPARE(rig.slice->dspMode(), DSPMode::LSB);
        QCOMPARE(rig.slice->filterLow(), -2800);
        QCOMPARE(rig.slice->filterHigh(), -200);
        // Persisted like txtMemoryQuick.
        QCOMPARE(AppSettings::instance().value(QStringLiteral("MemoryQuick")).toString(),
                 QStringLiteral("3.700000"));
    }

    // Three memories in the grid, rendered to a PNG when LONGPATH_GRAB_DIR
    // is set -- the picture for the design doc, without a screen.
    void dialogRendersTheGrid()
    {
        Rig rig;
        MemoryRecord a;
        a.group = QStringLiteral("Broadcast"); a.rxFreqMHz = 0.78; a.name = QStringLiteral("WBBM 780");
        a.dspMode = DSPMode::SAM; a.tuneStep = QStringLiteral("500Hz"); a.rxFilter = QStringLiteral("VAR1");
        a.rxFilterLow = -5000; a.rxFilterHigh = 3407; a.agcT = 76;
        a.comments = QStringLiteral("Chicago news radio");
        MemoryRecord b;
        b.group = QStringLiteral("Contest"); b.rxFreqMHz = 14.2; b.name = QStringLiteral("20m SSB");
        b.dspMode = DSPMode::USB; b.tuneStep = QStringLiteral("100Hz"); b.rxFilter = QStringLiteral("F6");
        b.rxFilterLow = 100; b.rxFilterHigh = 2700; b.agcMode = AGCMode::Fast; b.power = 100;
        MemoryRecord c;
        c.group = QStringLiteral("Relais"); c.rxFreqMHz = 145.6; c.name = QStringLiteral("OE5XLL");
        c.dspMode = DSPMode::FM; c.rptr = FmTxMode::Low; c.rptrOffsetMHz = 0.6; c.ctcssOn = true;
        c.ctcssFreq = 88.5; c.tuneStep = QStringLiteral("12.5kHz"); c.rxFilter = QStringLiteral("F1");
        c.rxFilterLow = -5000; c.rxFilterHigh = 5000; c.power = 50;
        for (const MemoryRecord& r : {a, b, c}) { rig.radio->memories()->add(r); }

        MemoryDialog dlg(rig.radio.get());
        dlg.resize(1100, 380);
        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));
        dlg.tableForTest()->selectRow(2);
        const QPixmap pm = dlg.grab();
        QVERIFY(!pm.isNull());
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            QVERIFY(pm.save(grabDir + QStringLiteral("/longpath-grab-MemoryDialog.png")));
        }
        dlg.close();
    }

    void dialogAddsCopiesSelectsAndDeletes()
    {
        Rig rig;
        rig.slice->setFrequency(7'050'000.0);
        rig.slice->setDspMode(DSPMode::CWL);

        MemoryDialog dlg(rig.radio.get());
        int asked = 0;
        dlg.setOperatorHooks([&asked](const QString&) { ++asked; return true; });
        dlg.show();

        QPushButton* add = button(dlg, QStringLiteral("Add"));
        QPushButton* copy = button(dlg, QStringLiteral("Copy"));
        QPushButton* del = button(dlg, QStringLiteral("Delete"));
        QPushButton* select = button(dlg, QStringLiteral("Select"));
        QVERIFY(add && copy && del && select);

        add->click();
        QCOMPARE(rig.radio->memories()->count(), 1);
        QCOMPARE(rig.radio->memories()->at(0).rxFreqMHz, 7.05);
        QCOMPARE(rig.radio->memories()->at(0).dspMode, DSPMode::CWL);
        QCOMPARE(dlg.currentRow(), 0);

        copy->click();
        QCOMPARE(rig.radio->memories()->count(), 2);
        QCOMPARE(dlg.currentRow(), 1);

        // Move the slice away, then Select row 0 brings it back.
        rig.slice->setFrequency(14'000'000.0);
        rig.slice->setDspMode(DSPMode::USB);
        dlg.tableForTest()->selectRow(0);
        select->click();
        QCOMPARE(rig.slice->frequency(), 7'050'000.0);
        QCOMPARE(rig.slice->dspMode(), DSPMode::CWL);

        del->click();
        QCOMPARE(asked, 1);
        QCOMPARE(rig.radio->memories()->count(), 1);

        // memory.xml was written next to the settings file.
        QVERIFY(QFileInfo::exists(MemoryList::filePath(rig.radio->memoriesDir())));

        // Closing hides rather than destroys.
        dlg.close();
        QVERIFY(!dlg.isVisible());
    }
};

QTEST_MAIN(TestMemoryDialog)
#include "tst_memory_dialog.moc"
