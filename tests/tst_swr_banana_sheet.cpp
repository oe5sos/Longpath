// SPDX-License-Identifier: GPL-3.0-or-later
//
// WERKZEUG, keine Pruefung: malt Entwuerfe fuer ein kompaktes
// Stehwellen-Widget nebeneinander, damit der Betreiber vergleichen und
// auswaehlen kann.
//
// Anlass, 2026-08-23. Der Betreiber hat es in mehreren Zuegen
// eingegrenzt, und der letzte gilt:
//
//   "grundsätzlich habe ich gemeint, oben einen halbkreis für die
//    stehwelle, darunter, größe 1/3 ca. eine flache kurve, die
//    aussieht wie die form einer banane, unabhängig von oben, für das
//    swr"
//   "es geht darum, um am laptop platz zu sparen, aber trotzdem einen
//    tollen auffälligen stehwelle/swr zeiger zu haben"
//
// Damit steht die Aufgabe: ZWEI Drittel Halbkreis, EIN Drittel flache
// Bananenkurve, beide mit eigener Skala. Und das Ganze klein — auf
// einem Notebook zaehlt jeder Streifen Hoehe.
//
// Ich hatte davor auf ein Kreuzzeigerinstrument getippt (zwei Nadeln,
// zwei Drehpunkte, SWR am Kreuzungspunkt). Das war falsch: es baut
// hoch statt flach und laeuft dem Zweck genau zuwider. Der Entwurf
// ist verworfen, bevor er auf ein Blatt kam.
//
// Alle Entwuerfe sind hier auf 260 x 132 Punkte gerechnet — die
// wirkliche Groesse im Betrieb. Das Blatt zeigt sie doppelt so gross,
// damit man Einzelheiten sieht; wer ueber die Lesbarkeit urteilt,
// muss die kleine Fassung ansehen, und die steht in der letzten
// Spalte.

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

// Die WIRKLICHE Groesse. Alles andere ist nur Vergroesserung fuers
// Blatt.
constexpr int kW = 260;
constexpr int kH = 132;

QColor face()     { return QColor(Style::kInstrumentFace); }
QColor glowHi()   { return QColor(Style::kInstrumentGlowHi); }
QColor glowLo()   { return QColor(Style::kInstrumentGlowLo); }
QColor scaleInk() { return QColor(Style::kTextScale); }
QColor ink()      { return QColor(Style::kTextPrimary); }
QColor danger()   { return QColor("#c85a5a"); }
QColor warn()     { return QColor("#d4a23c"); }
QColor good()     { return QColor("#4caf6a"); }

