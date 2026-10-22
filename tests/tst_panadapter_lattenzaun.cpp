// =================================================================
// tests/tst_panadapter_lattenzaun.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigener Pruefstand.
//
// Der „Lattenzaun": bei der FLEX-8400-Quelle zeichnete der Panadapter
// einen vollkommen gleichmaeszigen Kamm senkrechter Zacken, alle gleich
// hoch, auf einer schraeg ansteigenden Grundlinie. Der Betreiber am
// 2026-10-22: „keine signale obwohl wasserfall", „hier hat es was von
// grund auf".
//
// Dass es ein Zeichenfehler ist und nicht das Band, ist gemessen: ein
// vollstaendiger Rahmen von Martins Geraet (tests/data/
// flex_rahmen_20m.csv, 4096 Bins, 13,869–14,330 MHz, aufgenommen am
// 2026-10-22 um 20:57) traegt 26 Spitzen in UNREGELMAESZIGEN Abstaenden
// (342, 40, 457, 355, 62, 9, 6, …), gehaeuft im CW-Bereich und oben
// duenn — also ein echtes 20-m-Band. Gezeichnet wurde daraus ein
// regelmaesziger Kamm.
//
// Dieser Stand schiebt genau diesen Rahmen durch denselben Weg wie das
// Programm (updateKiwiSpectrumDbm -> Detektor -> Mittelung ->
// m_renderedPixels) und misst das Ergebnis auf Regelmaeszigkeit.
// Offscreen, ohne Geraet, ohne GPU.
// =================================================================

#include <QtTest/QtTest>

#include "gui/SpectrumWidget.h"
#include "gui/DssRenderer.h"

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace Longpath;

namespace {

// Die Bins der Maschine sind BILDZEILEN: 0 = maxDbm (oben), groesser =
// schwaecher. Derselbe Maszstab, den RadioModel am Geraet setzt.
constexpr double kMinDbm   = -130.0;
constexpr double kMaxDbm   = -40.0;
constexpr int    kYPixels  = 700;

double binZuDbm(int bin)
{
    return kMaxDbm - (static_cast<double>(bin) / (kYPixels - 1))
                       * (kMaxDbm - kMinDbm);
}

QVector<float> ladeRahmen(QString* fehler)
{
    QVector<float> dbm;
    QFile f(QStringLiteral(LONGPATH_TEST_DATA_DIR "/flex_rahmen_20m.csv"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (fehler) { *fehler = f.errorString(); }
        return dbm;
    }
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        const QStringList teile = ts.readLine().split(QLatin1Char(','));
        if (teile.size() != 2) { continue; }
        dbm.append(static_cast<float>(binZuDbm(teile[1].toInt())));
    }
    return dbm;
}

// Wie regelmaeszig ist eine Kurve? Die Autokorrelation ueber den
// abgezogenen Mittelwert: ein Kamm mit der Periode p zeigt dort eine
// hohe Spitze, ein Band zeigt keine.
//
// Zurueck kommt {Periode, Staerke}; Staerke ist auf 1 normiert (1,0
// hiesze: die Kurve wiederholt sich exakt).
QPair<int, double> staerksteWiederholung(const QVector<float>& kurve,
                                         int minPeriode, int maxPeriode)
{
    const int n = kurve.size();
    if (n < 4 * maxPeriode) { return {0, 0.0}; }

    // ── Glattheit ist keine Regelmaeszigkeit ────────────────────────
    //
    // Eine blosze Autokorrelation auf der Rohkurve misst bei kleinen
    // Verschiebungen vor allem, wie glatt die Kurve ist: benachbarte
    // Punkte eines Rauschteppichs aehneln einander ohnehin. Beim
    // ersten Lauf meldete sie darum fuer die ROHDATEN der Maschine
    // 0,45 bei Periode 3 — und die sind nachweislich kein Kamm.
    //
    // Darum erst den gleitenden Mittelwert ueber fuenf Punkte abziehen.
    // Was bleibt, ist die Welligkeit, und nur die kann einen Kamm
    // bilden.
    QVector<double> x(n);
    constexpr int kFenster = 5;
    for (int i = 0; i < n; ++i) {
        double s = 0.0; int z = 0;
        for (int j = i - kFenster / 2; j <= i + kFenster / 2; ++j) {
            if (j >= 0 && j < n) { s += kurve[j]; ++z; }
        }
        x[i] = kurve[i] - s / z;
    }
    double norm = 0.0;
    for (double v : std::as_const(x)) { norm += v * v; }
    if (norm <= 0.0) { return {0, 0.0}; }

    int besteP = 0;
    double beste = 0.0;
    for (int p = minPeriode; p <= maxPeriode; ++p) {
        double s = 0.0;
        for (int i = 0; i + p < n; ++i) { s += x[i] * x[i + p]; }
        const double r = s / norm;
        if (r > beste) { beste = r; besteP = p; }
    }
    return {besteP, beste};
}

} // namespace

