// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_azimutalkarte.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Die Azimutalkarte um das eigene QTH (2026-11-10), vierte Ansicht der
// QSO-Karte nach dem Entwurfsblatt vom 09.11.:
//   - Projektion und Umkehrung sind zueinander invers, Norden oben,
//     Osten rechts, Radius linear im Winkelabstand,
//   - die Strahlenebene ist nicht leer und liegt dort, wo die Punkte sind,
//   - Rotorkeil und nachgeschlagene Station werden gezeichnet,
//   - die Nacht folgt der Uhrzeit,
//   - im Kartenfenster: Knopf „Azimuthal", Radar und Azimutalkarte
//     schliessen einander aus, JEDER Kontakt kommt an (nicht einer je
//     Ort), Rotor und Hinflug kommen an, die Wahl wird gemerkt.
//
// Mit LONGPATH_GRAB_DIR schreibt der letzte Fall ein Bild mit
// synthetischen QSOs dorthin; mit LONGPATH_WELTBILD liegt ein echtes
// Weltbild darunter.

#include <QtTest>

#include "core/AppSettings.h"
#include "gui/QsoMapWindow.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/AzimutalKarteWidget.h"
#include "gui/widgets/GibsTileLayer.h"
#include "gui/widgets/WorldTexture.h"

#include <QDir>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTimeZone>

#include <cmath>
#include <memory>
#include "FensterFrist.h"

using namespace Longpath;

namespace {

// JN67VV, Mitte des Feldes.
constexpr double kHomeLat = 47.8958;
constexpr double kHomeLon = 13.7917;

int deckendePunkte(const QImage& img)
{
    int n = 0;
    for (int y = 0; y < img.height(); ++y) {
        const auto* z = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(z[x]) > 0) { ++n; }
        }
    }
    return n;
}

// Abstand zweier Farben, grob (Summe der Kanaldifferenzen).
int abstand(QRgb a, const QColor& b)
{
    return std::abs(qRed(a) - b.red()) + std::abs(qGreen(a) - b.green())
         + std::abs(qBlue(a) - b.blue());
}

// Der hellste Punkt in einem kleinen Fenster -- Kantenglaettung macht
// einen einzelnen Bildpunkt zur Lotterie.
QRgb hellsterUm(const QImage& img, QPointF at, int r = 2)
{
    QRgb best = 0;
    int bestSum = -1;
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            const QPoint p = (at + QPointF(dx, dy)).toPoint();
            if (!img.rect().contains(p)) { continue; }
            const QRgb c = img.pixel(p);
            const int s = qRed(c) + qGreen(c) + qBlue(c);
            if (s > bestSum) { bestSum = s; best = c; }
        }
    }
    return best;
}

LogEntry qso(const QString& call, const QString& grid, const QString& band,
             const QDateTime& t)
{
    LogEntry e;
    e.call = call;
    e.gridSquare = grid;
    e.band = band;
    e.mode = QStringLiteral("SSB");
    e.timeOn = t;
    return e;
}

} // namespace

class TstAzimutalkarte : public QObject { Q_OBJECT
private slots:
    void projektionUndUmkehrungSindInvers()
    {
        struct Ort { double lat, lon; };
        const Ort orte[] = {
            {kHomeLat, kHomeLon},          // die Mitte selbst
            {40.7, -74.0}, {35.7, 139.7}, {-33.9, 151.2}, {-34.6, -58.4},
            {64.1, -21.9}, {-77.8, 166.7}, {21.3, -157.8}, {-1.3, 36.8},
            {-47.0, -166.0},               // nahe am Gegenpunkt
        };
        for (const Ort& o : orte) {
            const QPointF q = AzimutalKarteWidget::projektion(kHomeLat, kHomeLon, o.lat, o.lon);
            QVERIFY(std::hypot(q.x(), q.y()) <= 1.0 + 1e-12);
            double lat = 0.0, lon = 0.0;
            QVERIFY(AzimutalKarteWidget::umkehrung(kHomeLat, kHomeLon, q.x(), q.y(), lat, lon));
            QVERIFY2(std::abs(lat - o.lat) < 1e-6,
                     qPrintable(QStringLiteral("lat %1 -> %2").arg(o.lat).arg(lat)));
            if (std::abs(o.lat - kHomeLat) > 1e-9 || std::abs(o.lon - kHomeLon) > 1e-9) {
                const double dLon = std::remainder(lon - o.lon, 360.0);
                QVERIFY2(std::abs(dLon) < 1e-6,
                         qPrintable(QStringLiteral("lon %1 -> %2").arg(o.lon).arg(lon)));
            }
        }
        // Ausserhalb der Scheibe gibt es keinen Ort.
        double lat = 0.0, lon = 0.0;
        QVERIFY(!AzimutalKarteWidget::umkehrung(kHomeLat, kHomeLon, 0.8, 0.8, lat, lon));

        // Norden oben, Osten rechts, Radius linear: ein Viertel des
        // Erdumfangs (90 Grad Bogen) liegt auf halbem Radius.
        const QPointF nord = AzimutalKarteWidget::projektion(0.0, 0.0, 30.0, 0.0);
        QVERIFY(std::abs(nord.x()) < 1e-9);
        QVERIFY(std::abs(nord.y() - 30.0 / 180.0) < 1e-9);
        const QPointF ost = AzimutalKarteWidget::projektion(0.0, 0.0, 0.0, 90.0);
        QVERIFY(std::abs(ost.x() - 0.5) < 1e-9);
        QVERIFY(std::abs(ost.y()) < 1e-9);
    }

