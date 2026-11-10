// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_qso_map_3d_flug.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der 3D-Flug im Kartenfenster (2026-11-10), ohne WebEngine gefahren:
//
//   * ohne WebEngine-Flaeche (dieser Pruefstand linkt sie nicht) oder
//     ohne Schluessel fliegt die flache Karte wie bisher, und „View in
//     Google Earth" erscheint und oeffnet die Station im Browser,
//   * mit einer Attrappe als Flaeche und einem Schluessel geht der Flug
//     an die 3D-Seite -- mit Heimat, Entfernung und Peilung aus derselben
//     Rechnung wie die Stationskarte,
//   * lehnt Google den Schluessel ab, faellt die Karte zurueck, nennt den
//     Grund, und fragt fuer diesen Schluessel nicht bei jedem Rufzeichen
//     wieder; ein neuer Schluessel gibt 3D wieder frei,
//   * die Seite selbst: festgenagelte CesiumJS-Fassung mit SRI, kein ion.

#include <QtTest>

#include "core/AppSettings.h"
#include "core/BeamHeading.h"
#include "core/GoogleMapTiles.h"
#include "core/Maidenhead.h"
#include "gui/Google3dSetupDialog.h"
#include "gui/QsoMapWindow.h"
#include "gui/widgets/FlatMapWidget.h"
#include "gui/widgets/GibsTileLayer.h"
#include "gui/widgets/Station3dSurface.h"

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QUrlQuery>
#include "FensterFrist.h"

using namespace Longpath;

namespace {

const QString kKey  = QStringLiteral("AIzaPruefstandNichtEchtXXXXXXXXXXXXXXXX");
const QString kKey2 = QStringLiteral("AIzaPruefstandZweiterXXXXXXXXXXXXXXXXXX");

class FakeSurface : public Station3dSurface {
public:
    using Station3dSurface::Station3dSurface;
    QVector<Station3dFlight> flights;
    void fly(const Station3dFlight& f) override { flights << f; }
};

FakeSurface* g_lastFake = nullptr;

void installFake()
{
    Station3dSurface::setFactory([](QWidget* parent) -> Station3dSurface* {
        g_lastFake = new FakeSurface(parent);
        return g_lastFake;
    });
}

// Linz und W1AW in Newington, wie im Logbuch.
constexpr double kStaLat = 41.7147, kStaLon = -72.7272;

void prepare(QsoMapWindow& w, QVector<QUrl>* opened)
{
    w.resize(1100, 620);
    w.setHomeGrid(QStringLiteral("JN78GH"));
    w.imagery()->setNetworkEnabled(false);
    w.setUrlOpenerForTest([opened](const QUrl& u) { if (opened) { *opened << u; } });
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
}

} // namespace

class TstQsoMap3dFlug : public QObject { Q_OBJECT
private slots:
    void init()
    {
        Station3dSurface::setFactory({});
        g_lastFake = nullptr;
        GoogleMapsKey::erase();
        GoogleMapsKey::forgetCachedForTest();
        AppSettings::instance().setValue(QStringLiteral("QsoMap3dFlight"), true);
    }
    void cleanup()
    {
        for (QWidget* t : QApplication::topLevelWidgets()) {
            if (qobject_cast<Google3dSetupDialog*>(t)) { t->close(); }
        }
        Station3dSurface::setFactory({});
    }

