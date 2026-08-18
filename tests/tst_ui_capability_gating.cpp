// no-port-check: widget-level capability-gating test (Phase 3P-I-a T21)
//
// Verifies that setBoardCapabilities() on RxApplet actually
// hides the ANT buttons when the connected board has no Alex front-end
// (HL2 / Atlas), and shows them when hasAlex is true with an adequate
// antennaInputCount. This is a widget-level integration test — it
// constructs the real widget and asserts against the child QPushButton
// states via findChild<QPushButton*>("m_rxAntBtn" / "m_txAntBtn").
//
// The objectName strings must stay in sync with the setObjectName calls
// in RxApplet::buildUi; the test will
// FAIL LOUDLY if those names change (which is the intended coupling).
//
// Related:
//   tests/tst_rxapplet_antenna_buttons.cpp — per-band AlexController sync
//   tests/tst_antenna_routing_model.cpp    — RadioModel pump integration

#include <QtTest/QtTest>
#include <QPushButton>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "gui/applets/RxApplet.h"

using namespace NereusSDR;

class TestUiCapabilityGating : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }

    // Die drei Faelle zur VFO-Flagge sind am 2026-08-18 mit ihr
    // gegangen. Sie prueften dieselbe Gatterung an ihren
    // Antennenknoepfen — dieselbe Zusicherung, zweite Flaeche. Die
    // RxApplet-Haelfte unten bleibt und ist jetzt die einzige.

    // --- RxApplet ---

    void rxApplet_hides_antButtons_when_noAlex() {
        RxApplet w(nullptr, nullptr, nullptr);
        BoardCapabilities caps;  // defaults: hasAlex=false
        w.setBoardCapabilities(caps);

        auto* rxBtn = w.findChild<QPushButton*>(QStringLiteral("m_rxAntBtn"));
        auto* txBtn = w.findChild<QPushButton*>(QStringLiteral("m_txAntBtn"));
        QVERIFY2(rxBtn, "RxApplet missing m_rxAntBtn objectName");
        QVERIFY2(txBtn, "RxApplet missing m_txAntBtn objectName");
        QVERIFY(rxBtn->isHidden());
        QVERIFY(txBtn->isHidden());
    }

    void rxApplet_shows_antButtons_when_hasAlex() {
        RxApplet w(nullptr, nullptr, nullptr);
        BoardCapabilities caps;
        caps.hasAlex = true;
        caps.antennaInputCount = 3;
        w.setBoardCapabilities(caps);

        auto* rxBtn = w.findChild<QPushButton*>(QStringLiteral("m_rxAntBtn"));
        auto* txBtn = w.findChild<QPushButton*>(QStringLiteral("m_txAntBtn"));
        QVERIFY(rxBtn && !rxBtn->isHidden());
        QVERIFY(txBtn && !txBtn->isHidden());
    }
};

QTEST_MAIN(TestUiCapabilityGating)
#include "tst_ui_capability_gating.moc"
