// no-port-check: Longpath-original test, no Thetis logic.
//
// ── ROTOR in der Eingabezeile des Logbuchs (2026-11-10) ─────────────
//
// Betreiber: „wenn man im logbuch das call eingibt und auf ROTOR klickt,
// sollte man sehen, wohin der rotor zeigt und wo das call steht. mit
// rotate sollte sich dann der rotor auch dorthin drehen."
//
// Bis heute stand fuer jede Station, die nicht im Log ist, „Not in your
// log — no bearing until you work them": der eigene Standort kam nur aus
// dem QSO oder aus StationGridSquare (bei Martin leer).
//
//   1. Eine neue Station bekommt eine Peilung (eigener Standort aus dem
//      Log, ihre Lage aus der Landesmitte) und ROTOR zeigt sie.
//   2. Rotate schickt den kurzen Weg, LP den langen -- mit Rufzeichen.
//   3. Der erlaubte Drehbereich gilt: ein Weg hinaus ist gesperrt und
//      sagt warum.
//   4. Die Karte sagt, woher die Lage stammt (QRZ-Adresse, Locator,
//      Landesmitte) und wann sie nur grob ist.

#include <QtTest/QtTest>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "FensterFrist.h"
#include "core/AdifLog.h"
#include "core/CallsignCache.h"
#include "core/RotorBereich.h"
#include "gui/LogbookWindow.h"
#include "gui/QsoMapWindow.h"
#include "gui/widgets/QsoDetailPane.h"
#include "models/LogEntry.h"

using namespace Longpath;

namespace {
LogEntry qso(const QString& call, const QString& grid)
{
    LogEntry e;
    e.call = call;
    e.band = QStringLiteral("20m");
    e.mode = QStringLiteral("SSB");
    e.gridSquare = grid;
    e.myGridSquare = QStringLiteral("JN67VV");
    e.timeOn = QDateTime(QDate(2026, 10, 1), QTime(12, 0), QTimeZone::UTC);
    return e;
}
// Landesmitte wie cty.dat sie liefert (hier fest: Japan).
bool landesmitte(const QString& call, double& lat, double& lon)
{
    if (call.startsWith(QStringLiteral("JA"))) { lat = 36.0; lon = 138.0; return true; }
    return false;
}
} // namespace

