// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_overlay_drag.cpp  (Longpath)
// =================================================================
// Die Einblendungen lassen sich schieben, wachsen und leuchten.
//
// Der Betreiber, 2026-08-21: „man sollte diese im pandapter
// verschieben koennen, noch groesser machen koennen und auch heller."
//
// Modification history (Longpath):
//   2026-08-21 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include "gui/SpectrumWidget.h"
#include "core/AppSettings.h"

using namespace Longpath;

class TestOverlayDrag : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("test-overlay-drag"));
    }

    void draggingMovesTheOverlay()
    {
        auto* w = new SpectrumWidget();
        w->resize(900, 500);
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w));

        // Ein Bild, damit es ueberhaupt etwas zu treffen gibt.
        QImage img(200, 200, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(255, 0, 255, 220));
        w->setOverlayImage(SpectrumWidget::OverlaySlot::Compass, img);
        w->setOverlayVisible(SpectrumWidget::OverlaySlot::Compass, true);
        w->setOverlayPos(SpectrumWidget::OverlaySlot::Compass,
                         QPointF(0.30, 0.50));
        QTest::qWait(60);

        const QPointF before =
            w->overlayPos(SpectrumWidget::OverlaySlot::Compass);

        // Auf die Mitte der Einblendung druecken — und zwar dort, wo
        // das Widget selbst sie sieht.
        //
        // Erste Fassung rechnete die Hoehe des Spektrums nach
        // (height * 3/4) und griff daneben: der Test fiel durch,
        // obwohl das Ziehen stimmte. Eine nachgebaute Rechnung ist
        // eine zweite, die auseinanderlaufen kann — genau der Fehler,
        // den overlayRect() im Programm vermeidet.
        const QRect r = w->overlayRectNow(SpectrumWidget::OverlaySlot::Compass);
        QVERIFY2(!r.isEmpty(), "die Einblendung muss eine Flaeche haben");
        const QPointF grab(r.center());

        auto send = [w](QEvent::Type t, QPointF pos, Qt::MouseButtons b) {
            QMouseEvent ev(t, pos, w->mapToGlobal(pos), Qt::LeftButton, b,
                           Qt::NoModifier);
            QApplication::sendEvent(w, &ev);
        };
        send(QEvent::MouseButtonPress, grab, Qt::LeftButton);
        send(QEvent::MouseMove, grab + QPointF(180, -60), Qt::LeftButton);
        send(QEvent::MouseButtonRelease, grab + QPointF(180, -60), Qt::NoButton);

        const QPointF after =
            w->overlayPos(SpectrumWidget::OverlaySlot::Compass);
        QVERIFY2(after.x() > before.x() + 0.05,
                 qPrintable(QStringLiteral(
                     "ein Zug nach rechts MUSS die Einblendung nach rechts "
                     "bewegen (%1 -> %2)").arg(before.x()).arg(after.x())));
        QVERIFY2(after.y() < before.y(),
                 "und nach oben ziehen muss sie nach oben bewegen");
        w->hide();
        delete w;
    }

    // Und die neuen Grenzen gelten wirklich.
    void theLimitsAllowBiggerAndBrighter()
    {
        auto* w = new SpectrumWidget();
        w->setOverlayScalePercent(60);
        QCOMPARE(w->overlayScalePercent(), 60);
        w->setOverlayScalePercent(999);
        QVERIFY2(w->overlayScalePercent() == 60,
                 "ueber 60 % bliebe kein Spektrum mehr uebrig");

        w->setOverlayOpacityPercent(150);
        QCOMPARE(w->overlayOpacityPercent(), 150);
        delete w;
    }

    // Die Lage ueberlebt den Neustart.
    void thePositionIsRemembered()
    {
        {
            auto* w = new SpectrumWidget();
            w->setOverlayPos(SpectrumWidget::OverlaySlot::Swr,
                             QPointF(0.42, 0.31));
            delete w;
        }
        auto* again = new SpectrumWidget();
        const QPointF p = again->overlayPos(SpectrumWidget::OverlaySlot::Swr);
        QVERIFY2(qAbs(p.x() - 0.42) < 0.01 && qAbs(p.y() - 0.31) < 0.01,
                 qPrintable(QStringLiteral(
                     "die Lage MUSS den Neustart ueberleben, kam als "
                     "%1,%2 zurueck").arg(p.x()).arg(p.y())));
        delete again;
    }
};
QTEST_MAIN(TestOverlayDrag)
#include "tst_overlay_drag.moc"
