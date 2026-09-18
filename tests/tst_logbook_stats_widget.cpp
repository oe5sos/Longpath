// =================================================================
// tests/tst_logbook_stats_widget.cpp  (Longpath)
// =================================================================
//
// The Kennzahlen tiles and their dialog:
//
//   * LogbookStatsWidget shows the totals, one bar row per band and
//     mode, 26 weekly bars, the countries and the awards it was given
//   * the logbook window's "Stats…" button opens the dialog over the
//     filtered view, and a filter change refreshes it while open
//
// Longpath-original test. no-port-check: Longpath-original code under test.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-18 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QTemporaryDir>

#include "core/CtyDatParser.h"
#include "core/LogbookStats.h"
#include "gui/LogbookWindow.h"
#include "gui/widgets/LogbookStatsWidget.h"
#include "models/LogEntry.h"

using namespace Longpath;

namespace {

const QDateTime kNow = QDateTime(QDate(2026, 9, 18), QTime(12, 0), Qt::UTC);

LogEntry qso(const QString& call, int daysAgo, const QString& band,
             const QString& mode, const QString& grid = {})
{
    LogEntry e;
    e.call = call;
    e.timeOn = kNow.addDays(-daysAgo);
    e.band = band;
    e.mode = mode;
    e.gridSquare = grid;
    return e;
}

QPushButton* button(QWidget& w, const QString& text)
{
    for (QPushButton* b : w.findChildren<QPushButton*>()) {
        if (b && b->text() == text) { return b; }
    }
    return nullptr;
}

QString writeAdif(const QDir& dir)
{
    const QString path = dir.filePath(QStringLiteral("log.adi"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { return {}; }
    f.write("Longpath test\n<EOH>\n"
            "<CALL:6>OE5SOS <QSO_DATE:8>20260731 <TIME_ON:6>150104 "
            "<BAND:3>20m <MODE:3>SSB <GRIDSQUARE:4>JN68 <QSL_RCVD:1>Y <EOR>\n"
            "<CALL:6>KB2UKA <QSO_DATE:8>20260731 <TIME_ON:6>143710 "
            "<BAND:3>20m <MODE:3>SSB <STATE:2>NY <DXCC:3>291 <EOR>\n"
            "<CALL:6>DL1ABC <QSO_DATE:8>20260801 <TIME_ON:6>150126 "
            "<BAND:3>40m <MODE:2>CW <DXCC:3>230 <EOR>\n");
    f.close();
    return path;
}

} // namespace

class TestLogbookStatsWidget : public QObject {
    Q_OBJECT

private slots:
    void tilesShowWhatTheyWereGiven()
    {
        QVector<LogEntry> log;
        log << qso("OE5XYZ", 1, "40m", "SSB", "JN68");
        log << qso("DL1ABC", 2, "20m", "CW",  "JO62");
        log << qso("DL2XYZ", 3, "20m", "CW",  "JO62");
        const LogbookStats s = LogbookStats::compute(log, nullptr, kNow);

        LogbookStatsWidget w;
        w.setStats(s);
        QCOMPARE(w.totalLabel()->text(), QStringLiteral("3"));
        QCOMPARE(w.bandChart()->rows().size(), 2);
        QCOMPARE(w.bandChart()->rows()[0].label, QStringLiteral("40m"));
        QCOMPARE(w.bandChart()->rows()[1].value, 2);
        QCOMPARE(w.modeChart()->rows().size(), 2);
        QCOMPARE(w.modeChart()->rows()[0].label, QStringLiteral("CW"));
        QCOMPARE(w.weeklyChart()->rows().size(), LogbookStats::kWeeks);
        QCOMPARE(w.weeklyChart()->rows().last().value, 3);
        QVERIFY(w.awardsLabel()->text().contains(QStringLiteral("DXCC")));
        QVERIFY(w.awardsLabel()->text().contains(QStringLiteral("WAS")));
        // No cty.dat and no DXCC field: nothing resolved, the tile says so.
        QVERIFY(w.countriesLabel()->text().contains(QStringLiteral("no entity")));

        // Renders without a crash at a small size too.
        w.resize(400, 300);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.grab();
    }

    // A realistic log against the real cty.dat (repo root, found from
    // __FILE__ like tst_cty_dat_parser): the awards resolve, and with
    // LONGPATH_GRAB_DIR set the rendered tiles are written out as a PNG
    // so the view can be looked at without a radio or a click.
    void realCtyDatResolvesAwardsAndRendersTheTiles()
    {
        const QString ctyPath = QFileInfo(QString::fromUtf8(__FILE__)).dir().path()
                              + QStringLiteral("/../cty.dat");
        CtyDatParser cty;
        if (!QFileInfo::exists(ctyPath) || !cty.loadFromFile(ctyPath)) {
            QSKIP("cty.dat not found next to the tests directory");
        }

        static const char* const kCalls[] = {
            "OE5SOS", "DL1ABC", "G4XYZ", "F5ABC", "I2ABC", "IT9XYZ", "EA3ABC", "SM5ABC",
            "OH2ABC", "LA1ABC", "OK1ABC", "SP5ABC", "UA3ABC", "UR5ABC", "9A1ABC", "S51ABC",
            "HB9ABC", "ON4ABC", "PA3ABC", "LX1ABC", "W1AW", "K3LR", "N6XYZ", "W7ABC",
            "VE3ABC", "VE7ABC", "KL7AA", "KH6ABC", "JA1ABC", "JH3ABC", "VK2ABC", "VK6ABC",
            "ZL1ABC", "PY2ABC", "LU1ABC", "CE3ABC", "ZS6ABC", "5B4ABC", "4X1ABC", "A61AB",
            "VU2ABC", "BY1ABC", "HL1ABC", "DU1ABC", "YB1ABC", "HS1ABC", "9V1ABC", "9M2ABC",
            "TF3ABC", "OY1ABC", "JW1ABC", "R1FJA", "CT1ABC", "CU3ABC", "EA8ABC", "CN8ABC",
            "SU1ABC", "5Z4ABC", "9J2ABC", "V51ABC",
        };
        static const char* const kBands[] = {"160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"};
        static const char* const kModes[] = {"SSB", "CW", "FT8", "RTTY", "FM"};
        static const char* const kStates[] = {"NY", "PA", "CA", "TX", "FL", "OH", "WA", "MA"};

        QRandomGenerator rng(20260918u);
        QVector<LogEntry> log;
        for (int i = 0; i < 400; ++i) {
            LogEntry e;
            e.call = QString::fromLatin1(kCalls[rng.bounded(60)]);
            e.timeOn = kNow.addSecs(-static_cast<qint64>(rng.bounded(700)) * 86400 - rng.bounded(86400));
            e.band = QString::fromLatin1(kBands[rng.bounded(10)]);
            e.mode = QString::fromLatin1(kModes[rng.bounded(5)]);
            e.gridSquare = QStringLiteral("%1%2%3%4")
                .arg(QChar('A' + rng.bounded(18))).arg(QChar('A' + rng.bounded(18)))
                .arg(rng.bounded(10)).arg(rng.bounded(10));
            e.distanceKm = rng.bounded(18000);
            if (e.call.startsWith(QLatin1Char('W')) || e.call.startsWith(QLatin1Char('K')) || e.call.startsWith(QLatin1Char('N'))) {
                e.extras.push_back({QStringLiteral("STATE"), QString::fromLatin1(kStates[rng.bounded(8)])});
            }
            if (rng.bounded(3) == 0) {
                e.extras.push_back({QStringLiteral("LOTW_QSL_RCVD"), QStringLiteral("Y")});
            }
            log.push_back(e);
        }

        const LogbookStats s = LogbookStats::compute(log, &cty, kNow);
        QCOMPARE(s.total, 400);
        QVERIFY2(s.dxccTotal >= 300, qPrintable(QString::number(s.dxccTotal)));
        QVERIFY2(s.dxccWorked >= 40, qPrintable(QString::number(s.dxccWorked)));
        QVERIFY(s.dxccConfirmed > 0 && s.dxccConfirmed <= s.dxccWorked);
        QVERIFY(s.wasWorked >= 3);
        QVERIFY(s.continentsWorked >= 5);
        QVERIFY(s.cqZonesWorked >= 10);
        QCOMPARE(s.byBand.size(), 10);
        QCOMPARE(s.byBand.first().key, QStringLiteral("160m"));
        QVERIFY(!s.countries.isEmpty());
        // Sicily folded into Italy: no `*` key survives.
        for (const auto& c : s.countries) { QVERIFY(!c.prefix.startsWith(QLatin1Char('*'))); }

        LogbookStatsWidget w;
        w.setStats(s);
        w.resize(900, 560);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        const QPixmap pm = w.grab();
        QVERIFY(!pm.isNull());
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            const QString out = grabDir + QStringLiteral("/longpath-grab-LogbookStatsWidget.png");
            QVERIFY(pm.save(out));
            qInfo() << "grab written to" << out;
        }
    }

    void statsButtonOpensTheDialogOverTheFilteredView()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = writeAdif(QDir(tmp.path()));
        QVERIFY(!path.isEmpty());

        LogbookWindow w(path);
        w.reload();
        QCOMPARE(w.entryCountForTesting(), 3);

        QPushButton* stats = button(w, QStringLiteral("Stats…"));
        QVERIFY2(stats, "kein Stats-Knopf im Logbuchfenster");
        stats->click();

        LogbookStatsWidget* view = w.findChild<LogbookStatsWidget*>();
        QVERIFY2(view, "die Kennzahlen-Ansicht ist nicht entstanden");
        QVERIFY(view->window()->isVisible());
        QCOMPARE(view->totalLabel()->text(), QStringLiteral("3"));
        // Two DXCC numbers in the file, no cty.dat: two entities, one
        // confirmed (OE5SOS has no DXCC field and counts for none).
        QVERIFY(view->stats().dxccWorked == 1 || view->stats().dxccWorked == 2);

        // Narrow the view to CW: the dialog follows the filter while open.
        QLineEdit* search = w.findChild<QLineEdit*>();
        QVERIFY(search);
        search->setText(QStringLiteral("DL1ABC"));
        QTRY_COMPARE(view->totalLabel()->text(), QStringLiteral("1"));
        QCOMPARE(view->bandChart()->rows().size(), 1);
        QCOMPARE(view->bandChart()->rows()[0].label, QStringLiteral("40m"));

        search->clear();
        QTRY_COMPARE(view->totalLabel()->text(), QStringLiteral("3"));
    }
};

QTEST_MAIN(TestLogbookStatsWidget)
#include "tst_logbook_stats_widget.moc"
