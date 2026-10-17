// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test, no Thetis logic.
//
// Die Drehrichtung auf der Karteikarte des Logbuchs (2026-09-26).
// OE5VVM in Laakirchen liegt 9 km noerdlich der Station; im Log stand
// nur JN67, und dessen Mittelpunkt liegt 74 km im Suedwesten. Die Karte
// sagte "11°", die Karteikarte "Turn to 234°" -- der Rotor waere in die
// Gegenrichtung gefahren. Jetzt rechnen beide aus QRZs Koordinaten.

#include <QtTest>
#include <QPushButton>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/BeamHeading.h"
#include "core/CallsignCache.h"
#include "core/Maidenhead.h"
#include "gui/widgets/QsoDetailPane.h"
#include "models/LogEntry.h"

using namespace Longpath;

namespace {

LogEntry oe5vvm()
{
    LogEntry e;
    e.call = QStringLiteral("OE5VVM");
    e.gridSquare = QStringLiteral("JN67");        // nur das Grossfeld
    e.myGridSquare = QStringLiteral("JN67VW");    // die Station
    e.bearingDeg = 234.0;                        // wie im Log gespeichert
    e.distanceKm = 74.0;
    return e;
}

QPushButton* turnButton(QsoDetailPane& pane)
{
    for (QPushButton* b : pane.findChildren<QPushButton*>()) {
        if (b->text().startsWith(QStringLiteral("Turn"))) { return b; }
    }
    return nullptr;
}

} // namespace

class TstLogbookBeamBearing : public QObject { Q_OBJECT
private slots:
    // Mit QRZ-Koordinaten zeigt und dreht die Karte dorthin, wo die
    // Station wirklich ist -- dieselbe Zahl wie die Karte.
    void qrzCoordinatesBeatTheLoggedLocator()
    {
        CallsignCache cache;
        CallsignInfo info;
        info.call = QStringLiteral("OE5VVM");
        info.grid = QStringLiteral("JN67");
        info.latitude = 47.98;                    // Laakirchen
        info.longitude = 13.82;
        info.hasLatLon = true;
        info.fetchedUtc = QDateTime::currentSecsSinceEpoch();
        cache.put(info.call, info);

        QsoDetailPane pane;
        pane.setCache(&cache);
        pane.setEntry(oe5vvm());

        double hlat = 0.0, hlon = 0.0;
        calculateLatLonFromGridSquare(QStringLiteral("JN67VW"), hlat, hlon);
        const BeamHeading::GreatCircle want =
            BeamHeading::greatCircle(hlat, hlon, 47.98, 13.82);

        QPushButton* turn = turnButton(pane);
        QVERIFY(turn);
        QVERIFY(turn->isEnabled());
        QCOMPARE(turn->text(), QStringLiteral("Turn to %1°").arg(want.bearingDeg, 0, 'f', 0));

        QSignalSpy spy(&pane, &QsoDetailPane::turnRotorRequested);
        turn->click();
        QCOMPARE(spy.count(), 1);
        const double sent = spy.first().at(0).toDouble();
        QVERIFY2(std::abs(sent - want.bearingDeg) < 0.01, qPrintable(QString::number(sent)));
        // Nicht mehr die 234° aus dem Log.
        QVERIFY2(std::abs(sent - 234.0) > 90.0, qPrintable(QString::number(sent)));
    }

    // Ohne Lookup bleibt es beim Locator aus dem Log -- neu gerechnet,
    // nicht der gespeicherte Wert, aber aus derselben Quelle.
    void withoutLookupTheLoggedLocatorCounts()
    {
        QsoDetailPane pane;
        pane.setEntry(oe5vvm());
        const double want = calculateBearingInDegrees(QStringLiteral("JN67VW"),
                                                      QStringLiteral("JN67"));
        QSignalSpy spy(&pane, &QsoDetailPane::turnRotorRequested);
        QPushButton* turn = turnButton(pane);
        QVERIFY(turn && turn->isEnabled());
        turn->click();
        QCOMPARE(spy.count(), 1);
        QVERIFY2(std::abs(spy.first().at(0).toDouble() - want) < 1.0,
                 qPrintable(QString::number(spy.first().at(0).toDouble())));
    }

    // Ein QRZ-Locator mit sechs Zeichen schlaegt vier aus dem Log.
    void aFinerQrzLocatorBeatsACoarseLoggedOne()
    {
        CallsignCache cache;
        CallsignInfo info;
        info.call = QStringLiteral("OE5VVM");
        info.grid = QStringLiteral("JN67VX");
        info.fetchedUtc = QDateTime::currentSecsSinceEpoch();
        cache.put(info.call, info);

        QsoDetailPane pane;
        pane.setCache(&cache);
        pane.setEntry(oe5vvm());
        const double want = calculateBearingInDegrees(QStringLiteral("JN67VW"),
                                                      QStringLiteral("JN67VX"));
        QSignalSpy spy(&pane, &QsoDetailPane::turnRotorRequested);
        QPushButton* turn = turnButton(pane);
        QVERIFY(turn && turn->isEnabled());
        turn->click();
        QCOMPARE(spy.count(), 1);
        QVERIFY2(std::abs(spy.first().at(0).toDouble() - want) < 1.0,
                 qPrintable(QString::number(spy.first().at(0).toDouble())));
    }

    // Ohne eigenen Locator gibt es keine Richtung -- lieber nichts als Nord.
    void noHomeNoBearing()
    {
        LogEntry e = oe5vvm();
        e.myGridSquare.clear();
        QsoDetailPane pane;
        pane.setEntry(e);
        QPushButton* turn = turnButton(pane);
        QVERIFY(turn);
        // Faellt auf den Locator der Station zurueck; ist der auch leer,
        // bleibt der Knopf aus.
        const QString station = AppSettings::instance()
            .value(QStringLiteral("StationGridSquare"), QString{}).toString();
        if (isValidGridSquare(station)) { QSKIP("StationGridSquare gesetzt"); }
        QVERIFY(!turn->isEnabled());
    }
};

QTEST_MAIN(TstLogbookBeamBearing)
#include "tst_logbook_beam_bearing.moc"