    void dieStrahlenebeneLiegtWoDiePunkteSind()
    {
        AzimutalKarteWidget w;
        w.resize(600, 600);
        w.setHome(kHomeLat, kHomeLon, QStringLiteral("JN67VV"));
        QVector<AzimutalKarteWidget::Punkt> pkt;
        for (int i = 0; i < 40; ++i) {
            pkt.append({40.7, -74.0, QStringLiteral("20m"), QStringLiteral("W2XYZ"), true, false});
        }
        pkt.append({35.7, 139.7, QStringLiteral("15m"), QStringLiteral("JA1ABC"), true, false});
        w.setPunkte(pkt);
        QCOMPARE(w.punktAnzahl(), 41);   // nichts zusammengefasst

        const QImage s = w.strahlenForTest();
        QVERIFY(!s.isNull());
        QVERIFY2(deckendePunkte(s) > 300,
                 qPrintable(QString::number(deckendePunkte(s))));

        // Am Ort von New York liegt ein Punkt in der Farbe des 20-m-Bandes,
        // halbwegs dorthin der Strahl.
        const QPointF ny = w.bildpunktForTest(40.7, -74.0);
        const QRgb amPunkt = hellsterUm(s, ny);
        QVERIFY(qAlpha(amPunkt) > 150);
        const QColor band20 = AzimutalKarteWidget::bandFarbe(QStringLiteral("20m"));
        const QColor gesehen = QColor::fromRgba(amPunkt).toRgb();
        // Vormultipliziert: auf volle Deckung zurueckrechnen.
        const double a = qAlpha(amPunkt) / 255.0;
        QVERIFY(std::abs(int(gesehen.red() / a) - band20.red()) < 30);
        const QPointF halb = (w.mitteForTest() + ny) / 2.0;
        QVERIFY(qAlpha(hellsterUm(s, halb)) > 0);

        // Ein Ort ohne Punkt und ohne Strahl bleibt leer (Sueden).
        const QPointF sued = w.mitteForTest() + QPointF(0.0, w.radiusForTest() * 0.7);
        QCOMPARE(qAlpha(hellsterUm(s, sued)), 0);
    }

