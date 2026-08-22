// SPDX-License-Identifier: GPL-3.0-or-later
//
// WERKZEUG, keine Pruefung: malt die Zeigerinstrumente in mehreren
// Helligkeitsstufen nebeneinander, damit der Betreiber vergleichen
// und auswaehlen kann.
//
// Anlass, 2026-08-22: "die anzeigen sollten ein bisschen heller sein,
// zeige sie mir bitte hier, dann kann ich vergleichen."
//
// Aufgehellt wird das GEZEICHNETE BILD, nicht der Hintergrund: nur
// Punkte, die heller als der Grund sind, werden angehoben. Das
// entspricht dem, was eine Aenderung der drei Instrumentenrollen
// (Bogen, Glimmen, Zeiger) bewirken wuerde, und braucht dafuer keine
// Umbauten am Farbwerk.

#include <QtTest>
#include <QPainter>
#include <QImage>

#include "gui/instruments/NeedleInstrument.h"
#include "gui/instruments/ReadingSource.h"
#include "gui/meters/MeterPoller.h"
#include "gui/StyleConstants.h"

using namespace Longpath;

class TstInstrumentBrightnessSheet : public QObject
{
    Q_OBJECT

private:
    static QImage lift(const QImage& src, double factor)
    {
        const QColor bg(Style::kAppBg);
        QImage out = src.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < out.height(); ++y) {
            auto* line = reinterpret_cast<QRgb*>(out.scanLine(y));
            for (int x = 0; x < out.width(); ++x) {
                QColor c = QColor::fromRgb(line[x]);
                // Der Grund bleibt, wie er ist.
                const int lum = (c.red() + c.green() + c.blue()) / 3;
                const int bgl = (bg.red() + bg.green() + bg.blue()) / 3;
                if (lum <= bgl + 2) { continue; }
                int h, s, l;
                c.getHsl(&h, &s, &l);
                c.setHsl(h, s, qBound(0, int(l * factor + 0.5), 255));
                line[x] = c.rgba();
            }
        }
        return out;
    }

    static QImage tile(NeedleInstrument& inst, double factor,
                       const QString& caption)
    {
        QImage img(inst.size(), QImage::Format_ARGB32);
        img.fill(QColor(Style::kAppBg));
        inst.render(&img);
        if (factor > 1.0) { img = lift(img, factor); }

        QImage sheet(img.width(), img.height() + 26, QImage::Format_ARGB32);
        sheet.fill(QColor(Style::kAppBg));
        QPainter p(&sheet);
        p.drawImage(0, 0, img);
        p.setPen(QColor(Style::kTextPrimary));
        QFont f = p.font();
        f.setPointSize(11);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(0, img.height(), img.width(), 24),
                   Qt::AlignCenter, caption);
        p.end();
        return sheet;
    }

private slots:
    void drawTheSheet()
    {
        NeedleInstrument swr;
        swr.setPrimary(MeterBinding::TxSwr);
        swr.resize(300, 200);
        swr.onReading(MeterBinding::TxSwr, 1.8);

        NeedleInstrument sig;
        sig.setPrimary(MeterBinding::SignalAvg);
        sig.resize(300, 200);
        sig.onReading(MeterBinding::SignalAvg, -73.0);

        struct Step { double f; const char* name; };
        const QVector<Step> steps = {
            { 1.00, "heute" },
            { 1.20, "+20 %" },
            { 1.40, "+40 %" },
            { 1.65, "+65 %" },
        };

        const int gap = 14;
        QVector<QImage> rowA, rowB;
        for (const Step& st : steps) {
            rowA.append(tile(swr, st.f, QString::fromLatin1(st.name)));
            rowB.append(tile(sig, st.f, QString::fromLatin1(st.name)));
        }

        int w = gap;
        for (const QImage& t : rowA) { w += t.width() + gap; }
        const int rowH = rowA.first().height();
        QImage sheet(w, 2 * rowH + 3 * gap + 40, QImage::Format_ARGB32);
        sheet.fill(QColor(Style::kAppBg));
        QPainter p(&sheet);
        p.setPen(QColor(Style::kTextPrimary));
        QFont f = p.font();
        f.setPointSize(14);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(0, 8, w, 26), Qt::AlignCenter,
                   QStringLiteral("Stehwelle und S-Meter — wie hell?"));
        int x = gap;
        for (int i = 0; i < rowA.size(); ++i) {
            p.drawImage(x, 40, rowA[i]);
            p.drawImage(x, 40 + rowH + gap, rowB[i]);
            x += rowA[i].width() + gap;
        }
        p.end();

        const QString out =
            QStringLiteral("/tmp/anzeigen_helligkeit.png");
        QVERIFY2(sheet.save(out), qPrintable(out));
        qInfo().noquote() << "Blatt:" << out;
    }
};

QTEST_MAIN(TstInstrumentBrightnessSheet)
#include "tst_instrument_brightness_sheet.moc"
