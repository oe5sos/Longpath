// SPDX-License-Identifier: GPL-3.0-or-later
//
// WERKZEUG, keine Pruefung: die WAAGRECHTE Fassung der
// Stehwellenanzeige, Layout 1:1 nach Zeus Link.
//
// Anlass, 2026-08-23: "bitte mache die horizontalen entwürfe wie bei
// zeus" — nachdem davor schon galt: "das design 1:1 grundsätzlich
// übernehmen."
//
// ── Was ich aus seinem Bild abgelesen habe ──────────────────────────
//
// Zeus' S-METER ist eine Zeile aus fuenf Spalten, und jede hat eine
// Aufgabe:
//
//   1. Wortmarke links ("RX"), darunter kleiner die EINHEIT ("dBm").
//      Zwei Zeilen an derselben Stelle, damit die Einheit keine
//      eigene Breite kostet.
//   2. Die Segmentkette. Duenne Striche mit sichtbarem Spalt — auch
//      der unbeleuchtete Teil bleibt gut zu sehen. Auf seinem Bild
//      steht der Wert bei -135 dBm, praktisch am Anschlag, und man
//      erkennt trotzdem den ganzen Balken.
//   3. Eine senkrechte HELLE Marke im Balken (bei ihm um S9).
//   4. Die Skala unter dem Balken. Die Marken bis S9 grau, die
//      darueber (+10, +20, +40, +60) BERNSTEIN — die Skala faerbt
//      also selbst schon die kritische Zone ein, bevor der Balken
//      dort ankommt.
//   5. Rechts die Zahl gross, die Einheit klein DANEBEN (nicht
//      darunter), und unter beiden eine Zusatzzeile aus zwei Spalten
//      (bei ihm AVG/SNR mit ihren Werten).
//
// Diese Aufteilung ist hier uebernommen. Was sich aendert, sind die
// Messwerte: statt RX-Pegel und SNR stehen Leistung und
// Stehwellenverhaeltnis darin.
//
// Alle Entwuerfe sind auf 300 x 96 Punkte gerechnet — die wirkliche
// Groesse. Das Blatt zeigt sie doppelt so gross; ueber die Lesbarkeit
// am Notebook entscheidet die Spalte ganz rechts.

#include <QtTest>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>

#include "gui/StyleConstants.h"

#include <cmath>

using namespace Longpath;

