// Das Messblatt: zeigt es, was gemessen wurde, und raeumt es auf,
// wenn nichts Brauchbares da ist?
//
// Der leere Zustand ist die Haelfte, auf die es ankommt. Ein Blatt,
// das nach einer missglueckten Aufnahme die Werte der vorigen stehen
// laesst, sieht nicht kaputt aus -- jede Zahl ist plausibel, und alle
// beschreiben eine Stimme von vor zwei Minuten.
//
// Offscreen (QT_QPA_PLATFORM=offscreen in longpath_add_test).
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
    a.noiseFloorDbFs = -62.0;
    a.speechDbFs     = -20.0;
    a.crestFactorDb  = 14.0;
    a.sibilanceDb    = 1.0;
    a.sibilanceHz    = 6300.0;
    a.pitchHz        = 110.0;
    a.rolloffHz      = 6000.0;
    a.loudnessLufs   = -23.4;
    a.loudnessRangeLu = 5.2;
    a.loudnessRangeValid = true;
    return a;
}

QStringList gridValues(QWidget* panel)
{
    QStringList out;
    for (QLabel* l : panel->findChildren<QLabel*>()) {
        if (l->font().fixedPitch()
            || l->font().family().contains(QStringLiteral("Mono"),
                                           Qt::CaseInsensitive)) {
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

    // Hausstil Regel 7: Unbekannt ist ein Strich, keine Null.
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

    void fourChannelsAfterATake()
    {
        BroadcastOptimizerPanel panel;
        panel.setAnalysis(ordinary());

        QCOMPARE(panel.plans().size(), BroadcastOptimizer::kWidthCount);
        QCOMPARE(panel.selectedIndex(), 1);          // 2,7 k als Vorgabe

        const QList<QPushButton*> pills = widthPills(&panel);
        QCOMPARE(pills.size(), BroadcastOptimizer::kWidthCount);
        int lit = 0;
        for (QPushButton* b : pills) {
            if (b->isChecked()) { ++lit; }
        }
        QCOMPARE(lit, 1);

        int filled = 0;
        for (const QString& v : gridValues(&panel)) {
            if (v != QStringLiteral("——")) { ++filled; }
        }
        QVERIFY2(filled >= 5, "die Messwerte stehen da");
    }

    // Eine andere Breite muss die Anzeige aendern -- sonst haengt das
    // Ablesegitter an der Liste statt an der Auswahl, und alle vier
    // Pillen zeigen dasselbe.
    void pickingAChannelChangesTheReadout()
    {
        BroadcastOptimizerPanel panel;
        panel.setAnalysis(ordinary());

        const QStringList before = gridValues(&panel);
        widthPills(&panel)[3]->click();
        QCOMPARE(panel.selectedIndex(), 3);
        QVERIFY(gridValues(&panel) != before);
    }

    // Eine unbrauchbare Aufnahme raeumt auf.
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

    // Eine neue Messung derselben Stimme waehlt nicht fuer den
    // Bediener: wer 4,0 k hoert und neu misst, bleibt bei 4,0 k.
    void aNewTakeKeepsTheChosenChannel()
    {
        BroadcastOptimizerPanel panel;
        panel.setAnalysis(ordinary());
        widthPills(&panel)[3]->click();
        QCOMPARE(panel.selectedIndex(), 3);

        panel.setAnalysis(ordinary());
        QCOMPARE(panel.selectedIndex(), 3);
    }

    // Das Blatt tut nichts selbst -- es fragt. Und zwar mit der
    // Breite, die gewaehlt ist, nicht mit der ersten.
    void itAsksWithTheSelectedChannel()
    {
        BroadcastOptimizerPanel panel;
        panel.setAnalysis(ordinary());
        widthPills(&panel)[0]->click();

        QSignalSpy spy(&panel, &BroadcastOptimizerPanel::afterRequested);
        QVERIFY(spy.isValid());
        for (QPushButton* b : panel.findChildren<QPushButton*>()) {
            if (b->text().contains(QStringLiteral("your strip"))) {
                b->click();
                break;
            }
        }
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<BroadcastOptimizer::Plan>().widthHz,
                 2300.0);

        QSignalSpy save(&panel,
                        &BroadcastOptimizerPanel::saveProfileRequested);
        for (QPushButton* b : panel.findChildren<QPushButton*>()) {
            if (b->text().contains(QStringLiteral("Save as profile"))) {
                b->click();
                break;
            }
        }
        QCOMPARE(save.count(), 1);
    }

    // Pegelabgleich ist AUS: man soll hoeren, was die eigene
    // Einstellung an Lautheit bringt.
    void levelMatchingDefaultsOff()
    {
        BroadcastOptimizerPanel panel;
        QVERIFY(!panel.levelMatched());
        QCheckBox* box = panel.findChild<QCheckBox*>();
        QVERIFY(box != nullptr);
        box->setChecked(true);
        QVERIFY(panel.levelMatched());
    }

    void rendersForTheRecord()
    {
        const QByteArray dir = qgetenv("LONGPATH_RENDER_DIR");
        if (dir.isEmpty()) { QSKIP("LONGPATH_RENDER_DIR nicht gesetzt"); }

        BroadcastOptimizerPanel panel;
        panel.setStyleSheet(
            QStringLiteral("background: #0c0c0e; color: #dcdce1;"));
        panel.resize(900, 520);
        panel.setAnalysis(ordinary());
        panel.grab().save(QString::fromUtf8(dir)
                          + QStringLiteral("/messblatt-2700.png"));
        widthPills(&panel)[0]->click();
        panel.grab().save(QString::fromUtf8(dir)
                          + QStringLiteral("/messblatt-2300.png"));
    }
};

QTEST_MAIN(TstBroadcastOptimizerPanel)
#include "tst_broadcast_optimizer_panel.moc"
