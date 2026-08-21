// SPDX-License-Identifier: GPL-3.0-or-later
//
// Das Malen fuer die Einblendung im Panadapter darf die Instrumente in
// der Spalte nicht veraendern.
//
// Vorgeschichte, 2026-08-21. Der Betreiber schickte ein Bildschirmfoto:
// die Rose im Rotor/Log-Feld auf Briefmarkengroesse zusammengefallen,
// darunter ein leeres Loch. „den rotor bitte links wieder einblenden."
//
// Ursache war eine Zeile, die ich selbst geschrieben hatte:
//
//     resize(sidePx, sidePx);   // mitten im Layout
//     render(&img, …);
//     resize(was);
//
// Auf einem Widget in einem Layout ist resize() kein Vorschlag,
// sondern ein Eingriff, den das Layout beantwortet — und die
// Einblendung malt alle 500 ms neu. Derselbe Fehler stand in
// InstrumentApplet, dort noch unentdeckt, MIT einem Kommentar, der das
// Gegenteil behauptete („das Instrument in der Spalte soll davon
// nichts merken").
//
// Dieser Test prueft nicht den Quelltext, sondern das Verhalten: nach
// zwoelf Durchgaengen muss das Instrument noch genauso gross sein.

#include <QtTest>
#include <QImage>
#include <QVBoxLayout>
#include <QWidget>

#include "gui/applets/InstrumentApplet.h"
#include "gui/meters/MeterPoller.h"

using namespace Longpath;

namespace {

int inkOf(const QImage& img)
{
    int ink = 0;
    for (int y = 0; y < img.height(); y += 2) {
        for (int x = 0; x < img.width(); x += 2) {
            if (qAlpha(img.pixel(x, y)) > 40) { ++ink; }
        }
    }
    return ink;
}

} // namespace

class TstOverlayRenderIsHarmless : public QObject
{
    Q_OBJECT

private:
    /// Ein Instrument in einem echten Layout, so wie es in der Spalte
    /// steht — nicht freistehend. Freistehend gaebe es den Fehler gar
    /// nicht, und der Test waere wertlos.
    void checkOne(InstrumentApplet::Form form, const char* what)
    {
        QWidget host;
        auto* col = new QVBoxLayout(&host);
        col->setContentsMargins(0, 0, 0, 0);
        auto* inst = new InstrumentApplet(QStringLiteral("probe"), QStringLiteral("Probe"), nullptr, &host);
        // OHNE Messgroesse malt ein Instrument gar nichts — und dann
        // prueft der Groessenvergleich unten nichts, weil das Malen nie
        // stattfindet. Beim ersten Lauf war genau das der Fall: das
        // Bild kam leer zurueck, und die bestandene Groessenpruefung
        // war wertlos.
        QVERIFY2(inst->setPrimary(MeterBinding::SignalAvg),
                 "Messgroesse liess sich nicht setzen");
        inst->setForm(form);
        col->addWidget(inst, 1);
        host.resize(360, 320);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QCoreApplication::processEvents();

        const QSize before = inst->size();
        QVERIFY2(before.height() > 120,
                 qPrintable(QStringLiteral("%1: vorher schon zu klein: %2x%3")
                                .arg(QLatin1String(what))
                                .arg(before.width()).arg(before.height())));

        int lastInk = 0;
        for (int i = 0; i < 12; ++i) {
            const QImage img = inst->renderTransparent(240);
            QVERIFY(!img.isNull());
            lastInk = inkOf(img);
            QCoreApplication::processEvents();
        }

        QVERIFY2(inst->size() == before,
                 qPrintable(QStringLiteral(
                     "%1 ist vom Einblenden geschrumpft: %2x%3 "
                     "(vorher %4x%5)")
                     .arg(QLatin1String(what))
                     .arg(inst->width()).arg(inst->height())
                     .arg(before.width()).arg(before.height())));

        // Und die Einblendung ist nicht leer — sonst waere der Fehler
        // nur verlagert.
        QVERIFY2(lastInk > 200,
                 qPrintable(QStringLiteral("%1: Einblendung fast leer (%2)")
                                .arg(QLatin1String(what)).arg(lastInk)));
    }

private slots:
    void theNeedleInTheColumnKeepsItsSize()
    {
        checkOne(InstrumentApplet::Form::Needle, "Zeiger");
    }

    void theBarInTheColumnKeepsItsSize()
    {
        checkOne(InstrumentApplet::Form::Bar, "Balken");
    }

    /// Das Bild darf nicht quadratisch verzerrt sein.
    ///
    /// Ein Zeigerbogen ist breiter als hoch. In ein Quadrat gepresst
    /// steht er verzogen ueber dem Spektrum — und genau das war der
    /// naheliegende Weg, den Fehler oben zu beheben.
    void theOverlayKeepsTheShapeOfTheInstrument()
    {
        QWidget host;
        auto* col = new QVBoxLayout(&host);
        auto* inst = new InstrumentApplet(QStringLiteral("probe"), QStringLiteral("Probe"), nullptr, &host);
        QVERIFY(inst->setPrimary(MeterBinding::SignalAvg));
        inst->setForm(InstrumentApplet::Form::Needle);
        col->addWidget(inst, 1);
        host.resize(400, 240);           // deutlich breiter als hoch
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QCoreApplication::processEvents();

        const QImage img = inst->renderTransparent(240);
        QVERIFY(!img.isNull());
        QVERIFY2(img.width() > img.height(),
                 qPrintable(QStringLiteral(
                     "In ein Quadrat gepresst: %1x%2")
                     .arg(img.width()).arg(img.height())));
    }
};

QTEST_MAIN(TstOverlayRenderIsHarmless)
#include "tst_overlay_render_is_harmless.moc"
