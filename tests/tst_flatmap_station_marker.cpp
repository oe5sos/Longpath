// tests/tst_flatmap_station_marker.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── Der eigene Standort als Bild ─────────────────────────────────────
//
// Vorlage des Betreibers, 2026-08-15: bei Zeus ist der eigene Standort
// auf der Weltkarte kein Punkt und keine Nadel, sondern ein rundes Foto
// der Station mit Ring und Rufzeichen darunter.
//
// Ob das huebsch aussieht, kann kein Test sagen. Drei Dinge kann er:
//
//   · Ein Querformat wird MITTIG BESCHNITTEN, nicht gestaucht. Ein
//     gestauchtes Gesicht faellt sofort auf, und der Unterschied
//     zwischen copy() und scaled() ist ein Tippfehler weit.
//
//   · Das Bild wird EINMAL geschnitten und behalten. Die Karte
//     zeichnet beim Ziehen dutzendfach pro Sekunde neu; ein volles
//     Foto pro Bild zu skalieren merkt man genau dann, wenn man die
//     Karte bewegt.
//
//   · Ohne Foto faellt der Marker auf den Punkt zurueck, statt zu
//     verschwinden. Ein Standort, der nur mit Bild sichtbar ist, waere
//     eine Falle fuer jeden ohne QRZ-Portrait.

#include <QtTest>

#include "gui/widgets/FlatMapWidget.h"

#include <QColor>
#include <QImage>
#include <QPainter>

using namespace Longpath;

namespace {

/// Ein Querformat, dessen Mitte sich vom Rand unterscheidet: aussen
/// blau, das mittlere Quadrat rot. Nach mittigem Beschnitt darf nur
/// Rot uebrig sein.
QImage wideImage()
{
    QImage img(300, 100, QImage::Format_ARGB32);
    img.fill(QColor(0, 0, 255));
    QPainter p(&img);
    p.fillRect(100, 0, 100, 100, QColor(255, 0, 0));
    p.end();
    return img;
}

} // namespace

class TestFlatMapStationMarker : public QObject
{
    Q_OBJECT

private slots:
    void withoutAPhotoThereIsNoPixmapButStillACallsign()
    {
        FlatMapWidget w;
        w.setStationMarker(QStringLiteral("oe5sos"), QImage{});
        QVERIFY2(w.stationMarkerPixmap().isNull(),
                 "ein leeres Bild hat trotzdem eine Pixmap erzeugt");
        // Das Rufzeichen bleibt — der Punkt traegt es dann statt des
        // Wortes „HOME".
        QCOMPARE(w.stationCallsign(), QStringLiteral("OE5SOS"));
    }

    void theCallsignIsTrimmedAndUppercased()
    {
        FlatMapWidget w;
        w.setStationMarker(QStringLiteral("  oe5sos  "), QImage{});
        QCOMPARE(w.stationCallsign(), QStringLiteral("OE5SOS"));
    }

    void aPhotoBecomesASquarePixmap()
    {
        FlatMapWidget w;
        w.setStationMarker(QStringLiteral("OE5SOS"), wideImage());
        const QPixmap px = w.stationMarkerPixmap();
        QVERIFY(!px.isNull());
        QCOMPARE(px.width(), px.height());
    }

    // ── Der eine, der einen echten Fehler faengt ─────────────────────
    void aWideImageIsCroppedFromTheCentreNotSquashed()
    {
        FlatMapWidget w;
        w.setStationMarker(QStringLiteral("OE5SOS"), wideImage());
        const QImage out = w.stationMarkerPixmap().toImage();
        QVERIFY(!out.isNull());

        // Die Mitte muss rot sein — das war das mittlere Quadrat.
        const QColor mid = out.pixelColor(out.width() / 2, out.height() / 2);
        QVERIFY2(mid.red() > 200 && mid.blue() < 60,
                 qPrintable(QStringLiteral(
                     "die Mitte des Markers ist %1 — bei gestauchtem statt "
                     "beschnittenem Bild waere hier Blau vom Rand")
                         .arg(mid.name())));

        // Und links auf der waagrechten Mittellinie, noch innerhalb des
        // Kreises: auch rot. Waere das Bild gestaucht worden, laege hier
        // der blaue Rand des Querformats.
        const int inset = out.width() / 6;
        const QColor left = out.pixelColor(inset, out.height() / 2);
        QVERIFY2(left.red() > 200 && left.blue() < 60,
                 qPrintable(QStringLiteral(
                     "bei einem Sechstel Breite steht %1 statt Rot")
                         .arg(left.name())));
    }

    void theCornersAreClippedAwaySoTheMarkerIsRound()
    {
        FlatMapWidget w;
        w.setStationMarker(QStringLiteral("OE5SOS"), wideImage());
        const QImage out = w.stationMarkerPixmap().toImage();
        QVERIFY(out.hasAlphaChannel());
        // Die aeusserste Ecke liegt ausserhalb des Kreises.
        QCOMPARE(out.pixelColor(0, 0).alpha(), 0);
        QCOMPARE(out.pixelColor(out.width() - 1, 0).alpha(), 0);
    }

    void theCutIsDoneOnceAndKept()
    {
        // Zweimal dasselbe Bild setzen darf nicht zwei verschiedene
        // Ergebnisse liefern, und ein zweiter Aufruf mit leerem Bild
        // muss die alte Pixmap wegraeumen statt sie stehen zu lassen.
        FlatMapWidget w;
        w.setStationMarker(QStringLiteral("OE5SOS"), wideImage());
        const QSize first = w.stationMarkerPixmap().size();
        w.setStationMarker(QStringLiteral("OE5SOS"), wideImage());
        QCOMPARE(w.stationMarkerPixmap().size(), first);

        w.setStationMarker(QStringLiteral("OE5SOS"), QImage{});
        QVERIFY2(w.stationMarkerPixmap().isNull(),
                 "das alte Foto blieb stehen, nachdem es entfernt wurde");
    }

    void aPortraitIsAlsoCroppedSquare()
    {
        // Der andere Weg herum: hoch statt breit.
        QImage tall(100, 300, QImage::Format_ARGB32);
        tall.fill(QColor(0, 0, 255));
        {
            QPainter p(&tall);
            p.fillRect(0, 100, 100, 100, QColor(255, 0, 0));
        }
        FlatMapWidget w;
        w.setStationMarker(QStringLiteral("OE5SOS"), tall);
        const QImage out = w.stationMarkerPixmap().toImage();
        QCOMPARE(out.width(), out.height());
        const QColor mid = out.pixelColor(out.width() / 2, out.height() / 2);
        QVERIFY2(mid.red() > 200 && mid.blue() < 60, qPrintable(mid.name()));
    }
};

QTEST_MAIN(TestFlatMapStationMarker)
#include "tst_flatmap_station_marker.moc"
