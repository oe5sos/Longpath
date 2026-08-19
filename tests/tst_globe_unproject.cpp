// =================================================================
// tests/tst_globe_unproject.cpp  (NereusSDR)
// =================================================================
//
// Die Rueckrechnung der Kugel: Bildpunkt -> Ort.
//
// Auf Ansage des Betreibers (2026-08-19): „die Weltkugel würde ich mit
// Zoom machen, wie bei Google Earth". Der spuerbarste Teil davon ist
// nicht die Zoom-Decke, sondern WOHIN gezoomt wird — zur Mitte oder zu
// dem Ort, auf den man zeigt. Letzteres braucht diese Umkehrung.
//
// Geprueft wird sie ueber den RUNDGANG: einen Ort projizieren, den
// Bildpunkt zurueckrechnen, denselben Ort erwarten. Ein Vorzeichenfehler
// in der Umkehrung faellt so auf, ohne dass irgendwo ein Bild verglichen
// werden muss.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>

#include "gui/widgets/GlobeWidget.h"

using namespace NereusSDR;

class TestGlobeUnproject : public QObject
{
    Q_OBJECT

private:
    static void prepare(GlobeWidget& g)
    {
        g.resize(400, 400);
        g.resetView();
    }

private slots:

    // Der Rundgang, an mehreren Orten. Toleranz 0,05 Grad — die
    // Projektion rechnet in Bildpunkten, und bei 400 Bildpunkten
    // Kantenlaenge ist ein Bildpunkt etwa 0,3 Grad am Aequator. Enger
    // waere ein Test der Rundungsart, nicht der Formel.
    void projectAndBackAgain_data()
    {
        QTest::addColumn<double>("lat");
        QTest::addColumn<double>("lon");

        QTest::newRow("Mitte der Ansicht")     << 20.0  << 0.0;
        QTest::newRow("Aequator, Nullmeridian") << 0.0   << 0.0;
        QTest::newRow("Nordwaerts")            << 55.0  << 10.0;
        QTest::newRow("Suedwaerts")            << -30.0 << -20.0;
        QTest::newRow("weit oestlich")         << 15.0  << 60.0;
        QTest::newRow("weit westlich")         << 15.0  << -60.0;
    }

    void projectAndBackAgain()
    {
        QFETCH(double, lat);
        QFETCH(double, lon);

        GlobeWidget g;
        prepare(g);

        QPointF px;
        QVERIFY2(g.projectForTest(lat, lon, px),
                 "der Ort muss auf der sichtbaren Halbkugel liegen");

        double gotLat = 0.0, gotLon = 0.0;
        QVERIFY(g.unproject(px, gotLat, gotLon));
        QVERIFY2(qAbs(gotLat - lat) < 0.05,
                 qPrintable(QStringLiteral("Breite %1 statt %2")
                                .arg(gotLat).arg(lat)));
        QVERIFY2(qAbs(gotLon - lon) < 0.05,
                 qPrintable(QStringLiteral("Laenge %1 statt %2")
                                .arg(gotLon).arg(lon)));
    }

    // Ausserhalb der Scheibe ist KEIN Ort. Wer dort etwas zurueckgibt,
    // laesst die Kugel beim Zoomen neben dem Rand irgendwohin springen.
    void outsideTheDiscIsNotAPlace()
    {
        GlobeWidget g;
        prepare(g);

        double lat = 0.0, lon = 0.0;
        QVERIFY2(!g.unproject(QPointF(2, 2), lat, lon),
                 "die Ecke des Widgets liegt neben der Kugel");
        QVERIFY2(!g.unproject(QPointF(398, 398), lat, lon),
                 "die andere Ecke auch");
    }

    // Die Mitte der Scheibe ist die Mitte der Ansicht — der einfachste
    // Fall, und der, der einen vertauschten Sinus sofort zeigt.
    void theCentreOfTheDiscIsTheCentreOfTheView()
    {
        GlobeWidget g;
        prepare(g);

        double lat = 0.0, lon = 0.0;
        QVERIFY(g.unproject(QPointF(200, 200), lat, lon));
        QVERIFY2(qAbs(lat - 20.0) < 0.05, "resetView() blickt auf 20 Grad Nord");
        QVERIFY2(qAbs(lon) < 0.05, "und auf den Nullmeridian");
    }

    // ── Die Zoom-Decke haengt an der Textur ──────────────────────────
    // Ohne Textur bleibt es bei 6x: mehr zeigt ein Gitternetz nicht.
    void withoutATextureTheCeilingStaysAtSix()
    {
        GlobeWidget g;
        prepare(g);
        QCOMPARE(g.maxZoom(), 6.0);
    }

    void zoomStopsAtTheCeiling()
    {
        GlobeWidget g;
        prepare(g);
        for (int i = 0; i < 40; ++i) { g.zoomBy(1.15); }
        QVERIFY2(g.zoom() <= g.maxZoom() + 1e-9,
                 "der Zoom darf die Decke nicht durchbrechen");
        QVERIFY2(g.zoom() > 5.0, "und er muss sie auch erreichen");
    }

    void zoomingOutStopsToo()
    {
        GlobeWidget g;
        prepare(g);
        for (int i = 0; i < 40; ++i) { g.zoomBy(1.0 / 1.15); }
        QVERIFY2(g.zoom() >= 0.6 - 1e-9,
                 "herausgezoomt bleibt die Kugel sichtbar");
    }

    void resetViewComesBackToOne()
    {
        GlobeWidget g;
        prepare(g);
        g.zoomBy(3.0);
        QVERIFY(g.zoom() > 1.0);
        g.resetView();
        QCOMPARE(g.zoom(), 1.0);
    }
};

QTEST_MAIN(TestGlobeUnproject)
#include "tst_globe_unproject.moc"
