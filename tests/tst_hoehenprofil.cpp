// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Hoehenprofil: Geometrie und Hoehenabruf (2026-11-11) ────────────
//
//   1. Ausbuchtung, Fresnel-Radius, Wellenlaenge -- gegen Handrechnung.
//   2. Flaches Meer, 100 km, Antennen 10 m: der Horizontpunkt liegt beim
//      Radiohorizont sqrt(2kRh) = 13,03 km, die Strahlen treffen sich in
//      der Mitte 80,4 m ueber dem Wasser, Streuwinkel (D-2dh)/(kR) =
//      0,499°. Alles aus Formeln, nicht aus dem Code abgeschrieben.
//   3. Die vier Urteile an gebauten Wegen.
//   4. Die drei Wege des Entwurfsblatts vom Gmundnerberg mit den damals
//      aufgezeichneten Hoehen: dieselben Zahlen wie profil.py
//      (1297 / 570 m / 1,18°, 1160 / 1274 m / 1,80°, 13 m / 0 m / 0,13°).
//   5. Der Abruf: 300 Punkte = drei Anfragen zu 100, eine je Sekunde,
//      danach aus dem Cache ohne Anfrage; eine Absage kommt als fehler().
//      Kein Netz: der Transport ist eine Attrappe.

#include <QtTest/QtTest>

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>

#include "core/BeamHeading.h"
#include "core/HoehenDaten.h"
#include "core/Hoehenprofil.h"
#include "core/Maidenhead.h"

using namespace Longpath;
using namespace Longpath::Hoehenprofil;

namespace {

constexpr double kGipfelLat = 47.9058;   // OE/OO-106, SOTA-Datenbank
constexpr double kGipfelLon = 13.7353;

QString datenPfad(const QString& name)
{
    return qEnvironmentVariable("LONGPATH_SOURCE_DIR")
           + QStringLiteral("/tests/data/hoehenprofil/") + name;
}

QVector<double> ladeHoehen(const QString& call)
{
    QFile f(datenPfad(QStringLiteral("hoehe-%1-gipfel.json").arg(call)));
    if (!f.open(QIODevice::ReadOnly)) { return {}; }
    QVector<double> h;
    for (const QJsonValue& v : QJsonDocument::fromJson(f.readAll()).array()) {
        h << v.toDouble();
    }
    return h;
}

Eingabe flach(double km, int n, double antA, double antB, double freq = 144.3)
{
    Eingabe e;
    for (int i = 0; i < n; ++i) {
        e.distKm << km * i / (n - 1);
        e.gelaendeM << 0.0;
    }
    e.antenneA = antA;
    e.antenneB = antB;
    e.freqMHz = freq;
    return e;
}

} // namespace

class TstHoehenprofil : public QObject {
    Q_OBJECT
private slots:

    void ausbuchtungUndFresnel()
    {
        // 50 km in der Mitte von 100 km: 50*50 / (2 * 4/3 * 6371) km.
        QVERIFY(std::abs(ausbuchtungM(50.0, 100.0) - 147.150) < 0.01);
        QCOMPARE(ausbuchtungM(0.0, 100.0), 0.0);
        QCOMPARE(ausbuchtungM(100.0, 100.0), 0.0);
        // Mit k = 1 (ohne Brechung) mehr Ausbuchtung: 196,2 m.
        QVERIFY(std::abs(ausbuchtungM(50.0, 100.0, 1.0) - 196.20) < 0.01);

        QVERIFY(std::abs(wellenlaengeM(144.3) - 2.077564) < 1e-5);
        // r1 = sqrt(lambda * 50 km * 50 km / 100 km) = sqrt(2,07756 * 25000)
        QVERIFY(std::abs(fresnelRadiusM(50.0, 50.0, 144.3) - 227.90) < 0.01);
        // 10 km von einem Ende eines 40-km-Weges: sqrt(lambda * 7500)
        QVERIFY(std::abs(fresnelRadiusM(10.0, 30.0, 144.3) - 124.82) < 0.01);
        // Viermal die Frequenz, halber Radius.
        QVERIFY(std::abs(fresnelRadiusM(50.0, 50.0, 4 * 144.3) - 227.90 / 2) < 0.01);
        QCOMPARE(fresnelRadiusM(0.0, 40.0, 144.3), 0.0);
    }

