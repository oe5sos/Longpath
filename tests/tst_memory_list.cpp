// =================================================================
// tests/tst_memory_list.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis source (tests for the port of):
//   Project Files/Source/Console/Memory/MemoryRecord.cs
//   Project Files/Source/Console/Memory/MemoryList.cs
//   original licences from Thetis source are included in the ported
//   headers (src/models/MemoryRecord.h, src/models/MemoryList.h)
//
// MemoryRecord + MemoryList:
//   * a Thetis memory.xml (the sample in MemoryForm.cs:51-71, with the
//     ke9ns schedule elements) parses into the right fields, and the
//     elements this program does not model come back out untouched
//   * toXml -> fromXml round-trips every field
//   * restore(): memory.xml wins, a backup is written; a broken main
//     file falls back to memory_bak.xml; no file at all is an empty list
//   * the table model: headers, cell edits with Thetis's "not a number
//     -> 0" rule, sorting by frequency
//   * the enum spellings and the tune-step table
//
// no-port-check: tests a Thetis port (see models/MemoryRecord.h, models/MemoryList.h); registered in THETIS-PROVENANCE.md
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================
//=================================================================
// MemoryRecord.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2003-2013  FlexRadio Systems
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
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
//    USA
//=================================================================
// --- From MemoryList.cs ---
//=================================================================
// MemoryList.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2003-2013  FlexRadio Systems
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
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
//    USA
//=================================================================
// --- From MemoryForm.cs ---
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

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "models/MemoryList.h"
#include "models/MemoryRecord.h"

using namespace Longpath;

namespace {

// From Thetis Memory/MemoryForm.cs:51-71 [@852bf0e] -- the sample record
// in the form's own comment, wrapped in the XmlSerializer envelope and
// given the ke9ns schedule elements a current Thetis writes.
// (Plain string literals, not a raw string: moc leaves an empty .moc
//  behind when a raw string carries "//" -- the URL below.)
const char* kThetisSample =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<MemoryList xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">\n"
    "  <List>\n"
    "    <MemoryRecord>\n"
    "      <Group>Am Broadcast</Group>\n"
    "      <RXFreq>0.78</RXFreq>\n"
    "      <Name>WBBM 780</Name>\n"
    "      <DSPMode>SAM</DSPMode>\n"
    "      <StartDate>2026-09-20T10:00:00</StartDate>\n"
    "      <Duration>25</Duration>\n"
    "      <Recording>false</Recording>\n"
    "      <Repeating>false</Repeating>\n"
    "      <Repeatingm>false</Repeatingm>\n"
    "      <Comments>http://chicago.cbslocal.com/station/wbbm-newsradio-780-and-1059fm/</Comments>\n"
    "      <Scan>true</Scan>\n"
    "      <TuneStep>500Hz</TuneStep>\n"
    "      <RPTR>Low</RPTR>\n"
    "      <RPTROffset>0</RPTROffset>\n"
    "      <CTCSSOn>true</CTCSSOn>\n"
    "      <CTCSSFreq>114.8</CTCSSFreq>\n"
    "      <Deviation>2500</Deviation>\n"
    "      <Power>64</Power>\n"
    "      <Split>false</Split>\n"
    "      <TXFreq>0.78</TXFreq>\n"
    "      <RXFilter>VAR1</RXFilter>\n"
    "      <RXFilterLow>-5000</RXFilterLow>\n"
    "      <RXFilterHigh>3407</RXFilterHigh>\n"
    "      <AGCMode>MED</AGCMode>\n"
    "      <AGCT>76</AGCT>\n"
    "      <ScheduleOn>false</ScheduleOn>\n"
    "      <Extra>0</Extra>\n"
    "    </MemoryRecord>\n"
    "    <MemoryRecord>\n"
    "      <Group>Contest</Group>\n"
    "      <RXFreq>14.2</RXFreq>\n"
    "      <Name>20m SSB</Name>\n"
    "      <DSPMode>USB</DSPMode>\n"
    "      <Comments />\n"
    "      <Scan>true</Scan>\n"
    "      <TuneStep>100Hz</TuneStep>\n"
    "      <RPTR>High</RPTR>\n"
    "      <RPTROffset>0.1</RPTROffset>\n"
    "      <CTCSSOn>false</CTCSSOn>\n"
    "      <CTCSSFreq>0</CTCSSFreq>\n"
    "      <Deviation>5000</Deviation>\n"
    "      <Power>100</Power>\n"
    "      <Split>false</Split>\n"
    "      <TXFreq>14.2</TXFreq>\n"
    "      <RXFilter>F5</RXFilter>\n"
    "      <RXFilterLow>150</RXFilterLow>\n"
    "      <RXFilterHigh>2850</RXFilterHigh>\n"
    "      <AGCMode>FAST</AGCMode>\n"
    "      <AGCT>80</AGCT>\n"
    "    </MemoryRecord>\n"
    "  </List>\n"
    "  <MajorVersion>1</MajorVersion>\n"
    "  <MinorVersion>1</MinorVersion>\n"
    "</MemoryList>\n"
    "\n";


QString extra(const MemoryRecord& r, const QString& name)
{
    for (const auto& kv : r.extras) {
        if (kv.first == name) { return kv.second; }
    }
    return QStringLiteral("<absent>");
}

} // namespace

