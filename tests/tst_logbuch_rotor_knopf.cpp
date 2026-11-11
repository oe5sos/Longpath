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
//   5. (2026-11-11) Ein 6-stelliger Log-Locator schlaegt eine QRZ-Adresse
//      ausserhalb seines Feldes -- Peilung und Karte gleich (SN7L).
//   6. (2026-11-11) Leeres/anderes Rufzeichen nimmt die Rotorzeile weg,
//      auch wenn der Wert nicht getippt wurde.
//   7. (2026-11-11) Nach Rotate/LP zeigt die Zeile, ob er wirklich dreht:
//      kein Rotor, dreht (mit Stand), am Ziel, steht, abgelehnt -- und
//      ein Durchlauf mit echtem Rotorfenster und Ersatz-rotctld.

#include <QtTest/QtTest>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include "FensterFrist.h"
#include "core/AdifLog.h"
#include "core/AppSettings.h"
#include "core/RotorController.h"
#include "core/CallsignCache.h"
#include "core/Maidenhead.h"
#include "core/RotorBereich.h"
#include "gui/LogbookWindow.h"
#include "gui/QsoMapWindow.h"
#include "gui/widgets/QsoDetailPane.h"
#include "gui/widgets/RotorLogbookPanel.h"
#include "gui/StyleConstants.h"
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
// Eine frische QRZ-Antwort mit Koordinaten (Adresse).
CallsignInfo qrz(const QString& call, double lat, double lon)
{
    CallsignInfo i;
    i.call = call;
    i.latitude = lat;
    i.longitude = lon;
    i.hasLatLon = true;
    i.geoloc = QStringLiteral("user");
    i.fetchedUtc = QDateTime::currentSecsSinceEpoch();
    return i;
}
// Landesmitte wie cty.dat sie liefert (hier fest: Japan).
bool landesmitte(const QString& call, double& lat, double& lon)
{
    if (call.startsWith(QStringLiteral("JA"))) { lat = 36.0; lon = 138.0; return true; }
    // Kanaren: ~240 Grad von JN67VV -- im Westen, wie Martins Rotor derzeit darf.
    if (call.startsWith(QStringLiteral("EA8"))) { lat = 28.32; lon = -15.85; return true; }
    return false;
}
} // namespace

