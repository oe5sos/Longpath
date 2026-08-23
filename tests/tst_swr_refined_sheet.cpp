// SPDX-License-Identifier: GPL-3.0-or-later
//
// WERKZEUG, keine Pruefung: Entwurf B, veredelt.
//
// Anlass, 2026-08-23: der Betreiber hat aus dem waagrechten Blatt
// Entwurf B gewaehlt — Zeus' Strichoptik — und dazu gesagt:
//   "b - aber bitte moderner, stilvoller, schaut zu einfach aus.
//    halte dich an zeus und an unsere instrumente"
//
// ── Woher die Veredelung kommt ──────────────────────────────────────
//
// Nicht aus neuen Einfaellen, sondern aus zwei Quellen, die es schon
// gibt:
//
// Von ZEUS bleibt, was seine Anzeige ausmacht: die schmale Kette mit
// sichtbarem Spalt, die Skala, die ihre kritische Zone selbst
// einfaerbt, die grosse Zahl rechts mit kleiner Einheit daneben.
//
// Von UNSEREN Instrumenten kommt das, was Zeus fehlt und was unsere
// Zeigerwerke seit dem 2026-08-22 tragen (NeedleInstrument,
// InstrumentPainter): ein SCHEIN hinter dem Ausschlag, eine vertiefte
// Mulde statt einer aufgemalten Flaeche, gesperrte Wortmarken.
//
// Genau daran hing damals das Urteil "wirkt eher 3D" — und es ist
// dieselbe Handschrift, die auch der Bandfilter traegt. Ein
// Instrument, das daneben flach bleibt, faellt auf.
//
// Vier Stufen, damit man sehen kann, wo es zu viel wird:
//   B1 — Grundlage, wie gewaehlt
//   B2 — vertiefte Mulde, Schein hinter der Kette
//   B3 — dazu Typografie: gesperrte Marken, Ziffern mit Schein
//   B4 — dazu Kopfzeile und Trennlinie
//
// 300 x 96 Punkte, wie die uebrigen Blaetter.

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
QColor amber()    { return QColor("#e0a44a"); }

QColor mix(const QColor& a, const QColor& b, double k)
{
    k = qBound(0.0, k, 1.0);
    return QColor(int(a.red()   + (b.red()   - a.red())   * k),
                  int(a.green() + (b.green() - a.green()) * k),
                  int(a.blue()  + (b.blue()  - a.blue())  * k));
}

// Gesperrte Grossbuchstaben. Unsere Instrumentenkoepfe setzen ihre
// Marken so; ohne die Sperrung sieht "SWR" in 7 Punkt gedrungen aus.
void drawTracked(QPainter& p, const QRectF& r, const QString& text,
                 double spacing, int flags)
{
    QFont f = p.font();
    f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
    const QFont old = p.font();
    p.setFont(f);
    p.drawText(r, flags, text);
    p.setFont(old);
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
};