class TestMemoryList : public QObject {
    Q_OBJECT

private slots:
    void thetisSampleParses()
    {
        MemoryList list;
        QString err;
        QVERIFY2(list.fromXml(QByteArray(kThetisSample), &err), qPrintable(err));
        QCOMPARE(list.count(), 2);
        QCOMPARE(list.majorVersion(), 1);
        QCOMPARE(list.minorVersion(), 1);

        const MemoryRecord& r = list.at(0);
        QCOMPARE(r.group, QStringLiteral("Am Broadcast"));
        QCOMPARE(r.rxFreqMHz, 0.78);
        QCOMPARE(r.name, QStringLiteral("WBBM 780"));
        QCOMPARE(r.dspMode, DSPMode::SAM);
        QVERIFY(r.scan);
        QCOMPARE(r.tuneStep, QStringLiteral("500Hz"));
        QCOMPARE(r.rptr, FmTxMode::Low);
        QCOMPARE(r.rptrOffsetMHz, 0.0);
        QVERIFY(r.ctcssOn);
        QCOMPARE(r.ctcssFreq, 114.8);
        QCOMPARE(r.deviation, 2500);
        QCOMPARE(r.power, 64);
        QVERIFY(!r.split);
        QCOMPARE(r.txFreqMHz, 0.78);
        QCOMPARE(r.rxFilter, QStringLiteral("VAR1"));
        QCOMPARE(r.rxFilterLow, -5000);
        QCOMPARE(r.rxFilterHigh, 3407);
        QVERIFY(r.comments.startsWith(QStringLiteral("http://chicago")));
        QCOMPARE(r.agcMode, AGCMode::Med);
        QCOMPARE(r.agcT, 76);
        // The ke9ns schedule elements ride along, unmodelled.
        QCOMPARE(extra(r, QStringLiteral("StartDate")), QStringLiteral("2026-09-20T10:00:00"));
        QCOMPARE(extra(r, QStringLiteral("Duration")), QStringLiteral("25"));
        QCOMPARE(extra(r, QStringLiteral("ScheduleOn")), QStringLiteral("false"));
        QCOMPARE(r.extras.size(), 7);

        const MemoryRecord& c = list.at(1);
        QCOMPARE(c.dspMode, DSPMode::USB);
        QCOMPARE(c.agcMode, AGCMode::Fast);
        QCOMPARE(c.rxFilter, QStringLiteral("F5"));
        QVERIFY(c.extras.isEmpty());
    }

