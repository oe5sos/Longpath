// =================================================================
// tests/tst_instrument_spine.cpp  (NereusSDR)
// =================================================================
//
// Die Geometrie der Instrumente — der Teil, den man rechnen kann.
//
// Wie eine Mulde AUSSIEHT, kann kein Test beurteilen; dafür ist der
// Blick des Betreibers da. Was ein Test kann, ist die Abbildung
// Anteil → Ort festhalten, und die ist der Grund, warum es überhaupt
// zwei Geometrien und einen gemeinsamen Zeichenweg gibt: laufen Bogen
// und Gerade hier auseinander, zeigen zwei Instrumente denselben Wert
// an verschiedenen Stellen, und niemand sieht, dass es dieselbe Zahl
// ist.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>

#include "gui/instruments/InstrumentSpine.h"

using namespace Longpath;

namespace {
constexpr double kEps = 1e-6;
bool near(double a, double b) { return qAbs(a - b) < kEps; }
}

class TestInstrumentSpine : public QObject
{
    Q_OBJECT

private slots:

    // ── Gerade ───────────────────────────────────────────────────────

    void linearMapsFractionsToTheTroughEnds()
    {
        LinearSpine s(QRectF(10.0, 20.0, 300.0, 14.0));
        QVERIFY(near(s.xAt(0.0),  10.0));
        QVERIFY(near(s.xAt(1.0), 310.0));
        QVERIFY(near(s.xAt(0.5), 160.0));
    }

    void linearClampsOutsideTheRange()
    {
        LinearSpine s(QRectF(0.0, 0.0, 100.0, 10.0));
        QVERIFY(near(s.xAt(-3.0),   0.0));
        QVERIFY(near(s.xAt( 4.0), 100.0));
    }

    void linearFillGrowsWithTheValue()
    {
        LinearSpine s(QRectF(0.0, 0.0, 100.0, 10.0));
        QVERIFY(s.fillArea(0.0).boundingRect().width() < kEps);
        const double half = s.fillArea(0.5).boundingRect().width();
        const double full = s.fillArea(1.0).boundingRect().width();
        QVERIFY(half > 0.0);
        QVERIFY(full > half);
    }

    // Der Aufrufer soll die Reihenfolge nicht sortieren müssen — die
    // Schwellenmarke kommt mal als (a,1), mal als (1,a).
    void linearSpanIsOrderIndependent()
    {
        LinearSpine s(QRectF(0.0, 0.0, 100.0, 10.0));
        const QRectF ab = s.troughSpan(0.3, 0.8).boundingRect();
        const QRectF ba = s.troughSpan(0.8, 0.3).boundingRect();
        QVERIFY(near(ab.left(),  ba.left()));
        QVERIFY(near(ab.right(), ba.right()));
    }

    void linearShadowSitsOnTheUpperEdge()
    {
        LinearSpine s(QRectF(0.0, 50.0, 100.0, 20.0));
        const QRectF sh = s.shadowPath().boundingRect();
        QVERIFY2(near(sh.top(), 50.0),
                 "der Innenschatten gehoert an die OBERE Kante");
        QVERIFY(sh.height() <= s.shadowWidth() + kEps);
    }

    void linearCrossReachesBeyondTheTrough()
    {
        LinearSpine s(QRectF(0.0, 50.0, 100.0, 20.0));
        const QLineF l = s.crossAt(0.5, 4.0, 4.0);
        QVERIFY(near(l.x1(), 50.0));
        QVERIFY(near(l.x2(), 50.0));
        QVERIFY(near(qMin(l.y1(), l.y2()), 46.0));
        QVERIFY(near(qMax(l.y1(), l.y2()), 74.0));
    }

    // ── Bogen ────────────────────────────────────────────────────────

