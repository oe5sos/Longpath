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
        const QStringList zahlen = {
            QStringLiteral("-40dB"), QStringLiteral("-30"),
            QStringLiteral("-20"),   QStringLiteral("-10"),
            QStringLiteral("0"),     QStringLiteral("+5"),
            QStringLiteral("+10")};

        const QImage mit  = male(zahlen);
        const QImage ohne = male({});
        QCOMPARE(mit.size(), ohne.size());

        const int barH = 12;
        const int barY = (mit.height() - barH) / 2;
        const int oben = barY + barH + 1;

        // Die Enden: 44 Punkte breit (Kasten 40 plus Rand).
        const int links  = unterschied(mit, ohne, QRect(0, oben, 44, 10));
        const int rechts = unterschied(
            mit, ohne, QRect(mit.width() - 44, oben, 44, 10));
        // Eine Zahl in der Mitte als Mass fuer „ganz da": sie war nie
        // abgeschnitten.
        const int mitte = unterschied(
            mit, ohne, QRect(mit.width() / 2 - 22, oben, 44, 10));

        qInfo() << "Tinte links" << links << "rechts" << rechts
                << "Mitte (Vergleich)" << mitte;

        QVERIFY2(mitte > 10,
                 "schon in der Mitte steht keine Zahl — dann prueft das "
                 "hier nichts");

        // „-40dB" hat FUENF Zeichen, die Mitte („-20") drei. Wer die
        // Enden abschneidet, bleibt deutlich darunter.
        // Gemessen: ganz 93, abgeschnitten 56, Mitte 37. Die Schwelle
        // liegt dazwischen.
        QVERIFY2(links >= mitte * 2,
                 qPrintable(QStringLiteral(
                     "die erste Zahl ist abgeschnitten: %1 Bildpunkte "
                     "Tinte gegen %2 in der Mitte")
                                .arg(links).arg(mitte)));
        QVERIFY2(rechts * 3 >= mitte * 2,
                 qPrintable(QStringLiteral(
                     "die letzte Zahl ist abgeschnitten: %1 Bildpunkte "
                     "Tinte gegen %2 in der Mitte")
                                .arg(rechts).arg(mitte)));
    }
};

QTEST_MAIN(TstSkalaEndenLesbar)
#include "tst_skala_enden_lesbar.moc"
