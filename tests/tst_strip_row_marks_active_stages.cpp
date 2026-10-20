// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_strip_row_marks_active_stages.cpp  (Longpath)
// =================================================================
// „wenn ich ein feld aktiviere sollte das feld oben als ganze
//  markiert sein, andere farbe."
//
// Bis zum 2026-10-10 wechselte nur der Rahmen -- ein Pixel breit, auf
// Schwarz, aus zwei Metern nichts. Der Prueftand misst deshalb nicht,
// ob irgendwo eine Farbe gesetzt wird, sondern was auf der FLAECHE
// der Kachel ankommt: ist sie blau, ist sie dunkel, und liegt der
// geparkte Zustand dazwischen.
//
// Gegenprobe gefahren: mit der alten Fuellung (kButtonBg #1a1a1e)
// faellt „blau genug" durch -- dort ist Blau nur vier Stufen ueber
// Rot. Ein Prueftand, der auch die alte Fassung bestanden haette,
// haette hier nichts bewiesen.
//
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>
#include <QImage>
#include <QPalette>

#include "core/strip/StripChain.h"
#include "gui/applets/StripGraphics.h"

using namespace Longpath;

namespace {

// Dieselben Masse wie in StripGraphics.cpp. Sie stehen dort in einem
// anonymen Namensraum; hier nachgebaut statt freigelegt, damit die
// Anzeige ihre Innereien nicht fuer einen Prueftand oeffnen muss.
constexpr int kTileW   = 84;   // Wunschbreite; gilt ab 1400 px Reihe
constexpr int kTileH   = 46;
constexpr int kGapW    = 14;
constexpr int kTileTop = 4;

QRect tileRect(int stage)
{
    return QRect(kGapW + stage * (kTileW + kGapW), kTileTop, kTileW, kTileH);
}

// Ohne das hier malt render() den hellen Standardgrund des Systems
// zwischen die Kacheln -- im Programm liegt die Reihe auf dem dunklen
// Panel. Ein Prueftand auf hellem Grund misst eine Oberflaeche, die es
// nicht gibt (hier zuerst passiert: die Suche nach dem Ring fand 100
// helle Punkte, bevor es einen Ring gab).
void darkBackground(QWidget& w)
{
    QPalette pal = w.palette();
    pal.setColor(QPalette::Window, QColor(0x0c, 0x0c, 0x0e));
    w.setPalette(pal);
    w.setAutoFillBackground(true);
}

int blueLead(const QColor& c) { return c.blue() - c.red(); }
int brightness(const QColor& c) { return (c.red() + c.green() + c.blue()) / 3; }

} // namespace

class TstStripRowMarksActiveStages : public QObject {
    Q_OBJECT

private slots:
    // Eine eingeschaltete Stufe traegt die Auswahlfarbe des Hauses --
    // auf der ganzen Flaeche, nicht nur am Rand.
    void activeTileIsFilledBlue()
    {
        StripChain c;
        c.prepare(48000.0);
        c.setEnabled(true);
        c.setStageEnabled(StripChain::Stage::Gate, true);
        c.setStageEnabled(StripChain::Stage::DeEss, false);

        StripChainView v;
        darkBackground(v);
        v.setChain(&c);
        // Breit genug, dass die Kacheln ihre Wunschbreite bekommen --
        // seit die Reihe zehn Stufen hat, macht sie sich in einem
        // engen Fenster schmaler, und dann stimmen die Masse unten
        // nicht mehr.
        v.resize(1400, v.sizeHint().height());

        QImage img(v.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(0x0c, 0x0c, 0x0e));
        v.render(&img);

        const QRect on  = tileRect(static_cast<int>(StripChain::Stage::Gate));
        const QRect off = tileRect(static_cast<int>(StripChain::Stage::DeEss));

        // Zwei Stellen je Kachel: oben und unten. Die Fuellung ist ein
        // Verlauf, und eine Markierung, die nur am oberen Rand
        // stattfindet, ist die Markierung nicht, die verlangt war.
        for (const int dy : {10, 40}) {
            const QColor a = img.pixelColor(on.left() + 6, on.top() + dy);
            const QColor b = img.pixelColor(off.left() + 6, off.top() + dy);

            QVERIFY2(blueLead(a) > 40,
                     qPrintable(QStringLiteral("eingeschaltet, dy=%1: %2")
                                    .arg(dy).arg(a.name())));
            QVERIFY2(brightness(a) > brightness(b) + 30,
                     qPrintable(QStringLiteral("zu nah an aus, dy=%1: %2 vs %3")
                                    .arg(dy).arg(a.name(), b.name())));
            // Und die ausgeschaltete bleibt, was sie war.
            QVERIFY2(brightness(b) < 20,
                     qPrintable(QStringLiteral("aus ist nicht dunkel, dy=%1: %2")
                                    .arg(dy).arg(b.name())));
        }
    }

