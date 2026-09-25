// =================================================================
// tests/tst_zoom_buttons_legible.cpp  (Longpath)
// =================================================================
//
// Die vier Zoomknoepfe unten links im Wasserfall ([S] [B] [-] [+])
// muessen ihre Beschriftung auch ZEIGEN koennen.
//
// Befund 2026-09-25 (Betreiber, am Bildschirm): "fuer was sind
// eigentlich die 4 Quadrate" — vier leere Kaestchen. Die Knoepfe sind
// 24x20 gross, der Stil setzte aber keinen Innenabstand, und so blieb
// der Standard-Innenabstand eines Knopfes stehen: fuer das Zeichen
// blieb kein Platz, es wurde weggeschnitten. Das Vorbild (AetherSDR
// SpectrumWidget.cpp:2162-2168 [@d58e2b8a]) setzt ausdruecklich
// "padding: 0; margin: 0; min-width: 0;" — beim Uebernehmen verloren.
//
// Warum es erst seit dem 2026-09-18 auffiel: seit "Glas & Tiefe" gibt
// der app-weite Grundstil (applyAppBaselineQss, Style::kButtonStyle)
// JEDEM QPushButton "padding: 4px 12px". Ein Widget-Stil, der padding
// nicht selbst nennt, erbt das — 24 px Knopf minus 24 px Innenabstand.
// Darum laeuft dieser Test mit genau diesem Grundstil (und Fusion, wie
// main.cpp); ohne ihn ist die Textflaeche 22x18 und alles scheint gut.
//
// Geprueft wird die Flaeche, die der Stil dem Text wirklich laesst
// (SE_PushButtonContents), gegen die Groesse des Zeichens. Das ist
// unabhaengig davon, ob die Testumgebung Schriften zeichnen kann.
// =================================================================

#include <QtTest/QtTest>
#include <QPushButton>
#include <QStyle>
#include <QStyleOptionButton>

#include "core/AppSettings.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/styles/AppTheme.h"

#include <QApplication>
#include <QStyleFactory>

using namespace Longpath;

class TstZoomButtonsLegible : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Wie main.cpp: Fusion, dunkle Palette, app-weiter Grundstil.
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
    }

    void init() { AppSettings::instance().clear(); }

    void everyZoomButtonHasRoomForItsLabel()
    {
        QWidget host;
        host.resize(900, 400);
        auto* panel = new SpectrumOverlayPanel(&host);
        Q_UNUSED(panel);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));

        const QStringList names = {
            QStringLiteral("panZoomSegBtn"), QStringLiteral("panZoomBandBtn"),
            QStringLiteral("panZoomOutBtn"), QStringLiteral("panZoomInBtn")};
        for (const QString& name : names) {
            auto* btn = host.findChild<QPushButton*>(name);
            QVERIFY2(btn, qPrintable(QStringLiteral("kein Knopf %1").arg(name)));
            btn->ensurePolished();

            QStyleOptionButton opt;
            opt.initFrom(btn);
            opt.text = btn->text();
            const QRect contents =
                btn->style()->subElementRect(QStyle::SE_PushButtonContents, &opt, btn);
            const QFontMetrics fm(btn->font());
            const QRect glyph = fm.tightBoundingRect(btn->text());

            qDebug() << name << "Knopf" << btn->size() << "Textflaeche" << contents.size()
                     << "Zeichen" << btn->text() << glyph.size();

            QVERIFY2(!btn->text().isEmpty(), qPrintable(name + QStringLiteral(" ohne Beschriftung")));
            QVERIFY2(contents.width() >= glyph.width() && contents.height() >= glyph.height(),
                     qPrintable(QStringLiteral("%1: Textflaeche %2x%3 zu klein fuer das "
                                               "Zeichen %4x%5 — es wird weggeschnitten")
                                    .arg(name)
                                    .arg(contents.width()).arg(contents.height())
                                    .arg(glyph.width()).arg(glyph.height())));
        }
    }
};

QTEST_MAIN(TstZoomButtonsLegible)
#include "tst_zoom_buttons_legible.moc"