    void grosskreis()
    {
        const QVector<Punkt> p = grosskreisPunkte(0.0, 0.0, 0.0, 90.0, 3);
        QCOMPARE(p.size(), 3);
        QVERIFY(std::abs(p.at(1).lat) < 1e-9);
        QVERIFY(std::abs(p.at(1).lon - 45.0) < 1e-9);
        const QVector<Punkt> q = grosskreisPunkte(kGipfelLat, kGipfelLon, 50.77, 15.54, 100);
        QVERIFY(std::abs(q.first().lat - kGipfelLat) < 1e-9);
        QVERIFY(std::abs(q.last().lon - 15.54) < 1e-9);
        // Dieselbe Laenge wie die Peilung der Stationskarte.
        const double km = grosskreisKm(kGipfelLat, kGipfelLon, 50.77, 15.54);
        const BeamHeading::GreatCircle g =
            BeamHeading::greatCircle(kGipfelLat, kGipfelLon, 50.77, 15.54);
        QVERIFY(std::abs(km - g.km) < 0.5);

        QCOMPARE(punktzahlFuer(40.0), 100);
        QCOMPARE(punktzahlFuer(150.0), 151);
        QCOMPARE(punktzahlFuer(344.0), 300);
    }

    void baender()
    {
        QCOMPARE(profilFrequenzMHz(QStringLiteral("2m"), 145.5), 145.5);
        QCOMPARE(profilFrequenzMHz(QStringLiteral("2m"), 0.0), 144.3);
        QCOMPARE(profilFrequenzMHz(QStringLiteral("70CM"), 0.0), 432.2);
        QCOMPARE(profilFrequenzMHz(QStringLiteral("23cm"), 1296.2), 1296.2);
        QCOMPARE(profilFrequenzMHz(QString(), 432.1), 432.1);
        QCOMPARE(profilFrequenzMHz(QStringLiteral("20m"), 14.2), 0.0);
        QVERIFY(!istUkwBand(QStringLiteral("6m"), 50.2));
        QVERIFY(istUkwBand(QStringLiteral("2m"), 0.0));
        QCOMPARE(locatorGenauigkeit(QStringLiteral("JO70SS")), QStringLiteral("±3 km"));
        QCOMPARE(locatorGenauigkeit(QStringLiteral("JN67")), QStringLiteral("±60 km"));
    }

    void flachesMeerHorizontstrahlen()
    {
        // 100 km, 0,1 km Schritt, beide Antennen 10 m ueber dem Wasser.
        const Ergebnis e = rechne(flach(100.0, 1001, 10.0, 10.0));
        QVERIFY(e.gueltig);
        QCOMPARE(e.urteil, Urteil::KeineSicht);
        // Hoechster Punkt ueber der Linie: Mitte, Ausbuchtung - 10 m.
        QVERIFY(std::abs(e.maxUeberLinieM - (147.150 - 10.0)) < 0.01);

        // Radiohorizont d_h = sqrt(2 k R h) = 13,034 km.
        const double dh = std::sqrt(2.0 * kK * kErdradiusKm * 1000.0 * 10.0) / 1000.0;
        QVERIFY(std::abs(dh - 13.034) < 0.001);
        QVERIFY(e.hatHorizont);
        QVERIFY(std::abs(e.tangenteA * 0.1 - dh) < 0.11);
        QVERIFY(std::abs((1000 - e.tangenteB) * 0.1 - dh) < 0.11);
        // Steigung der Tangente: (D - dh)/(2kR) - h/dh, in m je km.
        const double s = (100.0 - dh) * 1000.0 / (2.0 * kK * kErdradiusKm) - 10.0 / dh;
        QVERIFY(std::abs(e.steigungA - s) < 1e-4);
        QVERIFY(std::abs(e.steigungB - s) < 1e-4);
        // Schnitt in der Mitte, (D/2 - dh)^2 / (2kR) ueber dem Wasser.
        QVERIFY(std::abs(e.schnittKm - 50.0) < 1e-6);
        const double ueber = std::pow(50.0 - dh, 2) / (2.0 * kK * kErdradiusKm) * 1000.0;
        QVERIFY(std::abs(ueber - 80.43) < 0.01);
        QVERIFY(std::abs(e.schnittUeberGrundM - ueber) < 0.05);
        // Streuwinkel (D - 2 dh) / (kR).
        const double winkel = (100.0 - 2.0 * dh) / (kK * kErdradiusKm) * 180.0 / M_PI;
        QVERIFY(std::abs(winkel - 0.4987) < 0.0005);
        QVERIFY(std::abs(e.streuwinkelGrad - winkel) < 0.0005);
    }