    void arcMapsFractionsToTheSweepEnds()
    {
        // Wie im Entwurf: 168 Grad links, 12 Grad rechts, ueber oben.
        ArcSpine s(QPointF(260.0, 168.0), 148.0, 13.0, 168.0, 12.0);
        QVERIFY(near(s.degreeAt(0.0), 168.0));
        QVERIFY(near(s.degreeAt(1.0),  12.0));
        QVERIFY(near(s.degreeAt(0.5),  90.0));
    }

    void arcClampsOutsideTheRange()
    {
        ArcSpine s(QPointF(0.0, 0.0), 100.0, 10.0, 180.0, 0.0);
        QVERIFY(near(s.degreeAt(-1.0), 180.0));
        QVERIFY(near(s.degreeAt( 2.0),   0.0));
    }

    // Der Bildschirm zaehlt y nach unten, die Entwuerfe nach oben. Ein
    // Vorzeichenfehler hier stellt jedes Zeigerinstrument auf den Kopf,
    // und zwar so, dass es auf den ersten Blick plausibel aussieht.
    void arcPointsFollowScreenCoordinates()
    {
        ArcSpine s(QPointF(100.0, 100.0), 50.0, 10.0, 180.0, 0.0);
        const QPointF right = s.pointAt(50.0, 0.0);
        QVERIFY(near(right.x(), 150.0));
        QVERIFY(near(right.y(), 100.0));

        const QPointF top = s.pointAt(50.0, 90.0);
        QVERIFY(near(top.x(), 100.0));
        QVERIFY2(near(top.y(), 50.0),
                 "90 Grad muss NACH OBEN zeigen, also kleineres y");

        const QPointF left = s.pointAt(50.0, 180.0);
        QVERIFY(near(left.x(), 50.0));
        QVERIFY(near(left.y(), 100.0));
    }

    void arcFillGrowsWithTheValue()
    {
        ArcSpine s(QPointF(260.0, 168.0), 148.0, 13.0, 168.0, 12.0);
        const double quarter = s.fillArea(0.25).boundingRect().width();
        const double full    = s.fillArea(1.00).boundingRect().width();
        QVERIFY(quarter > 0.0);
        QVERIFY(full > quarter);
    }

    // Die Glut soll „ueber gut die halbe Hoehe reichen und dort
    // verschwinden" — nicht als voller Kreis unter das Instrument
    // laufen. Die Beschneidung ist das, was das verhindert.
    void arcGlowIsClippedToTheUpperSector()
    {
        ArcSpine s(QPointF(100.0, 100.0), 100.0, 10.0, 180.0, 0.0);
        const QPainterPath clip = s.glowClip();
        QVERIFY(!clip.isEmpty());
        QVERIFY2(clip.boundingRect().bottom() <= 100.0 + kEps,
                 "die Glut reicht unter den Drehpunkt");
        // Und sie bleibt innerhalb der von glowBounds angekuendigten
        // Flaeche, sonst zeichnete der Painter ausserhalb.
        QVERIFY(s.glowBounds().contains(clip.boundingRect()));
    }

    void arcCrossStraddlesTheTrough()
    {
        ArcSpine s(QPointF(100.0, 100.0), 50.0, 10.0, 180.0, 0.0);
        // Bei 90 Grad steht die Querlinie senkrecht ueber dem
        // Drehpunkt: aussen weiter oben, innen weiter unten.
        const QLineF l = s.crossAt(0.5, 4.0, 4.0);
        QVERIFY(near(l.x1(), 100.0));
        QVERIFY(near(l.x2(), 100.0));
        QVERIFY(near(l.y1(), 100.0 - (50.0 + 5.0 + 4.0)));
        QVERIFY(near(l.y2(), 100.0 - (50.0 - 5.0 - 4.0)));
    }

