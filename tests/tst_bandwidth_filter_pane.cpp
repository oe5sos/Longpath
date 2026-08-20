// =================================================================
// tests/tst_bandwidth_filter_pane.cpp  (NereusSDR)
// =================================================================
//
// Die Durchlassflaeche auf echter Frequenzachse.
//
// Vorlage: Zeus Link „BANDWIDTH FILTER" (Bildschirmfoto des
// Betreibers, 2026-08-20). Der Gewinn gegenueber dem alten
// FilterPassbandWidget ist die ACHSE: dort ein festes Trapez ohne
// Frequenzbezug, hier eine Skala, auf der 13.137 MHz steht.
//
// Geprueft wird die Rechnung und die Aufteilung in Ziehbereiche —
// nicht das Aussehen. Ein Test, der Pixel vergleicht, geht bei jeder
// Farbaenderung kaputt und sagt nichts.
//
// DER FALL, DER STILL FALSCH WIRD, ist der mittlere Ziehbereich. Er
// muss die MITTE melden und nicht zwei Kanten — nur so kann das Modell
// beim Anstossen die Breite erhalten. Meldete er Kanten, wuerde ein
// verschobener Durchlass am Rand heimlich schmaler.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QSignalSpy>

#include "gui/widgets/BandwidthFilterPane.h"

using namespace Longpath;

namespace {

// Eine Maustaste an Bildpunkt x druecken, ziehen, loslassen.
void dragFrom(QWidget* w, int x0, int x1)
{
    const int y = w->height() / 2;
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(x0, y),
                      w->mapToGlobal(QPointF(x0, y)),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &press);

    QMouseEvent move(QEvent::MouseMove, QPointF(x1, y),
                     w->mapToGlobal(QPointF(x1, y)),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &move);

    QMouseEvent rel(QEvent::MouseButtonRelease, QPointF(x1, y),
                    w->mapToGlobal(QPointF(x1, y)),
                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(w, &rel);
}

} // namespace

class TestBandwidthFilterPane : public QObject
{
    Q_OBJECT

private:
    // Eine Flaeche mit runden Massen, damit sich Bildpunkte in Hertz
    // im Kopf nachrechnen lassen: 1000 breit minus 2×6 Rand = 988
    // nutzbar fuer 10 000 Hz.
    BandwidthFilterPane* makePane()
    {
        auto* p = new BandwidthFilterPane;
        p->resize(1000, 160);
        p->setSpan(10000);
        p->setVfoFrequency(7'131'300.0);
        p->setHasFrequency(true);
        p->setFilter(-2850, -150);
        return p;
    }

private slots:

    void itKeepsWhatItIsGiven()
    {
        QScopedPointer<BandwidthFilterPane> p(makePane());
        QCOMPARE(p->filterLow(),  -2850);
        QCOMPARE(p->filterHigh(), -150);
        QCOMPARE(p->span(), 10000);
    }

    void theSpanStaysWithinReason()
    {
        QScopedPointer<BandwidthFilterPane> p(makePane());
        p->setSpan(100);
        QVERIFY2(p->span() >= 2000,
                 "unter 2 kHz wird die Achse unlesbar");
        p->setSpan(500000);
        QVERIFY2(p->span() <= 40000,
                 "ueber 40 kHz ist der Durchlass nur noch ein Strich");
    }

    // ── Die drei Ziehbereiche ────────────────────────────────────────

    void draggingAnEdgeChangesOnlyThatEdge()
    {
        QScopedPointer<BandwidthFilterPane> p(makePane());
        QSignalSpy edges(p.data(), &BandwidthFilterPane::filterChanged);
        QSignalSpy centre(p.data(), &BandwidthFilterPane::filterCentreChanged);

        // Die untere Kante liegt bei −2850 Hz. In Bildpunkten:
        // (−2850 + 5000)/10000 · 988 + 6 ≈ 218.
        dragFrom(p.data(), 218, 268);

        QVERIFY2(edges.count() >= 1, "das Ziehen muss etwas melden");
        QCOMPARE(centre.count(), 0);

        const QList<QVariant> a = edges.last();
        QVERIFY2(a.at(1).toInt() == -150,
                 "die andere Kante darf sich beim Kantenziehen nicht "
                 "bewegen");
        QVERIFY2(a.at(0).toInt() > -2850,
                 "die gezogene Kante muss nach rechts gewandert sein");
    }

    // DER FALL, DER STILL FALSCH WIRD.
    void draggingTheBodyReportsACentreAndNotTwoEdges()
    {
        QScopedPointer<BandwidthFilterPane> p(makePane());
        QSignalSpy edges(p.data(), &BandwidthFilterPane::filterChanged);
        QSignalSpy centre(p.data(), &BandwidthFilterPane::filterCentreChanged);

        // Mitte des Durchlasses: (−2850 + −150)/2 = −1500 Hz ≈ x 352.
        dragFrom(p.data(), 352, 402);

        QVERIFY2(centre.count() >= 1,
                 "der mittlere Bereich muss die MITTE melden — nur so "
                 "kann das Modell die Breite am Rand erhalten");
        QVERIFY2(edges.count() == 0,
                 "meldete er Kanten, wuerde ein verschobener Durchlass "
                 "am Rand heimlich schmaler");
        QVERIFY2(centre.last().at(0).toInt() > -1500,
                 "die Mitte muss nach rechts gewandert sein");
    }

    void draggingOutsideThePassbandDoesNothing()
    {
        QScopedPointer<BandwidthFilterPane> p(makePane());
        QSignalSpy edges(p.data(), &BandwidthFilterPane::filterChanged);
        QSignalSpy centre(p.data(), &BandwidthFilterPane::filterCentreChanged);

        // Weit rechts vom Durchlass, bei etwa +3 kHz.
        dragFrom(p.data(), 800, 850);

        QCOMPARE(edges.count(),  0);
        QCOMPARE(centre.count(), 0);
    }

    // Die Kanten duerfen einander nicht ueberholen — alles andere
    // entscheidet das Modell, aber ein negativer Durchlass waere schon
    // hier Unsinn.
    void theEdgesDoNotOvertakeEachOther()
    {
        QScopedPointer<BandwidthFilterPane> p(makePane());
        QSignalSpy edges(p.data(), &BandwidthFilterPane::filterChanged);

        // Untere Kante weit nach rechts, ueber die obere hinaus.
        dragFrom(p.data(), 218, 700);

        QVERIFY(edges.count() >= 1);
        const QList<QVariant> a = edges.last();
        QVERIFY2(a.at(0).toInt() < a.at(1).toInt(),
                 "die untere Kante darf die obere nicht ueberholen");
    }

    // Ohne Verbindung steht keine Frequenz auf der Achse. Eine
    // erfundene waere eine Behauptung — dieselbe Regel wie beim
    // Panadapter-Kopf und beim Rotorzeiger.
    void withoutARadioTheAxisStaysEmpty()
    {
        QScopedPointer<BandwidthFilterPane> p(makePane());
        p->setHasFrequency(false);

        // Geprueft wird der Zustand, nicht das Bild: die Flaeche darf
        // sich malen lassen, ohne zu behaupten, sie kenne eine
        // Frequenz.
        QImage img(p->size(), QImage::Format_ARGB32);
        img.fill(Qt::black);
        p->render(&img);
        QVERIFY2(!img.isNull(), "sie muss sich auch ohne Radio malen lassen");
    }
};

QTEST_MAIN(TestBandwidthFilterPane)
#include "tst_bandwidth_filter_pane.moc"