    void vierUrteile()
    {
        // 10 km flach, Mitte: Ausbuchtung 1,47 m, 0,6 r1 = 43,2 m.
        const Ergebnis frei = rechne(flach(10.0, 101, 50.0, 50.0));
        QCOMPARE(frei.urteil, Urteil::FreieSicht);
        QVERIFY(!frei.hatHorizont);
        QCOMPARE(frei.anteilInZone, 0.0);

        const Ergebnis zone = rechne(flach(10.0, 101, 30.0, 30.0));
        QCOMPARE(zone.urteil, Urteil::FresnelAngeschnitten);
        QVERIFY(zone.inZone.at(50));
        QVERIFY(!zone.verdeckt.at(50));

        // Eine Schneide in der Mitte, 20 m ueber der Sichtlinie.
        Eingabe k = flach(20.0, 201, 10.0, 10.0);
        k.gelaendeM[100] = 10.0 + 20.0 - ausbuchtungM(10.0, 20.0);
        const Ergebnis knapp = rechne(k);
        QCOMPARE(knapp.urteil, Urteil::FastSicht);
        QVERIFY(std::abs(knapp.maxUeberLinieM - 20.0) < 1e-6);
        // Beide Horizontstrahlen beruehren dieselbe Schneide: Schnitt dort,
        // 0 m ueber Grund.
        QVERIFY(knapp.hatHorizont);
        QCOMPARE(knapp.tangenteA, 100);
        QCOMPARE(knapp.tangenteB, 100);
        QVERIFY(std::abs(knapp.schnittKm - 10.0) < 1e-6);
        QVERIFY(std::abs(knapp.schnittUeberGrundM) < 1e-6);
        QVERIFY(urteilText(knapp, QStringLiteral("±3 km")).contains(QStringLiteral("±3 km")));

        k.gelaendeM[100] += 40.0;
        QCOMPARE(rechne(k).urteil, Urteil::KeineSicht);

        // Zu wenig Punkte oder ungleich lange Reihen: kein Ergebnis.
        Eingabe kaputt = flach(10.0, 101, 10.0, 10.0);
        kaputt.gelaendeM.removeLast();
        QVERIFY(!rechne(kaputt).gueltig);
    }

    void ungefaehresEndeZaehltNicht()
    {
        // Flaches Meer, 100 km; 2 km vor B ein 80-m-Huegel. Steht B nur als
        // Locator fest (+-3 km), bestimmt er weder Urteil noch Horizont.
        Eingabe ein = flach(100.0, 1001, 10.0, 10.0);
        ein.gelaendeM[980] = 80.0;
        const Ergebnis genau = rechne(ein);
        QCOMPARE(genau.tangenteB, 980);
        QVERIFY(genau.streuwinkelGrad > 2.0);     // der Huegel steilt auf

        ein.unsicherBKm = 3.0;
        const Ergebnis ungefaehr = rechne(ein);
        QVERIFY(ungefaehr.unsicher.at(980));
        QVERIFY(!ungefaehr.unsicher.at(969));
        QVERIFY(!ungefaehr.verdeckt.at(980));
        QVERIFY(ungefaehr.tangenteB < 970);
        // Dann ist es wieder das flache Meer: 0,499°.
        const Ergebnis meer = rechne(flach(100.0, 1001, 10.0, 10.0));
        QVERIFY(std::abs(ungefaehr.streuwinkelGrad - meer.streuwinkelGrad) < 1e-9);
        QVERIFY(std::abs(ungefaehr.schnittUeberGrundM - meer.schnittUeberGrundM) < 1e-9);
        // Die Enden selbst bleiben, und hoechstens ein Viertel des Weges.
        QVERIFY(!ungefaehr.unsicher.last());
        ein.unsicherBKm = 60.0;
        QCOMPARE(rechne(ein).unsicherBKm, 25.0);
        QCOMPARE(locatorUnsicherheitKm(QStringLiteral("JO70SS")), 3.0);
        QCOMPARE(locatorUnsicherheitKm(QStringLiteral("JO70SS12")), 0.3);
    }