    void rotorKeilUndZielWerdenGezeichnet()
    {
        AzimutalKarteWidget w;
        w.resize(600, 600);
        w.setHome(kHomeLat, kHomeLon, QStringLiteral("JN67VV"));
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        const QPointF c = w.mitteForTest();
        const double R = w.radiusForTest();
        // Osten, 80 % des Radius: dort soll die Nadel liegen.
        const QPointF nadel = c + QPointF(R * 0.8, 0.0);
        // Neben der Nadel, aber im 30-Grad-Keil.
        const double k = 10.0 * M_PI / 180.0;
        const QPointF imKeil = c + QPointF(std::cos(k), std::sin(k)) * (R * 0.5);

        const QImage ohne = w.grab().toImage();
        w.setRotorHeading(90.0);
        w.setRotorBeamWidth(30.0);
        const QImage mit = w.grab().toImage();

        // Die Nadel (1,6 px) liegt genau auf einer Bildpunktgrenze; die
        // Kantenglaettung gibt je Zeile ~80 % Deckung -- also Bernstein,
        // aber nicht ganz.
        const QColor amber(Style::kAmberText);
        QVERIFY2(abstand(hellsterUm(mit, nadel, 1), amber) < 90,
                 qPrintable(QColor(hellsterUm(mit, nadel, 1)).name()));
        QVERIFY(abstand(hellsterUm(ohne, nadel, 1), amber) > 120);
        // Der Keil hellt warm auf: im Keil heller als vorher.
        const QRgb vorher = ohne.pixel(imKeil.toPoint());
        const QRgb nachher = mit.pixel(imKeil.toPoint());
        QVERIFY(qRed(nachher) > qRed(vorher));

        // Eine negative Stellung heisst unbekannt: kein Keil.
        w.setRotorHeading(-1.0);
        QCOMPARE(w.grab().toImage().pixel(nadel.toPoint()), ohne.pixel(nadel.toPoint()));

        // Die Station: der Punkt in warmem Weiss am Ort, der kurze Weg
        // dorthin, der lange Weg gestrichelt in Gegenrichtung.
        w.setZiel(QStringLiteral("VK2ABC"), -33.9, 151.2);
        QVERIFY(w.hatZiel());
        const QImage ziel = w.grab().toImage();
        const QPointF z = w.bildpunktForTest(-33.9, 151.2);
        const QColor hell(Style::kAzZiel);
        QVERIFY2(abstand(ziel.pixel(z.toPoint()), hell) < 40,
                 qPrintable(QColor(ziel.pixel(z.toPoint())).name()));
        const QPointF aufDemWeg = c + (z - c) * 0.5;
        QVERIFY(abstand(hellsterUm(ziel, aufDemWeg, 1), hell) < 60);
        // Langer Weg: entgegengesetzt, ein Stueck vor dem Rand.
        const QPointF dirLp = -(z - c) / std::hypot(z.x() - c.x(), z.y() - c.y());
        // Gestrichelt und halb deckend: also nur „deutlich heller als
        // vorher", an mehreren Stellen entlang der Linie.
        const auto helligkeit = [](QRgb p) { return qRed(p) + qGreen(p) + qBlue(p); };
        int lpStellen = 0;
        for (double f = 0.55; f < 0.95; f += 0.01) {
            const QPointF at = c + dirLp * (R * f);
            if (helligkeit(hellsterUm(ziel, at, 1)) - helligkeit(hellsterUm(ohne, at, 1)) > 150) {
                ++lpStellen;
            }
        }
        const bool lpGesehen = lpStellen >= 10;
        QVERIFY(lpGesehen);

        w.clearZiel();
        QVERIFY(!w.hatZiel());
        QVERIFY(abstand(w.grab().toImage().pixel(z.toPoint()), hell) > 100);
    }

    void dieNachtFolgtDerUhrzeit()
    {
        AzimutalKarteWidget w;
        w.resize(500, 500);
        w.setHome(kHomeLat, kHomeLon);
        // Mittag UTC am 21.06.: JN67 liegt in der Sonne, der Gegenpunkt
        // in der Nacht.
        w.setZeitForTest(QDateTime(QDate(2026, 6, 21), QTime(12, 0), QTimeZone::UTC));
        QImage tag = w.nachtForTest();
        QVERIFY(!tag.isNull());
        const QPoint mitte(tag.width() / 2, tag.height() / 2);
        QCOMPARE(qAlpha(tag.pixel(mitte)), 0);
        // Mitternacht UTC: jetzt ist es hier Nacht.
        w.setZeitForTest(QDateTime(QDate(2026, 6, 21), QTime(0, 0), QTimeZone::UTC));
        const QImage nacht = w.nachtForTest();
        QVERIFY(qAlpha(nacht.pixel(mitte)) > 100);
        QVERIFY(tag != nacht);
    }

