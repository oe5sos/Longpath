// SPDX-License-Identifier: GPL-3.0-or-later
//
// Kein Test — ein Werkzeug. Es malt unsere echten Bedienelemente in ein
// Blatt und daneben denselben Satz mit dem Vorschlag, damit ueber
// Gestaltung an Bildern entschieden wird und nicht an Eindruecken.
//
// Anlass: „das design von zeus hat teilweise mehr stil, was koennen wir
// tun? ich glaube die farblichen verlaeufe machen es" (2026-08-21).
// Die Verlaeufe sind seit zwei Tagen drin und nachgemessen. Der
// Unterschied liegt woanders, und das Blatt soll zeigen, wo.

#include <QtTest>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QLabel>

#include "gui/StyleConstants.h"

using namespace Longpath;

namespace {

QImage shot(QWidget* w, QSize size)
{
    w->resize(size);
    QImage img(size, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    w->render(&img);
    return img;
}

/// Ein Knopf, wie ihn das Programm heute malt.
QImage buttonNow(const QString& text, bool active)
{
    QPushButton b(text);
    b.setStyleSheet(active ? Style::blueCheckedStyle()
                           : Style::buttonBaseStyle());
    b.setCheckable(active);
    b.setChecked(active);
    return shot(&b, QSize(84, 30));
}

/// Derselbe Knopf mit dem Vorschlag: gesaettigter Akzent, mehr Luft.
QImage buttonProposed(const QString& text, bool active)
{
    QPushButton b(text);
    if (active) {
        b.setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "    stop:0 #4f90ff, stop:1 #2f6fdd);"
            "  border: 1px solid #2a5fbe; border-radius: 7px;"
            "  color: #ffffff; font-size: 11px; font-weight: bold;"
            "  padding: 4px 10px;"
            "}"));
    } else {
        b.setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "    stop:0 #23232b, stop:1 #16161b);"
            "  border: 1px solid #303038; border-radius: 7px;"
            "  color: #c8ccd4; font-size: 11px; font-weight: bold;"
            "  padding: 4px 10px;"
            "}"));
    }
    b.setCheckable(active);
    b.setChecked(active);
    return shot(&b, QSize(84, 30));
}

void row(QPainter& p, int y, const QString& caption,
         const QImage& a, const QImage& b)
{
    p.setPen(QColor(0x9a, 0xa0, 0xaa));
    QFont f = p.font();
    f.setPointSize(10);
    p.setFont(f);
    p.drawText(QRect(24, y, 200, 30), Qt::AlignVCenter | Qt::AlignLeft,
               caption);
    p.drawImage(QPoint(240, y), a);
    p.drawImage(QPoint(460, y), b);
}

int saturationOf(const QImage& img)
{
    // Die kraeftigste Farbe im Bild — das ist der Akzent.
    int best = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QColor c(img.pixel(x, y));
            if (qAlpha(img.pixel(x, y)) < 200) { continue; }
            best = qMax(best, c.hslSaturation());
        }
    }
    return best * 100 / 255;
}

} // namespace

class TstStilblatt : public QObject
{
    Q_OBJECT

private slots:
    void drawTheSheet()
    {
        const int W = 700, H = 300;
        QImage sheet(W, H, QImage::Format_ARGB32);
        sheet.fill(QColor(Style::kAppBg));

        QPainter p(&sheet);
        p.setRenderHint(QPainter::Antialiasing);

        QFont title = p.font();
        title.setPointSize(12);
        title.setBold(true);
        p.setFont(title);
        p.setPen(QColor(0xe0, 0xe4, 0xea));
        p.drawText(QRect(24, 16, 300, 24), Qt::AlignLeft, "JETZT");
        p.drawText(QRect(460, 16, 300, 24), Qt::AlignLeft, "VORSCHLAG");
        p.setPen(QColor(0x30, 0x30, 0x38));
        p.drawLine(24, 44, W - 24, 44);

        const QImage aOff = buttonNow(QStringLiteral("USB"), false);
        const QImage bOff = buttonProposed(QStringLiteral("USB"), false);
        const QImage aOn  = buttonNow(QStringLiteral("LSB"), true);
        const QImage bOn  = buttonProposed(QStringLiteral("LSB"), true);

        row(p, 70,  QStringLiteral("Knopf, nicht aktiv"), aOff, bOff);
        row(p, 120, QStringLiteral("Knopf, AKTIV"),       aOn,  bOn);

        p.setPen(QColor(0x9a, 0xa0, 0xaa));
        QFont s = p.font();
        s.setPointSize(10);
        s.setBold(false);
        p.setFont(s);
        p.drawText(QRect(24, 180, W - 48, 90), Qt::TextWordWrap,
            QStringLiteral(
                "Der Unterschied ist die Sättigung des Akzents, nicht der "
                "Verlauf. Beide Seiten tragen denselben Verlauf.\n"
                "Aktiv jetzt: %1 %  ·  Vorschlag: %2 %")
                .arg(saturationOf(aOn)).arg(saturationOf(bOn)));
        p.end();

        QVERIFY(sheet.save(QStringLiteral("/tmp/longpath-stilblatt.png")));
        qInfo() << "Saettigung aktiv jetzt:" << saturationOf(aOn) << "%"
                << " Vorschlag:" << saturationOf(bOn) << "%";
    }
};

QTEST_MAIN(TstStilblatt)
#include "tst_stilblatt.moc"