    // Eingeschaltet, aber die Kette laeuft nicht: dieselbe Markierung,
    // geparkt. Weder wie „aus" noch wie „arbeitet" -- sonst schliesst
    // der Betreiber auf ein kaputtes Programm.
    void parkedStageSitsBetween()
    {
        const auto sample = [](bool master) {
            StripChain c;
            c.prepare(48000.0);
            c.setEnabled(master);
            c.setStageEnabled(StripChain::Stage::Gate, true);
            StripChainView v;
            darkBackground(v);
            v.setChain(&c);
            v.resize(1400, v.sizeHint().height());
            QImage img(v.size(), QImage::Format_ARGB32_Premultiplied);
            img.fill(QColor(0x0c, 0x0c, 0x0e));
            v.render(&img);
            const QRect r = tileRect(static_cast<int>(StripChain::Stage::Gate));
            return img.pixelColor(r.left() + 6, r.top() + 24);
        };

        const QColor running = sample(true);
        const QColor parked  = sample(false);

        QVERIFY2(brightness(parked) < brightness(running) - 20,
                 qPrintable(QStringLiteral("geparkt wie laufend: %1 vs %2")
                                .arg(parked.name(), running.name())));
        // Die Schwelle kommt von einem Befund am Geraet: mit der
        // ersten Mischung (0,30) war die geparkte Kachel fast Schwarz
        // -- und genau so steht das Fenster da, solange kein Funkgeraet
        // haengt. „funktioniert grafisch leider nicht, nur die font
        // wird dicker oder andersfaerbig." Sie muss ein blaues Feld
        // sein, nicht eine Ahnung davon.
        QVERIFY2(brightness(parked) > 30,
                 qPrintable(QStringLiteral("geparkt wie aus: %1")
                                .arg(parked.name())));
        QVERIFY2(blueLead(parked) > 30,
                 qPrintable(QStringLiteral("geparkt ohne Blau: %1")
                                .arg(parked.name())));
    }

    // Und so sieht es aus -- beide Zustaende auf ein Blatt, wirkliche
    // Groesse. Beurteilt wird Gestaltung am Bild, nicht an der
    // Beschreibung.
    void bothStatesOnFile()
    {
        const QByteArray dir = qgetenv("LONGPATH_RENDER_DIR");
        if (dir.isEmpty()) { QSKIP("kein LONGPATH_RENDER_DIR"); }

        const auto shot = [&](bool master, const QString& file) {
            StripChain c;
            c.prepare(48000.0);
            c.setEnabled(master);
            for (int i = 0; i < StripChain::kStageCount; ++i) {
                c.setStageEnabled(static_cast<StripChain::Stage>(i),
                                  i % 2 == 0);
            }
            StripChainView v;
            darkBackground(v);
            v.setChain(&c);
            v.resize(1400, v.sizeHint().height());
            v.setOpenStage(0);
            QImage img(v.size(), QImage::Format_ARGB32_Premultiplied);
            img.fill(QColor(0x0c, 0x0c, 0x0e));
            v.render(&img);
            QVERIFY(img.save(QString::fromUtf8(dir) + QLatin1Char('/') + file));
        };
        shot(true,  QStringLiteral("reihe-kette-an.png"));
        shot(false, QStringLiteral("reihe-kette-aus.png"));
    }

    // Der Ring um die offene Seite. Er liegt ausserhalb der Kachel,
    // also wird auch dort gesucht -- und er darf nur dort sein, wo er
    // hingehoert.
    void openPageGetsARing()
    {
        StripChain c;
        c.prepare(48000.0);
        c.setEnabled(true);

        StripChainView v;
        darkBackground(v);
        v.setChain(&c);
        // Breit genug, dass die Kacheln ihre Wunschbreite bekommen --
        // seit die Reihe zehn Stufen hat, macht sie sich in einem
        // engen Fenster schmaler, und dann stimmen die Masse unten
        // nicht mehr.
        v.resize(1400, v.sizeHint().height());

        const auto ringAt = [&](int stage) {
            QImage img(v.size(), QImage::Format_ARGB32_Premultiplied);
            img.fill(QColor(0x0c, 0x0c, 0x0e));
            v.render(&img);
            const QRect r = tileRect(stage);
            int bright = 0;
            for (int y = r.top() + 10; y < r.bottom() - 10; ++y) {
                for (int x = r.left() - 4; x < r.left(); ++x) {
                    if (brightness(img.pixelColor(x, y)) > 120) { ++bright; }
                }
            }
            return bright;
        };

        QCOMPARE(ringAt(2), 0);
        v.setOpenStage(2);
        QVERIFY2(ringAt(2) > 4, "kein Ring an der offenen Seite");
        QCOMPARE(ringAt(4), 0);

        v.setOpenStage(-1);
        QCOMPARE(ringAt(2), 0);
    }
};

QTEST_MAIN(TstStripRowMarksActiveStages)
#include "tst_strip_row_marks_active_stages.moc"