namespace {

constexpr int kW = 300;
constexpr int kH = 96;

QColor ink()      { return QColor(Style::kTextPrimary); }
QColor scaleInk() { return QColor(Style::kTextScale); }
QColor danger()   { return QColor("#c85a5a"); }
QColor warn()     { return QColor("#d4a23c"); }
QColor good()     { return QColor("#4caf6a"); }
QColor amber()    { return QColor("#e0a44a"); }   // Zeus' Bernstein

QColor mix(const QColor& a, const QColor& b, double k)
{
    k = qBound(0.0, k, 1.0);
    return QColor(int(a.red()   + (b.red()   - a.red())   * k),
                  int(a.green() + (b.green() - a.green()) * k),
                  int(a.blue()  + (b.blue()  - a.blue())  * k));
}

struct RowSpec {
    QString label;
    QString unit;
    double  value;
    double  peak;
    double  vmin;
    double  vmax;
    double  orangeAt;
    double  redAt;
    int     decimals;
    QStringList tickText;
    QVector<double> tickVal;
    QString subLabelA;   // Zusatzzeile links  (bei Zeus "AVG")
    QString subValueA;
    QString subLabelB;   // Zusatzzeile rechts (bei Zeus "SNR")
    QString subValueB;
};

// style 0 — Segmente als Bloecke, unsere Farben (gruen/orange/rot)
// style 1 — Segmente als DUENNE Striche, Zeus' Strichoptik
// style 2 — wie 0, aber Bernstein statt Gruen (Zeus' Farbe 1:1)
// style 3 — durchgehend mit Verlauf
void drawZeusRow(QPainter& p, const QRectF& box, const RowSpec& r,
                 int style, bool withSubline, bool withMarker)
{
    const double labelW = 30.0;
    const double numW   = 58.0;

    const double barH = withSubline ? box.height() * 0.44
                                    : box.height() * 0.56;
    const QRectF bar(box.left() + labelW, box.top(),
                     box.width() - labelW - numW, barH);
    const QRectF scale(bar.left(), bar.bottom() + 2.0, bar.width(), 11.0);

    auto tOf = [&](double v) {
        return qBound(0.0, (v - r.vmin) / (r.vmax - r.vmin), 1.0);
    };
    const double tOrange = tOf(r.orangeAt);
    const double tRed    = tOf(r.redAt);
    const QColor base    = (style == 2) ? amber() : good();

    auto colourAt = [&](double t) {
        // Weicher Uebergang: die Mischung beginnt ein Stueck VOR der
        // Schwelle. Eine harte Kante saehe nach Ampel aus.
        const double fade = 0.14;
        if (t <= tOrange - fade) { return base; }
        if (t <= tOrange)        { return mix(base, warn(),
                                              (t - (tOrange - fade)) / fade); }
        if (t <= tRed - fade)    { return warn(); }
        if (t <= tRed)           { return mix(warn(), danger(),
                                              (t - (tRed - fade)) / fade); }
        return danger();
    };

    // Die Mulde: flach und dunkel, ohne Rundung. So macht es Zeus.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#101016"));
    p.drawRect(bar);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#23232a"), 1.0));
    p.drawRect(bar);

    const double t = tOf(r.value);

    if (style == 3) {
        QLinearGradient g(bar.left(), 0, bar.right(), 0);
        for (int i = 0; i <= 20; ++i) { g.setColorAt(i / 20.0, colourAt(i / 20.0)); }
        QRectF fill = bar.adjusted(1, 1, 0, -1);
        fill.setWidth((bar.width() - 2) * t);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawRect(fill);
        QLinearGradient sh(0, fill.top(), 0, fill.center().y());
        sh.setColorAt(0.0, QColor(255, 255, 255, 44));
        sh.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.setBrush(sh);
        p.drawRect(QRectF(fill.left(), fill.top(),
                          fill.width(), fill.height() * 0.5));
    } else {
        // Die Kette. style 1 nimmt Zeus' Strichoptik: schmaler Strich,
        // breiter Spalt — dadurch bleibt der unbeleuchtete Teil ein
        // ablesbarer Massstab und wird nicht zur grauen Flaeche.
        const double segW = (style == 1) ? 2.0 : 3.4;
        const double gap  = (style == 1) ? 2.6 : 1.4;
        const int    N    = int((bar.width() - 2.0) / (segW + gap));
        for (int i = 0; i < N; ++i) {
            const double tc = (i + 0.5) / N;
            const QRectF seg(bar.left() + 1.0 + i * (segW + gap), bar.top() + 1.5,
                             segW, bar.height() - 3.0);
            if (tc > t) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#4e4e58"));
                p.drawRect(seg);
                continue;
            }
            const QColor c = colourAt(tc);
            // Die Plastik: hell oben, satt in der Mitte, dunkel unten.
            // Mehr braucht es nicht — ein Schlagschatten waere auf
            // zwei Punkten Breite ohnehin nur Schmutz.
            QLinearGradient g(0, seg.top(), 0, seg.bottom());
            g.setColorAt(0.0,  c.lighter(140));
            g.setColorAt(0.45, c);
            g.setColorAt(1.0,  c.darker(130));
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawRect(seg);
        }
    }

