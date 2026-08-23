// SPDX-License-Identifier: GPL-3.0-or-later
//
// WERKZEUG, keine Pruefung: SECHS Arten, Tiefe zu erzeugen — alle auf
// derselben Grundlage (Entwurf B3).
//
// Anlass, 2026-08-23: "schaut schon besser aus, welche arten hast du
// noch, wenn das die basis ist. Zeus hat einen übergang im
// hintergrund, schaut oder wirkt teilweise wie 3d. versuche weitere
// möglichkeiten zu übermitteln."
//
// Der Hinweis auf den Uebergang IM HINTERGRUND ist der wichtigste:
// bisher lag alle Plastik in den Segmenten selbst, der Trog war eine
// flache Flaeche. Zeus arbeitet genau andersherum — sein Trog traegt
// einen Schimmer, und die Striche stehen darin.
//
// Die sechs sind absichtlich UNTERSCHIEDLICH, nicht abgestuft. Es geht
// nicht darum, wieviel, sondern woher:
//
//   D1  Schimmer quer  — der Trog ist in der Mitte heller als an den
//                        Enden. Das ist Zeus' Griff.
//   D2  Glaskante      — ein Lichtreflex ueber der oberen Haelfte des
//                        ganzen Trogs, wie auf einer Scheibe davor.
//   D3  Schlagschatten — die Striche werfen einen Schatten nach unten
//                        rechts; sie stehen dann VOR dem Trog.
//   D4  Zonengrund     — die kritische Zone ist schon im leeren Trog
//                        schwach eingefaerbt. Man sieht, wohin es
//                        geht, bevor man dort ist.
//   D5  Roehre         — der Trog ist oben und unten dunkel, in der
//                        Mitte hell: er woelbt sich dem Auge entgegen.
//   D6  Kette in Zonenfarbe — die unbeleuchteten Striche tragen ihre
//                        Zonenfarbe stark abgedunkelt. Die Skala steht
//                        damit schon im Trog.
//
// 300 x 96 Punkte wie die uebrigen Blaetter.

#include <QtTest>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>

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

QColor mix(const QColor& a, const QColor& b, double k)
{
    k = qBound(0.0, k, 1.0);
    return QColor(int(a.red()   + (b.red()   - a.red())   * k),
                  int(a.green() + (b.green() - a.green()) * k),
                  int(a.blue()  + (b.blue()  - a.blue())  * k));
}

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
    QString label;  QString unit;
    double value;   double peak;
    double vmin;    double vmax;
    double orangeAt; double redAt;
    int decimals;
    QStringList tickText;  QVector<double> tickVal;
};

enum Depth { ShimmerAcross = 1, GlassEdge, DropShadow,
             ZoneGround, Tube, ZoneChain };