// ── Oben: der Halbkreis ─────────────────────────────────────────────
//
// Ein echter Halbkreis (180 Grad, links nach rechts) — nicht der
// uebliche Dreiviertelbogen. Er baut nur halb so hoch wie breit und
// gibt damit genau die Ersparnis, um die es geht.
//
// needles: 1 = eine Nadel, 2 = zwei Nadeln in EINEM Feld (der
// Betreiber am 2026-08-23: "2 zeiger in einem").
void drawHalfDial(QPainter& p, const QRectF& box,
                  double v1, double v2, int needles,
                  double vmin, double vmax, double threshold,
                  const QStringList& tickText, const QVector<double>& tickVal,
                  const QString& caption, int decimals, int style)
{
    // Der Drehpunkt sitzt auf der Grundlinie; der Radius fuellt die
    // Breite. So ist der Halbkreis so gross, wie er ueberhaupt sein
    // kann.
    const QPointF c(box.center().x(), box.bottom());
    const double  r = qMin(box.width() * 0.5, box.height()) - 1.0;

    auto ang = [&](double v) {
        return (180.0 - 180.0 * qBound(0.0, (v - vmin) / (vmax - vmin), 1.0))
               * M_PI / 180.0;
    };
    auto dirOf = [&](double v) {
        const double a = ang(v);
        return QPointF(std::cos(a), -std::sin(a));
    };

    // Glimmen unter dem Bogen. Das ist der Teil, der das Ding
    // "auffaellig" macht, ohne laut zu sein.
    {
        QRadialGradient g(c, r);
        QColor hi = glowHi(); hi.setAlpha(style == 2 ? 90 : 55);
        QColor lo = glowLo(); lo.setAlpha(0);
        g.setColorAt(0.30, lo);
        g.setColorAt(0.88, hi);
        g.setColorAt(1.00, lo);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawPie(QRectF(c.x() - r, c.y() - r, r * 2, r * 2), 0, 180 * 16);
    }

    const QRectF arcBox(c.x() - r * 0.88, c.y() - r * 0.88,
                        r * 1.76, r * 1.76);

    // Der Bogen.
    p.setBrush(Qt::NoBrush);
    QColor arc = face(); arc.setAlpha(140);
    p.setPen(QPen(arc, style == 2 ? 2.2 : 1.5));
    p.drawArc(arcBox, 0, 180 * 16);

    // Der rote Abschnitt ab der Grenze.
    if (threshold > vmin && threshold < vmax) {
        const double t = (threshold - vmin) / (vmax - vmin);
        QColor red = danger(); red.setAlpha(220);
        p.setPen(QPen(red, 3.2));
        p.drawArc(arcBox, 0, int((180.0 * (1.0 - t)) * 16));
    }

    // Teilung.
    QFont f = p.font();
    f.setPointSizeF(6.0);
    p.setFont(f);
    for (int i = 0; i < tickVal.size(); ++i) {
        const QPointF d = dirOf(tickVal[i]);
        p.setPen(QPen(face(), 1.2));
        p.drawLine(c + d * (r * 0.88), c + d * (r * 0.76));
        p.setPen(scaleInk());
        const QPointF q = c + d * (r * 0.64);
        p.drawText(QRectF(q.x() - 13, q.y() - 6, 26, 12),
                   Qt::AlignCenter, tickText.value(i));
    }

    // Die Nadel(n).
    auto needle = [&](double v, const QColor& col, double wide, double len) {
        const QPointF d = dirOf(v);
        const QPointF n(-d.y(), d.x());
        QPainterPath path;
        path.moveTo(c + n * wide);
        path.lineTo(c + d * (r * len));
        path.lineTo(c - n * wide);
        path.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawPath(path);
    };
    if (needles == 2) {
        // Zwei Nadeln in EINEM Feld: die zweite kuerzer und in einem
        // eigenen Ton, sonst haelt man sie fuer einen Schatten der
        // ersten.
        needle(v2, QColor(Style::kAccent).lighter(135), 2.0, 0.72);
    }
    needle(v1, face(), 2.6, 0.95);

    // Nabe.
    p.setBrush(QColor(Style::kAppBg));
    p.setPen(QPen(face(), 1.2));
    p.drawEllipse(c, 3.2, 3.2);

    // Zahl und Wortmarke, in den Bogen hinein.
    QFont big = p.font();
    big.setPointSizeF(style == 2 ? 12.0 : 10.5);
    big.setBold(true);
    p.setFont(big);
    p.setPen(ink());
    p.drawText(QRectF(c.x() - 60, c.y() - r * 0.44, 120, 16),
               Qt::AlignCenter, QString::number(v1, 'f', decimals));
    QFont sm = p.font();
    sm.setPointSizeF(6.0);
    sm.setBold(false);
    p.setFont(sm);
    p.setPen(scaleInk());
    p.drawText(QRectF(c.x() - 60, c.y() - r * 0.44 + 15, 120, 10),
               Qt::AlignCenter, caption);
}

