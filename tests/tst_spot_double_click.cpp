// =================================================================
// tests/tst_spot_double_click.cpp  (NereusSDR)
// =================================================================
//
// Doppelklick auf ein gespottetes Rufzeichen im Panadapter.
//
// Auf Ansage des Betreibers (2026-08-19): „wenn ein Call gespottet ist
// und ich sehe es am Panadapter, soll es mit Doppelklick sofort ins
// Logbuch übernommen werden … sowie der Zeiger in Zielposition vom
// Call."
//
// Geprueft wird hier die PANADAPTER-Seite: dass der Doppelklick auf
// einem Etikett das richtige Rufzeichen meldet, auf freier Flaeche
// nichts meldet, und dass eine Verlaufsmarke des S-Verlaufs
// ausgenommen bleibt — ihr Etikett ist eine S-Stufe, kein Rufzeichen.
//
// Was danach passiert (Log auf, Zeiger auf Zielpeilung, Entfernung)
// haengt an RotorLogbookPanel und braucht QRZ-Zugangsdaten oder
// cty.dat; das steht in der Bank-Matrix.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalSpy>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

namespace {

// Die Klickfelder entstehen erst beim Malen — derselbe Weg wie in
// tst_spot_overlay_render.
void renderSpots(SpectrumWidget& sw, const QRect& specRect)
{
    QImage img(specRect.size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QPainter p(&img);
    p.translate(-specRect.topLeft());
    sw.drawSpotMarkersForTest(p, specRect);
    p.end();
}

SpectrumWidget::SpotMarker spot(int idx, const QString& call, double freqMhz)
{
    SpectrumWidget::SpotMarker m;
    m.index    = idx;
    m.callsign = call;
    m.freqMhz  = freqMhz;
    m.source   = QStringLiteral("DXCluster");
    return m;
}

void doubleClickAt(SpectrumWidget& sw, const QPoint& pos)
{
    QMouseEvent ev(QEvent::MouseButtonDblClick, QPointF(pos),
                   sw.mapToGlobal(QPointF(pos)),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&sw, &ev);
}

} // namespace

class TestSpotDoubleClick : public QObject
{
    Q_OBJECT

private:
    static void prepare(SpectrumWidget& sw)
    {
        sw.resize(800, 400);
        sw.setFrequencyRange(14'250'000.0, 96'000.0);
        sw.setShowSpots(true);
    }

private slots:

    void doubleClickOnASpotLabelReportsTheCallsign()
    {
        SpectrumWidget sw;
        prepare(sw);
        sw.setSpotMarkers({spot(1, QStringLiteral("DL1ABC"), 14.250)});

        const QRect specRect(0, 0, 800, 200);
        renderSpots(sw, specRect);
        const auto rects = sw.spotClickRectsForTest();
        QCOMPARE(rects.size(), 1);

        QSignalSpy spy(&sw, &SpectrumWidget::spotLogRequested);
        doubleClickAt(sw, rects.first().rect.center());

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QStringLiteral("DL1ABC"));
    }

    // Auf freier Flaeche darf nichts gemeldet werden — sonst landete
    // beim Zoomen per Doppelklick ein leeres Rufzeichen im Log.
    void doubleClickOnEmptySpectrumReportsNothing()
    {
        SpectrumWidget sw;
        prepare(sw);
        sw.setSpotMarkers({spot(1, QStringLiteral("DL1ABC"), 14.250)});

        const QRect specRect(0, 0, 800, 200);
        renderSpots(sw, specRect);

        QSignalSpy spy(&sw, &SpectrumWidget::spotLogRequested);
        doubleClickAt(sw, QPoint(20, 180));   // weit weg vom Etikett
        QCOMPARE(spy.count(), 0);
    }

    // Der richtige von zwei Spots, nicht einfach der erste.
    void theRightSpotOfSeveralIsReported()
    {
        SpectrumWidget sw;
        prepare(sw);
        sw.setSpotMarkers({
            spot(1, QStringLiteral("DL1ABC"), 14.230),
            spot(2, QStringLiteral("G0XYZ"),  14.270),
        });

        const QRect specRect(0, 0, 800, 200);
        renderSpots(sw, specRect);
        const auto rects = sw.spotClickRectsForTest();
        QCOMPARE(rects.size(), 2);

        // Das Klickfeld zu G0XYZ finden, nicht auf die Reihenfolge
        // vertrauen: die Kollisionsstapelung darf sie aendern.
        QRect target;
        for (const auto& hr : rects) {
            if (qAbs(hr.freqMhz - 14.270) < 0.0005) { target = hr.rect; }
        }
        QVERIFY(!target.isNull());

        QSignalSpy spy(&sw, &SpectrumWidget::spotLogRequested);
        doubleClickAt(sw, target.center());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QStringLiteral("G0XYZ"));
    }

    // Eine Verlaufsmarke traegt „S7" als Etikett. Das ist kein
    // Rufzeichen und gehoert nicht ins Logbuch.
    void doubleClickOnAHistoryMarkerReportsNothing()
    {
        SpectrumWidget sw;
        prepare(sw);
        sw.setSpotMarkers({});
        sw.setShowSignalHistory(true);

        SpectrumWidget::SpotMarker sh;
        sh.callsign = QStringLiteral("S7");
        sh.freqMhz  = 14.250;
        sh.source   = QStringLiteral("SHistory");
        sw.setSignalHistoryMarkers({sh});

        const QRect specRect(0, 0, 800, 200);
        renderSpots(sw, specRect);
        const auto rects = sw.spotClickRectsForTest();
        QCOMPARE(rects.size(), 1);

        QSignalSpy spy(&sw, &SpectrumWidget::spotLogRequested);
        doubleClickAt(sw, rects.first().rect.center());
        QVERIFY2(spy.count() == 0,
                 "eine S-Stufe ist kein Rufzeichen");
    }

    // Sind die Spots ausgeblendet, gibt es nichts zu treffen — auch
    // wenn die Klickfelder aus einem frueheren Malvorgang noch liegen.
    void hiddenSpotsReportNothing()
    {
        SpectrumWidget sw;
        prepare(sw);
        sw.setSpotMarkers({spot(1, QStringLiteral("DL1ABC"), 14.250)});

        const QRect specRect(0, 0, 800, 200);
        renderSpots(sw, specRect);
        const auto rects = sw.spotClickRectsForTest();
        QCOMPARE(rects.size(), 1);

        sw.setShowSpots(false);
        QSignalSpy spy(&sw, &SpectrumWidget::spotLogRequested);
        doubleClickAt(sw, rects.first().rect.center());
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestSpotDoubleClick)
#include "tst_spot_double_click.moc"