void drawRow(QPainter& p, const QRectF& box, const RowSpec& r, int depth)
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
    const QColor tip = colourAt(t);

    // Schein hinter der Kette — bleibt aus B3, in allen sechs.
    if (t > 0.001) {
        const QRectF lit(bar.left() + 1, bar.top() + 1,
                         (bar.width() - 2) * t, bar.height() - 2);
        struct Pass { double grow; int alpha; };
        static const Pass kPasses[] = {{7.0, 16}, {4.0, 22}, {1.5, 30}};
        p.setPen(Qt::NoPen);
        for (const Pass& s : kPasses) {
            QColor c = tip; c.setAlpha(s.alpha);
            p.setBrush(c);
            p.drawRoundedRect(lit.adjusted(-s.grow, -s.grow, s.grow, s.grow),
                              s.grow, s.grow);
        }
    }

    // ── HIER liegt der Unterschied: der Grund des Trogs ─────────────
    p.setPen(Qt::NoPen);
    switch (depth) {
    case ShimmerAcross: {
        // Zeus' Griff: quer heller in der Mitte. Der Trog bekommt damit
        // eine Achse, ohne dass irgendwo eine Kante entsteht.
        QLinearGradient g(bar.left(), 0, bar.right(), 0);
        g.setColorAt(0.00, QColor("#0a0a0f"));
        g.setColorAt(0.45, QColor("#171720"));
        g.setColorAt(1.00, QColor("#0a0a0f"));
        p.setBrush(g);
        break;
    }
    case GlassEdge: {
        QLinearGradient g(0, bar.top(), 0, bar.bottom());
        g.setColorAt(0.0, QColor("#0c0c11"));
        g.setColorAt(1.0, QColor("#121219"));
        p.setBrush(g);
        break;
    }
    case Tube: {
        // Die Roehre: dunkel an den Raendern, hell in der Mitte. Sie
        // woelbt sich dem Auge entgegen statt sich einzugraben.
        QLinearGradient g(0, bar.top(), 0, bar.bottom());
        g.setColorAt(0.00, QColor("#08080c"));
        g.setColorAt(0.42, QColor("#1c1c26"));
        g.setColorAt(0.58, QColor("#181821"));
        g.setColorAt(1.00, QColor("#07070b"));
        p.setBrush(g);
        break;
    }
    case ZoneGround: {
        // Der Grund traegt die Zonen schon vor, sehr leise.
        QLinearGradient g(bar.left(), 0, bar.right(), 0);
        auto dim = [](QColor c) { c.setAlpha(255);
                                  return QColor(c.red()  / 7 + 10,
                                                c.green()/ 7 + 10,
                                                c.blue() / 7 + 12); };
        g.setColorAt(0.00, QColor("#0d0d12"));
        g.setColorAt(qBound(0.02, tOrange - 0.02, 0.96), QColor("#0d0d12"));
        g.setColorAt(qBound(0.03, tOrange + 0.01, 0.97), dim(warn()));
        g.setColorAt(qBound(0.04, tRed + 0.01, 0.99),    dim(danger()));
        g.setColorAt(1.00, dim(danger()));
        p.setBrush(g);
        break;
    }
    default:
        p.setBrush(QColor("#101016"));
        break;
    }
    p.drawRect(bar);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#23232a"), 1.0));
    p.drawRect(bar);
    p.setPen(QPen(QColor(0, 0, 0, 150), 1.0));
    p.drawLine(bar.topLeft() + QPointF(1, 1), bar.topRight() + QPointF(-1, 1));

    // ── Die Kette ──────────────────────────────────────────────────
    const double segW = 2.0;
    const double gap  = 2.6;
    const int    N    = int((bar.width() - 2.0) / (segW + gap));
    for (int i = 0; i < N; ++i) {
        const double tc = (i + 0.5) / N;
        const QRectF seg(bar.left() + 1.0 + i * (segW + gap), bar.top() + 2.0,
                         segW, bar.height() - 4.0);
        p.setPen(Qt::NoPen);
        if (tc > t) {
            if (depth == ZoneChain) {
                // Die unbeleuchteten Striche tragen ihre Zonenfarbe,
                // stark abgedunkelt. Der Trog ist damit selbst schon
                // eine Skala — man sieht, wo es eng wird, ohne die
                // Zahlen zu lesen.
                QColor c = colourAt(tc).darker(320);
                QLinearGradient g(0, seg.top(), 0, seg.bottom());
                g.setColorAt(0.0, c.lighter(150));
                g.setColorAt(1.0, c);
                p.setBrush(g);
            } else {
                QLinearGradient g(0, seg.top(), 0, seg.bottom());
                g.setColorAt(0.0, QColor("#585862"));
                g.setColorAt(1.0, QColor("#3c3c45"));
                p.setBrush(g);
            }
            p.drawRect(seg);
            continue;
        }
        const QColor c = colourAt(tc);
        if (depth == DropShadow) {
            // Erst der Schatten, dann der Strich darauf: er steht dann
            // VOR dem Trog statt darin.
            p.setBrush(QColor(0, 0, 0, 120));
            p.drawRect(seg.translated(1.0, 1.0));
        }
        QLinearGradient g(0, seg.top(), 0, seg.bottom());
        g.setColorAt(0.0,  c.lighter(155));
        g.setColorAt(0.42, c);
        g.setColorAt(1.0,  c.darker(140));
        p.setBrush(g);
        p.drawRect(seg);
    }

    // Glaskante ÜBER allem — deshalb erst hier.
    if (depth == GlassEdge) {
        QLinearGradient g(0, bar.top(), 0, bar.center().y() + 1);
        g.setColorAt(0.0, QColor(255, 255, 255, 30));
        g.setColorAt(0.7, QColor(255, 255, 255, 10));
        g.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawRect(QRectF(bar.left() + 1, bar.top() + 1,
                          bar.width() - 2, bar.height() * 0.5));
    }

    if (t > 0.001) {
        const double x = bar.left() + 1.0 + (bar.width() - 2.0) * t;
        p.setPen(QPen(tip.lighter(180), 1.6));
        p.drawLine(QPointF(x, bar.top() + 2), QPointF(x, bar.bottom() - 2));
    }
    if (r.peak > r.value) {
        const double x = bar.left() + 1.0 + (bar.width() - 2.0) * tOf(r.peak);
        QColor c = colourAt(tOf(r.peak));
        QColor halo = c; halo.setAlpha(70);
        p.setPen(QPen(halo, 3.4));
        p.drawLine(QPointF(x, bar.top() + 2), QPointF(x, bar.bottom() - 2));
        p.setPen(QPen(c.lighter(190), 1.4));
        p.drawLine(QPointF(x, bar.top() + 2), QPointF(x, bar.bottom() - 2));
    }
    {
        const double x = bar.left() + 1.0 + (bar.width() - 2.0) * tOrange;
        p.setPen(QPen(QColor(230, 232, 238, 160), 1.2));
        p.drawLine(QPointF(x, bar.top() + 1), QPointF(x, bar.bottom() - 1));
    }

    // Skala.
    QFont f = p.font();
    f.setPointSizeF(5.0);
    p.setFont(f);
    for (int i = 0; i < r.tickVal.size(); ++i) {
        const double v = r.tickVal[i];
        const double x = bar.left() + 1.0 + (bar.width() - 2.0) * tOf(v);
        p.setPen(QPen(QColor("#33333c"), 1.0));
        p.drawLine(QPointF(x, scale.top() - 1), QPointF(x, scale.top() + 2.0));
        QColor c = v >= r.redAt ? danger()
                                : (v >= r.orangeAt ? warn() : scaleInk());
        c.setAlpha(215);
        p.setPen(c);
        drawTracked(p, QRectF(x - 14, scale.top() + 2.0, 28, scale.height() - 2.0),
                    r.tickText.value(i), 0.3, Qt::AlignHCenter | Qt::AlignTop);
    }

    // Wortmarke, Einheit.
    {
        QFont lf = p.font();
        lf.setPointSizeF(7.0);
        lf.setBold(true);
        p.setFont(lf);
        p.setPen(scaleInk().lighter(115));
        drawTracked(p, QRectF(box.left(), bar.top(), labelW - 4, bar.height()),
                    r.label, 0.9, Qt::AlignLeft | Qt::AlignVCenter);
        QFont uf = p.font();
        uf.setPointSizeF(5.5);
        uf.setBold(false);
        p.setFont(uf);
        p.setPen(QColor("#5a5a63"));
        p.drawText(QRectF(box.left(), scale.top() + 1, labelW - 4, 10),
                   Qt::AlignLeft | Qt::AlignTop, r.unit);
    }

    // Zahl.
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
        const double totalW = fm.horizontalAdvance(num) + 2
                              + um.horizontalAdvance(r.unit);
        const double x0 = bar.right() + (numW - totalW) - 2;
        const QColor c = t >= tRed ? danger()
                                   : (t >= tOrange ? warn() : ink());
        if (t >= tOrange) {
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

QImage draft(int depth, double swr, double pwr)
{
    QImage img(kW, kH, QImage::Format_ARGB32);
    img.fill(QColor(Style::kAppBg));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF panel(2, 2, kW - 4, kH - 4);
    p.setPen(Qt::NoPen);
    {
        QLinearGradient g(0, panel.top(), 0, panel.bottom());
        g.setColorAt(0.0, QColor("#131319"));
        g.setColorAt(1.0, QColor("#0b0b0f"));
        p.setBrush(g);
    }
    p.drawRoundedRect(panel, 4, 4);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#1e1e25"), 1.0));
    p.drawRoundedRect(panel, 4, 4);
    p.setPen(QPen(QColor(255, 255, 255, 14), 1.0));
    p.drawLine(panel.topLeft() + QPointF(5, 1), panel.topRight() + QPointF(-5, 1));

    drawRow(p, QRectF(8,  8, kW - 16, 38), pwrRow(pwr), depth);
    drawRow(p, QRectF(8, 54, kW - 16, 38), swrRow(swr), depth);
    return img;
}

} // namespace

class TstSwrDepthSheet : public QObject
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
            QStringLiteral("D1 · Schimmer quer (Zeus)"),
            QStringLiteral("D2 · Glaskante"),
            QStringLiteral("D3 · Schlagschatten"),
            QStringLiteral("D4 · Zonengrund"),
            QStringLiteral("D5 · Roehre"),
            QStringLiteral("D6 · Kette in Zonenfarbe"),
        };

        const int zoom = 3;
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

        const QString out = QStringLiteral("/tmp/swr_tiefe.png");
        QVERIFY2(sheet.save(out), qPrintable(out));
        qInfo().noquote() << "Blatt:" << out
                          << sheet.width() << "x" << sheet.height();
    }
};

QTEST_MAIN(TstSwrDepthSheet)
#include "tst_swr_depth_sheet.moc"