    void entwurfsblattGmundnerberg_data()
    {
        QTest::addColumn<QString>("call");
        QTest::addColumn<QString>("grid");
        QTest::addColumn<double>("km");
        QTest::addColumn<int>("urteil");
        QTest::addColumn<double>("ueberLinie");
        QTest::addColumn<double>("volumen");
        QTest::addColumn<double>("winkel");
        // Sollwerte aus profil.py (werkzeug/logbuch-2026-11-09), dort
        // nachgerechnet am 2026-11-11.
        QTest::newRow("SN7L")   << "SN7L"   << "JO70SS" << 344.386
            << int(Urteil::KeineSicht) << 1297.401 << 569.989 << 1.1759;
        QTest::newRow("OM2Y")   << "OM2Y"   << "JN88RS" << 291.476
            << int(Urteil::KeineSicht) << 1160.145 << 1273.860 << 1.8000;
        QTest::newRow("OE5PCH") << "OE5PCH" << "JN68XF" << 39.592
            << int(Urteil::FastSicht) << 12.741 << 0.0 << 0.1296;
    }

    void entwurfsblattGmundnerberg()
    {
        QFETCH(QString, call);
        QFETCH(QString, grid);
        QFETCH(double, km);
        QFETCH(int, urteil);
        QFETCH(double, ueberLinie);
        QFETCH(double, volumen);
        QFETCH(double, winkel);

        const QVector<double> h = ladeHoehen(call);
        QCOMPARE(h.size(), 100);
        double lat = 0.0, lon = 0.0;
        calculateLatLonFromGridSquare(grid, lat, lon);
        const double D = grosskreisKm(kGipfelLat, kGipfelLon, lat, lon);
        QVERIFY2(std::abs(D - km) < 0.01, qPrintable(QString::number(D, 'f', 4)));

        Eingabe ein;
        for (int i = 0; i < 100; ++i) { ein.distKm << D * i / 99.0; }
        ein.gelaendeM = h;
        ein.antenneA = 5.0;    // portabel am Gipfel
        ein.antenneB = 10.0;
        ein.freqMHz = 144.3;
        const Ergebnis e = rechne(ein);
        QVERIFY(e.gueltig);
        QCOMPARE(int(e.urteil), urteil);
        QVERIFY2(std::abs(e.maxUeberLinieM - ueberLinie) < 0.01,
                 qPrintable(QString::number(e.maxUeberLinieM, 'f', 3)));
        QVERIFY(e.hatHorizont);
        QVERIFY2(std::abs(e.schnittUeberGrundM - volumen) < 0.01,
                 qPrintable(QString::number(e.schnittUeberGrundM, 'f', 3)));
        QVERIFY2(std::abs(e.streuwinkelGrad - winkel) < 0.0001,
                 qPrintable(QString::number(e.streuwinkelGrad, 'f', 5)));
    }