    // Die helle Marke — bei Zeus steht sie um S9. Hier auf der
    // Orangeschwelle: die Stelle, ab der es der Betreiber wissen will.
    if (withMarker) {
        const double x = bar.left() + 1.0 + (bar.width() - 2.0) * tOrange;
        p.setPen(QPen(QColor(235, 235, 240, 210), 1.4));
        p.drawLine(QPointF(x, bar.top() + 1), QPointF(x, bar.bottom() - 1));
    }

    // Spitzenhaltung als schmaler heller Strich.
    if (r.peak > r.value) {
        const double x = bar.left() + 1.0 + (bar.width() - 2.0) * tOf(r.peak);
        p.setPen(QPen(colourAt(tOf(r.peak)).lighter(175), 1.6));
        p.drawLine(QPointF(x, bar.top() + 2), QPointF(x, bar.bottom() - 2));
    }

    // Skala unter dem Balken. Marken jenseits der Schwelle in ihrer
    // Zonenfarbe — genau das tut Zeus mit seinen +10/+20/+40/+60.
    QFont f = p.font();
    f.setPointSizeF(5.5);
    p.setFont(f);
    for (int i = 0; i < r.tickVal.size(); ++i) {
        const double v = r.tickVal[i];
        const double x = bar.left() + 1.0 + (bar.width() - 2.0) * tOf(v);
        p.setPen(QPen(QColor("#33333c"), 1.0));
        p.drawLine(QPointF(x, scale.top()), QPointF(x, scale.top() + 2.5));
        p.setPen(v >= r.redAt ? danger()
                              : (v >= r.orangeAt ? warn() : scaleInk()));
        p.drawText(QRectF(x - 14, scale.top() + 2.5, 28, scale.height() - 2.5),
                   Qt::AlignHCenter | Qt::AlignTop, r.tickText.value(i));
    }

