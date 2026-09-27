// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original regression test (behaviour from Thetis,
// cited in the code under test).
//
// Die FM-Seite des Phone/CW-Applets -- die zeigt das Hauptfenster in FM
// (MainWindow: showPage(2)) -- war bis auf die Deviation eine Attrappe
// (2026-09-27). Jetzt steuert sie CTCSS, Ton, Simplex/-/+, Rev und die
// Ablage des aktiven Slices, dieselben Werte wie die VFO-Flagge, und beide
// folgen dem Slice. Dazu zwei Thetis-Regeln aus console.cs:40400-40440:
// eine neue Sendeablage schaltet Reverse aus (mit der alten Ablage, damit
// die Empfangsfrequenz richtig zurueckgeht), und in Simplex ist Rev
// gesperrt.

#include <QtTest/QtTest>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>

#include "gui/applets/PhoneCwApplet.h"
#include "gui/widgets/VfoModeContainers.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

template <typename T>
T* named(QWidget& w, const QString& accessibleName)
{
    for (T* c : w.findChildren<T*>()) {
        if (c && c->accessibleName() == accessibleName) { return c; }
    }
    return nullptr;
}

} // namespace

class TstFmAppletWiring : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void theFmPageDrivesAndFollowsTheActiveSlice()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int id = model.addSlice();
        model.setActiveSlice(id);
        SliceModel* s = model.activeSlice();
        QVERIFY(s);
        s->setFrequency(145'600'000.0);
        s->setDspMode(DSPMode::FM);
        s->setFmCtcssMode(0);
        s->setFmOffsetHz(600'000);
        s->setFmTxMode(FmTxMode::Low);

        PhoneCwApplet applet(&model);
        applet.showPage(2);
        auto* ctcss = named<QPushButton>(applet, QStringLiteral("CTCSS sub-audible tone squelch"));
        auto* tones = named<QComboBox>(applet, QStringLiteral("CTCSS tone frequency"));
        auto* simplex = named<QPushButton>(applet, QStringLiteral("Simplex (no repeater offset)"));
        auto* minus = named<QPushButton>(applet, QStringLiteral("Negative repeater offset"));
        auto* plus = named<QPushButton>(applet, QStringLiteral("Positive repeater offset"));
        auto* rev = named<QPushButton>(applet, QStringLiteral("Reverse repeater offset"));
        auto* offset = named<QSlider>(applet, QStringLiteral("Repeater offset (kHz)"));
        QVERIFY(ctcss && tones && simplex && minus && plus && rev && offset);

        // Anfangszustand aus dem Slice.
        QVERIFY(!ctcss->isChecked());
        QVERIFY(minus->isChecked());
        QVERIFY(!plus->isChecked() && !simplex->isChecked());
        QCOMPARE(offset->value(), 600);
        QCOMPARE(tones->count(), 49);   // Thetis CTCSS_array (console.cs:236-241)

        // CTCSS schaltet den Sendeton; ein Decode aus der Flagge bleibt.
        ctcss->click();
        QCOMPARE(s->fmCtcssMode(), 1);
        s->setFmCtcssMode(2);
        QVERIFY(!ctcss->isChecked());
        ctcss->click();
        QCOMPARE(s->fmCtcssMode(), 3);
        ctcss->click();
        QCOMPARE(s->fmCtcssMode(), 2);

        tones->setCurrentIndex(tones->findText(QStringLiteral("88.5")));
        QCOMPARE(s->fmCtcssValueHz(), 88.5);

        // Sendeablage: eine von dreien; Rev in Simplex gesperrt.
        plus->click();
        QCOMPARE(s->fmTxMode(), FmTxMode::High);
        QVERIFY(plus->isChecked() && !minus->isChecked() && !simplex->isChecked());
        QVERIFY(rev->isEnabled());
        simplex->click();
        QCOMPARE(s->fmTxMode(), FmTxMode::Simplex);
        QVERIFY(!rev->isEnabled());
        plus->click();

        // Ablage (kHz) in beide Richtungen.
        offset->setValue(1600);
        QCOMPARE(s->fmOffsetHz(), 1'600'000);
        s->setFmOffsetHz(5'000'000);
        QCOMPARE(offset->value(), 5000);

        // Aenderung von anderswo: die Seite folgt.
        s->setFmReverse(true);
        QVERIFY(rev->isChecked());
        s->setFmTxMode(FmTxMode::Low);
        QVERIFY(!rev->isChecked());
        QVERIFY(minus->isChecked());

        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            s->setFmReverse(true);
            applet.resize(300, 520);
            applet.show();
            QVERIFY(QTest::qWaitForWindowExposed(&applet));
            applet.grab().save(grabDir + QStringLiteral("/fm-seite.png"));
        }
    }

    void aNewTxModeTurnsReverseOffAndTheReceiverComesBack()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        SliceModel* s = model.sliceById(a);
        QVERIFY(s);
        model.wireSliceSignalsForTest();
        s->setFrequency(145'600'000.0);
        s->setDspMode(DSPMode::FM);
        s->setFmOffsetHz(600'000);
        s->setFmTxMode(FmTxMode::High);
        s->setFmReverse(true);
        QCOMPARE(s->frequency(), 146'200'000.0);    // RX auf der Eingabe

        s->setFmTxMode(FmTxMode::Low);
        QVERIFY(!s->fmReverse());
        QCOMPARE(s->frequency(), 145'600'000.0);    // zurueck, mit der ALTEN Ablage
    }

    void theVfoFlagFollowsTheSlice()
    {
        RadioModel model;
        const int a = model.addSlice();
        SliceModel* s = model.sliceById(a);
        QVERIFY(s);
        s->setDspMode(DSPMode::FM);
        s->setFmTxMode(FmTxMode::High);
        FmOptContainer flag;
        flag.setSlice(s);
        auto* rev = flag.findChild<QPushButton*>(QStringLiteral("revBtn"));
        auto* simplex = flag.findChild<QPushButton*>(QStringLiteral("simplexBtn"));
        QVERIFY(rev && simplex);
        QVERIFY(!rev->isChecked());
        s->setFmReverse(true);          // z. B. von der FM-Seite
        QVERIFY(rev->isChecked());
        s->setFmTxMode(FmTxMode::Simplex);
        QVERIFY(!rev->isChecked());     // Reverse aus
        QVERIFY(!rev->isEnabled());     // in Simplex gesperrt
        QVERIFY(simplex->isChecked());
    }
};

QTEST_MAIN(TstFmAppletWiring)
#include "tst_fm_applet_wiring.moc"