    void abrufStueckeltUndCacht()
    {
        QTemporaryDir dir;
        HoehenAbruf abruf;
        abruf.setCacheOrdner(dir.path());
        QVector<QUrl> gefragt;
        abruf.setAbruf([&gefragt](const QUrl& url,
                                  std::function<void(const HttpAntwort&)> fertig) {
            gefragt << url;
            const QStringList orte = QUrlQuery(url)
                .queryItemValue(QStringLiteral("locations"), QUrl::FullyDecoded)
                .split(QLatin1Char('|'));
            QJsonArray res;
            for (int i = 0; i < orte.size(); ++i) {
                QJsonObject o;
                // Lat * 10 als Hoehe: so ist jede Reihenfolge pruefbar;
                // ein Punkt ohne Wert (Meer) als null.
                const double lat = orte.at(i).section(QLatin1Char(','), 0, 0).toDouble();
                o.insert(QStringLiteral("elevation"),
                         i == 5 ? QJsonValue() : QJsonValue(lat * 10.0));
                res.append(o);
            }
            QJsonObject antwort{{QStringLiteral("status"), QStringLiteral("OK")},
                                {QStringLiteral("results"), res}};
            HttpAntwort a;
            a.status = 200;
            a.body = QJsonDocument(antwort).toJson();
            QTimer::singleShot(0, [fertig, a]() { fertig(a); });
        });

        const HoehenAbruf::Weg w{kGipfelLat, kGipfelLon, 50.7708, 15.5417, 300};
        QSignalSpy fertig(&abruf, &HoehenAbruf::fertig);
        QElapsedTimer uhr;
        uhr.start();
        abruf.hole(w);
        QVERIFY(fertig.wait(10000));
        QCOMPARE(gefragt.size(), 3);
        // Eine je Sekunde: drei Anfragen brauchen mindestens 2,2 s.
        QVERIFY2(uhr.elapsed() >= 2 * HoehenAbruf::kAbstandMs - 50,
                 qPrintable(QString::number(uhr.elapsed())));
        for (const QUrl& u : gefragt) {
            QCOMPARE(u.host(), QStringLiteral("api.opentopodata.org"));
            QCOMPARE(u.path(), QStringLiteral("/v1/eudem25m,srtm90m"));
            QCOMPARE(QUrlQuery(u).queryItemValue(QStringLiteral("locations"),
                                                 QUrl::FullyDecoded).split('|').size(), 100);
        }
        const QVector<double> h = fertig.first().at(1).value<QVector<double>>();
        QCOMPARE(h.size(), 300);
        QVERIFY(std::abs(h.first() - kGipfelLat * 10.0) < 1e-3);
        QVERIFY(std::abs(h.last() - 507.708) < 1e-3);
        QVERIFY(std::isnan(h.at(5)));          // null -> NaN, nicht 0
        // Die Attrappe laesst in jedem Stueck Punkt 5 leer: genau drei
        // Luecken, an der richtigen Stelle zusammengesetzt.
        QCOMPARE(std::count_if(h.cbegin(), h.cend(), [](double v) { return std::isnan(v); }), 3);
        QVERIFY(std::isnan(h.at(105)) && std::isnan(h.at(205)));
        QVERIFY(h.at(104) > 0.0 && h.at(106) > 0.0);

        // Zweites Mal: aus dem Cache, keine Anfrage.
        QSignalSpy wieder(&abruf, &HoehenAbruf::fertig);
        abruf.hole(w);
        QVERIFY(wieder.wait(2000));
        QCOMPARE(gefragt.size(), 3);
        QVector<double> c;
        QVERIFY(abruf.ausCache(w, c));
        QCOMPARE(c.size(), 300);
        QVERIFY(std::isnan(c.at(5)));
        QCOMPARE(HoehenAbruf::schluessel(w), HoehenAbruf::schluessel(w));
        HoehenAbruf::Weg anders = w;
        anders.punkte = 299;
        QVERIFY(HoehenAbruf::schluessel(anders) != HoehenAbruf::schluessel(w));
    }

    void abrufMeldetAbsage()
    {
        QTemporaryDir dir;
        HoehenAbruf abruf;
        abruf.setCacheOrdner(dir.path());
        abruf.setAbruf([](const QUrl&, std::function<void(const HttpAntwort&)> fertig) {
            HttpAntwort a;
            a.status = 429;
            a.body = "{\"status\":\"INVALID_REQUEST\",\"error\":\"Too many requests\"}";
            QTimer::singleShot(0, [fertig, a]() { fertig(a); });
        });
        QSignalSpy fehler(&abruf, &HoehenAbruf::fehler);
        abruf.hole({kGipfelLat, kGipfelLon, 48.229, 13.958, 100});
        QVERIFY(fehler.wait(5000));
        QVERIFY(fehler.first().at(1).toString().contains(QStringLiteral("too many")));
        QVector<double> c;
        QVERIFY(!abruf.ausCache({kGipfelLat, kGipfelLon, 48.229, 13.958, 100}, c));

        QVector<double> aus;
        QString text;
        QVERIFY(!HoehenAbruf::leseAntwort("{\"status\":\"INVALID_REQUEST\",\"error\":\"bad\"}",
                                          aus, &text));
        QCOMPARE(text, QStringLiteral("bad"));
    }
};

QTEST_GUILESS_MAIN(TstHoehenprofil)
#include "tst_hoehenprofil.moc"