// ── Unten: die Banane ───────────────────────────────────────────────
//
// Eine FLACHE Kurve — der Betreiber hat "flach" ausdruecklich gesagt,
// und darauf kommt es an: ein zu runder Bogen liest sich als zweites
// Instrument, ein flacher als Balken, der die Rundung oben aufnimmt.
//
// Der Durchhang ist darum an die HOEHE gekoppelt, nicht an die Breite.
//
// style 0 — durchgehendes Band mit Verlauf
// style 1 — in Rippen zerlegt (Kette)
// style 2 — nur Umriss, Fuellung als Leuchtspur
void drawBanana(QPainter& p, const QRectF& box, double value,
                double vmin, double vmax, double threshold,
                const QString& caption, int style, int decimals)
{
    const double thick = box.height() * 0.46;
    const double w     = box.width();
    const double sag   = box.height() * 0.30;
    const double R     = (w * w / 4.0 + sag * sag) / (2.0 * sag);
    const QPointF ctr(box.center().x(), box.top() + thick * 0.5 + R);
    const double half  = std::asin((w / 2.0) / R);

    auto pointAt = [&](double t, double off) {
        const double a = -half + 2.0 * half * t;
        return QPointF(ctr.x() + (R + off) * std::sin(a),
                       ctr.y() - (R + off) * std::cos(a));
    };
    auto bandPath = [&](double t0, double t1) {
        QPainterPath path;
        const int N = 40;
        path.moveTo(pointAt(t0, -thick * 0.5));
        for (int i = 1; i <= N; ++i) {
            path.lineTo(pointAt(t0 + (t1 - t0) * i / double(N), -thick * 0.5));
        }
        for (int i = N; i >= 0; --i) {
            path.lineTo(pointAt(t0 + (t1 - t0) * i / double(N), thick * 0.5));
        }
        path.closeSubpath();
        return path;
    };

    const double t  = qBound(0.0, (value - vmin) / (vmax - vmin), 1.0);
    const double tT = qBound(0.0, (threshold - vmin) / (vmax - vmin), 1.0);

    // Mulde.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(Style::kInsetBg));
    p.drawPath(bandPath(0.0, 1.0));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(Style::kBorder), 1.0));
    p.drawPath(bandPath(0.0, 1.0));

    QLinearGradient g(box.left(), 0, box.right(), 0);
    g.setColorAt(0.0, good());
    g.setColorAt(qBound(0.02, tT - 0.12, 0.97), good());
    g.setColorAt(qBound(0.03, tT, 0.98), warn());
    g.setColorAt(1.0, danger());

    if (t > 0.001) {
        const QPainterPath fill = bandPath(0.0, t);
        p.setPen(Qt::NoPen);
        if (style == 2) {
            // Leuchtspur: dreimal weich uebereinander, dann schmal und
            // kraeftig. Das ist derselbe Griff wie beim Bandfilter.
            for (double k : {3.0, 1.8, 1.0}) {
                p.setBrush(Qt::NoBrush);
                QColor c = (t > tT) ? danger() : good();
                c.setAlpha(int(40 / k));
                p.setPen(QPen(c, thick * k * 0.5));
                p.drawPath(bandPath(0.0, t));
            }
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawPath(bandPath(0.0, t));
        } else {
            p.setBrush(g);
            p.drawPath(fill);
            if (style == 1) {
                p.setPen(QPen(QColor(Style::kAppBg), 1.3));
                for (int k = 1; k < 20; ++k) {
                    const double tk = k / 20.0;
                    if (tk > t) { break; }
                    p.drawLine(pointAt(tk, -thick * 0.5),
                               pointAt(tk, thick * 0.5));
                }
                p.setPen(Qt::NoPen);
            }
        }
    }

    // Die Grenze als Kerbe quer durchs Band.
    p.setPen(QPen(danger(), 1.3));
    p.drawLine(pointAt(tT, -thick * 0.5 - 2), pointAt(tT, thick * 0.5 + 2));

    // Wortmarke links IM Band, Zahl rechts IM Band — so kostet die
    // Beschriftung keine eigene Zeile. Genau daher kommt die Ersparnis.
    QFont f = p.font();
    f.setPointSizeF(6.0);
    f.setBold(true);
    p.setFont(f);
    // Die Wortmarke steht IM Band. Ist der Balken dort schon gefuellt,
    // muss sie dunkel sein; ist er es nicht, hell. Sonst verschwindet
    // sie genau bei kleinem SWR — also im Normalfall.
    p.setPen(t > 0.14 ? QColor(0, 0, 0, 190) : scaleInk());
    const QPointF l = pointAt(0.0, 0.0);
    p.drawText(QRectF(l.x() + 4, l.y() - 6, 44, 12),
               Qt::AlignLeft | Qt::AlignVCenter, caption);
    QFont b = f; b.setPointSizeF(8.0);
    p.setFont(b);
    p.setPen(ink());
    const QPointF rr = pointAt(1.0, 0.0);
    p.drawText(QRectF(rr.x() - 46, rr.y() - 7, 44, 14),
               Qt::AlignRight | Qt::AlignVCenter,
               QString::number(value, 'f', decimals));
}


