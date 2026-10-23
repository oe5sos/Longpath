// =================================================================
// tests/tst_panadapter_lattenzaun.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigener Pruefstand.
//
// Der „Lattenzaun": bei der FLEX-8400-Quelle zeichnete der Panadapter
// einen Wald gleichmaesziger senkrechter Rippen ueber das ganze Band,
// dort, wo in den Daten nichts ist. Der Betreiber am 2026-10-22:
// „keine signale obwohl wasserfall", „hier hat es was von grund auf".
//
// Dass die DATEN in Ordnung sind, ist gemessen: ein vollstaendiger
// Rahmen von Martins Geraet (tests/data/flex_rahmen_20m.csv, 4096
// Bins, 13,869–14,330 MHz, aufgenommen um 20:57) traegt 26 Spitzen in
// UNREGELMAESZIGEN Abstaenden (342, 40, 457, 355, 62, 9, 6, …),
// gehaeuft im CW-Bereich und oben duenn. Ein echtes 20-m-Band, und
// sein Rauschteppich hat 1,9 dB Streuung.
//
// Martins Profil steht auf `DisplaySpectrumRenderMode = 1`, also auf
// der gestapelten 3D-Flaeche (DssRenderer). Dieser Stand schiebt den
// echten Rahmen durch denselben Weg wie das Programm und misst jede
// Stufe — offscreen, ohne Geraet, ohne GPU (DssRenderer ist reines
// Raster).
//
// ── Die Ursache ──────────────────────────────────────────────────
//
// Der Flur der 3D-Flaeche lag 6 dB UNTER dem gemessenen
// Rauschteppich (SpectrumWidget, nach AetherSDRs „3D Floor"-Vorgabe).
// Damit bekommt der Teppich selbst rund 14 Prozent der vollen
// Rippenhoehe, und 96 solcher Zeilen uebereinander ergeben den Wald.
// Mit dem Flur 3 dB UEBER dem Teppich liegt er flach, und die
// Signale stehen weiterhin.
//
// ── Zum Masz, und warum es viermal falsch war ────────────────────
//
// Vier Versuche maszen etwas anderes als die Sache:
//   1. Autokorrelation der Rohkurve — misst vor allem GLATTHEIT.
//      Meldete fuer die Rohdaten 0,45; die sind kein Kamm.
//   2. Oberkante von oben gesucht — trifft die HINTERSTE Zeile.
//   3. Kante von unten, kurzes Glaettungsfenster — laesst die
//      schraege Seite des TRAPEZES stehen. Ein nachweislich flacher
//      Eingang meldete damit 0,61.
//   4. Helligkeit je Spalte — normiert auf die eigene Streuung, ein
//      FLACHERES Bild bekommt dadurch eine HOEHERE Kennzahl.
//
// Gemessen wird jetzt die Sache selbst: wie viele Bildzeilen je
// Spalte bemalt sind. Der Teppich soll flach liegen, die Signale
// sollen stehen. Und das Masz ist an beiden Enden geprueft — ein
// flacher Eingang muss eine ebene Flaeche geben, der alte Flur muss
// den Wald zeigen.
// =================================================================

#include <QtTest/QtTest>

#include "gui/SpectrumWidget.h"
#include "gui/DssRenderer.h"

#include <algorithm>
#include <cmath>

using namespace Longpath;

