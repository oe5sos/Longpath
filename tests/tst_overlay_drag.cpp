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

    // ── Getroffen wird, wo gezeichnet wurde ─────────────────────────
    //
    // Der Betreiber, 2026-08-21: „verschieben funktioniert nicht."
    //
    // Gezeichnet wurde in `w - effectiveStripW()` (ohne die dBm-Leiste
    // rechts), getroffen in der VOLLEN Breite. Die Trefferflaeche lag
    // damit neben dem Bild — bei der rechten Einblendung um die
    // Streifenbreite daneben.
    //
    // Der erste Test bestand trotzdem: er benutzte dieselbe falsche
    // Rechnung wie der Code, den er prueft. Deshalb prueft dieser hier
    // nicht die Rechnung, sondern die FOLGE: ein Druck auf die rechte
    // Kante der Einblendung muss sie greifen.
    void theHitAreaSitsWhereTheOverlayIsDrawn()
    {
        auto* w = new SpectrumWidget();
        w->resize(1000, 520);
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w));

        QImage img(200, 200, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(255, 0, 255, 220));
        w->setOverlayImage(SpectrumWidget::OverlaySlot::Swr, img);
        w->setOverlayVisible(SpectrumWidget::OverlaySlot::Swr, true);
        // Ganz nach rechts — dort ist der Unterschied am groessten.
        w->setOverlayPos(SpectrumWidget::OverlaySlot::Swr, QPointF(0.90, 0.60));
        QTest::qWait(60);

        const QRect r = w->overlayRectNow(SpectrumWidget::OverlaySlot::Swr);
        QVERIFY(!r.isEmpty());
        QVERIFY2(r.right() <= w->spectrumRect().right(),
                 qPrintable(QStringLiteral(
                     "die Einblendung MUSS innerhalb der gezeichneten "
                     "Flaeche liegen: rechts bei %1, Flaeche endet bei %2")
                     .arg(r.right()).arg(w->spectrumRect().right())));

        const QPointF before = w->overlayPos(SpectrumWidget::OverlaySlot::Swr);
        // An der rechten INNENkante anfassen — genau der Bereich, der
        // bei der falschen Rechnung ins Leere ging.
        const QPointF grab(r.right() - 4, r.center().y());
        auto send = [w](QEvent::Type t, QPointF pos, Qt::MouseButtons b) {
            QMouseEvent ev(t, pos, w->mapToGlobal(pos), Qt::LeftButton, b,
                           Qt::NoModifier);
            QApplication::sendEvent(w, &ev);
        };
        send(QEvent::MouseButtonPress, grab, Qt::LeftButton);
        send(QEvent::MouseMove, grab + QPointF(-200, 0), Qt::LeftButton);
        send(QEvent::MouseButtonRelease, grab + QPointF(-200, 0), Qt::NoButton);

        const QPointF after = w->overlayPos(SpectrumWidget::OverlaySlot::Swr);
        QVERIFY2(after.x() < before.x() - 0.05,
                 qPrintable(QStringLiteral(
                     "ein Druck auf die Einblendung MUSS sie greifen "
                     "(%1 -> %2)").arg(before.x()).arg(after.x())));
        w->hide();
        delete w;
    }

    // Und der dritte Platz ist wirklich da.
    void thereAreThreeSlots()
    {
        auto* w = new SpectrumWidget();
        w->setOverlayVisible(SpectrumWidget::OverlaySlot::SMeter, true);
        QVERIFY2(w->overlayVisible(SpectrumWidget::OverlaySlot::SMeter),
                 "das S-Meter MUSS als eigener Platz existieren");
        // Und es hat eine eigene Lage, nicht die der anderen.
        w->setOverlayPos(SpectrumWidget::OverlaySlot::SMeter,
                         QPointF(0.44, 0.22));
        QVERIFY(qAbs(w->overlayPos(SpectrumWidget::OverlaySlot::SMeter).x()
                     - 0.44) < 0.01);
        QVERIFY2(qAbs(w->overlayPos(SpectrumWidget::OverlaySlot::Compass).x()
                      - 0.44) > 0.05,
                 "jeder Platz hat seine EIGENE Lage");
        delete w;
    }
};
QTEST_MAIN(TestOverlayDrag)
#include "tst_overlay_drag.moc"
