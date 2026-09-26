// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test, no Thetis logic.
//
// Fensterraster (2026-09-26). Betreiber: "einen minimalen kleinen Raster
// ueber die ganze Oberflaeche, an das jedes Widget andockt ... alle auf
// gleicher Linie, Hoehe und Breite". Bis dahin rastete nur die linke
// obere Ecke; rechte und untere Kanten lagen, wo die Groesse sie
// hinstellte ("vorhanden, passt aber nicht zu 100 %").

#include <QtTest>
#include <QWidget>

#include "gui/WindowPlacement.h"

using namespace Longpath;

class TstWindowGridSnap : public QObject { Q_OBJECT
private slots:
    // Jede Kante fuer sich aufs Raster, nicht Ecke + Groesse.
    void everyEdgeSnaps()
    {
        const QRect r = snappedFrameRect(QRect(900, 100, 330, 500));
        QCOMPARE(r, QRect(904, 104, 328, 496));
        QCOMPARE(r.x() % kSnapGridPx, 0);
        QCOMPARE(r.y() % kSnapGridPx, 0);
        QCOMPARE((r.x() + r.width()) % kSnapGridPx, 0);
        QCOMPARE((r.y() + r.height()) % kSnapGridPx, 0);
    }

    // Zwei Fenster, die aneinanderliegen, bleiben buendig: die gemeinsame
    // Kante rundet bei beiden gleich. Getrennt gerundete Lage und Groesse
    // rissen sie um bis zu ein Raster auseinander.
    void neighboursStayFlush()
    {
        const QRect a(97, 150, 803, 300);          // rechte Kante 900
        const QRect b(900, 150, 331, 300);         // linke Kante 900
        const QRect sa = snappedFrameRect(a);
        const QRect sb = snappedFrameRect(b);
        QCOMPARE(sa.x() + sa.width(), sb.x());
        // Gleiche Hoehe bleibt gleiche Hoehe.
        QCOMPARE(sa.height(), sb.height());
    }

    // Nie kleiner als die Mindestgroesse und nie null.
    void respectsMinimumSize()
    {
        const QRect r = snappedFrameRect(QRect(10, 10, 3, 3), QSize(20, 20));
        QVERIFY(r.width() >= 20);
        QVERIFY(r.height() >= 20);
        QCOMPARE(r.width() % kSnapGridPx, 0);
        const QRect z = snappedFrameRect(QRect(10, 10, 2, 2));
        QVERIFY(z.width() >= kSnapGridPx);
        QVERIFY(z.height() >= kSnapGridPx);
    }

    // Ein echtes Fenster rastet nach dem Loslassen an allen vier Kanten ein.
    void aWindowSettlesOnTheGrid()
    {
        QWidget w(nullptr, Qt::Tool | Qt::FramelessWindowHint);
        w.setGeometry(101, 99, 301, 203);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.setGeometry(101, 99, 301, 203);
        snapToGridAfterSettle(&w);
        QTRY_VERIFY_WITH_TIMEOUT(w.frameGeometry().x() % kSnapGridPx == 0
                                 && w.frameGeometry().width() % kSnapGridPx == 0, 2000);
        const QRect f = w.frameGeometry();
        QCOMPARE(f.y() % kSnapGridPx, 0);
        QCOMPARE((f.x() + f.width()) % kSnapGridPx, 0);
        QCOMPARE((f.y() + f.height()) % kSnapGridPx, 0);
        // Und bleibt dort -- kein Hin und Her.
        const QRect settled = w.frameGeometry();
        QTest::qWait(500);
        QCOMPARE(w.frameGeometry(), settled);
    }
};

QTEST_MAIN(TstWindowGridSnap)
#include "tst_window_grid_snap.moc"