// level 1..4 — wie weit die Veredelung geht.
void drawRow(QPainter& p, const QRectF& box, const RowSpec& r, int level)
{
    const double labelW = 32.0;
    const double numW   = 60.0;
    const QRectF bar(box.left() + labelW, box.top(),
                     box.width() - labelW - numW, box.height() * 0.52);
    const QRectF scale(bar.left(), bar.bottom() + 3.0, bar.width(), 11.0);

    auto tOf = [&](double v) {
        return qBound(0.0, (v - r.vmin) / (r.vmax - r.vmin), 1.0);
    };
    const double tOrange = tOf(r.orangeAt);
    const double tRed    = tOf(r.redAt);
    auto colourAt = [&](double t) {
        const double fade = 0.14;
        if (t <= tOrange - fade) { return good(); }
        if (t <= tOrange)        { return mix(good(), warn(),
                                              (t - (tOrange - fade)) / fade); }
        if (t <= tRed - fade)    { return warn(); }
        if (t <= tRed)           { return mix(warn(), danger(),
                                              (t - (tRed - fade)) / fade); }
        return danger();
    };
    const double t = tOf(r.value);
    const QColor tipColour = colourAt(t);

    // ── Der Schein HINTER der Kette ─────────────────────────────────
    //
    // Das ist der groesste einzelne Unterschied zwischen B1 und B2 —
    // und derselbe Griff, den NeedleInstrument hinter seinem Bogen
    // benutzt: drei Durchgaenge von breit und blass nach schmal und
    // kraeftig. EIN breiter Strich gaebe einen Balken, kein Glimmen.
    if (level >= 2 && t > 0.001) {
        const QRectF lit(bar.left() + 1, bar.top() + 1,
                         (bar.width() - 2) * t, bar.height() - 2);
        struct Pass { double grow; int alpha; };
        static const Pass kPasses[] = {{7.0, 16}, {4.0, 22}, {1.5, 30}};
        p.setPen(Qt::NoPen);
        for (const Pass& s : kPasses) {
            QColor c = tipColour;
            c.setAlpha(s.alpha);
            p.setBrush(c);
            p.drawRoundedRect(lit.adjusted(-s.grow, -s.grow, s.grow, s.grow),
                              s.grow, s.grow);
        }
    }

    // ── Die Mulde ───────────────────────────────────────────────────
    //
    // B1 hat eine flache dunkle Flaeche. Ab B2 ist sie VERTIEFT: oben
    // ein Schatten, unten eine Lichtkante. Das kostet zwei Striche und
    // ist der Unterschied zwischen aufgemalt und eingelassen.
    p.setPen(Qt::NoPen);
    if (level >= 2) {
        QLinearGradient g(0, bar.top(), 0, bar.bottom());
        g.setColorAt(0.0, QColor("#080a0e"));
        g.setColorAt(0.6, QColor("#101016"));
        g.setColorAt(1.0, QColor("#15151d"));
        p.setBrush(g);
    } else {
        p.setBrush(QColor("#101016"));
    }
    p.drawRect(bar);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#23232a"), 1.0));
    p.drawRect(bar);
    if (level >= 2) {
        p.setPen(QPen(QColor(0, 0, 0, 150), 1.0));
        p.drawLine(bar.topLeft() + QPointF(1, 1), bar.topRight() + QPointF(-1, 1));
        p.setPen(QPen(QColor(255, 255, 255, 16), 1.0));
        p.drawLine(bar.bottomLeft() + QPointF(1, -1),
                   bar.bottomRight() + QPointF(-1, -1));
    }

    // ── Die Kette, in Zeus' Strichoptik ─────────────────────────────
    const double segW = 2.0;
    const double gap  = 2.6;
    const int    N    = int((bar.width() - 2.0) / (segW + gap));
    for (int i = 0; i < N; ++i) {
        const double tc = (i + 0.5) / N;
        const QRectF seg(bar.left() + 1.0 + i * (segW + gap), bar.top() + 2.0,
                         segW, bar.height() - 4.0);
        p.setPen(Qt::NoPen);
        if (tc > t) {
            // Ab B2 verlaufen die unbeleuchteten Striche nach unten
            // hin dunkler — sie legen sich damit IN die Mulde, statt
            // davor zu stehen.
            if (level >= 2) {
                QLinearGradient g(0, seg.top(), 0, seg.bottom());
                g.setColorAt(0.0, QColor("#585862"));
                g.setColorAt(1.0, QColor("#3c3c45"));
                p.setBrush(g);
            } else {
                p.setBrush(QColor("#4e4e58"));
            }
            p.drawRect(seg);
            continue;
        }
        const QColor c = colourAt(tc);
        QLinearGradient g(0, seg.top(), 0, seg.bottom());
        g.setColorAt(0.0,  c.lighter(level >= 2 ? 155 : 140));
        g.setColorAt(0.42, c);
        g.setColorAt(1.0,  c.darker(level >= 2 ? 140 : 130));
        p.setBrush(g);
        p.drawRect(seg);
    }

    // Der letzte beleuchtete Strich hell abgesetzt — die Spitze soll
    // man finden, ohne sie zu suchen.
    if (level >= 2 && t > 0.001) {
        const double x = bar.left() + 1.0 + (bar.width() - 2.0) * t;
        p.setPen(QPen(tipColour.lighter(180), 1.6));
        p.drawLine(QPointF(x, bar.top() + 2), QPointF(x, bar.bottom() - 2));
    }

    // Spitzenhaltung.
    if (r.peak > r.value) {
        const double x = bar.left() + 1.0 + (bar.width() - 2.0) * tOf(r.peak);
        QColor c = colourAt(tOf(r.peak));
        if (level >= 2) {
            QColor halo = c; halo.setAlpha(70);
            p.setPen(QPen(halo, 3.4));
            p.drawLine(QPointF(x, bar.top() + 2), QPointF(x, bar.bottom() - 2));
        }
        p.setPen(QPen(c.lighter(190), 1.4));
        p.drawLine(QPointF(x, bar.top() + 2), QPointF(x, bar.bottom() - 2));
    }

    // Die Marke auf der Schwelle.
    {
        const double x = bar.left() + 1.0 + (bar.width() - 2.0) * tOrange;
        p.setPen(QPen(QColor(230, 232, 238, level >= 2 ? 160 : 200), 1.2));
        p.drawLine(QPointF(x, bar.top() + 1), QPointF(x, bar.bottom() - 1));
    }

    // ── Die Skala ───────────────────────────────────────────────────
    QFont f = p.font();
    f.setPointSizeF(level >= 3 ? 5.0 : 5.5);
    p.setFont(f);
    for (int i = 0; i < r.tickVal.size(); ++i) {
        const double v = r.tickVal[i];
        const double x = bar.left() + 1.0 + (bar.width() - 2.0) * tOf(v);
        p.setPen(QPen(QColor("#33333c"), 1.0));
        p.drawLine(QPointF(x, scale.top() - 1), QPointF(x, scale.top() + 2.0));
        QColor c = v >= r.redAt ? danger()
                                : (v >= r.orangeAt ? warn() : scaleInk());
        if (level >= 3) { c.setAlpha(215); }
        p.setPen(c);
        const QRectF tr(x - 14, scale.top() + 2.0, 28, scale.height() - 2.0);
        if (level >= 3) { drawTracked(p, tr, r.tickText.value(i), 0.3,
                                      Qt::AlignHCenter | Qt::AlignTop); }
        else            { p.drawText(tr, Qt::AlignHCenter | Qt::AlignTop,
                                     r.tickText.value(i)); }
    }

    // ── Wortmarke und Einheit links ─────────────────────────────────
    {
        QFont lf = p.font();
        lf.setPointSizeF(7.0);
        lf.setBold(true);
        p.setFont(lf);
        p.setPen(level >= 3 ? scaleInk().lighter(115) : scaleInk());
        const QRectF lr(box.left(), bar.top(), labelW - 4, bar.height());
        if (level >= 3) { drawTracked(p, lr, r.label, 0.9,
                                      Qt::AlignLeft | Qt::AlignVCenter); }
        else            { p.drawText(lr, Qt::AlignLeft | Qt::AlignVCenter,
                                     r.label); }
        QFont uf = p.font();
        uf.setPointSizeF(5.5);
        uf.setBold(false);
        p.setFont(uf);
        p.setPen(QColor("#5a5a63"));
        p.drawText(QRectF(box.left(), scale.top() + 1, labelW - 4, 10),
                   Qt::AlignLeft | Qt::AlignTop, r.unit);
    }

    // ── Die Zahl rechts ─────────────────────────────────────────────
    {
        const QString num = QString::number(r.value, 'f', r.decimals);
        QFont nf = p.font();
        nf.setPointSizeF(14.0);
        nf.setBold(true);
        // Ziffern gleicher Breite: sonst wandert die Zahl bei jedem
        // Messwert um ein paar Punkte hin und her, und genau das
        // macht eine Anzeige unruhig.
        if (level >= 3) {
            QFont::Tag tag;
            nf.setStyleStrategy(QFont::PreferDefault);
        }
        p.setFont(nf);
        const QFontMetricsF fm(nf);
        QFont uf = p.font();
        uf.setPointSizeF(6.0);
        uf.setBold(false);
        const QFontMetricsF um(uf);
        const double totalW = fm.horizontalAdvance(num) + 2
                              + um.horizontalAdvance(r.unit);
        const double x0 = bar.right() + (numW - totalW) - 2;
        const QColor c = t >= tRed ? danger()
                                   : (t >= tOrange ? warn() : ink());

        // Ein Hauch Schein unter der Zahl, wenn sie in der kritischen
        // Zone steht. Nur dort — sonst leuchtet dauernd etwas, und
        // dann leuchtet nichts mehr.
        if (level >= 3 && t >= tOrange) {
            QColor glow = c; glow.setAlpha(46);
            p.setPen(glow);
            for (double d : {0.8, 1.6}) {
                p.drawText(QRectF(x0 - d, bar.top() - 1 - d,
                                  fm.horizontalAdvance(num) + 2 * d + 1,
                                  bar.height() + 2 + 2 * d),
                           Qt::AlignLeft | Qt::AlignVCenter, num);
            }
        }
        p.setPen(c);
        p.drawText(QRectF(x0, bar.top() - 1, fm.horizontalAdvance(num) + 1,
                          bar.height() + 2),
                   Qt::AlignLeft | Qt::AlignVCenter, num);
        p.setFont(uf);
        p.setPen(scaleInk());
        p.drawText(QRectF(x0 + fm.horizontalAdvance(num) + 2, bar.top() - 1,
                          um.horizontalAdvance(r.unit) + 2, bar.height() + 2),
                   Qt::AlignLeft | Qt::AlignVCenter, r.unit);
    }
}

