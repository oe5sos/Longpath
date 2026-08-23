// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ein Balken in der KNAPPEN Fassung zeichnet auch wirklich etwas.
//
// Anlass, 2026-08-23: "stehwelle und swr wird nicht im frequenz widget
// angezeigt." Der Betreiber hatte beide Zeilen kurz zuvor noch auf
// einem Bildschirmfoto — sie sind mit MEINER Aenderung verschwunden.
//
// Die Ursache ist eine Zeile in paintInto:
//
//     const int footerH = (bare || !m_footer) ? 0 : m_footer->height();
//
// Ein VERBORGENES Widget behaelt seine Hoehe. In der knappen Fassung
// wird die Fusszeile versteckt, ihre Hoehe aber weiter abgezogen — und
// dann faellt die Zeichenflaeche unter kTroughHeight, worauf paintInto
// ohne einen Strich zurueckkehrt.
//
// Lautlos, wie so oft bei dieser Art Fehler: kein Absturz, keine
// Meldung, nur ein leeres Feld.
//
// Die Pruefung misst darum GEZEICHNETE BILDPUNKTE. Ein Balken, der
// nichts vom Grund unterscheidet, hat nichts gezeichnet — egal, was
// seine Methoden zurueckgeben.

#include <QtTest>
#include <QImage>
#include <QPainter>

#include "gui/StyleConstants.h"
#include "gui/instruments/BarInstrument.h"
#include "gui/meters/MeterPoller.h"

using namespace Longpath;

namespace {

int nonBackgroundPixels(const QImage& img)
{
    const QRgb bg = QColor(Style::kAppBg).rgb();
    int n = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            if ((img.pixel(x, y) & 0x00FFFFFF) != (bg & 0x00FFFFFF)) { ++n; }
        }
    }
    return n;
}

int drawnPixels(bool compact, QSize size)
{
    BarInstrument bar;
    bar.setCompact(compact);
    bar.setPrimary(MeterBinding::TxSwr);
    bar.onReading(MeterBinding::TxSwr, 1.6);
    bar.resize(size);

    QImage img(size, QImage::Format_ARGB32);
    img.fill(QColor(Style::kAppBg));
    QPainter p(&img);
    bar.paintInto(p, size, true);
    p.end();
    return nonBackgroundPixels(img);
}

} // namespace

class TstCompactBarDraws : public QObject
{
    Q_OBJECT

private slots:
    void knappZeichnetAuch_data()
    {
        QTest::addColumn<int>("h");
        // Die Hoehen, in denen die Zusatzzeilen im Frequenz-Widget
        // tatsaechlich stehen — und ein paar darum herum.
        QTest::newRow("28 px") << 28;
        QTest::newRow("34 px") << 34;
        QTest::newRow("40 px") << 40;
        QTest::newRow("56 px") << 56;
    }

    void knappZeichnetAuch()
    {
        QFETCH(int, h);
        const QSize size(300, h);
        const int n = drawnPixels(true, size);
        qInfo() << "knapp," << h << "px hoch → gezeichnete Bildpunkte:" << n;
        QVERIFY2(n > 200,
                 qPrintable(QStringLiteral(
                     "Bei %1 px Hoehe wurden nur %2 Bildpunkte gezeichnet — "
                     "die knappe Fassung bleibt leer").arg(h).arg(n)));
    }

    void dieVolleFassungBleibtWieSieWar()
    {
        // Gegenprobe: die Aenderung darf die gewoehnliche Fassung nicht
        // angetastet haben.
        const int n = drawnPixels(false, QSize(300, 72));
        qInfo() << "voll, 72 px → gezeichnete Bildpunkte:" << n;
        QVERIFY2(n > 200, "Auch die volle Fassung zeichnet nichts mehr");
    }
};

QTEST_MAIN(TstCompactBarDraws)
#include "tst_compact_bar_draws.moc"