// ── Der Balken, nach Zeus Link ──────────────────────────────────────
//
// Der Betreiber am 2026-08-23, mit einem Bild seines Zeus-S-Meters:
//   "generell wollte ich auch bei beiden zeigern die möglichkeit
//    haben, diese auf einen balken jeweils als option wechseln zu
//    können ... zeus hat einen tollen"
//   "ab 3 SWR rot, ab 2,5 orange, .... schöner übergang, 3d optik
//    vielleicht leicht. halte dich aber an unser design."
//
// Zeus zeichnet eine Kette schmaler Segmente, darunter eine Skala mit
// Marken, rechts die Zahl gross, links die Wortmarke. Genau das steht
// hier — aber in UNSEREN Farben und mit unserer Mulde, nicht in
// Zeus' Blau.
//
// ── Zur Skala, und warum sie bis 3,5 geht ───────────────────────────
//
// "ab 3 rot" und eine Skala, die BEI 3 endet, schliessen einander
// aus: Rot faellt dann auf den letzten Punkt und ist nie zu sehen.
// Die Skala laeuft darum bis 3,5. Wer das anders will, aendert eine
// Zahl — aber er soll es wissen, statt es zu erben.
//
// style 0 — Segmente
// style 1 — durchgehend, mit Verlauf (weichster Uebergang)
// style 2 — Segmente mit Spitzenhaltung
void drawZeusBar(QPainter& p, const QRectF& box, double value, double peak,
                 double vmin, double vmax,
                 double orangeAt, double redAt,
                 const QString& caption, const QStringList& tickText,
                 const QVector<double>& tickVal, int decimals,
                 const QString& unit, int style)
{
    // Aufteilung der Zeile: Wortmarke links, Zahl rechts, dazwischen
    // der Balken. Die Skala sitzt UNTER dem Balken, in derselben
    // Spalte — so kostet sie keine eigene Zeile.
    const double labelW = 30.0;
    const double numW   = 52.0;
    const QRectF bar(box.left() + labelW, box.top(),
                     box.width() - labelW - numW, box.height() * 0.56);
    const QRectF scale(bar.left(), bar.bottom() + 2.0,
                       bar.width(), box.height() * 0.34);

    auto tOf = [&](double v) {
        return qBound(0.0, (v - vmin) / (vmax - vmin), 1.0);
    };

    // Die Farbe an einer Stelle. Der Uebergang ist WEICH — zwischen
    // den Zonen wird gemischt, nicht umgeschaltet. Genau das meint
    // "schöner übergang"; eine harte Kante an 2,5 sieht nach Ampel
    // aus, nicht nach Instrument.
    const double tOrange = tOf(orangeAt);
    const double tRed    = tOf(redAt);
    auto colourAt = [&](double t) {
        auto mix = [](const QColor& a, const QColor& b, double k) {
            k = qBound(0.0, k, 1.0);
            return QColor(int(a.red()   + (b.red()   - a.red())   * k),
                          int(a.green() + (b.green() - a.green()) * k),
                          int(a.blue()  + (b.blue()  - a.blue())  * k));
        };
        // Die Mischung beginnt schon ein Stueck VOR der Schwelle,
        // sonst gaebe es doch wieder eine Kante.
        const double fade = 0.14;
        if (t <= tOrange - fade) { return good(); }
        if (t <= tOrange)        { return mix(good(), warn(),
                                              (t - (tOrange - fade)) / fade); }
        if (t <= tRed - fade)    { return warn(); }
        if (t <= tRed)           { return mix(warn(), danger(),
                                              (t - (tRed - fade)) / fade); }
        return danger();
    };

    // ── Die Mulde, 1:1 nach Zeus ────────────────────────────────────
    //
    // Der Betreiber am 2026-08-23: "form einmal ok, aber das design 1:1
    // grundsätzlich übernehmen."
    //
    // Auf seinem Bild ist die Mulde FLACH und dunkel, ohne Rundung und
    // ohne Verlauf; die Plastik entsteht allein in den Segmenten. Hier
    // stand vorher eine gerundete Mulde mit eigenem Verlauf — das war
    // unsere Handschrift, nicht seine.
    {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#14141a"));
        p.drawRect(bar);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#2a2a33"), 1.0));
        p.drawRect(bar);
    }

    const double t = tOf(value);

    if (style == 1) {
        // Durchgehend: der weichste Uebergang, den es gibt.
        QLinearGradient g(bar.left(), 0, bar.right(), 0);
        for (int i = 0; i <= 20; ++i) {
            g.setColorAt(i / 20.0, colourAt(i / 20.0));
        }
        QRectF fill = bar.adjusted(1, 1, 0, -1);
        fill.setWidth((bar.width() - 2) * t);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawRoundedRect(fill, 1.5, 1.5);
        // Glanzkante oben — die halbe Hoehe, deutlich aufgehellt.
        QLinearGradient sh(0, fill.top(), 0, fill.center().y());
        sh.setColorAt(0.0, QColor(255, 255, 255, 46));
        sh.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.setBrush(sh);
        p.drawRoundedRect(QRectF(fill.left(), fill.top(),
                                 fill.width(), fill.height() * 0.5), 1.5, 1.5);
    } else {
        // Segmente, wie bei Zeus.
        // Zeus setzt viele SCHMALE Segmente mit haarfeinem Spalt —
        // auf seinem Bild sind es rund vierzig auf 370 Punkten Breite.
        const int    N   = 40;
        const double gap = 1.0;
        const double sw  = (bar.width() - 2.0 - gap * (N - 1)) / N;
        for (int i = 0; i < N; ++i) {
            const double tc  = (i + 0.5) / N;
            const bool   lit = tc <= t;
            const QRectF seg(bar.left() + 1.0 + i * (sw + gap), bar.top() + 1.0,
                             sw, bar.height() - 2.0);
            QColor c = colourAt(tc);
            if (!lit) {
                // Bei Zeus sind die unbeleuchteten Segmente deutlich
                // HELLER, als ich sie zuerst hatte: ein mittleres Grau,
                // das die ganze Kette sichtbar macht, auch wenn der
                // Ausschlag klein ist. Auf seinem Bild steht der Wert
                // bei -135 dBm, also praktisch am Anschlag — und man
                // sieht trotzdem den ganzen Balken.
                c = QColor("#565660");
                p.setPen(Qt::NoPen);
                p.setBrush(c);
                p.drawRect(seg);
                continue;
            }
            QLinearGradient g(0, seg.top(), 0, seg.bottom());
            g.setColorAt(0.0, c.lighter(138));
            g.setColorAt(0.45, c);
            g.setColorAt(1.0, c.darker(128));
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawRect(seg);
        }
        if (style == 2 && peak > value) {
            // Spitzenhaltung: ein einzelnes helles Segment dort, wo es
            // zuletzt am hoechsten stand.
            const double tp = tOf(peak);
            const int i = qBound(0, int(tp * N), N - 1);
            const QRectF seg(bar.left() + 1.0 + i * (sw + gap), bar.top() + 1.0,
                             sw, bar.height() - 2.0);
            p.setPen(Qt::NoPen);
            p.setBrush(colourAt(tp).lighter(165));
            p.drawRect(seg);
        }
    }

    // Skala unter dem Balken.
    QFont f = p.font();
    f.setPointSizeF(5.5);
    p.setFont(f);
    for (int i = 0; i < tickVal.size(); ++i) {
        const double x = bar.left() + 1.0 + (bar.width() - 2.0) * tOf(tickVal[i]);
        p.setPen(QPen(QColor(Style::kBorder).lighter(130), 1.0));
        p.drawLine(QPointF(x, scale.top()), QPointF(x, scale.top() + 3));
        p.setPen(tickVal[i] >= orangeAt ? warn().darker(115) : scaleInk());
        p.drawText(QRectF(x - 14, scale.top() + 3, 28, scale.height() - 3),
                   Qt::AlignHCenter | Qt::AlignTop, tickText.value(i));
    }

    // Wortmarke links.
    QFont lf = p.font();
    lf.setPointSizeF(7.0);
    lf.setBold(true);
    p.setFont(lf);
    p.setPen(scaleInk());
    p.drawText(QRectF(box.left(), bar.top(), labelW - 4, bar.height()),
               Qt::AlignLeft | Qt::AlignVCenter, caption);

    // Zahl rechts, gross — bei Zeus die auffaelligste Stelle der Zeile.
    QFont nf = p.font();
    nf.setPointSizeF(13.5);
    nf.setBold(true);
    p.setFont(nf);
    p.setPen(t >= tRed ? danger() : (t >= tOrange ? warn() : ink()));
    const QRectF numBox(bar.right() + 2, bar.top() - 1,
                        numW - 4, bar.height() + 2);
    p.drawText(numBox, Qt::AlignRight | Qt::AlignVCenter,
               QString::number(value, 'f', decimals));
    if (!unit.isEmpty()) {
        QFont uf = p.font();
        uf.setPointSizeF(5.5);
        uf.setBold(false);
        p.setFont(uf);
        p.setPen(scaleInk());
        p.drawText(QRectF(numBox.left(), numBox.bottom() - 2,
                          numBox.width(), 9),
                   Qt::AlignRight | Qt::AlignTop, unit);
    }
}