    // ── Beide ────────────────────────────────────────────────────────
    //
    // Dieselbe Zusicherung fuer beide Formen: der Anteil 0 liegt am
    // Anfang, 1 am Ende, und dazwischen waechst es. Was hier gleich
    // ist, ist der Grund, warum InstrumentPainter nur Spine kennt.
    void bothGeometriesAgreeOnWhatAFractionMeans()
    {
        LinearSpine lin(QRectF(0.0, 0.0, 200.0, 12.0));
        ArcSpine    arc(QPointF(100.0, 150.0), 120.0, 12.0, 170.0, 10.0);

        double lastLin = -1.0;
        double lastArc = 1e9;
        for (int i = 0; i <= 10; ++i) {
            const double f = i / 10.0;
            const double x = lin.xAt(f);
            const double d = arc.degreeAt(f);
            QVERIFY2(x > lastLin, "die Gerade laeuft nicht monoton");
            QVERIFY2(d < lastArc, "der Bogen laeuft nicht monoton");
            lastLin = x;
            lastArc = d;
        }
    }

    // ── Der Massstab ─────────────────────────────────────────────────
    //
    // Der Fehler vom 2026-08-18: Radius und Drehpunkt wuchsen mit der
    // Flaeche, Strichstaerken und Nabe nicht. Auf dem Schirm sah der
    // Zeiger dann aus wie „ein duenner gerader Strich", weil er bei
    // halber Instrumentengroesse noch immer 2,2 Pixel breit war.
    //
    // Der Test nagelt fest, WORAN die Strichstaerken haengen: an der
    // Rille, nicht am Radius. Ohne das bekaeme der sehr flache
    // VFO-Bogen (R gross, Rille schmal) fingerdicke Striche.

    void theUnitFollowsTheTroughNotTheRadius()
    {
        // Der Entwurf selbst: Radius 148, Rille 13 → unit == Radius.
        ArcSpine design(QPointF(260.0, 168.0), 148.0, 13.0, 168.0, 12.0);
        QVERIFY(qAbs(design.unit() - 148.0) < 1e-3);

        // Dasselbe Instrument halb so gross: alles halbiert sich mit.
        ArcSpine half(QPointF(130.0, 84.0), 74.0, 6.5, 168.0, 12.0);
        QVERIFY(qAbs(half.unit() - 74.0) < 1e-3);

        // Der flache VFO-Bogen: sehr grosser Radius, schmale Rille. Der
        // Massstab bleibt in der Groessenordnung des Entwurfs — haetten
        // wir den Radius genommen, waere er hier fuenfmal so gross.
        ArcSpine flat(QPointF(260.0, 800.0), 700.0, 12.0, 100.6, 79.4);
        QVERIFY2(flat.unit() < 200.0,
                 "der flache Bogen bekaeme Striche nach Radiusmass");
    }

    void everyMeasureScalesWithTheInstrument()
    {
        ArcSpine big(QPointF(260.0, 168.0), 148.0, 13.0, 168.0, 12.0);
        ArcSpine small(QPointF(130.0, 84.0), 74.0, 6.5, 168.0, 12.0);

        // Halbe Groesse, halbe Masse — ausnahmslos.
        QVERIFY(near(big.shadowWidth(), 2.0 * small.shadowWidth()));
        QVERIFY(near(big.unit(),        2.0 * small.unit()));

        // Und der Teilstrich sitzt in beiden am selben relativen Ort.
        const QLineF lb = big.tick(0.5,   big.unit() * 4.0 / 148.0);
        const QLineF ls = small.tick(0.5, small.unit() * 4.0 / 148.0);
        QVERIFY(near(lb.length(), 2.0 * ls.length()));
    }

    void theShadowSitsFlushWithTheOuterEdge()
    {
        // Die dunkle Kante liegt buendig auf der Aussenkante der Rille,
        // nicht darueber hinaus — sonst franst die Mulde aus.
        ArcSpine s(QPointF(0.0, 0.0), 100.0, 10.0, 180.0, 0.0);
        const double outer = 100.0 + 10.0 / 2.0;
        const QRectF sh = s.shadowPath().boundingRect();
        QVERIFY(sh.width() / 2.0 <= outer + kEps);
        QVERIFY(sh.width() / 2.0 >= outer - s.shadowWidth() - kEps);
    }
};

QTEST_MAIN(TestInstrumentSpine)
#include "tst_instrument_spine.moc"