    void xmlRoundTripsEveryField()
    {
        MemoryList list;
        QVERIFY(list.fromXml(QByteArray(kThetisSample)));
        const QByteArray xml = list.toXml();
        QVERIFY(xml.contains("<MemoryList"));
        QVERIFY(xml.contains("<RXFreq>0.78</RXFreq>"));
        QVERIFY(xml.contains("<DSPMode>SAM</DSPMode>"));
        QVERIFY(xml.contains("<RPTR>Low</RPTR>"));
        QVERIFY(xml.contains("<AGCMode>FAST</AGCMode>"));
        QVERIFY(xml.contains("<StartDate>2026-09-20T10:00:00</StartDate>"));
        QVERIFY(xml.contains("<MajorVersion>1</MajorVersion>"));

        MemoryList again;
        QVERIFY(again.fromXml(xml));
        QCOMPARE(again.count(), 2);
        for (int i = 0; i < 2; ++i) {
            const MemoryRecord& a = list.at(i);
            const MemoryRecord& b = again.at(i);
            QCOMPARE(b.group, a.group);
            QCOMPARE(b.rxFreqMHz, a.rxFreqMHz);
            QCOMPARE(b.name, a.name);
            QCOMPARE(b.dspMode, a.dspMode);
            QCOMPARE(b.scan, a.scan);
            QCOMPARE(b.tuneStep, a.tuneStep);
            QCOMPARE(b.rptr, a.rptr);
            QCOMPARE(b.rptrOffsetMHz, a.rptrOffsetMHz);
            QCOMPARE(b.ctcssOn, a.ctcssOn);
            QCOMPARE(b.ctcssFreq, a.ctcssFreq);
            QCOMPARE(b.deviation, a.deviation);
            QCOMPARE(b.power, a.power);
            QCOMPARE(b.split, a.split);
            QCOMPARE(b.txFreqMHz, a.txFreqMHz);
            QCOMPARE(b.rxFilter, a.rxFilter);
            QCOMPARE(b.rxFilterLow, a.rxFilterLow);
            QCOMPARE(b.rxFilterHigh, a.rxFilterHigh);
            QCOMPARE(b.comments, a.comments);
            QCOMPARE(b.agcMode, a.agcMode);
            QCOMPARE(b.agcT, a.agcT);
            QCOMPARE(b.extras, a.extras);
        }
    }

    void restoreUsesMainFileThenBackupThenEmpty()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString dir = tmp.path();

        // No file at all: an empty list, and that is not an error.
        MemoryList empty;
        QVERIFY(empty.restore(dir));
        QCOMPARE(empty.count(), 0);

        // Save two records, restore them, and find the backup written.
        MemoryList src;
        QVERIFY(src.fromXml(QByteArray(kThetisSample)));
        QVERIFY(src.save(dir));
        QVERIFY(QFileInfo::exists(MemoryList::filePath(dir)));
        MemoryList back;
        QVERIFY(back.restore(dir));
        QCOMPARE(back.count(), 2);
        QVERIFY(QFileInfo::exists(MemoryList::backupPath(dir)));

        // Break the main file: the backup carries the list.
        {
            QFile f(MemoryList::filePath(dir));
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write("<MemoryList><List><MemoryRecord><Group>x");
            f.close();
        }
        MemoryList fromBackup;
        QVERIFY(fromBackup.restore(dir));
        QCOMPARE(fromBackup.count(), 2);
        QCOMPARE(fromBackup.at(1).name, QStringLiteral("20m SSB"));

