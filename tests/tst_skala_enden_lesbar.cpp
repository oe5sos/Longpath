// SPDX-License-Identifier: GPL-3.0-or-later
//
// Eine Skala, deren Enden fehlen, ist keine Skala.
//
// Im Bildschirmfoto des PHONE/CW-Feldes vom 2026-10-15 las sich die
// erste Zahl der Pegelanzeige als „0dB" und die letzte als „+1".
// Tatsaechlich stand dort „-40dB" und „+10" — Qt hatte beide
// abgeschnitten.
//
// HGauge zeichnete jede Zahl in ein Rechteck von 40 Punkten, mittig um
// ihren Strich. Der erste Strich sitzt bei barX = 2, das Rechteck
// begann also bei -18; der letzte sitzt an der rechten Kante, sein
// Rechteck endete 20 Punkte dahinter. Beide Male ausserhalb des
// Widgets.
//
// Dieser Lauf zeichnet die Anzeige und vergleicht die Tinte an den
// Enden mit derselben Zeichenkette, frei gezeichnet. Fehlt etwas, war
// es abgeschnitten.
//
// Gegenprobe (2026-10-15): mit dem alten, mittigen Rechteck fehlen
// links rund zwei Drittel der Tinte.

#include <QtTest>
#include <QFont>
#include <QImage>
#include <QPainter>

#include "gui/HGauge.h"

using namespace Longpath;

class TstSkalaEndenLesbar : public QObject
{
    Q_OBJECT

private:
    /// Zeichnet die Anzeige mit und ohne Zahlen und zaehlt, wo sich
    /// die beiden Bilder unterscheiden. DAS ist die Tinte der Zahlen —
    /// und nur sie. Ein erster Entwurf zaehlte stattdessen alles, was
    /// nicht schwarz war, und zaehlte damit den Grund des Widgets mit:
    /// er lief gruen, OBWOHL die Zahlen abgeschnitten waren
    /// (Gegenprobe 2026-10-15, 352 Bildpunkte in beiden Faellen).
    static QImage male(const QStringList& zahlen)
    {
        HGauge g;
        g.setRange(-40.0, 10.0);
        g.setTitle(QStringLiteral("Level"));
        g.setTickLabels(zahlen);
        g.setValue(-20.0);
        g.resize(355, 34);

        QImage bild(g.size(), QImage::Format_RGB32);
        bild.fill(Qt::black);
        g.render(&bild);
        return bild;
    }

    static int unterschied(const QImage& a, const QImage& b, const QRect& r)
    {
        int n = 0;
        for (int y = r.top(); y <= r.bottom() && y < a.height(); ++y) {
            for (int x = r.left(); x <= r.right() && x < a.width(); ++x) {
                if (x < 0 || y < 0) { continue; }
                if (a.pixel(x, y) != b.pixel(x, y)) { ++n; }
            }
        }
        return n;
    }

private slots:
    void beideEndenStehenGanzDa()
    {
        // ── Der Vergleich ist font-unabhaengig ──────────────────────
        //
        // Ein erster Entwurf verglich die Tinte des ERSTEN Schildes
        // („-40dB", fuenf Zeichen) mit der eines mittleren („-20",
        // drei Zeichen) und setzte die Schwelle auf das Doppelte. Das
        // haelt auf macOS, haengt aber an der Schrift: auf einem Linux-
        // Laeufer mit anderer Schrift kann dasselbe Verhaeltnis
        // darunter liegen, und der Lauf faellt, ohne dass etwas kaputt
        // ist.
        //
        // Jetzt wird DIESELBE Zeichenkette zweimal gezeichnet: einmal
        // am Ende der Skala, einmal mittendrin. Steht sie ganz da,
        // sind beide Male gleich viele Bildpunkte Tinte — egal, welche
        // Schrift der Laeufer hat.
        const QString lang = QStringLiteral("-40dB");

        // Am ENDE: Platz 0 von sieben.
        const QStringList amEnde = {
            lang,                    QStringLiteral("-30"),
            QStringLiteral("-20"),   QStringLiteral("-10"),
            QStringLiteral("0"),     QStringLiteral("+5"),
            QStringLiteral("+10")};
        // MITTENDRIN: dieselbe Zeichenkette auf Platz 3 von sieben.
        const QStringList mittig = {
            QStringLiteral("-30"),   QStringLiteral("-20"),
            QStringLiteral("-10"),   lang,
            QStringLiteral("0"),     QStringLiteral("+5"),
            QStringLiteral("+10")};

        const QImage mitEnde   = male(amEnde);
        const QImage mitMitte  = male(mittig);
        const QImage ohne      = male({});
        QCOMPARE(mitEnde.size(), ohne.size());

        const int barH = 12;
        const int barY = (ohne.height() - barH) / 2;
        const int oben = barY + barH + 1;
        const int barX = 2;
        const int barW = ohne.width() - 4;

        // Fenster um Platz 0 und um Platz 3.
        const QRect fensterEnde(0, oben, 44, 10);
        const int xMitte = barX + (3 * barW) / 6;
        const QRect fensterMitte(xMitte - 22, oben, 44, 10);

        const int tinteEnde  = unterschied(mitEnde,  ohne, fensterEnde);
        const int tinteMitte = unterschied(mitMitte, ohne, fensterMitte);

        qInfo() << "\"-40dB\" am Ende" << tinteEnde
                << "— dieselbe Zeichenkette mittendrin" << tinteMitte;

        QVERIFY2(tinteMitte > 20,
                 "mittendrin steht nichts — dann prueft das hier nichts");

        // Gleich, bis auf ein paar Punkte Kantenglaettung durch die
        // andere Lage im Raster.
        QVERIFY2(tinteEnde * 10 >= tinteMitte * 9,
                 qPrintable(QStringLiteral(
                     "die erste Zahl ist abgeschnitten: %1 Bildpunkte am "
                     "Ende gegen %2 fuer dieselbe Zahl mittendrin")
                                .arg(tinteEnde).arg(tinteMitte)));

        // Und dasselbe fuer das rechte Ende.
        const QStringList rechtsLang = {
            QStringLiteral("-30"),   QStringLiteral("-20"),
            QStringLiteral("-10"),   QStringLiteral("0"),
            QStringLiteral("+5"),    QStringLiteral("+10"),
            lang};
        const QImage mitRechts = male(rechtsLang);
        const QRect fensterRechts(ohne.width() - 44, oben, 44, 10);
        const int tinteRechts =
            unterschied(mitRechts, ohne, fensterRechts);
        qInfo() << "\"-40dB\" am rechten Ende" << tinteRechts;

        QVERIFY2(tinteRechts * 10 >= tinteMitte * 9,
                 qPrintable(QStringLiteral(
                     "die letzte Zahl ist abgeschnitten: %1 Bildpunkte "
                     "gegen %2 mittendrin")
                                .arg(tinteRechts).arg(tinteMitte)));
    }
};

QTEST_MAIN(TstSkalaEndenLesbar)
#include "tst_skala_enden_lesbar.moc"
