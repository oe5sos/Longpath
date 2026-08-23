// SPDX-License-Identifier: GPL-3.0-or-later
//
// WERKZEUG, keine Pruefung: der 1:1-NACHBAU von Zeus Links S-METER,
// zum unmittelbaren Vergleich mit dem Original.
//
// Anlass, 2026-08-23: "baue diesen 1:1 einmal nach" — zu zwei
// Bildschirmfotos des S-METER-Fensters (13:04 und 13:05 Uhr).
//
// ── Warum ein Nachbau und nicht gleich ein Widget ───────────────────
//
// Weil man erst SIEHT, ob man richtig abgelesen hat. Ein Nachbau
// neben dem Original zeigt in einer Sekunde, was nicht stimmt;
// dieselbe Frage an einem eingebauten Widget kostet einen Neustart
// und ein geuebtes Auge.
//
// ── Was ich aus den Fotos abgelesen habe ────────────────────────────
//
// Kasten:   rund 600 x 78 Punkte, Ecken leicht gerundet, Grund etwas
//           heller als der Fenstergrund, feine Kante.
// Spalte 1: "RX" (grau, klein) und darunter "dBm" (noch kleiner,
//           dunkler). Zwei Zeilen, eine Spaltenbreite.
// Spalte 2: die Kette. Sehr schmale Striche mit sichtbarem Spalt,
//           mittelgrau. Ganz links EIN bernsteinfarbener Strich —
//           der Pegel, bei -135 dBm praktisch am Anschlag. Etwa bei
//           zwei Dritteln eine HELLE senkrechte Marke (S9).
// Spalte 3: "-135" gross und weiss, "dBm" klein grau RECHTS daneben,
//           auf der Grundlinie ausgerichtet.
// Zeile 2:  unter der Kette die Skala: S0 S3 S5 S7 S9 grau, danach
//           +10 +20 +40 +60 in Bernstein. Die Abstaende sind NICHT
//           gleich — die S-Stufen liegen dicht, die Plus-Werte
//           weiten sich. Das ist eine echte S-Skala: 6 dB je S-Stufe
//           bis S9, danach dB ueber S9.
// Zeile 2r: "AVG" und "SNR" in Bernstein untereinander, rechts davon
//           "S0" und "-  dB" in Grau.
//
// Alles hier ist auf diese Beobachtungen gestuetzt. Wo ich mir nicht
// sicher war, steht es an der Stelle dabei.

#include <QtTest>
#include <QImage>
#include <QPainter>
#include <QLinearGradient>

#include <cmath>

namespace {

// Die Groesse aus dem Foto, so gut sie sich abmessen laesst.
constexpr int kW = 600;
constexpr int kH = 78;

// Zeus' Farben, aus dem Bild gegriffen.
const QColor kPanel   ("#0d1117");
const QColor kEdge    ("#1c222b");
const QColor kTrough  ("#0a0d12");
const QColor kSegOff  ("#3f4650");
const QColor kAmber   ("#e0a44a");
const QColor kAmberHot("#e8842c");   // der einzelne Strich ganz links
const QColor kInk     ("#f0f3f7");
const QColor kGrey    ("#7c8894");
const QColor kGreyDim ("#5b6672");
const QColor kMarker  ("#dfe6ee");

// ── Die S-Skala ─────────────────────────────────────────────────────
//
// S0 bis S9 in Schritten von 6 dB, danach +10, +20, +40, +60 dB ueber
// S9. S9 entspricht -73 dBm am Kurzwellenempfaenger; S0 liegt damit
// bei -127 dBm.
//
// Genau daher kommt die ungleiche Teilung im Foto: bis S9 sind es
// neun mal 6 dB auf gut zwei Drittel der Breite, die restlichen 60 dB
// teilen sich das letzte Drittel.
double dbmForS(double s)      { return -127.0 + 6.0 * s; }
double dbmForOver(double db)  { return -73.0 + db; }

// Der Platz auf der Achse. Die Achse laeuft von S0 bis S9+60.
double posFor(double dbm)
{
    const double lo = dbmForS(0.0);        // -127
    const double s9 = dbmForS(9.0);        //  -73
    const double hi = dbmForOver(60.0);    //  -13
    // Zwei Abschnitte mit verschiedenem Massstab, wie im Foto:
    // bis S9 nimmt die Skala 0,62 der Breite ein.
    constexpr double kS9Frac = 0.62;
    if (dbm <= s9) {
        return kS9Frac * qBound(0.0, (dbm - lo) / (s9 - lo), 1.0);
    }
    return kS9Frac + (1.0 - kS9Frac)
                       * qBound(0.0, (dbm - s9) / (hi - s9), 1.0);
}

void drawSMeter(QPainter& p, const QRectF& box, double dbm,
                const QString& avgText, const QString& snrText)
{
    // Kasten.
    p.setPen(Qt::NoPen);
    p.setBrush(kPanel);
    p.drawRoundedRect(box, 4, 4);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(kEdge, 1.0));
    p.drawRoundedRect(box, 4, 4);