    // Wortmarke links, Einheit klein darunter — Zeus' "RX" ueber "dBm".
    {
        QFont lf = p.font();
        lf.setPointSizeF(7.0);
        lf.setBold(true);
        p.setFont(lf);
        p.setPen(scaleInk());
        p.drawText(QRectF(box.left(), bar.top(), labelW - 4, bar.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, r.label);
        QFont uf = p.font();
        uf.setPointSizeF(5.5);
        uf.setBold(false);
        p.setFont(uf);
        p.setPen(QColor("#5a5a63"));
        p.drawText(QRectF(box.left(), scale.top() + 1, labelW - 4, 10),
                   Qt::AlignLeft | Qt::AlignTop, r.unit);
    }

    // Rechts: Zahl gross, Einheit klein DANEBEN.
    {
        const QString num = QString::number(r.value, 'f', r.decimals);
        QFont nf = p.font();
        nf.setPointSizeF(14.0);
        nf.setBold(true);
        p.setFont(nf);
        const QFontMetricsF fm(nf);
        QFont uf = p.font();
        uf.setPointSizeF(6.0);
        uf.setBold(false);
        const QFontMetricsF um(uf);
        const double totalW = fm.horizontalAdvance(num) + 2 + um.horizontalAdvance(r.unit);
        const double x0 = bar.right() + (numW - totalW) - 2;

        p.setPen(t >= tRed ? danger() : (t >= tOrange ? warn() : ink()));
        p.drawText(QRectF(x0, bar.top() - 1, fm.horizontalAdvance(num) + 1,
                          bar.height() + 2),
                   Qt::AlignLeft | Qt::AlignVCenter, num);
        p.setFont(uf);
        p.setPen(scaleInk());
        p.drawText(QRectF(x0 + fm.horizontalAdvance(num) + 2, bar.top() - 1,
                          um.horizontalAdvance(r.unit) + 2, bar.height() + 2),
                   Qt::AlignLeft | Qt::AlignVCenter, r.unit);
    }

    // Die Zusatzzeile: zwei Spalten wie Zeus' AVG/SNR — Wortmarke in
    // Bernstein, Wert grau daneben.
    if (withSubline) {
        const double y = scale.bottom() + 1.0;
        QFont sf = p.font();
        sf.setPointSizeF(5.5);
        sf.setBold(true);
        p.setFont(sf);
        p.setPen(amber());
        p.drawText(QRectF(bar.right() - 96, y, 26, 10),
                   Qt::AlignLeft | Qt::AlignTop, r.subLabelA);
        p.drawText(QRectF(bar.right() - 40, y, 26, 10),
                   Qt::AlignLeft | Qt::AlignTop, r.subLabelB);
        sf.setBold(false);
        p.setFont(sf);
        p.setPen(scaleInk());
        p.drawText(QRectF(bar.right() - 74, y, 30, 10),
                   Qt::AlignRight | Qt::AlignTop, r.subValueA);
        p.drawText(QRectF(bar.right() - 12, y, 66, 10),
                   Qt::AlignRight | Qt::AlignTop, r.subValueB);
    }
}

RowSpec pwrRow(double pwr)
{
    RowSpec r;
    r.label = QStringLiteral("PWR");
    r.unit  = QStringLiteral("W");
    r.value = pwr;
    r.peak  = pwr + 9.0;
    r.vmin = 0.0; r.vmax = 120.0;
    r.orangeAt = 95.0; r.redAt = 110.0;
    r.decimals = 0;
    r.tickText = {"0", "25", "50", "75", "100", "120"};
    r.tickVal  = {0.0, 25.0, 50.0, 75.0, 100.0, 120.0};
    r.subLabelA = QStringLiteral("AVG");  r.subValueA = QStringLiteral("62");
    r.subLabelB = QStringLiteral("PEP");  r.subValueB = QStringLiteral("104 W");
    return r;
}

RowSpec swrRow(double swr)
{
    RowSpec r;
    r.label = QStringLiteral("SWR");
    r.unit  = QStringLiteral(":1");
    r.value = swr;
    r.peak  = swr + 0.22;
    r.vmin = 1.0; r.vmax = 3.5;
    r.orangeAt = 2.5; r.redAt = 3.0;
    r.decimals = 2;
    r.tickText = {"1", "1.5", "2", "2.5", "3", "3.5"};
    r.tickVal  = {1.0, 1.5, 2.0, 2.5, 3.0, 3.5};
    r.subLabelA = QStringLiteral("AVG");  r.subValueA = QStringLiteral("1.62");
    r.subLabelB = QStringLiteral("RL");   r.subValueB = QStringLiteral("9.5 dB");
    return r;
}

QImage draft(int variant, double swr, double pwr)
{
    QImage img(kW, kH, QImage::Format_ARGB32);
    img.fill(QColor(Style::kAppBg));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Der Kasten drumherum — bei Zeus ein schwach abgesetztes Feld mit
    // feiner Kante, nicht der blanke Grund.
    const QRectF panel(2, 2, kW - 4, kH - 4);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#0f0f14"));
    p.drawRoundedRect(panel, 3, 3);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#1e1e25"), 1.0));
    p.drawRoundedRect(panel, 3, 3);

    switch (variant) {
    case 0:   // A — Zeus-Aufteilung voll, mit Zusatzzeile
        drawZeusRow(p, QRectF(8,  6, kW - 16, 40), pwrRow(pwr), 0, true, true);
        drawZeusRow(p, QRectF(8, 50, kW - 16, 40), swrRow(swr), 0, true, true);
        break;
    case 1:   // B — Zeus' Strichoptik
        drawZeusRow(p, QRectF(8,  6, kW - 16, 40), pwrRow(pwr), 1, true, true);
        drawZeusRow(p, QRectF(8, 50, kW - 16, 40), swrRow(swr), 1, true, true);
        break;
    case 2:   // C — Zeus' Bernstein statt Gruen
        drawZeusRow(p, QRectF(8,  6, kW - 16, 40), pwrRow(pwr), 2, true, true);
        drawZeusRow(p, QRectF(8, 50, kW - 16, 40), swrRow(swr), 2, true, true);
        break;
    case 3:   // D — ohne Zusatzzeile: knapper, dafuer hoehere Balken
        drawZeusRow(p, QRectF(8, 10, kW - 16, 36), pwrRow(pwr), 0, false, true);
        drawZeusRow(p, QRectF(8, 52, kW - 16, 36), swrRow(swr), 0, false, true);
        break;
    case 4:   // E — durchgehend statt Kette
        drawZeusRow(p, QRectF(8,  6, kW - 16, 40), pwrRow(pwr), 3, true, true);
        drawZeusRow(p, QRectF(8, 50, kW - 16, 40), swrRow(swr), 3, true, true);
        break;
    default:
        break;
    }
    return img;
}

} // namespace

