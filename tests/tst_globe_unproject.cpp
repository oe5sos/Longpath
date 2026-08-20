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
#include <QKeyEvent>
#include <QMouseEvent>

#include "gui/widgets/GlobeWidget.h"

using namespace Longpath;

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

    // ── Hinfliegen ───────────────────────────────────────────────────
    //
    // Doppelklick AUF die Kugel fliegt dorthin, NEBEN die Kugel setzt
    // zurueck. Beides ist gewollt: „naeher heran" ist die haeufige
    // Geste, „zurueck" die wichtige, und sie brauchen nicht dieselbe
    // Flaeche.

    void flyToAimsAtThePlaceAndZoomsIn()
    {
        GlobeWidget g;
        prepare(g);
        const double before = g.targetZoomForTest();

        g.flyTo(48.0, 16.0);
        QVERIFY2(g.targetZoomForTest() > before,
                 "hinfliegen heisst auch naeher kommen");
    }

    void flyToRespectsTheCeiling()
    {
        GlobeWidget g;
        prepare(g);
        for (int i = 0; i < 30; ++i) { g.flyTo(0.0, 0.0, 1.8); }
        QVERIFY2(g.targetZoomForTest() <= g.maxZoom() + 1e-9,
                 "auch der Flug bricht die Decke nicht");
    }

    void doubleClickOnTheGlobeFliesThere()
    {
        GlobeWidget g;
        prepare(g);
        const double before = g.targetZoomForTest();

        QMouseEvent ev(QEvent::MouseButtonDblClick, QPointF(230, 180),
                       g.mapToGlobal(QPointF(230, 180)),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&g, &ev);

        QVERIFY2(g.targetZoomForTest() > before,
                 "ein Doppelklick auf der Kugel holt naeher heran");
    }

    // Bei Zoom 1 liegt die Ecke NEBEN der Scheibe (Radius 0,44 der
    // kleineren Kante), dort setzt der Doppelklick zurueck.
    void doubleClickBesideTheGlobeResets()
    {
        GlobeWidget g;
        prepare(g);
        g.flyTo(48.0, 16.0);          // Kamera und Zoomziel verstellen
        QVERIFY(g.targetZoomForTest() > 1.0);

        QMouseEvent ev(QEvent::MouseButtonDblClick, QPointF(3, 3),
                       g.mapToGlobal(QPointF(3, 3)),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&g, &ev);

        QCOMPARE(g.zoom(), 1.0);
        QCOMPARE(g.targetZoomForTest(), 1.0);
    }

    // WICHTIGER FUND, und er kam aus einem falschen Test von mir: ab
    // etwa 1,15x fuellt die Scheibe das ganze Fenster. „Neben der Kugel"
    // gibt es dann NICHT MEHR — der Doppelklick landet ueberall auf der
    // Kugel und fliegt, statt zurueckzusetzen.
    //
    // Der Rueckweg darf also nicht allein an dieser Geste haengen. Er
    // liegt zusaetzlich auf der Taste 0 und im Rechtsklick-Menue, und
    // dieser Fall haelt fest, WARUM.
    void whenZoomedInThereIsNoBesideAnyMore()
    {
        GlobeWidget g;
        prepare(g);
        g.zoomBy(3.0);

        double lat = 0.0, lon = 0.0;
        QVERIFY2(g.unproject(QPointF(3, 3), lat, lon),
                 "bei Zoom 3 ist selbst die Ecke noch auf der Kugel");

        // Also bleibt der Rueckweg ueber die Taste.
        QKeyEvent zero(QEvent::KeyPress, Qt::Key_0, Qt::NoModifier);
        QCoreApplication::sendEvent(&g, &zero);
        QCOMPARE(g.zoom(), 1.0);
    }

    // ── Tastatur ─────────────────────────────────────────────────────

    void plusAndMinusZoom()
    {
        GlobeWidget g;
        prepare(g);

        QKeyEvent plus(QEvent::KeyPress, Qt::Key_Plus, Qt::NoModifier);
        QCoreApplication::sendEvent(&g, &plus);
        QVERIFY2(g.zoom() > 1.0, "Plus holt naeher");

        QKeyEvent minus(QEvent::KeyPress, Qt::Key_Minus, Qt::NoModifier);
        QCoreApplication::sendEvent(&g, &minus);
        QCoreApplication::sendEvent(&g, &minus);
        QVERIFY2(g.zoom() < 1.0, "Minus holt weiter weg");
    }

    void zeroGoesBack()
    {
        GlobeWidget g;
        prepare(g);
        g.zoomBy(4.0);
        QKeyEvent zero(QEvent::KeyPress, Qt::Key_0, Qt::NoModifier);
        QCoreApplication::sendEvent(&g, &zero);
        QCOMPARE(g.zoom(), 1.0);
    }

    // Ohne gesetztes Ziel tut die Taste absichtlich nichts: eine Taste,
    // die irgendwohin fliegt, ist schlimmer als eine, die schweigt.
    void theTargetKeyIsSilentWithoutATarget()
    {
        GlobeWidget g;
        prepare(g);
        const double before = g.targetZoomForTest();

        QKeyEvent t(QEvent::KeyPress, Qt::Key_T, Qt::NoModifier);
        QCoreApplication::sendEvent(&g, &t);
        QCOMPARE(g.targetZoomForTest(), before);
    }

    void theTargetKeyFliesToTheStation()
    {
        GlobeWidget g;
        prepare(g);
        g.setHome(48.2, 16.4);
        g.setTarget(-33.9, 151.2);           // Sydney
        const double before = g.targetZoomForTest();

        QKeyEvent t(QEvent::KeyPress, Qt::Key_T, Qt::NoModifier);
        QCoreApplication::sendEvent(&g, &t);
        QVERIFY2(g.targetZoomForTest() > before,
                 "mit gesetztem Ziel fliegt T dorthin");
    }

    // ── Die gemalten Knoepfe ─────────────────────────────────────────
    //
    // „es sollte in der Grafik auch ein Plus und Minus zum Vergroessern
    // sein" (Betreiber, 2026-08-19). Gemalt statt als Kind-Widget, damit
    // sie beim Ziehen nicht die Mausereignisse abfangen — also gehoert
    // die Trefferpruefung geprueft.

    void theDrawnPlusButtonZooms()
    {
        GlobeWidget g;
        prepare(g);
        const double before = g.zoom();

        // Unten rechts, oberer der beiden Knoepfe.
        const QPointF plus(400 - 8 - 11, 400 - 8 - 22 - 4 - 11);
        QMouseEvent ev(QEvent::MouseButtonPress, plus, g.mapToGlobal(plus),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&g, &ev);

        QVERIFY2(g.zoom() > before, "Plus muss vergroessern");
    }

    void theDrawnMinusButtonZoomsOut()
    {
        GlobeWidget g;
        prepare(g);
        g.zoomBy(2.0);
        const double before = g.zoom();

        const QPointF minus(400 - 8 - 11, 400 - 8 - 11);
        QMouseEvent ev(QEvent::MouseButtonPress, minus, g.mapToGlobal(minus),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&g, &ev);

        QVERIFY2(g.zoom() < before, "Minus muss verkleinern");
    }

    // Ein Druck auf einen Knopf darf die Kugel NICHT mitdrehen — sonst
    // rutscht die Ansicht bei jedem Zoomklick weg.
    void pressingAButtonDoesNotStartADrag()
    {
        GlobeWidget g;
        prepare(g);
        double lat0 = 0.0, lon0 = 0.0;
        g.viewForTest(lat0, lon0);

        const QPointF plus(400 - 8 - 11, 400 - 8 - 22 - 4 - 11);
        QMouseEvent press(QEvent::MouseButtonPress, plus, g.mapToGlobal(plus),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&g, &press);

        const QPointF away(200, 120);
        QMouseEvent move(QEvent::MouseMove, away, g.mapToGlobal(away),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&g, &move);

        double lat1 = 0.0, lon1 = 0.0;
        g.viewForTest(lat1, lon1);
        QCOMPARE(lat1, lat0);
        QCOMPARE(lon1, lon0);
    }
};

QTEST_MAIN(TestGlobeUnproject)
#include "tst_globe_unproject.moc"