        // Main broken and no backup: a real failure.
        QVERIFY(QFile::remove(MemoryList::backupPath(dir)));
        MemoryList none;
        QString err;
        QVERIFY(!none.restore(dir, &err));
        QVERIFY(!err.isEmpty());
    }

    void tableModelEditsAndSorts()
    {
        MemoryList list;
        QVERIFY(list.fromXml(QByteArray(kThetisSample)));
        QCOMPARE(list.rowCount(), 2);
        QCOMPARE(list.columnCount(), int(MemoryList::ColumnCount));
        QCOMPARE(list.headerData(MemoryList::RxFreq, Qt::Horizontal).toString(),
                 QStringLiteral("RX Freq"));
        QCOMPARE(list.headerData(MemoryList::AgcT, Qt::Horizontal).toString(),
                 QStringLiteral("AGC-T"));
        QCOMPARE(list.data(list.index(0, MemoryList::Mode)).toString(), QStringLiteral("SAM"));
        QCOMPARE(list.data(list.index(1, MemoryList::Power)).toInt(), 100);

        // Thetis CellValidating: a frequency that does not parse becomes 0.0,
        // an integer that does not parse becomes 0.
        QVERIFY(list.setData(list.index(1, MemoryList::RxFreq), QStringLiteral("abc")));
        QCOMPARE(list.at(1).rxFreqMHz, 0.0);
        QVERIFY(list.setData(list.index(1, MemoryList::Power), QStringLiteral("x")));
        QCOMPARE(list.at(1).power, 0);
        QVERIFY(list.setData(list.index(1, MemoryList::RxFreq), 7.1));
        QCOMPARE(list.at(1).rxFreqMHz, 7.1);
        QVERIFY(list.setData(list.index(1, MemoryList::Mode), QStringLiteral("CWU")));
        QCOMPARE(list.at(1).dspMode, DSPMode::CWU);
        QVERIFY(list.setData(list.index(1, MemoryList::AgcMode), QStringLiteral("SLOW")));
        QCOMPARE(list.at(1).agcMode, AGCMode::Slow);
        QVERIFY(list.setData(list.index(1, MemoryList::Rptr), QStringLiteral("Simplex")));
        QCOMPARE(list.at(1).rptr, FmTxMode::Simplex);

        // Sort by frequency descending: 7.1 first.
        list.sort(MemoryList::RxFreq, Qt::DescendingOrder);
        QCOMPARE(list.at(0).rxFreqMHz, 7.1);
        list.sort(MemoryList::RxFreq, Qt::AscendingOrder);
        QCOMPARE(list.at(0).rxFreqMHz, 0.78);

        // operator<: group, then frequency, then name.
        MemoryRecord a; a.group = QStringLiteral("A"); a.rxFreqMHz = 9.0;
        MemoryRecord b; b.group = QStringLiteral("B"); b.rxFreqMHz = 1.0;
        QVERIFY(a < b);
        b.group = QStringLiteral("A");
        QVERIFY(b < a);

        // add / copy / remove
        const int added = list.add(a);
        QCOMPARE(added, 2);
        QCOMPARE(list.count(), 3);
        list.removeAt(0);
        QCOMPARE(list.count(), 2);
        QCOMPARE(list.at(0).rxFreqMHz, 7.1);
    }

    void enumSpellingsAndTuneSteps()
    {
        QCOMPARE(MemoryRecord::agcModeName(AGCMode::Off), QStringLiteral("FIXD"));
        QCOMPARE(MemoryRecord::agcModeFromName(QStringLiteral("custom")), AGCMode::Custom);
        QCOMPARE(MemoryRecord::agcModeFromName(QStringLiteral("nonsense")), AGCMode::Med);
        QCOMPARE(MemoryRecord::fmTxModeName(FmTxMode::Simplex), QStringLiteral("Simplex"));
        QCOMPARE(MemoryRecord::fmTxModeFromName(QStringLiteral("low")), FmTxMode::Low);

        // From Thetis console.cs:1885-1910 [@852bf0e]: 26 steps, 1 Hz .. 10 MHz.
        QCOMPARE(MemoryRecord::tuneStepNames().size(), 26);
        QCOMPARE(MemoryRecord::tuneStepHzFromName(QStringLiteral("10Hz")), 10);
        QCOMPARE(MemoryRecord::tuneStepHzFromName(QStringLiteral("2.5kHz")), 2500);
        QCOMPARE(MemoryRecord::tuneStepHzFromName(QStringLiteral("10MHz")), 10000000);
        QCOMPARE(MemoryRecord::tuneStepHzFromName(QStringLiteral("7Hz")), 0);
        QCOMPARE(MemoryRecord::tuneStepNameFromHz(100), QStringLiteral("100Hz"));
        QCOMPARE(MemoryRecord::tuneStepNameFromHz(6250), QStringLiteral("6.25kHz"));
        QCOMPARE(MemoryRecord::tuneStepNameFromHz(123), QString());
    }
};

QTEST_APPLESS_MAIN(TestMemoryList)
#include "tst_memory_list.moc"