    // Kein WebEngine im Bau -- der Fall auf Linux und Windows.
    void without_webengine_the_flat_map_flies_and_offers_google_earth()
    {
        GoogleMapsKey::save(kKey);   // selbst MIT Schluessel
        QVector<QUrl> opened;
        QsoMapWindow w;
        prepare(w, &opened);

        QPushButton* b3d = w.threeDButtonForTest();
        QVERIFY(b3d);
        QVERIFY(!b3d->isEnabled());
        QVERIFY(b3d->toolTip().contains(QLatin1String("WebEngine")));

        w.flyToStation(QStringLiteral("w1aw"), kStaLat, kStaLon, QStringLiteral("Newington"));
        QCOMPARE(w.currentViewForTest(), 1);
        QCOMPARE(w.flatMapForTest()->focusStation(), QStringLiteral("W1AW"));
        QVERIFY(w.threeDSurfaceForTest() == nullptr);
        QPushButton* earth = w.googleEarthButtonForTest();
        QVERIFY(earth && earth->isVisible());
        QVERIFY(w.infoTextForTest().contains(QLatin1String(" km")));

        earth->click();
        QCOMPARE(opened.size(), 1);
        const QString url = opened.first().toString();
        QVERIFY2(url.startsWith(QLatin1String("https://earth.google.com/web/@41.714700,-72.727200,")),
                 qPrintable(url));
        // Blick in Flugrichtung vom eigenen QTH: dieselbe Peilung wie die Karte.
        double hlat = 0.0, hlon = 0.0;
        calculateLatLonFromGridSquare(QStringLiteral("JN78GH"), hlat, hlon);
        const double brg = BeamHeading::greatCircle(hlat, hlon, kStaLat, kStaLon).bearingDeg;
        QVERIFY2(url.contains(QStringLiteral(",%1h,").arg(brg, 0, 'f', 0)), qPrintable(url));
    }

    // WebEngine da, aber kein Schluessel: flach wie bisher; der Knopf
    // fuehrt zur Einrichtung statt in ein leeres 3D.
    void without_key_the_3d_button_opens_the_setup()
    {
        installFake();
        QsoMapWindow w;
        prepare(w, nullptr);
        QPushButton* b3d = w.threeDButtonForTest();
        QVERIFY(b3d->isEnabled());
        QVERIFY(!b3d->isChecked());
        QVERIFY(b3d->toolTip().contains(QLatin1String("own Google key")));

        w.flyToStation(QStringLiteral("W1AW"), kStaLat, kStaLon);
        QCOMPARE(w.currentViewForTest(), 1);
        QVERIFY(w.threeDSurfaceForTest() == nullptr);   // nichts gestartet
        QVERIFY(w.googleEarthButtonForTest()->isVisible());

        b3d->click();
        QVERIFY(!b3d->isChecked());
        Google3dSetupDialog* dlg = nullptr;
        for (QWidget* t : QApplication::topLevelWidgets()) {
            if (auto* d = qobject_cast<Google3dSetupDialog*>(t)) { dlg = d; }
        }
        QVERIFY(dlg && dlg->isVisible());

        // Schluessel eintragen und speichern: der Knopf rastet ein.
        dlg->keyEditForTest()->setText(kKey);
        dlg->saveButtonForTest()->click();
        QVERIFY(b3d->isChecked());
        QVERIFY(b3d->toolTip().contains(QLatin1String("Untick")));
    }

