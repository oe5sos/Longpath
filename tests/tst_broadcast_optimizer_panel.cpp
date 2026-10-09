// The optimiser panel: does it actually show four different profiles,
// and does it go properly blank when it has nothing?
//
// The blank state is the half worth testing hardest. A panel that
// keeps the previous take's four profiles on screen beside a new
// recording is the most quietly misleading thing this window could
// do — nothing looks broken, every number is plausible, and they all
// describe a voice that was measured two minutes ago.
//
// Offscreen (QT_QPA_PLATFORM=offscreen in longpath_add_test), so no
// window opens and nobody mistakes it for Longpath.
// no-port-check: Longpath-original.

#include <QtTest/QtTest>

#include "gui/applets/BroadcastOptimizerPanel.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>

using namespace Longpath;

namespace {

VoiceAnalysis ordinary()
{
    VoiceAnalysis a;
    a.valid = true;
    a.analysedSeconds = 9.0;
    a.speechFraction  = 0.6;
    a.bandDb = {-30.0, -20.0, -6.0, -2.0, -1.0, 0.0, 1.0, -6.0, -18.0, -30.0};
    a.suggestedEqDb = {0.0, 0.0, -4.0, -6.0, -3.0, 0.0, -1.0, 0.0, 0.0, 0.0};
    a.suggestedPreampDb = 4.0;
    a.noiseFloorDbFs = -62.0;
    a.speechDbFs     = -20.0;
    a.crestFactorDb  = 14.0;
    a.humDb          = -45.0;
    a.humBaseHz      = 50;
    a.sibilanceDb    = 1.0;
    a.pitchHz        = 110.0;
    a.rolloffHz      = 6000.0;
    a.loudnessLufs   = -23.4;
    a.loudnessRangeLu = 5.2;
    a.loudnessRangeValid = true;
    return a;
}

// Every value cell in the reading grid, found by walking the widget
// tree rather than by holding pointers the panel does not expose.
// Cheap, and it means the test breaks if a cell is removed — which is
// the right thing to happen.
QStringList gridValues(QWidget* panel)
{
    QStringList out;
    const QList<QLabel*> labels = panel->findChildren<QLabel*>();
    for (QLabel* l : labels) {
        // The value cells are the monospaced ones.
        if (l->font().family().contains(QStringLiteral("Mono"),
                                        Qt::CaseInsensitive)
            || l->font().fixedPitch()) {
            out << l->text();
        }
    }
    return out;
}

QList<QPushButton*> widthPills(QWidget* panel)
{
    QList<QPushButton*> out;
    for (QPushButton* b : panel->findChildren<QPushButton*>()) {
        if (b->isCheckable()) { out << b; }
    }
    return out;
}

} // namespace

class TstBroadcastOptimizerPanel : public QObject {
    Q_OBJECT

private slots:

    // Rule 7 of the house style: unknown is a dash, never a zero. A
    // grid of noughts looks like a measurement.
    void blankBeforeAnyTake()
    {
        BroadcastOptimizerPanel panel;
        QCOMPARE(panel.selectedIndex(), -1);
        QVERIFY(panel.plans().isEmpty());

        const QStringList values = gridValues(&panel);
        QVERIFY(!values.isEmpty());
        for (const QString& v : values) {
            QCOMPARE(v, QStringLiteral("——"));
        }
    }

    void fourProfilesAfterATake()
    {
        BroadcastOptimizerPanel panel;
        panel.setAnalysis(ordinary());

        QCOMPARE(panel.plans().size(), BroadcastOptimizer::kWidthCount);

        // 2.7 kHz comes up selected: it is where an operator who has
        // not said otherwise is transmitting.
        QCOMPARE(panel.selectedIndex(), 1);
        QCOMPARE(panel.plans()[panel.selectedIndex()].widthHz, 2700.0);

        // Rule 2: exactly one pill lit.
        const QList<QPushButton*> pills = widthPills(&panel);
        QCOMPARE(pills.size(), BroadcastOptimizer::kWidthCount);
        int lit = 0;
        for (QPushButton* b : pills) {
            if (b->isChecked()) { ++lit; }
            QVERIFY(b->isEnabled());
        }
        QCOMPARE(lit, 1);

        // And the grid is no longer dashes.
        const QStringList values = gridValues(&panel);
        int filled = 0;
        for (const QString& v : values) {
            if (v != QStringLiteral("——")) { ++filled; }
        }
        QVERIFY(filled >= 6);
    }

