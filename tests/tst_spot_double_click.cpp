// =================================================================
// tests/tst_spot_double_click.cpp  (Longpath)
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
// Modification history (Longpath):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalSpy>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

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

void pressReleaseAt(SpectrumWidget& sw, const QPoint& pos)
{
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(pos),
                      sw.mapToGlobal(QPointF(pos)),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&sw, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(pos),
                        sw.mapToGlobal(QPointF(pos)),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&sw, &release);
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

    // Am TS-590 (und an jedem Geraet ohne CTUN) folgt das Bild der
    // Abstimmung: der erste Klick des Doppelklicks stimmt auf den Spot ab,
    // das Bild zentriert neu, und unter der Maus liegt beim zweiten Klick
    // etwas anderes. Hier ein ANDERER Spot -- der falsche waere schlimmer
    // als keiner. Martin am 12.11.: „hat nicht funktioniert".
    // Qt liefert dem Widget Druck, Loslassen, Doppelklick (der zweite
    // Druck geht nicht durch, QTBUG-25831) -- genau das wird hier gespielt.
    void whenThePictureFollowsTheTuningTheFirstClicksSpotCounts()
    {
        SpectrumWidget sw;
        prepare(sw);
        // 14,250 Mitte, 96 kHz auf 800 px: DL1ABC (14,230) steht bei
        // x≈233. Nach dem Neuzentrieren auf 14,230 steht dort 14,210 --
        // und genau da liegt G0XYZ.
        sw.setSpotMarkers({
            spot(1, QStringLiteral("DL1ABC"), 14.230),
            spot(2, QStringLiteral("G0XYZ"),  14.210),
        });
        const QRect specRect(0, 0, 800, 200);
        renderSpots(sw, specRect);

        QRect dl;
        for (const auto& hr : sw.spotClickRectsForTest()) {
            if (qAbs(hr.freqMhz - 14.230) < 0.0005) { dl = hr.rect; }
        }
        QVERIFY(!dl.isNull());
        const QPoint maus = dl.center();

        // Das Bild folgt der Abstimmung, wie am 590.
        QList<double> abgestimmt;
        connect(&sw, &SpectrumWidget::frequencyClicked, &sw,
                [&](double hz) {
            abgestimmt << hz;
            sw.setFrequencyRange(hz, 96'000.0);
            renderSpots(sw, specRect);
        });

        QSignalSpy spy(&sw, &SpectrumWidget::spotLogRequested);
        pressReleaseAt(sw, maus);
        QCOMPARE(abgestimmt.size(), 1);

        // Voraussetzung: unter der Maus liegt jetzt wirklich G0XYZ.
        QString darunter;
        for (const auto& hr : sw.spotClickRectsForTest()) {
            if (hr.rect.contains(maus)) {
                darunter = qAbs(hr.freqMhz - 14.210) < 0.0005
                    ? QStringLiteral("G0XYZ") : QStringLiteral("anderer");
            }
        }
        QCOMPARE(darunter, QStringLiteral("G0XYZ"));

        doubleClickAt(sw, maus);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QStringLiteral("DL1ABC"));
        // Die Scheibe steht am Ende auf DL1ABC, nicht auf G0XYZ.
        QVERIFY(!abgestimmt.isEmpty());
        QCOMPARE(qRound64(abgestimmt.last()), qint64(14'230'000));
    }

    // Ein einzelner Klick auf einen Spot, und viel spaeter ein
    // Doppelklick woanders: der alte Spot darf nicht mitkommen.
    void anOldSpotClickElsewhereDoesNotCount()
    {
        SpectrumWidget sw;
        prepare(sw);
        sw.setSpotMarkers({spot(1, QStringLiteral("DL1ABC"), 14.230)});
        const QRect specRect(0, 0, 800, 200);
        renderSpots(sw, specRect);
        const auto rects = sw.spotClickRectsForTest();
        QCOMPARE(rects.size(), 1);

        pressReleaseAt(sw, rects.first().rect.center());
        QSignalSpy spy(&sw, &SpectrumWidget::spotLogRequested);
        doubleClickAt(sw, QPoint(700, 180));   // weit weg, leere Flaeche
        QCOMPARE(spy.count(), 0);
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
