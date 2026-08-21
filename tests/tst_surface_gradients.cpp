// SPDX-License-Identifier: GPL-3.0-or-later
//
// Prueft die Flaechen mit Verlauf — am gemalten Bild, nicht am Quelltext.
//
// Vorgeschichte: der Betreiber hat ein Entwurfsblatt mit mehreren
// Verlaufs-Vorschlaegen bekommen und gesagt: „sehe keinen unterschied
// beim den pdf." Der Unterschied war zu klein zum Sehen. Ein Test, der
// nur nachsieht, ob das Wort „qlineargradient" im Stil steht, haette
// genau diesen Fehler durchgelassen. Also wird hier ein echter Knopf
// gemalt und nachgemessen, ob oben und unten SICHTBAR verschieden sind.

#include <QtTest>
#include <QPushButton>
#include <QImage>
#include <QFile>
#include <QTemporaryDir>
#include <QRegularExpression>

#include "gui/StyleConstants.h"
#include "gui/styles/Theme.h"
#include "gui/styles/ThemeQss.h"

namespace {

/// Mittlere Helligkeit einer Bildzeile, Rand ausgespart.
int rowLightness(const QImage& img, int y)
{
    long sum = 0;
    int n = 0;
    for (int x = 6; x < img.width() - 6; ++x) {
        sum += QColor(img.pixel(x, y)).lightness();
        ++n;
    }
    return n > 0 ? static_cast<int>(sum / n) : 0;
}

QImage paintButton()
{
    QPushButton b(QStringLiteral("XX"));
    b.setStyleSheet(Longpath::Style::buttonBaseStyle());
    b.resize(80, 40);
    QImage img(b.size(), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    b.render(&img);
    return img;
}

} // namespace

class TstSurfaceGradients : public QObject
{
    Q_OBJECT

private slots:
    /// Oben heller als unten — und zwar so deutlich, dass man es sieht.
    void theSurfaceIsLitFromAbove()
    {
        const QImage img = paintButton();
        const int top    = rowLightness(img, 4);
        const int bottom = rowLightness(img, img.height() - 5);

        // Die Schwelle ist der eigentliche Inhalt dieses Tests. Sechs
        // Stufen Helligkeit sind auf einem Bildschirm nachweisbar, aber
        // nicht sichtbar — genau das war das Entwurfsblatt, das der
        // Betreiber nicht unterscheiden konnte. Zwanzig sieht man.
        QVERIFY2(top - bottom >= 20,
                 qPrintable(QStringLiteral(
                     "Verlauf zu schwach: oben %1, unten %2, Abstand %3 "
                     "(mindestens 20 noetig, sonst sieht man nichts)")
                     .arg(top).arg(bottom).arg(top - bottom)));
    }

    /// Die Mulde geht andersherum, sonst waere sie keine Mulde.
    void theHollowRunsTheOtherWay()
    {
        const QString sunk = Longpath::Style::sunkenFill(Longpath::Style::kInsetBg);
        const QString rise = Longpath::Style::raisedFill(Longpath::Style::kButtonBg);
        QVERIFY(sunk.contains(QLatin1String("qlineargradient")));
        QVERIFY(rise.contains(QLatin1String("qlineargradient")));
        QVERIFY2(sunk != rise, "Mulde und Platte duerfen nicht gleich sein");
    }

    /// Die Stufen muessen dem Thema folgen, nicht fest im Quelltext stehen.
    ///
    /// Der naheliegende Fehler waere gewesen, "#22222a" hinzuschreiben.
    /// Die Themenverwaltung tauscht ueber den Hexwert aus und kennt so
    /// einen Wert nicht — im hellen Thema "Kreide" waere der Knopf
    /// dunkel geblieben. Also wird hier ein echtes helles Thema geladen
    /// und nachgesehen, ob der Knopf mitgeht.
    void theStepsFollowTheTheme()
    {
        const QString darkFill = Longpath::Style::raisedFill(Longpath::Style::kButtonBg);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("hell.json"));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QByteArray("{\n"
                               "  \"name\": \"Pruefthema hell\",\n"
                               "  \"colors\": { \"button\": \"#e8e8ec\" }\n"
                               "}\n"));
        }

        QString err;
        QVERIFY2(Longpath::Style::Theme::instance().loadFile(path, &err),
                 qPrintable(err));
        const QString lightFill = Longpath::Style::raisedFill(Longpath::Style::kButtonBg);
        Longpath::Style::Theme::instance().clear();

        QVERIFY2(darkFill != lightFill,
                 "Der Verlauf haengt nicht am Thema — feste Hexwerte?");

        // Und er ist im hellen Thema auch wirklich hell.
        static const QRegularExpression re(QStringLiteral("#[0-9a-fA-F]{6}"));
        const QRegularExpressionMatch m = re.match(lightFill);
        QVERIFY(m.hasMatch());
        QVERIFY2(QColor(m.captured(0)).lightness() > 160,
                 qPrintable(QStringLiteral("Heller Knopf ist dunkel: %1")
                                .arg(m.captured(0))));
    }
};

QTEST_MAIN(TstSurfaceGradients)
#include "tst_surface_gradients.moc"