class TstLogbuchRotorKnopf : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // Das Rotorfenster liest LogbookDatei::pfad(); eine gesetzte
        // Sandbox-Variable lenkte das auf einen echten Ordner -- hier nie.
        qunsetenv("LONGPATH_CONFIG_DIR");
    }
    void init()
    {
        RotorBereich{}.save();
        // Jeder Fall beginnt ohne eingestellten eigenen Locator -- sonst
        // trüge ein frueherer Lauf (Sandbox-Datei) die Heimatfeld-Probe.
        AppSettings::instance().setValue(QStringLiteral("StationGridSquare"), QString());
    }

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

    // Leeres oder anderes Rufzeichen nach ROTOR: die Zeile geht weg, kein
    // „Rotate" zu einem alten Ziel bleibt stehen -- auch nicht, wenn der
    // Wert per Bedienungshilfe oder aus dem Programm kommt (textChanged,
    // nicht textEdited). Befund aus dem Probebau 2026-11-11 (W1AW).
    void einAnderesRufzeichenNimmtDieZeileWeg()
    {
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("logbook.adi"));
        QVERIFY(AdifLog::write(pfad, {qso(QStringLiteral("DL1ABC"), QStringLiteral("JO62"))}));
        LogbookWindow w(pfad);
        w.setPositionFallback(&landesmitte);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        QSignalSpy drehen(&w, &LogbookWindow::turnRotorRequested);
        const auto zeileSichtbar = [&w]() {
            return w.entryPeilungForTest()->isVisible() || w.entryRotateForTest()->isVisible()
                || w.entryLongPathForTest()->isVisible();
        };

        // Getippt: ein Zeichen mehr ist ein anderes Rufzeichen.
        QTest::keyClicks(w.entryCallForTest(), QStringLiteral("JA1XY"));
        w.entryRotorForTest()->click();
        QVERIFY(zeileSichtbar());
        QTest::keyClicks(w.entryCallForTest(), QStringLiteral("Z"));
        QVERIFY2(!zeileSichtbar(), qPrintable(w.entryPeilungForTest()->text()));

        // ROTOR fuer das neue Rufzeichen zeigt es wieder.
        w.entryRotorForTest()->click();
        QVERIFY(zeileSichtbar());
        QCOMPARE(w.detailForTest()->gezeigtesRufzeichen(), QStringLiteral("JA1XYZ"));
        QVERIFY(w.entryPeilungForTest()->text().contains(QStringLiteral("country centre")));
        QVERIFY(w.entryRotateForTest()->isEnabled());

        // Von aussen geleert (setText, kein Tippen).
        w.entryCallForTest()->setText(QString());
        QVERIFY(!zeileSichtbar());
        w.entryRotateForTest()->click();
        w.entryLongPathForTest()->click();
        QCOMPARE(drehen.count(), 0);

        // Von aussen ein anderes Rufzeichen gesetzt.
        w.entryCallForTest()->setText(QStringLiteral("JA1XYZ"));
        w.entryRotorForTest()->click();
        QVERIFY(zeileSichtbar());
        w.entryCallForTest()->setText(QStringLiteral("W1AW"));
        QVERIFY(!zeileSichtbar());
        w.entryRotateForTest()->click();
        QCOMPARE(drehen.count(), 0);
    }

    // ── Lage-Vorrang (2026-11-11) ──────────────────────────────────────
    // SN7L (2 m, 2026-07-04): JO70SS im Log -- 21°, 344 km. Die QRZ-Adresse
    // ist der Klub in Lodz (42°, 592 km). Ein genauer Log-Locator, dem der
    // QRZ-Punkt widerspricht, gewinnt -- bei Peilung UND Karte.
    void einGenauerLogLocatorSchlaegtDieQrzAdresse()
    {
        QTemporaryDir dir;
        CallsignCache cache(dir.filePath(QStringLiteral("cache.json")));
        cache.put(QStringLiteral("SN7L"), qrz(QStringLiteral("SN7L"), 51.77, 19.46));

        QsoDetailPane pane;
        pane.setCache(&cache);
        QSignalSpy karte(&pane, &QsoDetailPane::stationLocated);
        pane.setEntry(qso(QStringLiteral("SN7L"), QStringLiteral("JO70SS")));

        QCOMPARE(pane.lage(), QsoDetailPane::Lage::LogLocator);
        double sp = 0.0, km = 0.0;
        QVERIFY(pane.peilung(sp, km));
        QVERIFY2(sp > 19.0 && sp < 24.0, qPrintable(QString::number(sp)));
        QVERIFY2(km > 330.0 && km < 360.0, qPrintable(QString::number(km)));
        QVERIFY(pane.lageText().contains(QStringLiteral("JO70SS")));
        // Die Karte fliegt dorthin, wohin die Peilung zeigt -- nicht nach Lodz.
        QVERIFY(!karte.isEmpty());
        const double lat = karte.last().at(1).toDouble();
        const double lon = karte.last().at(2).toDouble();
        QCOMPARE(gridSquareFromLatLon(lat, lon).left(6).toUpper(), QStringLiteral("JO70SS"));
    }

    // Liegt der QRZ-Punkt IM Feld des Log-Locators, ist er genauer und bleibt.
    void einQrzPunktImFeldBleibt()
    {
        QTemporaryDir dir;
        CallsignCache cache(dir.filePath(QStringLiteral("cache.json")));
        double feldLat = 0.0, feldLon = 0.0;
        calculateLatLonFromGridSquare(QStringLiteral("JO70SS"), feldLat, feldLon);
        const double qLat = feldLat + 0.01, qLon = feldLon - 0.02;   // im Feld, nicht die Mitte
        QCOMPARE(gridSquareFromLatLon(qLat, qLon).left(6).toUpper(), QStringLiteral("JO70SS"));
        cache.put(QStringLiteral("SN7L"), qrz(QStringLiteral("SN7L"), qLat, qLon));

        QsoDetailPane pane;
        pane.setCache(&cache);
        QSignalSpy karte(&pane, &QsoDetailPane::stationLocated);
        pane.setEntry(qso(QStringLiteral("SN7L"), QStringLiteral("JO70SS")));
        QCOMPARE(pane.lage(), QsoDetailPane::Lage::QrzKoordinaten);
        QVERIFY(!karte.isEmpty());
        QVERIFY(std::abs(karte.last().at(1).toDouble() - qLat) < 1e-9);
        QVERIFY(std::abs(karte.last().at(2).toDouble() - qLon) < 1e-9);
    }

    // OE5VVM (2026-09-26): nur JN67 im Log, 75 km breit -- QRZ gewinnt weiter.
    void einVierstelligerLogLocatorSchlaegtNichts()
    {
        QTemporaryDir dir;
        CallsignCache cache(dir.filePath(QStringLiteral("cache.json")));
        cache.put(QStringLiteral("OE5VVM"), qrz(QStringLiteral("OE5VVM"), 47.98, 13.82));

        QsoDetailPane pane;
        pane.setCache(&cache);
        QSignalSpy karte(&pane, &QsoDetailPane::stationLocated);
        pane.setEntry(qso(QStringLiteral("OE5VVM"), QStringLiteral("JN67")));
        QCOMPARE(pane.lage(), QsoDetailPane::Lage::QrzKoordinaten);
        double sp = 0.0, km = 0.0;
        QVERIFY(pane.peilung(sp, km));
        QVERIFY2(km < 20.0, qPrintable(QString::number(km)));   // Laakirchen, nicht JN67-Mitte
        QVERIFY(!karte.isEmpty());
        QVERIFY(std::abs(karte.last().at(1).toDouble() - 47.98) < 1e-9);
    }

    // ── Dreht er wirklich? (2026-11-11) ────────────────────────────────
    // Betreiber: „im logbuch sollte auch ersichtlich sein, dass sobald ich
    // ROTATE klicke, der rotator auch wirklich dreht." Gezeigt wird nur,
    // was der Rotor meldet: kein Rotor, dreht, am Ziel, steht, abgelehnt.
    void rotateZeigtObErWirklichDreht()
    {
        QTemporaryDir dir;
        const QString pfad = dir.filePath(QStringLiteral("logbook.adi"));
        QVERIFY(AdifLog::write(pfad, {qso(QStringLiteral("DL1ABC"), QStringLiteral("JO62"))}));
        LogbookWindow w(pfad);
        w.setPositionFallback(&landesmitte);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        QLabel* dreh = w.entryDrehForTest();
        QTest::keyClicks(w.entryCallForTest(), QStringLiteral("JA1XYZ"));
        w.entryRotorForTest()->click();
        QVERIFY(dreh->isVisible());
        QVERIFY2(dreh->text().contains(QStringLiteral("rotor not connected")), qPrintable(dreh->text()));

        // Ohne Rotor: Rotate sagt, dass sich nichts dreht.
        w.entryRotateForTest()->click();
        QVERIFY2(dreh->text().contains(QStringLiteral("does not turn")), qPrintable(dreh->text()));

        // Der Rotor meldet sich: er faehrt, die Zeile zaehlt mit.
        w.setRotorBearing(0.0);
        QVERIFY2(dreh->text().contains(QStringLiteral("turning to 42°")), qPrintable(dreh->text()));
        QVERIFY(dreh->text().contains(QStringLiteral("now 0°")));
        QVERIFY(dreh->styleSheet().contains(QLatin1String(Style::kAmberText)));
        w.setRotorBearing(20.0);
        QVERIFY2(dreh->text().contains(QStringLiteral("now 20°")), qPrintable(dreh->text()));
        // Am Ziel (innerhalb von 4 Grad): gruen.
        w.setRotorBearing(40.0);
        QVERIFY2(dreh->text().contains(QStringLiteral("on target")), qPrintable(dreh->text()));
        QVERIFY(dreh->styleSheet().contains(QLatin1String(Style::kGreenText)));

        // Keine neue Position mehr: er steht -- und das wird gesagt.
        w.setStillstandMsForTest(300);
        w.entryLongPathForTest()->click();
        QVERIFY2(dreh->text().contains(QStringLiteral("turning to 222°")), qPrintable(dreh->text()));
        QTRY_VERIFY_WITH_TIMEOUT(dreh->text().contains(QStringLiteral("not moving")), 4000);
        QVERIFY(dreh->styleSheet().contains(QLatin1String(Style::kAmberWarn)));

        // Der Rotor lehnt ab (wie RotorController::pruefeZiel, synchron
        // im Drehbefehl): die Ablehnung steht in der Zeile.
        auto ablehnen = connect(&w, &LogbookWindow::turnRotorRequested, &w, [&w]() {
            w.setRotorMeldung(QStringLiteral("Not turned — outside the allowed range"));
        });
        w.entryRotateForTest()->click();
        QVERIFY2(dreh->text().contains(QStringLiteral("rotor: Not turned")), qPrintable(dreh->text()));
        disconnect(ablehnen);

        // Eine Meldung ohne Drehbefehl aus der Zeile gehoert ins Rotorfenster.
        w.entryCallForTest()->setText(QString());
        w.setRotorMeldung(QStringLiteral("irgendwas"));
        w.entryCallForTest()->setText(QStringLiteral("JA1XYZ"));
        w.entryRotorForTest()->click();
        QVERIFY2(!dreh->text().contains(QStringLiteral("irgendwas")), qPrintable(dreh->text()));
        QVERIFY(dreh->text().contains(QStringLiteral("rotor at 40°")));
    }

    // Durchlauf mit dem echten Rotorfenster und einem Ersatz-rotctld:
    // Rotate in der Eingabezeile -> "P 240" beim Rotor -> er faehrt in
    // Schritten -> die Zeile zeigt "turning", dann "on target".
    void rotateDrehtDenRotorWirklich()
    {
        double az = 231.0, ziel = 231.0;
        QStringList befehle;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        connect(&server, &QTcpServer::newConnection, this, [&]() {
            QTcpSocket* c = server.nextPendingConnection();
            connect(c, &QTcpSocket::readyRead, c, [c, &az, &ziel, &befehle]() {
                for (const QByteArray& zeile : c->readAll().split('\n')) {
                    const QByteArray z = zeile.trimmed();
                    if (z.isEmpty()) { continue; }
                    if (z == "p") {
                        // Drei Grad je Abfrage Richtung Ziel -- wie ein Rotor faehrt.
                        if (std::abs(ziel - az) <= 3.0) { az = ziel; }
                        else { az += ziel > az ? 3.0 : -3.0; }
                        c->write(QByteArray::number(az, 'f', 6) + "\n0.000000\n");
                    } else {
                        if (z.startsWith("P ")) {
                            befehle << QString::fromLatin1(z);
                            ziel = z.split(' ').value(1).toDouble();
                        }
                        c->write("RPRT 0\n");
                    }
                }
                c->flush();
            });
        });
        AppSettings& s = AppSettings::instance();
        s.setValue(QStringLiteral("StationGridSquare"), QStringLiteral("JN67VV"));
        s.setValue(QStringLiteral("RotorUseLocalSerial"), false);
        s.setValue(QStringLiteral("RotorRotctldHost"), QStringLiteral("127.0.0.1"));
        s.setValue(QStringLiteral("RotorRotctldPort"), int(server.serverPort()));

        RotorLogbookPanel panel(nullptr, nullptr, nullptr);
        QString err;
        QVERIFY2(panel.connectRotorFromSettings(&err), qPrintable(err));
        QTRY_VERIFY_WITH_TIMEOUT(panel.rotorForTest() && panel.rotorForTest()->isConnected(), 8000);
        panel.showLogbook();
        LogbookWindow* w = panel.findChild<LogbookWindow*>();
        QVERIFY(w);
        QVERIFY(QTest::qWaitForWindowExposed(w, kFensterFrist));
        w->setPositionFallback(&landesmitte);

        QTest::keyClicks(w->entryCallForTest(), QStringLiteral("EA8ABC"));
        w->entryRotorForTest()->click();
        QLabel* dreh = w->entryDrehForTest();
        QTRY_VERIFY_WITH_TIMEOUT(dreh->text().contains(QStringLiteral("rotor at 231°")), 8000);
        double sp = 0.0, km = 0.0;
        QVERIFY(w->detailForTest()->peilung(sp, km));
        QVERIFY2(sp > 235.0 && sp < 245.0, qPrintable(QString::number(sp)));

        w->entryRotateForTest()->click();
        QTRY_VERIFY_WITH_TIMEOUT(!befehle.isEmpty(), 4000);
        QVERIFY2(std::abs(befehle.first().split(' ').value(1).toDouble() - sp) < 0.6,
                 qPrintable(befehle.first()));
        QVERIFY2(dreh->text().contains(QStringLiteral("turning to")), qPrintable(dreh->text()));
        QTRY_VERIFY_WITH_TIMEOUT(dreh->text().contains(QStringLiteral("on target")), 15000);

        // Was der Rotor zu einem Drehbefehl meldet, reicht das Rotorfenster
        // an die Zeile weiter (RotorController::errorOccurred).
        w->entryRotateForTest()->click();
        emit panel.rotorForTest()->errorOccurred(QStringLiteral("Rotator stopped answering"));
        QVERIFY2(dreh->text().contains(QStringLiteral("rotor: Rotator stopped answering")),
                 qPrintable(dreh->text()));
        w->hide();
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
        QVERIFY(w.entryDrehForTest()->text().contains(QStringLiteral("rotor at 0")));
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