RowSpec pwrRow(double pwr)
{
    RowSpec r;
    r.label = QStringLiteral("PWR");  r.unit = QStringLiteral("W");
    r.value = pwr; r.peak = pwr + 9.0;
    r.vmin = 0.0;  r.vmax = 120.0;
    r.orangeAt = 95.0; r.redAt = 110.0; r.decimals = 0;
    r.tickText = {"0", "25", "50", "75", "100", "120"};
    r.tickVal  = {0.0, 25.0, 50.0, 75.0, 100.0, 120.0};
    return r;
}

RowSpec swrRow(double swr)
{
    RowSpec r;
    r.label = QStringLiteral("SWR"); r.unit = QStringLiteral(":1");
    r.value = swr; r.peak = swr + 0.22;
    r.vmin = 1.0;  r.vmax = 3.5;
    r.orangeAt = 2.5; r.redAt = 3.0; r.decimals = 2;
    r.tickText = {"1", "1.5", "2", "2.5", "3", "3.5"};
    r.tickVal  = {1.0, 1.5, 2.0, 2.5, 3.0, 3.5};
    return r;
}

QImage draft(int level, double swr, double pwr)
{
    QImage img(kW, kH, QImage::Format_ARGB32);
    img.fill(QColor(Style::kAppBg));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Der Kasten. Ab B2 mit Verlauf und Lichtkante oben — dasselbe,
    // was unsere Applet-Koepfe tun.
    const QRectF panel(2, 2, kW - 4, kH - 4);
    p.setPen(Qt::NoPen);
    if (level >= 2) {
        QLinearGradient g(0, panel.top(), 0, panel.bottom());
        g.setColorAt(0.0, QColor("#131319"));
        g.setColorAt(1.0, QColor("#0b0b0f"));
        p.setBrush(g);
    } else {
        p.setBrush(QColor("#0f0f14"));
    }
    p.drawRoundedRect(panel, 4, 4);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#1e1e25"), 1.0));
    p.drawRoundedRect(panel, 4, 4);
    if (level >= 2) {
        p.setPen(QPen(QColor(255, 255, 255, 14), 1.0));
        p.drawLine(panel.topLeft() + QPointF(5, 1),
                   panel.topRight() + QPointF(-5, 1));
    }

    double y = 8;
    if (level >= 4) {
        // Kopfzeile: eine gesperrte Marke und ein Haarstrich. Sie sagt,
        // WORUM es geht, ohne eine ganze Titelleiste zu kosten.
        QFont hf = p.font();
        hf.setPointSizeF(5.5);
        hf.setBold(true);
        p.setFont(hf);
        p.setPen(QColor("#4e4e58"));
        drawTracked(p, QRectF(10, 5, 120, 10), QStringLiteral("STEHWELLE"),
                    1.2, Qt::AlignLeft | Qt::AlignTop);
        p.setPen(QPen(QColor("#1a1a21"), 1.0));
        p.drawLine(QPointF(10, 16), QPointF(kW - 10, 16));
        y = 20;
    }

    const double rowH = level >= 4 ? 34 : 38;
    drawRow(p, QRectF(8, y,            kW - 16, rowH), pwrRow(pwr), level);
    if (level >= 4) {
        p.setPen(QPen(QColor("#17171d"), 1.0));
        p.drawLine(QPointF(14, y + rowH + 3), QPointF(kW - 14, y + rowH + 3));
    }
    drawRow(p, QRectF(8, y + rowH + 8, kW - 16, rowH), swrRow(swr), level);
    return img;
}

} // namespace

