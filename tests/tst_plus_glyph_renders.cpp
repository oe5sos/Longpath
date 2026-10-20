// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_plus_glyph_renders.cpp  (Longpath)
// =================================================================
// Das Plus, angesehen statt behauptet.
//
// „vergesse nicht auf das rendering des + fuer das hinzufuegen der
// widgets." Rendering heisst hier, wie immer bei diesem Betreiber,
// der ruhige Endschliff -- und beurteilt wird er an einem Bild in
// wirklicher Groesse, nicht an einer Beschreibung.
//
// Dieser Prueftand malt die vier Zustaende in eine Datei, damit man
// sie ansehen kann, UND haelt fest, was dabei nicht verschwinden
// darf: dass die vier ueberhaupt verschieden aussehen. Eine
// Zustandsanimation, die in allen Zustaenden dasselbe Bild liefert,
// ist keine -- und genau das merkt niemand, wenn nur die Mechanik
// geprueft wird.
//
// Modification history (Longpath):
//   2026-10-09 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>
#include <QAbstractButton>
#include <QDir>
#include <QImage>
#include <QPainter>

#include "gui/applets/AppletVisibilityController.h"
#include "gui/widgets/WidgetPicker.h"

using namespace Longpath;

namespace {

QImage shot(QWidget* w, qreal turn, qreal hover, qreal press)
{
    w->setProperty("turn",  turn);
    w->setProperty("hover", hover);
    w->setProperty("press", press);
    // Wirkliche Groesse, keine Geraeteskalierung. Vergroessert wird
    // erst das fertige Blatt -- sonst rechnet man mit zwei
    // Koordinatensystemen gleichzeitig und wundert sich.
    QImage img(w->size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(0x14, 0x16, 0x1a));
    w->render(&img, QPoint(), QRegion(), QWidget::DrawChildren);
    return img;
}

int differingPixels(const QImage& a, const QImage& b)
{
    if (a.size() != b.size()) { return -1; }
    int n = 0;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            if (a.pixel(x, y) != b.pixel(x, y)) { ++n; }
        }
    }
    return n;
}

} // namespace

class TstPlusGlyphRenders : public QObject
{
    Q_OBJECT
private slots:

    void theFourStatesLookDifferentAndAreOnFile()
    {
        AppletVisibilityController vis;
        AddWidgetButton b(&vis);
        auto* glyph = b.findChild<QAbstractButton*>();
        QVERIFY2(glyph != nullptr, "kein Plus im Knopf");

        // Kantenlaenge wie versprochen: genau die Aussenhoehe einer
        // Pille in der Kommandoleiste. Mit 34 war es einmal das
        // einzige Element der Reihe, das darueber hinausragte.
        QCOMPARE(glyph->width(),  AddWidgetButton::kSide);
        QCOMPARE(glyph->height(), AddWidgetButton::kSide);

        const QImage ruhe  = shot(glyph, 0.0, 0.0, 0.0);
        const QImage zeige = shot(glyph, 0.0, 1.0, 0.0);
        const QImage offen = shot(glyph, 1.0, 1.0, 0.0);
        const QImage druck = shot(glyph, 0.0, 1.0, 1.0);

        QVERIFY2(differingPixels(ruhe, zeige) > 50,
                 "der Zeiger darauf aendert nichts");
        QVERIFY2(differingPixels(zeige, offen) > 50,
                 "offen sieht aus wie geschlossen -- das Kreuz fehlt");
        QVERIFY2(differingPixels(zeige, druck) > 20,
                 "der Druck gibt keine Antwort");

        // Nebeneinander in eine Datei, in wirklicher Groesse und
        // vierfach. Der Pfad kommt aus der Umgebung, damit ein
        // CI-Lauf nichts in den Baum schreibt.
        const QByteArray dir = qgetenv("LONGPATH_RENDER_DIR");
        if (dir.isEmpty()) { return; }
        QDir().mkpath(QString::fromUtf8(dir));

        const int pad = 14;
        const int side = ruhe.width();
        QImage sheet(pad + 4 * (side + pad), side + 2 * pad,
                     QImage::Format_ARGB32_Premultiplied);
        sheet.fill(QColor(0x14, 0x16, 0x1a));
        {
            QPainter g(&sheet);
            int x = pad;
            for (const QImage& one : {ruhe, zeige, offen, druck}) {
                g.drawImage(QPoint(x, pad), one);
                x += side + pad;
            }
        }
        const QString base = QString::fromUtf8(dir);
        QVERIFY(sheet.save(base + "/plus-states.png"));
        // Und dasselbe Blatt achtfach, mit harten Kanten: so sieht
        // man, was eine Linie von 1,2 Pixeln wirklich tut.
        QVERIFY(sheet.scaled(sheet.size() * 8, Qt::IgnoreAspectRatio,
                             Qt::FastTransformation)
                    .save(base + "/plus-states-8x.png"));
    }
};

QTEST_MAIN(TstPlusGlyphRenders)
#include "tst_plus_glyph_renders.moc"
