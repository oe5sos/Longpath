// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original regression test (behaviour from Thetis,
// cited in the code under test).
//
// FM-Relaisbetrieb (2026-09-27). Die Einstellungen im FM-Feld (Ablage,
// Richtung, Reverse, CTCSS) waren nur Oberflaeche: nichts erreichte die
// Sendefrequenz oder WDSP, und nichts ueberlebte einen Neustart.
// Geprueft: Sendefrequenz mit Ablage wie Thetis (console.cs:29347-29366),
// Reverse verschiebt die Empfangsfrequenz (console.cs:40442-40468), nur
// in FM, die 49 Toene aus Thetis, und Speichern + Laden je Slice/Band.

#include <QtTest/QtTest>
#include <QComboBox>

#include "core/AppSettings.h"
#include "gui/widgets/VfoModeContainers.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {
constexpr double k10mFm = 29620000.0;   // 10-m-Relaisausgang
constexpr int kOffset = 100000;          // 100 kHz
}

class TstFmRepeaterWiring : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void theTxFrequencyCarriesTheRepeaterOffset()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        SliceModel* s = model.slices().at(a);
        s->setFrequency(k10mFm);
        s->setDspMode(DSPMode::FM);
        s->setFmOffsetHz(kOffset);

        s->setFmTxMode(FmTxMode::Simplex);
        QCOMPARE(model.txFrequencyForSliceForTest(s), quint64(29620000));
        s->setFmTxMode(FmTxMode::Low);       // usual case: TX below
        QCOMPARE(model.txFrequencyForSliceForTest(s), quint64(29520000));
        s->setFmTxMode(FmTxMode::High);
        QCOMPARE(model.txFrequencyForSliceForTest(s), quint64(29720000));

        // Nur in FM.
        s->setDspMode(DSPMode::USB);
        QCOMPARE(model.txFrequencyForSliceForTest(s), quint64(29620000));
    }

    // Reverse: auf der Eingabe hoeren, auf der Ausgabe senden.
    void reverseListensOnTheInput()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        SliceModel* s = model.slices().at(a);
        model.wireSliceSignalsForTest();
        s->setFrequency(k10mFm);
        s->setDspMode(DSPMode::FM);
        s->setFmOffsetHz(kOffset);
        s->setFmTxMode(FmTxMode::Low);

        s->setFmReverse(true);
        QCOMPARE(s->frequency(), 29520000.0);                              // RX auf der Eingabe
        QCOMPARE(model.txFrequencyForSliceForTest(s), quint64(29620000));  // TX auf der Ausgabe
        s->setFmReverse(false);
        QCOMPARE(s->frequency(), 29620000.0);
        QCOMPARE(model.txFrequencyForSliceForTest(s), quint64(29520000));

        s->setFmTxMode(FmTxMode::High);
        s->setFmReverse(true);
        QCOMPARE(s->frequency(), 29720000.0);
        QCOMPARE(model.txFrequencyForSliceForTest(s), quint64(29620000));
    }

    void theToneListIsThetis49()
    {
        FmOptContainer c;
        auto* tones = c.findChild<QComboBox*>(QStringLiteral("toneValueCmb"));
        QVERIFY(tones);
        QCOMPARE(tones->count(), 49);
        QCOMPARE(tones->itemText(1), QStringLiteral("69.3"));
        QVERIFY(tones->findText(QStringLiteral("199.5")) >= 0);
    }

    void fmSettingsSurviveARestart()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        SliceModel* s = model.slices().at(a);
        s->setFmCtcssMode(1);
        s->setFmCtcssValueHz(123.0);
        s->setFmOffsetHz(600000);
        s->setFmTxMode(FmTxMode::Low);
        s->saveToSettings(Band::Band10m);

        s->setFmCtcssMode(0);
        s->setFmCtcssValueHz(100.0);
        s->setFmOffsetHz(0);
        s->setFmTxMode(FmTxMode::Simplex);
        s->restoreFromSettings(Band::Band10m);
        QCOMPARE(s->fmCtcssMode(), 1);
        QCOMPARE(s->fmCtcssValueHz(), 123.0);
        QCOMPARE(s->fmOffsetHz(), 600000);
        QCOMPARE(s->fmTxMode(), FmTxMode::Low);
    }
};

QTEST_MAIN(TstFmRepeaterWiring)
#include "tst_fm_repeater_wiring.moc"