    const double left   = box.left() + 18;
    const double labelW = 52;
    // 132 statt 108: mit 108 lief die letzte Skalenmarke ("+60") in
    // den AVG-Block hinein. Im Foto endet die Kette deutlich vor der
    // Zahl — das war beim Abmessen untergegangen.
    const double numW   = 132;
    const QRectF bar(left + labelW, box.top() + 14,
                     box.width() - 18 * 2 - labelW - numW, 30);

    // Mulde.
    p.setPen(Qt::NoPen);
    p.setBrush(kTrough);
    p.drawRect(bar);

    // Die Kette. Im Foto sind es sehr schmale Striche mit einem Spalt
    // von etwa derselben Breite.
    const double segW = 2.0;
    const double gap  = 3.0;
    const int    N    = int((bar.width() - 4.0) / (segW + gap));
    const double t    = posFor(dbm);
    for (int i = 0; i < N; ++i) {
        const double tc = (i + 0.5) / N;
        const QRectF seg(bar.left() + 2.0 + i * (segW + gap), bar.top() + 3.0,
                         segW, bar.height() - 6.0);
        p.setPen(Qt::NoPen);
        if (tc <= t) {
            // Beleuchtet: Bernstein mit leichter Plastik.
            QLinearGradient g(0, seg.top(), 0, seg.bottom());
            g.setColorAt(0.0,  kAmberHot.lighter(125));
            g.setColorAt(0.5,  kAmberHot);
            g.setColorAt(1.0,  kAmberHot.darker(125));
            p.setBrush(g);
        } else {
            p.setBrush(kSegOff);
        }
        p.drawRect(seg);
    }

    // Die helle Marke bei S9 — im Foto der auffaelligste Strich.
    {
        const double x = bar.left() + 2.0 + (bar.width() - 4.0) * posFor(dbmForS(9.0));
        p.setPen(QPen(kMarker, 1.6));
        p.drawLine(QPointF(x, bar.top() + 1), QPointF(x, bar.bottom() - 1));
    }