class TstPanadapterLattenzaun : public QObject {
    Q_OBJECT

private:
    // Der Weg des Programms, Schritt fuer Schritt: fremde Quelle an,
    // Rahmen hinein, Ergebnis heraus.
    QVector<float> durchDenZeichenweg(SpectrumWidget& sw,
                                      const QVector<float>& dbm,
                                      double lowHz, double highHz,
                                      double mitteHz, double breiteHz)
    {
        sw.setKiwiDisplaySource(true);
        sw.setFrequencyRange(mitteHz, breiteHz);
        // Mittelung aus: sie verwischt genau das, was gemessen werden
        // soll, und kann einen Kamm ueber mehrere Rahmen aufbauen.
        sw.setSpectrumAveraging(SpectrumAveraging::None);
        for (int n = 0; n < 3; ++n) {
            sw.updateKiwiSpectrumDbm(dbm, lowHz, highHz);
        }
        return sw.renderedPixels();
    }

private slots:

    void initTestCase()
    {
        QString fehler;
        m_rahmen = ladeRahmen(&fehler);
        QVERIFY2(!m_rahmen.isEmpty(),
                 qPrintable(QStringLiteral("Rahmen nicht lesbar: ") + fehler));
        QCOMPARE(m_rahmen.size(), 4096);
    }

    // Die Vorbedingung: der aufgenommene Rahmen ist ein Band, kein Kamm.
    // Ohne diesen Fall belegt alles Weitere nichts — ein Kamm im Bild
    // waere dann schlicht richtig gezeichnet.
    void derRahmenSelbstIstKeinKamm()
    {
        const auto [p, staerke] = staerksteWiederholung(m_rahmen, 3, 200);
        qInfo() << "Rahmen: staerkste Wiederholung bei Periode" << p
                << "Staerke" << staerke;
        QVERIFY2(staerke < 0.35,
                 qPrintable(QStringLiteral("Die Rohdaten tragen schon eine "
                     "Regelmaeszigkeit (Periode %1, Staerke %2) — dann misst "
                     "dieser Stand den Zeichenweg nicht.")
                     .arg(p).arg(staerke, 0, 'f', 3)));
    }

    // Herausgezoomt: mehr Bins als Bildpunkte. Das ist der Zweig
    // `pixPerBin <= 1` im Detektor.
    void herausgezoomtKeinKamm()
    {
        SpectrumWidget sw;
        sw.setDisplayWidthOverrideForTest(1200);
        const double low = 13.8692e6, high = 14.3304e6;
        const auto pixel = durchDenZeichenweg(
            sw, m_rahmen, low, high, (low + high) * 0.5, high - low);
        QCOMPARE(pixel.size(), 1200);

        const auto [p, staerke] = staerksteWiederholung(pixel, 3, 200);
        qInfo() << "herausgezoomt: Periode" << p << "Staerke" << staerke;
        QVERIFY2(staerke < 0.35,
                 qPrintable(QStringLiteral("Lattenzaun im Bild: Periode %1, "
                     "Staerke %2").arg(p).arg(staerke, 0, 'f', 3)));
    }

    // Hineingezoomt: WENIGER Bins als Bildpunkte. Das ist der zweite
    // Zweig des Detektors (`pixPerBin > 1`), und genau so stand der
    // Panadapter, als der Betreiber den Kamm sah: 4096 Bins ueber
    // 461 kHz, gezeigt wurden rund 130 kHz.
    void hineingezoomtKeinKamm()
    {
        SpectrumWidget sw;
        sw.setDisplayWidthOverrideForTest(2270);   // Retina, 1168 pt breit
        const double low = 13.8692e6, high = 14.3304e6;
        const auto pixel = durchDenZeichenweg(
            sw, m_rahmen, low, high, 14.165e6, 130.0e3);
        QCOMPARE(pixel.size(), 2270);

        const auto [p, staerke] = staerksteWiederholung(pixel, 3, 200);
        qInfo() << "hineingezoomt: Periode" << p << "Staerke" << staerke;
        QVERIFY2(staerke < 0.35,
                 qPrintable(QStringLiteral("Lattenzaun im Bild: Periode %1, "
                     "Staerke %2").arg(p).arg(staerke, 0, 'f', 3)));
    }