// ── Dieselbe Kette, nur GEBOGEN ─────────────────────────────────────
//
// Der Betreiber am 2026-08-23, und das ist die Anweisung, die gilt:
//   "also stehwelle sollte 1:1 sein, darunter liegend SWR als banane,
//    aber designart die gleiche wie bestehend. sprich beide vereint,
//    design usw. gleich, form anders."
//
// Also: oben Zeus' Balken, unverfaelscht. Darunter GENAU derselbe
// Balken — dieselben Segmente, dieselben Farben, dieselbe Skala — nur
// auf einen flachen Bogen gelegt.
//
// Der Reiz liegt darin, dass die beiden Zeilen dadurch als EIN Geraet
// lesbar bleiben. Zwei verschiedene Gestaltungen untereinander waeren
// zwei Instrumente in einem Kasten; eine Gestaltung in zwei Formen ist
// ein Instrument mit zwei Angaben.
//
// sagFactor steuert, wie stark sie sich woelbt. Flach war ausdruecklich
// gewuenscht ("eine flache kurve").
void drawZeusBanana(QPainter& p, const QRectF& box, double value, double peak,
                    double vmin, double vmax,
                    double orangeAt, double redAt,
                    const QString& caption, const QStringList& tickText,
                    const QVector<double>& tickVal, int decimals,
                    int style, double sagFactor)
{
    const double labelW = 30.0;
    const double numW   = 52.0;
    const QRectF band(box.left() + labelW, box.top(),
                      box.width() - labelW - numW, box.height() * 0.58);
    const double thick = band.height() * 0.62;

    // Der Bogen. Der Mittelpunkt liegt weit unten, damit die Woelbung
    // flach bleibt.
    const double w   = band.width();
    const double sag = band.height() * sagFactor;
    const double R   = (w * w / 4.0 + sag * sag) / (2.0 * sag);
    const QPointF ctr(band.center().x(), band.top() + thick * 0.5 + R);
    const double half = std::asin((w / 2.0) / R);

    auto pointAt = [&](double t, double off) {
        const double a = -half + 2.0 * half * t;
        return QPointF(ctr.x() + (R + off) * std::sin(a),
                       ctr.y() - (R + off) * std::cos(a));
    };
    auto quad = [&](double t0, double t1) {
        QPolygonF q;
        q << pointAt(t0, -thick * 0.5) << pointAt(t1, -thick * 0.5)
          << pointAt(t1,  thick * 0.5) << pointAt(t0,  thick * 0.5);
        return q;
    };

    auto tOf = [&](double v) {
        return qBound(0.0, (v - vmin) / (vmax - vmin), 1.0);
    };
    const double tOrange = tOf(orangeAt);
    const double tRed    = tOf(redAt);
    auto colourAt = [&](double t) {
        auto mix = [](const QColor& a, const QColor& b, double k) {
            k = qBound(0.0, k, 1.0);
            return QColor(int(a.red()   + (b.red()   - a.red())   * k),
                          int(a.green() + (b.green() - a.green()) * k),
                          int(a.blue()  + (b.blue()  - a.blue())  * k));
        };
        const double fade = 0.14;
        if (t <= tOrange - fade) { return good(); }
        if (t <= tOrange)        { return mix(good(), warn(),
                                              (t - (tOrange - fade)) / fade); }
        if (t <= tRed - fade)    { return warn(); }
        if (t <= tRed)           { return mix(warn(), danger(),
                                              (t - (tRed - fade)) / fade); }
        return danger();
    };

    // Mulde, flach und dunkel — wie oben.
    {
        QPainterPath trough;
        const int N = 40;
        trough.moveTo(pointAt(0.0, -thick * 0.5));
        for (int i = 1; i <= N; ++i) {
            trough.lineTo(pointAt(i / double(N), -thick * 0.5));
        }
        for (int i = N; i >= 0; --i) {
            trough.lineTo(pointAt(i / double(N), thick * 0.5));
        }
        trough.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#14141a"));
        p.drawPath(trough);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#2a2a33"), 1.0));
        p.drawPath(trough);
    }

    const double t = tOf(value);

    if (style == 1) {
        // Durchgehend gebogen.
        QPainterPath fill;
        const int N = 40;
        fill.moveTo(pointAt(0.0, -thick * 0.5 + 1));
        for (int i = 1; i <= N; ++i) {
            fill.lineTo(pointAt(t * i / double(N), -thick * 0.5 + 1));
        }
        for (int i = N; i >= 0; --i) {
            fill.lineTo(pointAt(t * i / double(N), thick * 0.5 - 1));
        }
        fill.closeSubpath();
        QLinearGradient g(band.left(), 0, band.right(), 0);
        for (int i = 0; i <= 20; ++i) { g.setColorAt(i / 20.0, colourAt(i / 20.0)); }
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawPath(fill);
    } else {
        const int N = 40;
        for (int i = 0; i < N; ++i) {
            const double t0 = i / double(N);
            const double t1 = (i + 1) / double(N) - 0.004;   // Haarspalt
            const double tc = (t0 + t1) * 0.5;
            QPolygonF q = quad(t0, t1);
            if (tc > t) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#565660"));
                p.drawPolygon(q);
                continue;
            }
            const QColor c = colourAt(tc);
            // Die Plastik der Segmente: hell oben, dunkel unten. Auf
            // dem Bogen laeuft "oben" mit der Woelbung mit, darum wird
            // je Segment ein eigener Verlauf gesetzt.
            QLinearGradient g(pointAt(tc, -thick * 0.5), pointAt(tc, thick * 0.5));
            g.setColorAt(0.0, c.lighter(138));
            g.setColorAt(0.45, c);
            g.setColorAt(1.0, c.darker(128));
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawPolygon(q);
        }
        if (style == 2 && peak > value) {
            const double tp = tOf(peak);
            const int i = qBound(0, int(tp * N), N - 1);
            p.setPen(Qt::NoPen);
            p.setBrush(colourAt(tp).lighter(165));
            p.drawPolygon(quad(i / double(N), (i + 1) / double(N) - 0.004));
        }
    }

    // Skala — sie folgt dem Bogen, sonst liefe sie ihm davon.
    //
    // ACHTUNG beim Vorzeichen: der Mittelpunkt des Bogens liegt
    // UNTERHALB des Bandes, ein positiver Versatz zeigt also nach
    // aussen und damit nach OBEN. Die Skala gehoert nach unten und
    // braucht darum das Minus. Auf dem ersten Blatt stand sie deshalb
    // quer ueber der Banane.
    QFont f = p.font();
    f.setPointSizeF(5.5);
    p.setFont(f);
    for (int i = 0; i < tickVal.size(); ++i) {
        const double tv = tOf(tickVal[i]);
        const QPointF a = pointAt(tv, -thick * 0.5);
        const QPointF b = pointAt(tv, -thick * 0.5 - 3);
        p.setPen(QPen(QColor("#2a2a33").lighter(140), 1.0));
        p.drawLine(a, b);
        p.setPen(tickVal[i] >= orangeAt ? warn().darker(115) : scaleInk());
        p.drawText(QRectF(b.x() - 14, b.y(), 28, 10),
                   Qt::AlignHCenter | Qt::AlignTop, tickText.value(i));
    }

    QFont lf = p.font();
    lf.setPointSizeF(7.0);
    lf.setBold(true);
    p.setFont(lf);
    p.setPen(scaleInk());
    p.drawText(QRectF(box.left(), band.top(), labelW - 4, band.height()),
               Qt::AlignLeft | Qt::AlignVCenter, caption);

    QFont nf = p.font();
    nf.setPointSizeF(13.5);
    nf.setBold(true);
    p.setFont(nf);
    p.setPen(t >= tRed ? danger() : (t >= tOrange ? warn() : ink()));
    p.drawText(QRectF(band.right() + 2, band.top() - 1, numW - 4,
                      band.height() + 2),
               Qt::AlignRight | Qt::AlignVCenter,
               QString::number(value, 'f', decimals));
}

