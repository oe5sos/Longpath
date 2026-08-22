// SPDX-License-Identifier: GPL-3.0-or-later
//
// Kein Test — ein Werkzeug. Es malt DREI Fassungen derselben
// Oberflaeche nebeneinander, damit ueber Gestaltung an einem Bild
// entschieden wird und nicht an Eindruecken.
//
// Vorgeschichte: der Betreiber bekam am 2026-08-21 ein Entwurfsblatt
// mit Verlaufs-Vorschlaegen und sagte „sehe keinen unterschied beim
// den pdf." Die Unterschiede waren zu klein zum Sehen. Deshalb hier
// drei Fassungen, die sich WEIT auseinander bewegen, und die Zahlen
// stehen darunter.
//
// Gebaut wird mit echten Themendateien ueber die Form-Regler, die seit
// 0bae4d6a in der Themendatei stehen — es ist also genau das, was die
// gewaehlte Fassung spaeter tut, nicht eine Nachahmung davon.

#include <QtTest>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QTemporaryDir>

#include "gui/StyleConstants.h"
#include "gui/styles/Theme.h"

using namespace Longpath;

namespace {

struct Fassung {
    const char* name;
    const char* satz;      // was sie sein will
    int radius, luftV, luftH, relief, mulde;
};

const Fassung kFassungen[] = {
    {"FLACH", "ruhig, fast ohne Tiefe",        5, 3,  8,  6,  4},
    {"WEICH", "wie heute — nah an Zeus",       7, 4, 10, 16, 10},
    {"TIEF",  "deutlich plastisch",            9, 6, 14, 26, 18},
};

QImage shot(QWidget* w, QSize size)
{
    w->resize(size);
    QImage img(size * 2, QImage::Format_ARGB32);
    img.setDevicePixelRatio(2.0);
    img.fill(Qt::transparent);
    w->render(&img);
    return img;
}

/// Eine Themendatei mit genau diesen Form-Reglern.
QString schreibeThema(const QDir& dir, const Fassung& f)
{
    const QString path = dir.filePath(QStringLiteral("%1.json")
                                          .arg(QLatin1String(f.name)));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { return {}; }
    file.write(QStringLiteral(
        "{\n  \"name\": \"%1\",\n"
        "  \"formen\": { \"radius\": %2, \"luft-v\": %3, \"luft-h\": %4,\n"
        "                \"relief\": %5, \"mulde\": %6 }\n}\n")
        .arg(QLatin1String(f.name))
        .arg(f.radius).arg(f.luftV).arg(f.luftH)
        .arg(f.relief).arg(f.mulde).toUtf8());
    return path;
}

} // namespace

class TstStilblatt : public QObject
{
    Q_OBJECT

private slots:
    void drawTheSheet()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir dir(tmp.path());

        const int spalte = 300;
        const int W = spalte * 3 + 60;
        const int H = 470;

        QImage sheet(W * 2, H * 2, QImage::Format_ARGB32);
        sheet.setDevicePixelRatio(2.0);
        sheet.fill(QColor(Style::kAppBg));

        QPainter p(&sheet);
        p.setRenderHint(QPainter::Antialiasing);

        QFont kopf = p.font();
        kopf.setPointSize(13);
        kopf.setBold(true);

        QFont klein = p.font();
        klein.setPointSize(10);

        for (int i = 0; i < 3; ++i) {
            const Fassung& f = kFassungen[i];
            const QString path = schreibeThema(dir, f);
            QVERIFY(!path.isEmpty());

            QString err;
            QVERIFY2(Style::Theme::instance().loadFile(path, &err),
                     qPrintable(err));

            const int x = 30 + i * spalte;

            p.setFont(kopf);
            p.setPen(QColor(0xe4, 0xe6, 0xea));
            p.drawText(QRect(x, 22, spalte - 20, 26), Qt::AlignLeft,
                       QString::fromLatin1(f.name));

            p.setFont(klein);
            p.setPen(QColor(0x8e, 0x92, 0x9a));
            p.drawText(QRect(x, 48, spalte - 20, 22), Qt::AlignLeft,
                       QString::fromLatin1(f.satz));

            p.setPen(QColor(0x30, 0x32, 0x38));
            p.drawLine(x, 74, x + spalte - 26, 74);

            int y = 96;

            // Knopf, ruhend
            {
                QPushButton b(QStringLiteral("USB"));
                b.setStyleSheet(Style::buttonBaseStyle());
                p.drawImage(QPoint(x, y), shot(&b, QSize(96, 34)));
            }
            // Knopf, aktiv
            {
                QPushButton b(QStringLiteral("LSB"));
                b.setCheckable(true);
                b.setChecked(true);
                b.setStyleSheet(Style::buttonBaseStyle()
                                + Style::blueCheckedStyle());
                p.drawImage(QPoint(x + 110, y), shot(&b, QSize(96, 34)));
            }
            y += 52;

            // Eingabefeld — das, worauf der Betreiber gezeigt hat
            {
                QLineEdit e(QStringLiteral("2850"));
                e.setStyleSheet(Style::lineEditStyle());
                p.drawImage(QPoint(x, y), shot(&e, QSize(206, 32)));
            }
            y += 50;

            // Eine Platte mit einer Mulde darin — der Kern des Ganzen
            {
                QWidget card;
                card.setStyleSheet(QStringLiteral(
                    "QWidget { background: %1; border: 1px solid %2;"
                    " border-radius: %3px; }")
                    .arg(Style::raisedFill(Style::kPanelBg),
                         Style::hexRole(Style::kBorder))
                    .arg(Style::formInt("radius", 7)));
                p.drawImage(QPoint(x, y), shot(&card, QSize(206, 76)));
            }
            y += 92;

            p.setFont(klein);
            p.setPen(QColor(0x9a, 0x9e, 0xa6));
            p.drawText(QRect(x, y, spalte - 24, 90), Qt::TextWordWrap,
                       QStringLiteral(
                           "Ecken %1 · Luft %2/%3\nRelief %4 · Mulde %5")
                           .arg(f.radius).arg(f.luftV).arg(f.luftH)
                           .arg(f.relief).arg(f.mulde));
        }

        Style::Theme::instance().clear();
        p.end();

        QVERIFY(sheet.save(QStringLiteral("/tmp/longpath-stilblatt.png")));
        qInfo() << "Blatt geschrieben:" << sheet.size();
    }
};

QTEST_MAIN(TstStilblatt)
#include "tst_stilblatt.moc"