class TstLogbuchRotorKnopf : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init() { RotorBereich{}.save(); }

    void eineNeueStationBekommtEinePeilung()
    {
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("logbook.adi"));
        QVERIFY(AdifLog::write(pfad, {qso(QStringLiteral("DL1ABC"), QStringLiteral("JO62"))}));
        LogbookWindow w(pfad);
        w.setPositionFallback(&landesmitte);
        w.resize(1400, 900);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        QVERIFY(!w.entryPeilungForTest()->isVisible());
        QTest::keyClicks(w.entryCallForTest(), QStringLiteral("ja1xyz"));
        w.entryRotorForTest()->click();

        QVERIFY(w.entryPeilungForTest()->isVisible());
        QVERIFY(w.mapPanelForTest() && w.mapPanelForTest()->isVisible());
        double sp = 0.0, km = 0.0;
        QVERIFY2(w.detailForTest()->peilung(sp, km),
                 "eine Station, die nicht im Log steht, hat wieder keine Peilung");
        QCOMPARE(w.detailForTest()->gezeigtesRufzeichen(), QStringLiteral("JA1XYZ"));
        QCOMPARE(w.detailForTest()->lage(), QsoDetailPane::Lage::Landesmitte);
        QVERIFY(w.detailForTest()->lageGrob());
        // Von JN67 nach Japan: gut 40 Grad, rund 9000 km.
        QVERIFY2(sp > 30.0 && sp < 55.0, qPrintable(QString::number(sp)));
        QVERIFY2(km > 8500.0 && km < 9600.0, qPrintable(QString::number(km)));
        QVERIFY(w.entryPeilungForTest()->text().contains(QStringLiteral("country centre")));
        QVERIFY(w.entryRotateForTest()->isEnabled());
        QVERIFY(w.entryRotateForTest()->isVisible());

        // Esc / Leeren nimmt die Rotorzeile wieder weg.
        QTest::keyClick(w.entryCallForTest(), Qt::Key_Escape);
        QVERIFY(!w.entryPeilungForTest()->isVisible());
    }

    void rotateSchicktDenKurzenWegLpDenLangen()
    {
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("logbook.adi"));
        QVERIFY(AdifLog::write(pfad, {qso(QStringLiteral("DL1ABC"), QStringLiteral("JO62"))}));
        LogbookWindow w(pfad);
        w.setPositionFallback(&landesmitte);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        QSignalSpy drehen(&w, &LogbookWindow::turnRotorRequested);
        QTest::keyClicks(w.entryCallForTest(), QStringLiteral("JA1XYZ"));
        w.entryRotorForTest()->click();
        double sp = 0.0, km = 0.0;
        QVERIFY(w.detailForTest()->peilung(sp, km));

        w.entryRotateForTest()->click();
        QCOMPARE(drehen.count(), 1);
        QVERIFY(std::abs(drehen.at(0).at(0).toDouble() - sp) < 0.01);
        QCOMPARE(drehen.at(0).at(1).toString(), QStringLiteral("JA1XYZ"));
        w.entryLongPathForTest()->click();
        QCOMPARE(drehen.count(), 2);
        QVERIFY(std::abs(drehen.at(1).at(0).toDouble() - std::fmod(sp + 180.0, 360.0)) < 0.01);
    }

    // Weitertippen nach ROTOR: die Zeile zeigt nie die Peilung des alten
    // Rufzeichens unter dem neuen, und sie zieht nach, auch wenn die
    // Richtung gleich bleibt (beide Landesmitte Japan).
    void dieZeileFolgtDemNeuenRufzeichen()
    {
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("logbook.adi"));
        QVERIFY(AdifLog::write(pfad, {qso(QStringLiteral("DL1ABC"), QStringLiteral("JO62"))}));
        LogbookWindow w(pfad);
        w.setPositionFallback(&landesmitte);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        QTest::keyClicks(w.entryCallForTest(), QStringLiteral("JA1XY"));
        w.entryRotorForTest()->click();
        QVERIFY(w.entryPeilungForTest()->text().contains(QStringLiteral("country centre")));

        QTest::keyClicks(w.entryCallForTest(), QStringLiteral("Z"));
        QVERIFY2(w.entryPeilungForTest()->text().contains(QStringLiteral("no position yet")),
                 qPrintable(w.entryPeilungForTest()->text()));
        QVERIFY(!w.entryRotateForTest()->isEnabled());
        QTRY_COMPARE_WITH_TIMEOUT(w.detailForTest()->gezeigtesRufzeichen(),
                                  QStringLiteral("JA1XYZ"), 5000);
        QVERIFY2(w.entryPeilungForTest()->text().contains(QStringLiteral("country centre")),
                 qPrintable(w.entryPeilungForTest()->text()));
        QVERIFY(w.entryRotateForTest()->isEnabled());
    }

    void derErlaubteBereichGilt()
    {
        RotorBereich b;
        b.an = true; b.vonGrad = 270.0; b.bisGrad = 90.0; b.anschlagGrad = 180.0;
        b.save();
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("logbook.adi"));
        QVERIFY(AdifLog::write(pfad, {qso(QStringLiteral("DL1ABC"), QStringLiteral("JO62"))}));
        LogbookWindow w(pfad);
        w.setPositionFallback(&landesmitte);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        w.setRotorBearing(0.0);
        QTest::keyClicks(w.entryCallForTest(), QStringLiteral("JA1XYZ"));
        w.entryRotorForTest()->click();
        // Kurzer Weg (~42 Grad) liegt im Bogen 270..90, der lange (~222) nicht.
        QVERIFY(w.entryRotateForTest()->isEnabled());
        QVERIFY(!w.entryLongPathForTest()->isEnabled());
        QVERIFY(w.entryLongPathForTest()->toolTip().contains(QStringLiteral("Outside")));
        QVERIFY(w.entryPeilungForTest()->text().contains(QStringLiteral("rotor at 0")));
    }

    // Woher die Lage stammt -- QRZ meldet es mit <geoloc>.
    void dieLageSagtWoherSieStammt()
    {
        QTemporaryDir dir;
        CallsignCache cache(dir.filePath(QStringLiteral("cache.json")));
        CallsignInfo info;
        info.call = QStringLiteral("OE5ABC");
        info.latitude = 47.95; info.longitude = 13.95; info.hasLatLon = true;
        info.geoloc = QStringLiteral("geocode");
        info.fetchedUtc = QDateTime::currentSecsSinceEpoch();
        cache.put(info.call, info);

        QsoDetailPane pane;
        pane.setCache(&cache);
        pane.setHomeGrid([]() { return QStringLiteral("JN67VV"); });
        pane.showCallsign(QStringLiteral("OE5ABC"));
        QCOMPARE(pane.lage(), QsoDetailPane::Lage::QrzKoordinaten);
        QCOMPARE(pane.lageText(), QStringLiteral("street address (QRZ)"));
        QVERIFY(!pane.lageGrob());

        // Aus dem Log: ein vierstelliger Locator ist grob, ein sechsstelliger nicht.
        LogEntry e = qso(QStringLiteral("DL2XYZ"), QStringLiteral("JO62"));
        pane.setEntry(e);
        QCOMPARE(pane.lage(), QsoDetailPane::Lage::LogLocator);
        QVERIFY(pane.lageGrob());
        QVERIFY(pane.lageText().contains(QStringLiteral("±60 km")));
        e.gridSquare = QStringLiteral("JO62QM");
        pane.setEntry(e);
        QVERIFY(!pane.lageGrob());
        QVERIFY(pane.lageText().contains(QStringLiteral("JO62QM")));
    }
};

QTEST_MAIN(TstLogbuchRotorKnopf)
#include "tst_logbuch_rotor_knopf.moc"