namespace {

// Die Bins der Maschine sind BILDZEILEN: 0 = maxDbm (oben), groesser =
// schwaecher. Derselbe Maszstab, den RadioModel am Geraet setzt.
constexpr double kMinDbm  = -130.0;
constexpr double kMaxDbm  = -40.0;
constexpr int    kYPixels = 700;

// Der gemessene Rauschteppich des aufgenommenen Rahmens (Median).
constexpr float kTeppichDbm = -118.5f;

// Der Flurversatz, den SpectrumWidget benutzt. Aendert er sich dort,
// muss er hier mitgehen — sonst prueft dieser Stand eine Darstellung,
// die es nicht gibt.
constexpr float kFlurversatzDb = 0.0f;

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

// Wie regelmaeszig ist eine Kurve? Autokorrelation nach Abzug eines
// gleitenden Mittels (31 Punkte: kurz genug fuer einen Kamm, lang
// genug gegen grosze Schraegen). Nur noch fuer die Stufen VOR dem
// Zeichnen gebraucht.
QPair<int, double> staerksteWiederholung(const QVector<float>& kurve,
                                         int minPeriode, int maxPeriode)
{
    const int n = kurve.size();
    if (n < 4 * maxPeriode) {
        qWarning("Kurve zu kurz (%d) fuer Periode bis %d", n, maxPeriode);
        return {0, 0.0};
    }
    constexpr int kFenster = 31;
    QVector<double> x(n);
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

    int besteP = 0; double beste = 0.0;
    for (int p = minPeriode; p <= maxPeriode; ++p) {
        double s = 0.0;
        for (int i = 0; i + p < n; ++i) { s += x[i] * x[i + p]; }
        const double r = s / norm;
        if (r > beste) { beste = r; besteP = p; }
    }
    return {besteP, beste};
}

// Wie hoch steht die Flaeche je Spalte: Zahl der bemalten Bildzeilen,
// im mittleren Drittel (dort liegt das Trapez in voller Breite).
QVector<int> spaltenhoehen(const QImage& bild)
{
    const int xa = bild.width() / 3;
    const int xe = bild.width() - bild.width() / 3;
    QVector<int> h(xe - xa, 0);
    for (int x = xa; x < xe; ++x) {
        int z = 0;
        for (int y = 0; y < bild.height(); ++y) {
            if (bild.pixel(x, y) != qRgb(0, 0, 0)) { ++z; }
        }
        h[x - xa] = z;
    }
    return h;
}

// Der typische Stand der Flaeche — also der Teppich, unbeeindruckt
// von einzelnen hohen Signalen.
int medianHoehe(QVector<int> h)
{
    if (h.isEmpty()) { return 0; }
    std::sort(h.begin(), h.end());
    return h[h.size() / 2];
}

// Wie SpectrumWidget die Flaeche anfaehrt.
QImage flaecheAus(DssRenderer& dss, float flurversatzDb)
{
    return dss.image(QSize(1168, 260), /*scaleStripPx=*/0,
                     kTeppichDbm + flurversatzDb, /*rangeDb=*/100.0f,
                     /*zCurve=*/0.70f,
                     [](float) -> QRgb { return qRgb(0, 128, 255); },
                     /*paletteToken=*/1, QColor(Qt::black));
}

} // namespace