    // Choosing a different width must change what is displayed. If
    // the readout were wired to the plan list rather than to the
    // selection, every pill would show the same numbers and the panel
    // would still look entirely correct.
    void pickingAWidthChangesTheReadout()
    {
        BroadcastOptimizerPanel panel;
        panel.setAnalysis(ordinary());

        const QStringList before = gridValues(&panel);
        const QList<QPushButton*> pills = widthPills(&panel);
        pills[3]->click();                     // 4.0k

        QCOMPARE(panel.selectedIndex(), 3);
        QVERIFY(gridValues(&panel) != before);

        int lit = 0;
        for (QPushButton* b : pills) {
            if (b->isChecked()) { ++lit; }
        }
        QCOMPARE(lit, 1);
        QVERIFY(pills[3]->isChecked());
    }

    // A new recording that cannot be used must CLEAR the panel, not
    // leave the last good take's profiles standing.
    void aClippedTakeClearsIt()
    {
        BroadcastOptimizerPanel panel;
        panel.setAnalysis(ordinary());
        QCOMPARE(panel.plans().size(), 4);

        VoiceAnalysis clipped = ordinary();
        clipped.clippedSamples = 2000;
        panel.setAnalysis(clipped);

        QVERIFY(panel.plans().isEmpty());
        QCOMPARE(panel.selectedIndex(), -1);
        for (const QString& v : gridValues(&panel)) {
            QCOMPARE(v, QStringLiteral("——"));
        }
    }

    // The panel does nothing itself — it asks. Check it asks with the
    // plan that is actually selected, not with the first one.
    void auditionAsksForTheSelectedPlan()
    {
        BroadcastOptimizerPanel panel;
        panel.setAnalysis(ordinary());
        widthPills(&panel)[0]->click();        // 2.3k

        QSignalSpy spy(&panel,
                       &BroadcastOptimizerPanel::auditionRequested);
        QVERIFY(spy.isValid());

        for (QPushButton* b : panel.findChildren<QPushButton*>()) {
            if (b->text().contains(QStringLiteral("With this profile"))) {
                b->click();
                break;
            }
        }
        QCOMPARE(spy.count(), 1);
        const auto plan =
            spy.at(0).at(0).value<BroadcastOptimizer::Plan>();
        QCOMPARE(plan.widthHz, 2300.0);
    }

    // ── A picture, for the record ────────────────────────────────────
    //
    // Writes a PNG of the filled panel when LONGPATH_RENDER_DIR is
    // set, and does nothing otherwise — CI runs the test without the
    // variable and writes no files.
    //
    // This exists because the panel cannot be seen in the running
    // program without a radio, a microphone and a transmit pump: it
    // only fills after a real fifteen-second take. Rendering it here
    // is how the design gets looked at before any of that is
    // available, and it is the same thing the logbook statistics and
    // the RX-profile sheet do.
    void rendersForTheRecord()
    {
        const QByteArray dir = qgetenv("LONGPATH_RENDER_DIR");
        if (dir.isEmpty()) {
            QSKIP("LONGPATH_RENDER_DIR not set — nothing to draw into");
        }

        BroadcastOptimizerPanel panel;
        panel.setStyleSheet(QStringLiteral("background: #0c0c0e; color: %1;")
            .arg(QStringLiteral("#dcdce1")));
        panel.resize(900, 620);

        panel.setAnalysis(ordinary());
        panel.grab().save(QString::fromUtf8(dir)
                          + QStringLiteral("/optimierer-2700.png"));

        // The narrow one too: it is the profile that forgets stages,
        // and the row that says so is the one worth looking at.
        widthPills(&panel)[0]->click();
        panel.grab().save(QString::fromUtf8(dir)
                          + QStringLiteral("/optimierer-2300.png"));

        // And a microphone that cannot deliver 4 kHz, so the warning
        // line is in the picture too.
        VoiceAnalysis dull = ordinary();
        dull.rolloffHz = 2800.0;
        panel.setAnalysis(dull);
        widthPills(&panel)[3]->click();
        panel.grab().save(QString::fromUtf8(dir)
                          + QStringLiteral("/optimierer-4000-warnung.png"));
    }

    // Level matching is on unless somebody turns it off. The default
    // is not a preference: an unmatched A/B is won by whichever side
    // is louder, and the optimiser is the side adding make-up gain.
    void levelMatchingDefaultsOn()
    {
        BroadcastOptimizerPanel panel;
        QVERIFY(panel.levelMatched());

        QCheckBox* box = panel.findChild<QCheckBox*>();
        QVERIFY(box != nullptr);
        box->setChecked(false);
        QVERIFY(!panel.levelMatched());
    }
};

QTEST_MAIN(TstBroadcastOptimizerPanel)
#include "tst_broadcast_optimizer_panel.moc"
