// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original regression test (mapping from Thetis,
// cited in RxApplet.cpp).
//
// FM-Rauschsperre am SQL-Regler (2026-09-27). In FM erreichte der Regler
// nur die Sprach-Sperre; setFmsqEnabled rief niemand. Thetis steuert
// je Betriebsart (console.cs:47225-47339): in FM die FM-Sperre mit
// Schwelle 10^(-2n/100).

#include <QtTest>
#include <QPushButton>
#include <QSlider>

#include "gui/applets/RxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstRxAppletFmSquelch : public QObject {
    Q_OBJECT

    static QPushButton* sqlButton(RxApplet& a)
    {
        for (QPushButton* b : a.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("SQL")) { return b; }
        }
        return nullptr;
    }
    static QSlider* sqlSlider(RxApplet& a, QPushButton* btn)
    {
        // Der Regler steht in derselben Zeile rechts neben dem Knopf.
        for (QSlider* s : a.findChildren<QSlider*>()) {
            if (s->parentWidget() == btn->parentWidget() && s->maximum() == 100
                && std::abs(s->geometry().center().y() - btn->geometry().center().y()) < 12) {
                return s;
            }
        }
        return nullptr;
    }

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void thetisMapping()
    {
        // 10^(-2n/100) == 10^(dB/20)  ->  dB = -0.4 n
        QCOMPARE(RxApplet::fmSquelchDbFromSlider(0), 0.0);
        QCOMPARE(RxApplet::fmSquelchDbFromSlider(50), -20.0);
        QCOMPARE(RxApplet::fmSquelchSliderFromDb(-20.0), 50);
        QCOMPARE(RxApplet::fmSquelchSliderFromDb(-150.0), 100);   // alte Voreinstellung
        QVERIFY(std::abs(std::pow(10.0, RxApplet::fmSquelchDbFromSlider(37) / 20.0)
                         - std::pow(10.0, -2.0 * 37 / 100.0)) < 1e-12);
    }

    void inFmTheButtonDrivesTheFmSquelch()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        SliceModel* s = model.slices().at(a);
        RxApplet applet(s, &model);
        applet.resize(400, 600);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet));
        QPushButton* btn = sqlButton(applet);
        QVERIFY(btn);
        QSlider* slider = sqlSlider(applet, btn);
        QVERIFY(slider);

        s->setDspMode(DSPMode::FM);
        const bool ssqlBefore = s->ssqlEnabled();
        btn->setChecked(true);
        QVERIFY(s->fmsqEnabled());
        QCOMPARE(s->ssqlEnabled(), ssqlBefore);
        slider->setValue(50);
        QCOMPARE(s->fmsqThresh(), -20.0);

        // Anderswo wie bisher die Sprach-Sperre; der Knopf zeigt je Betriebsart.
        s->setDspMode(DSPMode::USB);
        QCOMPARE(btn->isChecked(), s->ssqlEnabled());
        btn->setChecked(!s->ssqlEnabled());
        QVERIFY(s->fmsqEnabled());                 // FM-Sperre unberuehrt
        s->setDspMode(DSPMode::FM);
        QVERIFY(btn->isChecked());                 // zeigt wieder die FM-Sperre
        QCOMPARE(slider->value(), 50);
    }
};

QTEST_MAIN(TstRxAppletFmSquelch)
#include "tst_rxapplet_fm_squelch.moc"