class TstPanadapterLattenzaun : public QObject {
    Q_OBJECT

private:
    QVector<float> durchDenZeichenweg(SpectrumWidget& sw,
                                      const QVector<float>& dbm,
                                      double lowHz, double highHz,
                                      double mitteHz, double breiteHz)
    {
        sw.setKiwiDisplaySource(true);
        sw.setFrequencyRange(mitteHz, breiteHz);
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

    // ── Das Masz, an beiden Enden ────────────────────────────────────
    //
    // Ein vollkommen flacher Eingang muss eine ebene Flaeche geben.
    // Tut er das nicht, misst dieser Stand sich selbst. Vier Masze
    // sind genau hier durchgefallen, bevor dieses stand.
    void flacherEingangGibtEbeneFlaeche()
    {
        DssRenderer dss;
        const QVector<float> flach(2270, -100.0f);
        for (int n = 0; n < 96; ++n) { dss.pushRow(flach); }

        const QImage bild = flaecheAus(dss, kFlurversatzDb);
        QVERIFY(!bild.isNull());
        const QVector<int> h = spaltenhoehen(bild);
        QVERIFY(!h.isEmpty());
        const int tief = *std::min_element(h.cbegin(), h.cend());
        const int hoch = *std::max_element(h.cbegin(), h.cend());
        qInfo().nospace() << "flacher Eingang: Hoehen " << tief << " bis "
                          << hoch << " px";
        QVERIFY2(hoch - tief <= 2,
                 qPrintable(QStringLiteral("Ein flacher Eingang gibt eine "
                     "unebene Flaeche (%1 bis %2 px) — dann taugt das Masz "
                     "nicht.").arg(tief).arg(hoch)));
    }

    // ── Die Vorbedingung: der Rahmen ist ein Band, kein Kamm ─────────
    void derRahmenSelbstIstKeinKamm()
    {
        const auto [p, st] = staerksteWiederholung(m_rahmen, 3, 200);
        qInfo().nospace() << "Rohdaten: Periode " << p << " Welligkeit " << st;
        QVERIFY2(st < 0.25,
                 qPrintable(QStringLiteral("Die Rohdaten tragen schon eine "
                     "Regelmaeszigkeit (Periode %1, Welligkeit %2).")
                     .arg(p).arg(st, 0, 'f', 3)));
    }

    // ── Die Rechnung bis zu den Bildpunkten ──────────────────────────
    void dieBildpunkteSindKeinKamm()
    {
        const double low = 13.8692e6, high = 14.3304e6;

        // Herausgezoomt: mehr Bins als Bildpunkte (Detektorzweig
        // `pixPerBin <= 1`).
        SpectrumWidget sw;
        sw.setDisplayWidthOverrideForTest(1200);
        const auto weit = durchDenZeichenweg(
            sw, m_rahmen, low, high, (low + high) * 0.5, high - low);
        QCOMPARE(weit.size(), 1200);
        const auto [p1, s1] = staerksteWiederholung(weit, 3, 200);
        qInfo().nospace() << "Bildpunkte herausgezoomt: Periode " << p1
                          << " Welligkeit " << s1;
        QVERIFY2(s1 < 0.25, "Lattenzaun schon in den Bildpunkten");

        // Hineingezoomt: WENIGER Bins als Bildpunkte (zweiter Zweig).
        // So stand der Panadapter, als der Betreiber den Wald sah.
        SpectrumWidget sw2;
        sw2.setDisplayWidthOverrideForTest(2270);   // Retina, 1168 pt
        const auto nah = durchDenZeichenweg(
            sw2, m_rahmen, low, high, 14.165e6, 130.0e3);
        QCOMPARE(nah.size(), 2270);
        const auto [p2, s2] = staerksteWiederholung(nah, 3, 200);
        qInfo().nospace() << "Bildpunkte hineingezoomt: Periode " << p2
                          << " Welligkeit " << s2;
        QVERIFY2(s2 < 0.25, "Lattenzaun schon in den Bildpunkten");
    }

    // ── Und die gestapelte 3D-Flaeche ────────────────────────────────
    //
    // 96 VERSCHIEDENE Zeilen, so viele wie die Flaeche tief ist
    // (kVisibleRows). Mit immer derselben Zeile steht die Flaeche
    // still, und die zeitliche Glaettung in `pushRow` verwischt genau
    // das, was gemessen werden soll.
    void derTeppichLiegtFlachUndSignaleStehen()
    {
        SpectrumWidget sw;
        sw.setDisplayWidthOverrideForTest(2270);
        const double low = 13.8692e6, high = 14.3304e6;

        DssRenderer dss;
        for (int n = 0; n < 96; ++n) {
            QVector<float> variiert = m_rahmen;
            for (int i = 0; i < variiert.size(); ++i) {
                variiert[i] += 0.8f * std::sin(0.013f * (i + 37 * n));
            }
            dss.pushRow(durchDenZeichenweg(sw, variiert, low, high,
                                           14.165e6, 130.0e3));
        }
        QVERIFY(dss.hasData());

        const QImage bild = flaecheAus(dss, kFlurversatzDb);
        QVERIFY(!bild.isNull());

        const QByteArray ablage = qgetenv("LONGPATH_GRAB_DIR");
        if (!ablage.isEmpty()) {
            for (const auto& [name, versatz] :
                 {std::pair<const char*, float>{"m6", -6.0f},
                  {"m0", 0.0f}, {"p3", +3.0f}, {"p6", +6.0f}}) {
                flaecheAus(dss, versatz)
                    .save(QString::fromLocal8Bit(ablage)
                          + QStringLiteral("/dss_flur_")
                          + QLatin1String(name) + QStringLiteral(".png"));
            }
        }

        // ── Der Nullpunkt: dieselbe Flaeche ohne jedes Relief ───────
        //
        // Das gestapelte Trapez fuellt von sich aus schon rund 170
        // Bildzeilen je Spalte, auch bei vollkommen flachem Eingang.
        // Gemessen wird darum, wie viel die Flaeche DARUEBER
        // hinausgeht — das ist das Relief.
        DssRenderer eben;
        const QVector<float> flach(2270, kTeppichDbm);
        for (int n = 0; n < 96; ++n) { eben.pushRow(flach); }
        const int nullpunkt =
            medianHoehe(spaltenhoehen(flaecheAus(eben, kFlurversatzDb)));

        // Gemessen wird gegen diesen Nullpunkt:
        //
        //   Flur     p10   p50   p90   max      (ebene Flaeche = 150)
        //   −6 dB    158   161   163   168   <- der ganze Teppich traegt
        //                                       Relief: der Wald
        //    0 dB    150   152   156   162
        //    0 dB    150   152   156   162   <- Teppich fast flach,
        //                                       Signale am hoechsten
        //   +6 dB    150   150   150   154   <- Signale schon gestutzt
        //
        // Also: p90 (der Teppich samt seiner Welligkeit) muss am
        // Nullpunkt liegen, und das hoechste Signal deutlich darueber.
        auto p90 = [](QVector<int> h) {
            std::sort(h.begin(), h.end());
            return h.isEmpty() ? 0 : h[h.size() * 9 / 10];
        };

        const QVector<int> hoehen = spaltenhoehen(bild);
        QVERIFY(!hoehen.isEmpty());
        const int teppich = p90(hoehen) - nullpunkt;
        const int spitze  = *std::max_element(hoehen.cbegin(), hoehen.cend())
                          - nullpunkt;
        qInfo().nospace() << "3D-Flaeche: Nullpunkt " << nullpunkt
                          << " px, Teppich +" << teppich
                          << " px, hoechstes Signal +" << spitze << " px";

        QVERIFY2(teppich <= 6,
                 qPrintable(QStringLiteral("Der Rauschteppich steht %1 px "
                     "ueber der ebenen Flaeche — das ist der Lattenzaun.")
                     .arg(teppich)));
        // Ein flachgebuegeltes Bild waere keine Behebung.
        QVERIFY2(spitze >= 8,
                 qPrintable(QStringLiteral("Kein Signal steht ueber dem "
                     "Teppich (+%1 px).").arg(spitze)));

        // ── Die Gegenprobe, im selben Lauf ──────────────────────────
        //
        // Mit dem ALTEN Flur (−6 dB) MUSS derselbe Stapel den Wald
        // zeigen. Ohne diesen Teil belegte der Fall oben nur, dass
        // heute nichts zu sehen ist — nicht, dass er den Fehler faengt.
        // Gegen DENSELBEN Nullpunkt wie oben, nicht gegen eine eigene
        // Bezugsflaeche: bei −6 dB hebt sich auch die ebene Flaeche
        // mit, und die Differenz zu ihr schrumpft wieder. Was man
        // SIEHT, ist die absolute Hoehe — der Teppich fuellt das Bild.
        const int teppichAlt = p90(spaltenhoehen(flaecheAus(dss, -6.0f)))
                             - nullpunkt;
        qInfo().nospace() << "mit dem alten Flur (-6 dB): Teppich +"
                          << teppichAlt << " px";
        QVERIFY2(teppichAlt >= teppich + 5,
                 qPrintable(QStringLiteral("Der alte Flur zeigt KEINEN Wald "
                     "(Teppich +%1 px gegen +%2 px) — dann faengt dieser "
                     "Stand den Fehler nicht.")
                     .arg(teppichAlt).arg(teppich)));
    }

    // ── Das Rot unter dem Panadapter ─────────────────────────────────
    //
    // Der Wasserfall war bei dieser Quelle durchgehend EINFARBIG rot.
    // Einfarbig heiszt: jeder Punkt auf derselben Palettenstufe — die
    // Schwellen sind zusammengefallen oder liegen neben den Daten.
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
                     "zusammengefallen (%1 bis %2 dBm).").arg(u).arg(o)));

        const auto wf = sw.wfRenderedPixels();
        QVERIFY(!wf.isEmpty());
        int drueber = 0, drunter = 0;
        for (float v : wf) {
            if (v >= o) { ++drueber; }
            if (v <= u) { ++drunter; }
        }
        const double anteil = double(drueber + drunter) / wf.size();
        qInfo() << "ausserhalb der Schwellen:" << drunter << "unten,"
                << drueber << "oben von" << wf.size();
        QVERIFY2(anteil < 0.95,
                 "fast alle Punkte auszerhalb der Schwellen — einfarbig");
    }

    // ── Clarity an, aber ohne Boden ──────────────────────────────────
    //
    // Clarity wird ausschliesslich aus `primaryFftEngine()`
    // gefuettert, und die gibt es beim Flex nicht. Vorher fiel dieser
    // Fall durch beide Zweige von `composeWaterfallActiveThresholds`,
    // und die Schwellen blieben auf dem Stand des letzten Geraets
    // stehen — am 2026-10-22 ein durchgehend EINFARBIG ROTER
    // Wasserfall (Rot ist bei Farbschema 0 der Anschlag).
    void clarityOhneBodenFaelltZurueck()
    {
        SpectrumWidget sw;
        sw.setDisplayWidthOverrideForTest(1200);

        // Die Schwellen des Betreibers …
        sw.setWfLowThreshold(-149.0f);
        sw.setWfHighThreshold(-95.0f);
        // … und darueber das Erbe des VORIGEN Geraets: Clarity hat
        // zuletzt dort gerechnet und Schwellen hinterlassen, die zu
        // den Pegeln der Flex nicht passen. `setClarityActive(true)`
        // allein genuegt nicht — der tueckische Zustand ist der, in
        // dem `m_wfActive*` bereits auf fremden Werten STEHT.
        sw.setClarityActive(true);
        sw.setClarityWaterfallThresholds(-200.0f, -190.0f);
        QCOMPARE(sw.wfActiveHighThreshold(), -190.0f);

        // Die Schwellen werden in `composeWaterfallActiveThresholds`
        // neu gebildet, und das laeuft NUR in `pushWaterfallRow` —
        // also braucht das Fenster eine Groesze (sonst ist das
        // Wasserfallbild leer und der Schub kehrt sofort um) und einen
        // Umlauf, damit der Zeitgeber die Zeile schiebt.
        sw.resize(1200, 600);
        sw.show();
        const double low = 13.8692e6, high = 14.3304e6;
        durchDenZeichenweg(sw, m_rahmen, low, high,
                           (low + high) * 0.5, high - low);
        QTest::qWait(400);

        const float u = sw.wfActiveLowThreshold();
        const float o = sw.wfActiveHighThreshold();
        qInfo() << "Clarity ohne Boden: Schwellen" << u << "bis" << o;

        // Ohne Boden gelten die Nutzerschwellen — nicht irgendein Erbe.
        QCOMPARE(u, -149.0f);
        QCOMPARE(o, -95.0f);

        // Und damit faerbt sich nicht alles gleich.
        const auto wf = sw.wfRenderedPixels();
        QVERIFY(!wf.isEmpty());
        int amAnschlag = 0;
        for (float v : wf) { if (v >= o) { ++amAnschlag; } }
        QVERIFY2(double(amAnschlag) / wf.size() < 0.5,
                 qPrintable(QStringLiteral("%1 von %2 Punkten am oberen "
                     "Anschlag — der Wasserfall waere einfarbig.")
                     .arg(amAnschlag).arg(wf.size())));
    }

private:
    QVector<float> m_rahmen;
};

QTEST_MAIN(TstPanadapterLattenzaun)
#include "tst_panadapter_lattenzaun.moc"