// Ein Entwurf in wirklicher Groesse.
//
// Oben Zeus' Balken fuer die STEHWELLE, darunter dieselbe Kette als
// Banane fuer das SWR. Die Entwuerfe unterscheiden sich nur in
// Kleinigkeiten — Woelbung, Segmente gegen Verlauf, Spitzenhaltung.
QImage draft(int variant, double swr, double pwr)
{
    QImage img(kW, kH, QImage::Format_ARGB32);
    img.fill(QColor(Style::kAppBg));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Zwei gleich hohe Zeilen. Der Halbkreis ist damit weg — er passt
    // nicht mehr zur Ansage "beide vereint, design gleich".
    const QRectF top(6, 12,          kW - 12, 46);
    const QRectF bot(6, 12 + 58.0,   kW - 12, 52);

    // Die Skala der Stehwelle laeuft wie bei Zeus in Leistung; das SWR
    // hat seine eigene.
    const QStringList pwrTicks = {"0", "25", "50", "75", "100"};
    const QVector<double> pwrVals = {0.0, 25.0, 50.0, 75.0, 100.0};
    const QStringList swrTicks = {"1", "1.5", "2", "2.5", "3", "3.5"};
    const QVector<double> swrVals = {1.0, 1.5, 2.0, 2.5, 3.0, 3.5};

    // Die Stehwelle oben: Zeus 1:1, Segmente, keine Ausnahme.
    auto stehwelle = [&](int style) {
        drawZeusBar(p, top, pwr, pwr + 8.0, 0.0, 120.0, 95.0, 110.0,
                    QStringLiteral("PWR"), pwrTicks, pwrVals, 0,
                    QStringLiteral("W"), style);
    };

    switch (variant) {
    case 0:   // A — flache Banane, Segmente
        stehwelle(0);
        drawZeusBanana(p, bot, swr, swr + 0.2, 1.0, 3.5, 2.5, 3.0,
                       QStringLiteral("SWR"), swrTicks, swrVals, 2, 0, 0.22);
        break;
    case 1:   // B — deutlichere Woelbung
        stehwelle(0);
        drawZeusBanana(p, bot, swr, swr + 0.2, 1.0, 3.5, 2.5, 3.0,
                       QStringLiteral("SWR"), swrTicks, swrVals, 2, 0, 0.42);
        break;
    case 2:   // C — Banane durchgehend statt Segmente
        stehwelle(0);
        drawZeusBanana(p, bot, swr, swr + 0.2, 1.0, 3.5, 2.5, 3.0,
                       QStringLiteral("SWR"), swrTicks, swrVals, 2, 1, 0.22);
        break;
    case 3:   // D — mit Spitzenhaltung in beiden Zeilen
        stehwelle(2);
        drawZeusBanana(p, bot, swr, swr + 0.25, 1.0, 3.5, 2.5, 3.0,
                       QStringLiteral("SWR"), swrTicks, swrVals, 2, 2, 0.22);
        break;
    case 4:   // E — beide durchgehend (weichster Uebergang)
        stehwelle(1);
        drawZeusBanana(p, bot, swr, swr + 0.2, 1.0, 3.5, 2.5, 3.0,
                       QStringLiteral("SWR"), swrTicks, swrVals, 2, 1, 0.30);
        break;
    default:
        break;
    }
    return img;
}

} // namespace

