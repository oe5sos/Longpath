// SPDX-License-Identifier: GPL-3.0-or-later
//
// WERKZEUG, keine Pruefung: die ROEHRE am echten Instrument, fuer SWR
// und fuer die Stehwelle (Leistung).
//
// Anlass, 2026-08-23: "baue D5 einmal mit SWR und mit Stehwelle.
// danach als option bei den einzelnen widget hinzu."
//
// ── Warum dieses Blatt anders ist als die vorigen ───────────────────
//
// Die Entwurfsblaetter davor haben die Anzeige NACHGEMALT — freihand
// mit QPainter, neben dem echten Code. Das ist zum Vergleichen von
// Formen richtig und zum Beurteilen des Ergebnisses falsch: was dort
// gut aussah, muss im Instrument nicht so aussehen, weil dort andere
// Groessen, andere Randabstaende und der echte Fusszeilenbereich
// gelten.
//
// Hier zeichnet das WIRKLICHE BarInstrument. Was auf diesem Blatt
// steht, steht nach dem Einschalten des Hakens genauso in der App.
//
// Vier Spalten, damit sichtbar wird, dass Roehre und Segmente
// unabhaengig sind: flach/durchgehend, flach/Segmente,
// Roehre/durchgehend, Roehre/Segmente.

#include <QtTest>
#include <QImage>
#include <QPainter>

#include "gui/instruments/BarInstrument.h"
#include "gui/meters/MeterPoller.h"
#include "gui/StyleConstants.h"

using namespace Longpath;

class TstInstrumentTubeSheet : public QObject
{
    Q_OBJECT

private:
    static QImage tile(int bindingId, double value, bool tube, bool segments,
                       QSize want)
    {
        BarInstrument bar;
        bar.setPrimary(bindingId);
        bar.setSegmented(segments);
        bar.setTube(tube);
        bar.onReading(bindingId, value);
        bar.resize(want);

        QImage img(want, QImage::Format_ARGB32);
        img.fill(QColor(Style::kAppBg));
        QPainter p(&img);
        bar.paintInto(p, want, true);
        p.end();
        return img;
    }

private slots:
    void drawTheSheet()
    {
        struct Col { QString title; bool tube; bool seg; };
        const QVector<Col> cols = {
            {QStringLiteral("flach · durchgehend"), false, false},
            {QStringLiteral("flach · Segmente"),    false, true },
            {QStringLiteral("ROEHRE · durchgehend"), true, false},
            {QStringLiteral("ROEHRE · Segmente"),    true, true },
        };
        struct Row { QString caption; int binding; double value; };
        const QVector<Row> rows = {
            {QStringLiteral("SWR 1,15  ·  gut"),           MeterBinding::TxSwr,   1.15},
            {QStringLiteral("SWR 2,40  ·  grenzwertig"),   MeterBinding::TxSwr,   2.40},
            {QStringLiteral("SWR 2,95  ·  schlecht"),      MeterBinding::TxSwr,   2.95},
            {QStringLiteral("Stehwelle 25 W"),             MeterBinding::TxPower, 25.0},
            {QStringLiteral("Stehwelle 70 W"),             MeterBinding::TxPower, 70.0},
            {QStringLiteral("Stehwelle 112 W"),            MeterBinding::TxPower, 112.0},
        };

        const QSize want(300, 72);
        const int zoom = 3;
        const int labelW = 220;
        const int tw = want.width() * zoom + 14;
        const int th = want.height() * zoom + 18;

        QImage sheet(labelW + cols.size() * tw,
                     26 + rows.size() * th, QImage::Format_ARGB32);
        sheet.fill(QColor(Style::kAppBg));
        QPainter p(&sheet);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        QFont f = p.font();
        f.setPointSizeF(11.0);
        f.setBold(true);
        p.setFont(f);

        for (int c = 0; c < cols.size(); ++c) {
            p.setPen(QColor(Style::kTextSecondary));
            p.drawText(QRect(labelW + c * tw, 4, tw, 20),
                       Qt::AlignCenter, cols[c].title);
        }
        f.setBold(false);
        p.setFont(f);
        for (int r = 0; r < rows.size(); ++r) {
            const int y = 26 + r * th;
            p.setPen(QColor(Style::kTextPrimary));
            p.drawText(QRect(10, y, labelW - 20, want.height() * zoom),
                       Qt::AlignLeft | Qt::AlignVCenter, rows[r].caption);
            for (int c = 0; c < cols.size(); ++c) {
                p.drawImage(QRect(labelW + c * tw, y,
                                  want.width() * zoom, want.height() * zoom),
                            tile(rows[r].binding, rows[r].value,
                                 cols[c].tube, cols[c].seg, want));
            }
        }
        p.end();

        const QString out = QStringLiteral("/tmp/instrument_roehre.png");
        QVERIFY2(sheet.save(out), qPrintable(out));
        qInfo().noquote() << "Blatt:" << out
                          << sheet.width() << "x" << sheet.height();
    }

    // Eine echte PRUEFUNG neben dem Werkzeug: die Roehre muss das Bild
    // aendern. Ein Haken, der nichts tut, faellt im Betrieb erst auf,
    // wenn ihn jemand sucht.
    void dieRoehreAendertDasBild()
    {
        const QSize want(300, 72);
        const QImage flach = tile(MeterBinding::TxSwr, 2.40, false, true, want);
        const QImage roehre = tile(MeterBinding::TxSwr, 2.40, true, true, want);
        QVERIFY2(flach != roehre,
                 "Der Haken 'Roehre' aendert am gezeichneten Bild nichts");

        int different = 0;
        for (int y = 0; y < want.height(); ++y) {
            for (int x = 0; x < want.width(); ++x) {
                if (flach.pixel(x, y) != roehre.pixel(x, y)) { ++different; }
            }
        }
        const double share = 100.0 * different / (want.width() * want.height());
        qInfo().noquote() << "geaenderte Bildpunkte:"
                          << QString::number(share, 'f', 1) + "%";
        // Die Mulde nimmt nur einen Streifen des Feldes ein; mehr als
        // ein Prozent heisst, dass wirklich sie es ist, die sich
        // aendert, und nicht bloss ein Rundungsrest.
        QVERIFY2(share > 1.0,
                 qPrintable(QStringLiteral("nur %1% geaendert").arg(share)));
    }
};

QTEST_MAIN(TstInstrumentTubeSheet)
#include "tst_instrument_tube_sheet.moc"