    // Spalte 1: RX ueber dBm.
    {
        QFont f = p.font();
        f.setPointSizeF(11.0);
        p.setFont(f);
        p.setPen(kGrey);
        p.drawText(QRectF(left, bar.top(), labelW, bar.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("RX"));
        f.setPointSizeF(8.0);
        p.setFont(f);
        p.setPen(kGreyDim);
        p.drawText(QRectF(left, bar.bottom() + 6, labelW, 14),
                   Qt::AlignLeft | Qt::AlignTop, QStringLiteral("dBm"));
    }

    // Spalte 3: die Zahl, gross, mit kleiner Einheit daneben.
    {
        const QString num = QString::number(int(std::lround(dbm)));
        QFont nf = p.font();
        nf.setPointSizeF(26.0);
        p.setFont(nf);
        const QFontMetricsF fm(nf);
        QFont uf = p.font();
        uf.setPointSizeF(9.0);
        const QFontMetricsF um(uf);

        const double unitW = um.horizontalAdvance(QStringLiteral("dBm"));
        const double x1 = box.right() - 18 - unitW;
        const double x0 = x1 - 3 - fm.horizontalAdvance(num);

        p.setPen(kInk);
        p.drawText(QRectF(x0, bar.top() - 6, fm.horizontalAdvance(num) + 2,
                          bar.height() + 12),
                   Qt::AlignLeft | Qt::AlignVCenter, num);
        p.setFont(uf);
        p.setPen(kGrey);
        // Die Einheit sitzt im Foto auf der Grundlinie der Zahl, nicht
        // in ihrer Mitte.
        p.drawText(QRectF(x1, bar.top() - 6, unitW + 2, bar.height() + 12),
                   Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("dBm"));
    }

    // Zeile 2: die S-Skala.
    {
        QFont f = p.font();
        f.setPointSizeF(8.0);
        p.setFont(f);
        struct Tick { double dbm; QString text; bool over; };
        const QVector<Tick> ticks = {
            {dbmForS(0), QStringLiteral("S0"), false},
            {dbmForS(3), QStringLiteral("S3"), false},
            {dbmForS(5), QStringLiteral("S5"), false},
            {dbmForS(7), QStringLiteral("S7"), false},
            {dbmForS(9), QStringLiteral("S9"), false},
            {dbmForOver(10), QStringLiteral("+10"), true},
            {dbmForOver(20), QStringLiteral("+20"), true},
            {dbmForOver(40), QStringLiteral("+40"), true},
            {dbmForOver(60), QStringLiteral("+60"), true},
        };
        for (const Tick& tk : ticks) {
            const double x = bar.left() + 2.0
                             + (bar.width() - 4.0) * posFor(tk.dbm);
            p.setPen(QPen(kEdge.lighter(140), 1.0));
            p.drawLine(QPointF(x, bar.bottom() + 1), QPointF(x, bar.bottom() + 4));
            p.setPen(tk.over ? kAmber : kGrey);
            p.drawText(QRectF(x - 20, bar.bottom() + 5, 40, 14),
                       Qt::AlignHCenter | Qt::AlignTop, tk.text);
        }
    }

    // Zeile 2 rechts: AVG / SNR in Bernstein, Werte grau daneben.
    {
        QFont f = p.font();
        f.setPointSizeF(8.0);
        p.setFont(f);
        const double xl = box.right() - 18 - 100;
        const double xr = box.right() - 18 - 44;
        p.setPen(kAmber);
        p.drawText(QRectF(xl, bar.bottom() + 3, 40, 12),
                   Qt::AlignLeft | Qt::AlignTop, QStringLiteral("AVG"));
        p.drawText(QRectF(xl, bar.bottom() + 15, 40, 12),
                   Qt::AlignLeft | Qt::AlignTop, QStringLiteral("SNR"));
        p.setPen(kGrey);
        p.drawText(QRectF(xr, bar.bottom() + 3, 62, 12),
                   Qt::AlignRight | Qt::AlignTop, avgText);
        p.drawText(QRectF(xr, bar.bottom() + 15, 62, 12),
                   Qt::AlignRight | Qt::AlignTop, snrText);
    }
}

} // namespace

class TstZeusSMeterReplica : public QObject
{
    Q_OBJECT

private slots:
    void drawTheReplica()
    {
        // Drei Zustaende: der aus dem Foto (-135, also unter S0), ein
        // mittlerer und ein starker. Ein Nachbau, den man nur im
        // Ruhezustand geprueft hat, ist nicht geprueft.
        struct Case { double dbm; QString avg; QString snr; QString note; };
        const QVector<Case> cases = {
            {-135.0, QStringLiteral("S0"), QStringLiteral("-  dB"),
             QStringLiteral("wie im Foto  ·  -135 dBm")},
            {-95.0,  QStringLiteral("S5"), QStringLiteral("18  dB"),
             QStringLiteral("mittleres Signal  ·  -95 dBm")},
            {-55.0,  QStringLiteral("S9+18"), QStringLiteral("41  dB"),
             QStringLiteral("starkes Signal  ·  -55 dBm")},
        };

        const int pad = 16;
        QImage sheet(kW + pad * 2,
                     pad + cases.size() * (kH + 30),
                     QImage::Format_ARGB32);
        sheet.fill(QColor("#05070a"));
        QPainter p(&sheet);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        for (int i = 0; i < cases.size(); ++i) {
            const int y = pad + i * (kH + 30);
            QFont f = p.font();
            f.setPointSizeF(9.0);
            p.setFont(f);
            p.setPen(kGreyDim);
            p.drawText(QRect(pad, y - 14, kW, 14),
                       Qt::AlignLeft, cases[i].note);
            drawSMeter(p, QRectF(pad, y, kW, kH),
                       cases[i].dbm, cases[i].avg, cases[i].snr);
        }
        p.end();

        const QString out = QStringLiteral("/tmp/zeus_smeter_nachbau.png");
        QVERIFY2(sheet.save(out), qPrintable(out));
        qInfo().noquote() << "Blatt:" << out
                          << sheet.width() << "x" << sheet.height();
    }
};

QTEST_MAIN(TstZeusSMeterReplica)
#include "tst_zeus_smeter_replica.moc"