class TstSwrHorizontalSheet : public QObject
{
    Q_OBJECT

private slots:
    void drawTheSheet()
    {
        struct Row { QString caption; double swr; double pwr; };
        const QVector<Row> rows = {
            {QStringLiteral("gut  ·  1,15"),         1.15,  95.0},
            {QStringLiteral("grenzwertig  ·  2,40"), 2.40,  70.0},
            {QStringLiteral("schlecht  ·  2,90"),    2.90,  25.0},
        };
        const QStringList titles = {
            QStringLiteral("A · Zeus-Aufteilung voll"),
            QStringLiteral("B · Zeus-Strichoptik"),
            QStringLiteral("C · Bernstein statt Gruen"),
            QStringLiteral("D · ohne Zusatzzeile"),
            QStringLiteral("E · durchgehend"),
        };

        const int zoom   = 2;
        const int labelW = 130;
        const int tw = kW * zoom + 10, th = kH * zoom + 16;
        const int trueW = kW + 24;

        QImage sheet(labelW + titles.size() * tw + trueW,
                     qMax(22 + rows.size() * th,
                          22 + titles.size() * (kH + 18)),
                     QImage::Format_ARGB32);
        sheet.fill(QColor(Style::kAppBg));
        QPainter p(&sheet);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        QFont f = p.font();
        f.setPointSizeF(9.0);
        p.setFont(f);

        for (int c = 0; c < titles.size(); ++c) {
            p.setPen(QColor(Style::kTextSecondary));
            p.drawText(QRect(labelW + c * tw, 2, tw, 16),
                       Qt::AlignCenter, titles[c]);
        }
        for (int r = 0; r < rows.size(); ++r) {
            const int y = 22 + r * th;
            p.setPen(QColor(Style::kTextPrimary));
            p.drawText(QRect(8, y, labelW - 16, kH * zoom),
                       Qt::AlignLeft | Qt::AlignVCenter, rows[r].caption);
            for (int c = 0; c < titles.size(); ++c) {
                p.drawImage(QRect(labelW + c * tw, y, kW * zoom, kH * zoom),
                            draft(c, rows[r].swr, rows[r].pwr));
            }
        }
        {
            const int x = labelW + titles.size() * tw + 8;
            p.setPen(QColor(Style::kTextSecondary));
            p.drawText(QRect(x, 2, trueW - 16, 16),
                       Qt::AlignLeft, QStringLiteral("wirkliche Groesse"));
            for (int c = 0; c < titles.size(); ++c) {
                const int y = 22 + c * (kH + 18);
                p.drawImage(x, y, draft(c, 2.40, 70.0));
                p.setPen(QColor(Style::kTextScale));
                p.drawText(QRect(x, y + kH, kW, 16), Qt::AlignLeft, titles[c]);
            }
        }
        p.end();

        const QString out = QStringLiteral("/tmp/swr_horizontal.png");
        QVERIFY2(sheet.save(out), qPrintable(out));
        qInfo().noquote() << "Blatt:" << out
                          << sheet.width() << "x" << sheet.height();
    }
};

QTEST_MAIN(TstSwrHorizontalSheet)
#include "tst_swr_horizontal_sheet.moc"