    // ── Die 3D-Flaeche: dort entsteht der Kamm ───────────────────────
    //
    // Martins Profil steht auf `DisplaySpectrumRenderMode = 1`, also
    // Mode3D. Die berechneten Bildpunkte sind nachweislich glatt
    // (die Faelle darueber); gezeichnet wird daraus aber eine Flaeche
    // mit vollkommen gleichmaeszigen senkrechten Zacken.
    //
    // DssRenderer ist reines Raster — dieselbe Flaeche, die der
    // Betreiber sieht, laesst sich hier ohne GPU bauen und ausmessen.
    void dieFlaecheIstKeinKamm()
    {
        SpectrumWidget sw;
        sw.setDisplayWidthOverrideForTest(2270);
        const double low = 13.8692e6, high = 14.3304e6;
        const auto pixel = durchDenZeichenweg(
            sw, m_rahmen, low, high, 14.165e6, 130.0e3);

        DssRenderer dss;
        for (int n = 0; n < 40; ++n) { dss.pushRow(pixel); }
        QVERIFY(dss.hasData());

        const QSize groesse(1168, 260);
        const QImage bild = dss.image(
            groesse, /*scaleStripPx=*/0,
            /*floorDbm=*/-125.0f, /*rangeDb=*/60.0f, /*zCurve=*/0.6f,
            [](float) -> QRgb { return qRgb(0, 128, 255); },
            /*paletteToken=*/1, QColor(Qt::black));
        QVERIFY(!bild.isNull());

        // Die Oberkante der vorderen Spur: fuer jede Spalte die
        // oberste Zeile, die nicht Hintergrund ist. Ein Kamm zeigt
        // sich darin als regelmaeszige Welligkeit.
        QVector<float> kante(bild.width(), float(bild.height()));
        for (int x = 0; x < bild.width(); ++x) {
            for (int y = 0; y < bild.height(); ++y) {
                if (qAlpha(bild.pixel(x, y)) > 0
                    && bild.pixel(x, y) != qRgb(0, 0, 0)) {
                    kante[x] = float(y);
                    break;
                }
            }
        }
        const auto [p, staerke] = staerksteWiederholung(kante, 3, 200);
        qInfo() << "3D-Flaeche: Periode" << p << "Staerke" << staerke;
        QVERIFY2(staerke < 0.35,
                 qPrintable(QStringLiteral("Lattenzaun in der 3D-Flaeche: "
                     "Periode %1, Staerke %2").arg(p).arg(staerke, 0, 'f', 3)));
    }

    // ── Das Rot unter dem Panadapter ─────────────────────────────────
    //
    // Der Wasserfall war bei dieser Quelle durchgehend EINFARBIG rot,
    // ohne jede Zeitstruktur. Einfarbig heiszt: jeder Punkt landet auf
    // derselben Palettenstufe, und das passiert, wenn die beiden
    // Schwellen zusammenfallen oder beide neben den Daten liegen.
    //
    // Die Daten: Mittelwert −118,5 dBm, staerkster Punkt −40 dBm. Die
    // Vorgabeschwellen stammen vom eigenen Geraet (−122 / −62).
    void wasserfallschwellenPassenZuDenDaten()
    {
        SpectrumWidget sw;
        sw.setDisplayWidthOverrideForTest(1200);
        const double low = 13.8692e6, high = 14.3304e6;
        durchDenZeichenweg(sw, m_rahmen, low, high,
                           (low + high) * 0.5, high - low);

        const float u = sw.wfActiveLowThreshold();
        const float o = sw.wfActiveHighThreshold();
        qInfo() << "Wasserfallschwellen: unten" << u << "oben" << o;

        QVERIFY2(o - u > 5.0f,
                 qPrintable(QStringLiteral("Die Schwellen sind "
                     "zusammengefallen (%1 bis %2 dBm) — dann ist jeder "
                     "Punkt dieselbe Farbe.").arg(u).arg(o)));

        // Und sie muessen die Daten auch TREFFEN: liegt das ganze Band
        // ueber der oberen oder unter der unteren Schwelle, ist das
        // Bild ebenso einfarbig.
        const auto wf = sw.wfRenderedPixels();
        QVERIFY(!wf.isEmpty());
        int drueber = 0, drunter = 0;
        for (float v : wf) {
            if (v >= o) { ++drueber; }
            if (v <= u) { ++drunter; }
        }
        const double anteil =
            double(drueber + drunter) / wf.size();
        qInfo() << "ausserhalb der Schwellen:" << drunter << "unten,"
                << drueber << "oben von" << wf.size();
        QVERIFY2(anteil < 0.95,
                 qPrintable(QStringLiteral("%1 %2 der Punkte liegen "
                     "auszerhalb der Schwellen — das Bild ist einfarbig.")
                     .arg(anteil * 100.0, 0, 'f', 1)
                     .arg(QStringLiteral("%"))));
    }

private:
    QVector<float> m_rahmen;
};

QTEST_MAIN(TstPanadapterLattenzaun)
#include "tst_panadapter_lattenzaun.moc"
