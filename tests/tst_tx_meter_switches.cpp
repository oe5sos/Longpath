// SPDX-License-Identifier: GPL-3.0-or-later
//
// Eine Flaeche, zwei Groessen: beim Abstimmen das SWR, beim Senden die
// Leistung — und das Umschalten geschieht von selbst.
//
// Anlass, 2026-08-23: "ein widget, wo SWR und Stehwelle in einem
// diagramm sind. wenn ich tune stellt es auf das diagramm SWR um, beim
// senden habe ich Stehwelle. dann würde ich mir auch einen platz
// sparen."
//
// Gemessen wird, WAS DAS INSTRUMENT ZEIGT — nicht, was das Applet
// zurueckgibt. Der Unterschied ist in dieser Sitzung mehrfach der
// entscheidende gewesen: eine Verbindung, die im Kommentar steht und
// im Code fehlt, faellt einer Pruefung des Rueckgabewerts nicht auf.

#include <QtTest>

#include "gui/applets/TxMeterApplet.h"
#include "gui/instruments/BarInstrument.h"
#include "gui/meters/MeterPoller.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

class TstTxMeterSwitches : public QObject
{
    Q_OBJECT

private slots:
    void abstimmenZeigtSwrSendenZeigtLeistung()
    {
        RadioModel model;
        TxMeterApplet applet(&model);
        BarInstrument* bar = applet.bar();
        QVERIFY(bar);

        // Abstimmen.
        model.transmitModel().setTune(true);
        QCOMPARE(bar->primary(), int(MeterBinding::TxSwr));
        qInfo() << "TUNE ->" << bar->primary();

        model.transmitModel().setTune(false);

        // Senden.
        model.transmitModel().setMox(true);
        QCOMPARE(bar->primary(), int(MeterBinding::TxPower));
        qInfo() << "MOX  ->" << bar->primary();
    }

    void nachDemSendenBleibtDieGroesseStehen()
    {
        // Sonst zeigte die Flaeche nach dem Loslassen der Taste eine
        // Groesse, die seit einer Minute nicht mehr gemessen wurde.
        RadioModel model;
        TxMeterApplet applet(&model);
        BarInstrument* bar = applet.bar();

        model.transmitModel().setMox(true);
        QCOMPARE(bar->primary(), int(MeterBinding::TxPower));
        model.transmitModel().setMox(false);
        QCOMPARE(bar->primary(), int(MeterBinding::TxPower));
        qInfo() << "nach dem Senden bleibt:" << bar->primary();
    }

    void dieSpitzeWandertNichtMit()
    {
        // Die Spitzenhaltung gehoert zur GROESSE. Ohne Zuruecksetzen
        // stuende beim Umschalten auf SWR die Spitze der Leistung im
        // Bild — 100 auf einer Skala, die bis 3 geht.
        RadioModel model;
        TxMeterApplet applet(&model);
        BarInstrument* bar = applet.bar();

        model.transmitModel().setMox(true);
        applet.onReading(MeterBinding::TxPower, 100.0);
        QVERIFY2(bar->peakHold().value() > 50.0,
                 "die Leistungsspitze wurde gar nicht erst gehalten");

        model.transmitModel().setMox(false);
        model.transmitModel().setTune(true);
        qInfo() << "Spitze nach dem Umschalten:" << bar->peakHold().value();
        QVERIFY2(bar->peakHold().value() < 10.0,
                 qPrintable(QStringLiteral("Spitze steht noch bei %1")
                                .arg(bar->peakHold().value())));
    }

    void dieZuordnungLaesstSichUmdrehen()
    {
        // Ich musste die Woerter des Betreibers deuten — "Stehwelle"
        // und "SWR" sind dasselbe Wort. Die Deutung muss widerrufbar
        // sein, sonst steht eine Vermutung fest verbaut im Programm.
        RadioModel model;
        TxMeterApplet applet(&model);
        BarInstrument* bar = applet.bar();

        applet.setBindings(MeterBinding::TxPower, MeterBinding::TxSwr);
        model.transmitModel().setTune(true);
        QCOMPARE(bar->primary(), int(MeterBinding::TxPower));
        model.transmitModel().setTune(false);
        model.transmitModel().setMox(true);
        QCOMPARE(bar->primary(), int(MeterBinding::TxSwr));
        qInfo() << "umgedreht: TUNE->Leistung, MOX->SWR";
    }
};

QTEST_MAIN(TstTxMeterSwitches)
#include "tst_tx_meter_switches.moc"
