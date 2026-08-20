// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_real_zoom_visible.cpp  (Longpath)
// =================================================================
// Sind die Zoomknoepfe im Panadapter zu FINDEN?
//
// Der Betreiber, 2026-08-20: „weiters sollten im pandapter die
// optionen sowohl auch vergroesserung des padapters (zoom) dort
// einfach zu finden sein."
//
// Verdacht: der Zoomstreifen ist Kind des Panadapters, und der ist
// ein QRhiWidget mit WA_NativeWindow. Auf macOS zeichnet ein natives
// NSView ueber allen nicht-nativen Kindern — siehe den Vermerk in
// SpectrumWidget.cpp:551. Dann WAEREN die Knoepfe da und trotzdem
// unsichtbar.
//
// Modification history (Longpath):
//   2026-08-20 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QPushButton>
#include "gui/MainWindow.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"

using namespace Longpath;

class TestRealZoomVisible : public QObject
{
    Q_OBJECT
private slots:
    void zoomStripIsNotHiddenBehindTheNativePanadapter()
    {
        auto* mwp = new MainWindow();
        mwp->resize(1280, 800);
        mwp->show();
        QVERIFY(QTest::qWaitForWindowExposed(mwp));
        QTest::qWait(500);

        auto* spec = mwp->findChild<SpectrumWidget*>();
        QVERIFY2(spec, "es muss einen Panadapter geben");
        const bool specNative = spec->testAttribute(Qt::WA_NativeWindow);
        qDebug() << "Panadapter nativ:" << specNative;

        auto* ov = mwp->findChild<SpectrumOverlayPanel*>();
        QVERIFY2(ov, "es muss die Bedienflaeche geben");
        qDebug() << "Bedienflaeche Elternteil:"
                 << (ov->parentWidget()
                         ? ov->parentWidget()->metaObject()->className()
                         : "keins")
                 << " sichtbar:" << ov->isVisible();

        // Den Zoomstreifen ueber seine Knoepfe finden.
        QPushButton* plus = nullptr;
        for (QPushButton* b : mwp->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("+")
                && b->toolTip().contains(QStringLiteral("Zoom in"))) {
                plus = b; break;
            }
        }
        QVERIFY2(plus, "die Zoomknoepfe muessen existieren");
        QWidget* strip = plus->parentWidget();
        qDebug() << "Zoomstreifen Elternteil:"
                 << (strip->parentWidget()
                         ? strip->parentWidget()->metaObject()->className()
                         : "keins")
                 << " sichtbar:" << strip->isVisible()
                 << " Lage:" << strip->pos() << " Groesse:" << strip->size()
                 << " nativ:" << strip->testAttribute(Qt::WA_NativeWindow);

        // DIE Frage: liegt der Streifen im nativen Panadapter, ohne
        // selbst nativ zu sein? Dann ist er auf macOS unsichtbar.
        bool insideNative = false;
        for (QWidget* w = strip->parentWidget(); w; w = w->parentWidget()) {
            if (w == spec) { insideNative = true; break; }
        }
        qDebug() << "liegt im Panadapter:" << insideNative;

#ifdef Q_OS_MAC
        QVERIFY2(!(insideNative && specNative
                   && !strip->testAttribute(Qt::WA_NativeWindow)),
                 "der Zoomstreifen liegt als NICHT-natives Kind im nativen "
                 "Panadapter — auf macOS zeichnet das native NSView darueber, "
                 "die Knoepfe sind unsichtbar (SpectrumWidget.cpp:551)");
#endif
    }
};
QTEST_MAIN(TestRealZoomVisible)
#include "tst_real_zoom_visible.moc"