class TstSwrBananaSheet : public QObject
{
    Q_OBJECT

private slots:
    void drawTheSheet()
    {
        struct Row { QString caption; double swr; double pwr; };
        // Drei Betriebsfaelle. Ein Entwurf, der nur bei SWR 1,15 gut
        // aussieht, taugt nicht.
        const QVector<Row> rows = {
            {QStringLiteral("gut  ·  1,15"),         1.15,  95.0},
            {QStringLiteral("grenzwertig  ·  2,40"), 2.40,  70.0},
            {QStringLiteral("schlecht  ·  2,90"),    2.90,  25.0},
        };
        const QStringList titles = {
            QStringLiteral("A · flache Banane"),
            QStringLiteral("B · staerker gewoelbt"),
            QStringLiteral("C · Banane durchgehend"),
            QStringLiteral("D · mit Spitzenhaltung"),
            QStringLiteral("E · beide durchgehend"),
        };

        const int zoom   = 2;
        const int labelW = 130;
        const int tw = kW * zoom, th = kH * zoom + 20;
        // Eine zusaetzliche Spalte ganz rechts: alle Entwuerfe noch
        // einmal in WIRKLICHER Groesse untereinander. Ueber die
        // Lesbarkeit auf dem Notebook entscheidet nur die.
        const int trueW = kW + 20;

        QImage sheet(labelW + titles.size() * tw + trueW,
                     qMax(rows.size() * th, titles.size() * (kH + 16) + 30),
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
            const int y = 20 + r * th;
            p.setPen(QColor(Style::kTextPrimary));
            p.drawText(QRect(8, y, labelW - 16, kH * zoom),
                       Qt::AlignLeft | Qt::AlignVCenter, rows[r].caption);
            for (int c = 0; c < titles.size(); ++c) {
                const QImage d = draft(c, rows[r].swr, rows[r].pwr);
                p.drawImage(QRect(labelW + c * tw, y, kW * zoom, kH * zoom), d);
            }
        }

        // Die wahre Groesse, rechts.
        {
            const int x = labelW + titles.size() * tw + 8;
            p.setPen(QColor(Style::kTextSecondary));
            p.drawText(QRect(x, 2, trueW - 16, 16),
                       Qt::AlignLeft, QStringLiteral("wirkliche Groesse"));
            for (int c = 0; c < titles.size(); ++c) {
                p.drawImage(x, 22 + c * (kH + 16), draft(c, 2.40, 70.0));
                p.setPen(QColor(Style::kTextScale));
                p.drawText(QRect(x, 22 + c * (kH + 16) + kH, kW, 14),
                           Qt::AlignLeft, titles[c]);
            }
        }
        p.end();

        const QString out = QStringLiteral("/tmp/swr_entwuerfe.png");
        QVERIFY2(sheet.save(out), qPrintable(out));
        qInfo().noquote() << "Blatt:" << out
                          << sheet.width() << "x" << sheet.height();
    }
};

QTEST_MAIN(TstSwrBananaSheet)
#include "tst_swr_banana_sheet.moc"