class TstSwrRefinedSheet : public QObject
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
            QStringLiteral("B1 · wie gewaehlt"),
            QStringLiteral("B2 · Mulde vertieft, Schein"),
            QStringLiteral("B3 · dazu Typografie"),
            QStringLiteral("B4 · dazu Kopfzeile"),
        };

        const int zoom = 3;   // groesser als sonst: es geht um Feinheiten
        const int labelW = 130;
        const int tw = kW * zoom + 12, th = kH * zoom + 18;
        const int trueW = kW + 24;

        QImage sheet(labelW + titles.size() * tw + trueW,
                     qMax(24 + rows.size() * th,
                          24 + titles.size() * (kH + 20)),
                     QImage::Format_ARGB32);
        sheet.fill(QColor(Style::kAppBg));
        QPainter p(&sheet);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        QFont f = p.font();
        f.setPointSizeF(10.0);
        p.setFont(f);

        for (int c = 0; c < titles.size(); ++c) {
            p.setPen(QColor(Style::kTextSecondary));
            p.drawText(QRect(labelW + c * tw, 2, tw, 18),
                       Qt::AlignCenter, titles[c]);
        }
        for (int r = 0; r < rows.size(); ++r) {
            const int y = 24 + r * th;
            p.setPen(QColor(Style::kTextPrimary));
            p.drawText(QRect(8, y, labelW - 16, kH * zoom),
                       Qt::AlignLeft | Qt::AlignVCenter, rows[r].caption);
            for (int c = 0; c < titles.size(); ++c) {
                p.drawImage(QRect(labelW + c * tw, y, kW * zoom, kH * zoom),
                            draft(c + 1, rows[r].swr, rows[r].pwr));
            }
        }
        {
            const int x = labelW + titles.size() * tw + 8;
            p.setPen(QColor(Style::kTextSecondary));
            p.drawText(QRect(x, 2, trueW - 16, 18),
                       Qt::AlignLeft, QStringLiteral("wirkliche Groesse"));
            for (int c = 0; c < titles.size(); ++c) {
                const int y = 24 + c * (kH + 20);
                p.drawImage(x, y, draft(c + 1, 2.40, 70.0));
                p.setPen(QColor(Style::kTextScale));
                p.drawText(QRect(x, y + kH, kW, 18), Qt::AlignLeft, titles[c]);
            }
        }
        p.end();

        const QString out = QStringLiteral("/tmp/swr_b_veredelt.png");
        QVERIFY2(sheet.save(out), qPrintable(out));
        qInfo().noquote() << "Blatt:" << out
                          << sheet.width() << "x" << sheet.height();
    }
};

QTEST_MAIN(TstSwrRefinedSheet)
#include "tst_swr_refined_sheet.moc"