    void imKartenfensterAlsVierteAnsicht()
    {
        AppSettings::instance().setValue(QStringLiteral("QsoMapView"), QString{});
        auto w = std::make_unique<QsoMapWindow>();
        w->resize(1100, 700);
        w->imagery()->setNetworkEnabled(false);
        w->setHomeGrid(QStringLiteral("JN67VV"));
        const QDateTime t = QDateTime::currentDateTimeUtc();
        QVector<LogEntry> log;
        // Dreimal derselbe Ort: die Kugel zeigt einen Punkt, die
        // Azimutalkarte drei Strahlen.
        log << qso(QStringLiteral("W2XYZ"), QStringLiteral("FN30"), QStringLiteral("20m"), t)
            << qso(QStringLiteral("W2XYZ"), QStringLiteral("FN30"), QStringLiteral("20m"), t.addSecs(-60))
            << qso(QStringLiteral("K2AB"), QStringLiteral("FN30"), QStringLiteral("15m"), t.addSecs(-120))
            << qso(QStringLiteral("JA1ABC"), QStringLiteral("PM95"), QStringLiteral("15m"), t.addSecs(-180));
        w->setEntries(log);
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.get(), kFensterFrist));

        auto* stack = w->findChild<QStackedWidget*>();
        QVERIFY(stack);
        QCOMPARE(stack->currentIndex(), 0);
        QPushButton* az = w->azimutalButtonForTest();
        QPushButton* radar = w->radarButtonForTest();
        QVERIFY(az && radar);
        QCOMPARE(az->text(), QStringLiteral("Azimuthal"));

        az->setChecked(true);
        QCOMPARE(stack->currentIndex(), 3);
        QCOMPARE(stack->currentWidget(), w->azimutalForTest());
        QCOMPARE(w->azimutalForTest()->punktAnzahl(), 4);
        QVERIFY(w->azimutalForTest()->hasHome());

        // Radar an: Azimutalkarte aus, und umgekehrt.
        radar->setChecked(true);
        QCOMPARE(stack->currentIndex(), 2);
        QVERIFY(!az->isChecked());
        az->setChecked(true);
        QCOMPARE(stack->currentIndex(), 3);
        QVERIFY(!radar->isChecked());
        // Aus: zurueck dorthin, wo man herkam (die Kugel).
        az->setChecked(false);
        QCOMPARE(stack->currentIndex(), 0);

        // Rotor kommt an.
        w->setRotorHeading(123.0);
        w->setRotorBeamWidth(40.0);
        QCOMPARE(w->azimutalForTest()->rotorHeading(), 123.0);

        // Der Hinflug: in der Azimutalkarte bleibt man dort und sieht die
        // Station; sonst geht er wie bisher auf die flache Karte.
        az->setChecked(true);
        w->flyToStation(QStringLiteral("vk2abc"), -33.9, 151.2);
        QCOMPARE(stack->currentIndex(), 3);
        QVERIFY(w->azimutalForTest()->hatZiel());
        QCOMPARE(w->azimutalForTest()->zielCall(), QStringLiteral("VK2ABC"));
        az->setChecked(false);
        w->flyToStation(QStringLiteral("W2XYZ"), 40.7, -74.0);
        QCOMPARE(stack->currentIndex(), 1);

        // Die Wahl wird gemerkt: ein neues Fenster oeffnet dort.
        az->setChecked(true);
        w.reset();
        QsoMapWindow zweites;
        zweites.imagery()->setNetworkEnabled(false);
        auto* stack2 = zweites.findChild<QStackedWidget*>();
        QCOMPARE(stack2->currentIndex(), 3);
        QVERIFY(zweites.azimutalButtonForTest()->isChecked());
        AppSettings::instance().setValue(QStringLiteral("QsoMapView"), QString{});
    }

    void einBildMitSynthetischenQsos()
    {
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (grabDir.isEmpty()) { QSKIP("LONGPATH_GRAB_DIR nicht gesetzt"); }
        const QString welt = qEnvironmentVariable("LONGPATH_WELTBILD");
        if (!welt.isEmpty()) { QVERIFY(WorldTexture::setPath(welt)); }

        // Eine Verteilung wie ein echtes Kurzwellenlog aus OE: dicht in
        // Europa, Ostkueste Nordamerika, Suedamerika, Japan, Australien.
        struct Haufen { double lat, lon, sLat, sLon; int n; const char* band; };
        const Haufen haufen[] = {
            {50.0, 10.0, 6.0, 10.0, 1800, "20m"}, {48.0, 14.0, 4.0, 6.0, 900, "40m"},
            {52.0, 5.0, 5.0, 8.0, 700, "15m"},   {45.0, 20.0, 5.0, 8.0, 400, "10m"},
            {41.0, -76.0, 4.0, 6.0, 900, "20m"}, {43.0, -82.0, 5.0, 8.0, 500, "15m"},
            {35.0, -100.0, 6.0, 15.0, 250, "20m"}, {-23.0, -46.0, 6.0, 6.0, 300, "15m"},
            {-34.0, -60.0, 4.0, 5.0, 150, "10m"},  {36.0, 138.0, 3.0, 4.0, 350, "15m"},
            {-34.0, 150.0, 4.0, 5.0, 120, "20m"},  {55.0, 37.0, 4.0, 6.0, 300, "17m"},
            {60.0, 90.0, 6.0, 25.0, 120, "20m"},   {30.0, 31.0, 4.0, 4.0, 90, "6m"},
            {18.0, -66.0, 2.0, 3.0, 80, "12m"},    {-26.0, 28.0, 3.0, 3.0, 60, "17m"},
            {64.0, -20.0, 1.0, 3.0, 40, "80m"},    {48.0, 15.0, 2.0, 2.0, 60, "2m"},
            {51.0, 0.0, 3.0, 3.0, 40, "160m"},
        };
        QRandomGenerator zufall(20261110);
        auto normal = [&zufall]() {
            const double u1 = std::max(1e-12, zufall.generateDouble());
            const double u2 = zufall.generateDouble();
            return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
        };
        QVector<AzimutalKarteWidget::Punkt> pkt;
        int nr = 0;
        for (const Haufen& h : haufen) {
            for (int i = 0; i < h.n; ++i) {
                const double lat = std::clamp(h.lat + normal() * h.sLat, -85.0, 85.0);
                const double lon = std::remainder(h.lon + normal() * h.sLon, 360.0);
                pkt.append({lat, lon, QString::fromLatin1(h.band),
                            QStringLiteral("SY%1").arg(++nr), (nr % 3) != 0, false});
            }
        }
        // Eine markierte Station, wie aus der Tabelle gewaehlt.
        pkt.append({-33.9, 18.4, QStringLiteral("15m"), QStringLiteral("ZS1AB"), true, true});

        AzimutalKarteWidget w;
        w.resize(1100, 1100);
        w.setHome(kHomeLat, kHomeLon, QStringLiteral("JN67VV"));
        w.setZeitForTest(QDateTime(QDate(2026, 11, 10), QTime(17, 0), QTimeZone::UTC));
        w.setPunkte(pkt);
        w.setRotorHeading(300.0);
        w.setRotorBeamWidth(30.0);
        w.setZiel(QStringLiteral("VK2ABC"), -33.9, 151.2);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        QDir().mkpath(grabDir);
        QVERIFY(w.grab().save(grabDir + QStringLiteral("/azimutalkarte.png")));

        // Und im Kartenfenster, wie der Betreiber es oeffnet.
        AppSettings::instance().setValue(QStringLiteral("QsoMapView"), QStringLiteral("azimuthal"));
        QsoMapWindow fenster;
        fenster.resize(1200, 860);
        fenster.imagery()->setNetworkEnabled(false);
        fenster.setHomeGrid(QStringLiteral("JN67VV"));
        QVector<LogEntry> log;
        const QDateTime t = QDateTime::currentDateTimeUtc();
        for (int i = 0; i < pkt.size(); i += 3) {
            const auto& p = pkt.at(i);
            LogEntry e;
            e.call = p.call;
            // Locator aus der Position, damit die Karte sie platziert.
            const int lonF = int((p.lon + 180.0) / 20.0), latF = int((p.lat + 90.0) / 10.0);
            const int lonQ = int(std::fmod(p.lon + 180.0, 20.0) / 2.0);
            const int latQ = int(std::fmod(p.lat + 90.0, 10.0));
            e.gridSquare = QStringLiteral("%1%2%3%4").arg(QChar('A' + lonF)).arg(QChar('A' + latF))
                               .arg(lonQ).arg(latQ);
            e.band = p.band;
            e.mode = QStringLiteral("SSB");
            e.timeOn = t.addSecs(-i);
            log << e;
        }
        fenster.setEntries(log);
        fenster.setRotorHeading(300.0);
        fenster.setRotorBeamWidth(30.0);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));
        fenster.flyToStation(QStringLiteral("VK2ABC"), -33.9, 151.2);
        QTest::qWait(200);
        QVERIFY(fenster.grab().save(grabDir + QStringLiteral("/azimutalkarte-fenster.png")));
        AppSettings::instance().setValue(QStringLiteral("QsoMapView"), QString{});
    }
};

QTEST_MAIN(TstAzimutalkarte)
#include "tst_azimutalkarte.moc"