    void with_key_the_flight_goes_to_3d()
    {
        installFake();
        GoogleMapsKey::save(kKey);
        QsoMapWindow w;
        prepare(w, nullptr);
        QVERIFY(w.threeDButtonForTest()->isChecked());

        w.flyToStation(QStringLiteral("w1aw"), kStaLat, kStaLon,
                       QStringLiteral("Hiram Percy Maxim &amp; friends · Newington"));
        QCOMPARE(w.currentViewForTest(), int(QsoMapWindow::kView3d));
        QVERIFY(g_lastFake);
        QCOMPARE(w.threeDSurfaceForTest(), g_lastFake);
        QCOMPARE(g_lastFake->flights.size(), 1);
        const Station3dFlight f = g_lastFake->flights.first();
        QCOMPARE(f.call, QStringLiteral("W1AW"));
        QCOMPARE(f.apiKey, kKey);
        QCOMPARE(f.lat, kStaLat);
        QCOMPARE(f.lon, kStaLon);
        // Klartext fuer die Anzeige in der Seite, nicht HTML.
        QCOMPARE(f.caption, QStringLiteral("Hiram Percy Maxim & friends · Newington"));
        QVERIFY(f.hasHome);
        double hlat = 0.0, hlon = 0.0;
        calculateLatLonFromGridSquare(QStringLiteral("JN78GH"), hlat, hlon);
        QCOMPARE(f.homeLat, hlat);
        QCOMPARE(f.homeLon, hlon);
        const BeamHeading::GreatCircle g = BeamHeading::greatCircle(hlat, hlon, kStaLat, kStaLon);
        QCOMPARE(f.km, g.km);
        QCOMPARE(f.bearingDeg, g.bearingDeg);

        QVERIFY(!w.googleEarthButtonForTest()->isVisible());
        QVERIFY(w.infoTextForTest().contains(QLatin1String("Google 3D")));
        // Die flache Karte kennt die Station trotzdem.
        QCOMPARE(w.flatMapForTest()->focusStation(), QStringLiteral("W1AW"));

        // Ein zweites Rufzeichen nimmt dieselbe Flaeche, keine neue.
        FakeSurface* first = g_lastFake;
        w.flyToStation(QStringLiteral("OE5SOS"), 47.9, 13.8);
        QCOMPARE(g_lastFake, first);
        QCOMPARE(first->flights.size(), 2);

        // „3D" ab: zurueck auf die flache Karte, und es bleibt so.
        w.threeDButtonForTest()->click();
        QCOMPARE(w.currentViewForTest(), 1);
        QVERIFY(w.googleEarthButtonForTest()->isVisible());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("QsoMap3dFlight")).toBool(), false);
        w.flyToStation(QStringLiteral("W1AW"), kStaLat, kStaLon);
        QCOMPARE(w.currentViewForTest(), 1);
        QCOMPARE(first->flights.size(), 2);
    }

    void refused_key_falls_back_until_a_new_key()
    {
        installFake();
        GoogleMapsKey::save(kKey);
        QVector<QUrl> opened;
        QsoMapWindow w;
        prepare(w, &opened);

        w.flyToStation(QStringLiteral("W1AW"), kStaLat, kStaLon);
        QVERIFY(g_lastFake);
        QCOMPARE(w.currentViewForTest(), int(QsoMapWindow::kView3d));

        emit g_lastFake->failed(QStringLiteral("The project has no billing account."), true);
        QCOMPARE(w.currentViewForTest(), 1);
        QCOMPARE(w.flatMapForTest()->focusStation(), QStringLiteral("W1AW"));
        QVERIFY(w.infoTextForTest().contains(QLatin1String("no billing account")));
        QVERIFY(w.infoTextForTest().contains(QLatin1String("longpath:google3d")));
        QVERIFY(w.googleEarthButtonForTest()->isVisible());

        // Derselbe Schluessel: nicht bei jedem Rufzeichen neu versuchen.
        w.flyToStation(QStringLiteral("K1TTT"), 42.6, -73.1);
        QCOMPARE(w.currentViewForTest(), 1);
        QCOMPARE(g_lastFake->flights.size(), 1);

        // Ein neuer Schluessel: wieder 3D.
        GoogleMapsKey::save(kKey2);
        w.flyToStation(QStringLiteral("K1TTT"), 42.6, -73.1);
        QCOMPARE(w.currentViewForTest(), int(QsoMapWindow::kView3d));
        QCOMPARE(g_lastFake->flights.size(), 2);
        QCOMPARE(g_lastFake->flights.last().apiKey, kKey2);

        // Ein Netzproblem sperrt nichts: der naechste Flug versucht es wieder.
        emit g_lastFake->failed(QStringLiteral("no internet"), false);
        QCOMPARE(w.currentViewForTest(), 1);
        w.flyToStation(QStringLiteral("W1AW"), kStaLat, kStaLon);
        QCOMPARE(w.currentViewForTest(), int(QsoMapWindow::kView3d));
        QCOMPARE(g_lastFake->flights.size(), 3);
    }

    void the_page_is_pinned_and_ion_free()
    {
        const QString html = Station3dPage::html();
        QVERIFY(!html.isEmpty());
        QVERIFY(!html.contains(QLatin1String("@@")));
        const QString base = Station3dPage::cesiumBaseUrl();
        QCOMPARE(base, QStringLiteral("https://cdn.jsdelivr.net/npm/cesium@%1/Build/Cesium/")
                           .arg(QLatin1String(Station3dPage::kCesiumVersion)));
        QVERIFY(html.contains(base + QStringLiteral("Cesium.js")));
        QVERIFY(html.contains(base + QStringLiteral("Widgets/widgets.css")));
        // Beide Dateien mit Pruefsumme; die Fassung steht auch im Kopf.
        QCOMPARE(html.count(QLatin1String("integrity=\"sha384-")), 2);
        QVERIFY(html.contains(QStringLiteral("CesiumJS %1").arg(QLatin1String(Station3dPage::kCesiumVersion))));
        QVERIFY(html.contains(QLatin1String("Apache License 2.0")));
        // Google direkt mit dem eigenen Schluessel; ion bekommt keinen Zugang.
        QVERIFY(html.contains(QLatin1String("createGooglePhotorealistic3DTileset")));
        QVERIFY(html.contains(QLatin1String("Cesium.Ion.defaultAccessToken = \"\"")));
        QVERIFY(html.contains(QLatin1String("baseLayer: false")));
        QVERIFY(!html.contains(QLatin1String("fromIonAssetId")));

        // Ein Test-Ersatz fuer die Basis wird ueberall eingesetzt.
        const QString other = Station3dPage::html(QStringLiteral("http://127.0.0.1:9/c/"));
        QVERIFY(!other.contains(QLatin1String("cdn.jsdelivr.net")));
        QVERIFY(other.contains(QLatin1String("http://127.0.0.1:9/c/Cesium.js")));
    }

    void flight_script_is_json()
    {
        Station3dFlight f;
        f.call = QStringLiteral("W1AW");
        f.caption = QStringLiteral("say \"hi\" </script>");
        f.lat = kStaLat; f.lon = kStaLon;
        f.hasHome = true; f.homeLat = 48.3; f.homeLon = 14.5;
        f.km = 6543.2; f.bearingDeg = 296.4;
        f.apiKey = kKey;
        const QString js = Station3dPage::flightScript(f);
        QVERIFY(js.startsWith(QLatin1String("window.longpath && window.longpath.fly(")));
        QVERIFY(js.endsWith(QLatin1String(");")));
        const QString json = js.mid(js.indexOf(QLatin1Char('(')) + 1).chopped(2);
        QJsonParseError err;
        const QJsonObject o = QJsonDocument::fromJson(json.toUtf8(), &err).object();
        QCOMPARE(err.error, QJsonParseError::NoError);
        QCOMPARE(o.value(QStringLiteral("call")).toString(), f.call);
        QCOMPARE(o.value(QStringLiteral("caption")).toString(), f.caption);
        QCOMPARE(o.value(QStringLiteral("key")).toString(), kKey);
        QCOMPARE(o.value(QStringLiteral("home")).toObject().value(QStringLiteral("lat")).toDouble(), 48.3);
        QCOMPARE(o.value(QStringLiteral("bearing")).toDouble(), 296.4);

        // Ohne Heimat fehlen home/km/bearing ganz -- die Seite faellt
        // dann senkrecht von oben auf die Station.
        Station3dFlight bare;
        bare.call = QStringLiteral("X"); bare.lat = 1; bare.lon = 2;
        const QString js2 = Station3dPage::flightScript(bare);
        QVERIFY(!js2.contains(QLatin1String("\"home\"")));
        QVERIFY(!js2.contains(QLatin1String("\"km\"")));
    }

    void google_earth_url_shape()
    {
        const QUrl u = Station3dPage::googleEarthWebUrl(48.123456, -0.5, -45.0);
        QCOMPARE(u.toString(),
                 QStringLiteral("https://earth.google.com/web/@48.123456,-0.500000,0a,600d,35y,315h,60t,0r"));
    }
};

QTEST_MAIN(TstQsoMap3dFlug)
#include "tst_qso_map_3d_flug.moc"
