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

#include "gui/StyleConstants.h"
#include "gui/WindowPlacement.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstWindowGridSnap : public QObject { Q_OBJECT
private slots:
    // Jede Kante fuer sich aufs Raster, nicht Ecke + Groesse.
    void everyEdgeSnaps()
    {
        // Nachgerechnet fuer kSnapGridPx = 16 (kaufmaennisch, lround):
        // links  lround(900/16)=56  -> 896   oben   lround(100/16)=6  ->  96
        // rechts lround(1230/16)=77 -> 1232  unten  lround(600/16)=38 -> 608
        // Mit der alten Weite 8 ergibt dieselbe Rechnung 904/104/328/496,
        // also genau die Zahlen, die bis zum 2026-10-20 hier standen --
        // die Erwartung ist gerechnet, nicht vom Lauf abgeschrieben.
        //
        // 2026-10-31: die FERNE Kante liegt seither um die Fensterluft
        // VOR der Linie (`Style::kFensterLuftPx`), die nahe weiter auf
        // ihr. So bringt das Raster den Abstand selbst mit, statt dass
        // ihn `mitMindestabstand` danach aufschlaegt und das Fenster
        // damit dauerhaft neben das Raster schiebt — der Befund des
        // Betreibers vom selben Tag.
        const int luft = Style::kFensterLuftPx;
        const QRect r = snappedFrameRect(QRect(900, 100, 330, 500));
        QCOMPARE(r, QRect(896, 96, 336 - luft, 512 - luft));
        QCOMPARE(r.x() % kSnapGridPx, 0);
        QCOMPARE(r.y() % kSnapGridPx, 0);
        QCOMPARE((r.x() + r.width() + luft) % kSnapGridPx, 0);
        QCOMPARE((r.y() + r.height() + luft) % kSnapGridPx, 0);
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
        // Nicht mehr buendig, sondern genau eine Fensterluft dazwischen
        // — und das ist der Punkt: der Abstand steckt im Raster, also
        // haengen BEIDE Kanten weiter an derselben Linie.
        QCOMPARE(sa.x() + sa.width() + Style::kFensterLuftPx, sb.x());
        // Gleiche Hoehe bleibt gleiche Hoehe.
        QCOMPARE(sa.height(), sb.height());
    }

    // Und der Abstand bleibt, wenn `mitMindestabstand` danach noch
    // darueberschaut: er findet genau die Luft vor, die er verlangt,
    // und ruehrt nichts an. Vorher schob er hier um 2 Punkte — und
    // genau dadurch fiel das Fenster neben das Raster.
    void derAbstandUeberlebtDieNachschau()
    {
        const QRect a = snappedFrameRect(QRect(97, 150, 803, 300));
        const QRect b = snappedFrameRect(QRect(900, 150, 331, 300));
        const QRect nachher = mitMindestabstand(b, {a}, Style::kFensterLuftPx);
        QCOMPARE(nachher, b);
        QCOMPARE(nachher.x() % kSnapGridPx, 0);
    }

    // Nie kleiner als die Mindestgroesse und nie null.
    void respectsMinimumSize()
    {
        const QRect r = snappedFrameRect(QRect(10, 10, 3, 3), QSize(20, 20));
        QVERIFY(r.width() >= 20);
        QVERIFY(r.height() >= 20);
        QCOMPARE((r.width() + Style::kFensterLuftPx) % kSnapGridPx, 0);
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
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        w.setGeometry(101, 99, 301, 203);
        snapToGridAfterSettle(&w);
        const int luft = Style::kFensterLuftPx;
        QTRY_VERIFY_WITH_TIMEOUT(
            w.frameGeometry().x() % kSnapGridPx == 0
            && (w.frameGeometry().width() + luft) % kSnapGridPx == 0, 2000);
        const QRect f = w.frameGeometry();
        QCOMPARE(f.y() % kSnapGridPx, 0);
        QCOMPARE((f.x() + f.width() + luft) % kSnapGridPx, 0);
        QCOMPARE((f.y() + f.height() + luft) % kSnapGridPx, 0);
        // Und bleibt dort -- kein Hin und Her.
        const QRect settled = w.frameGeometry();
        QTest::qWait(500);
        QCOMPARE(w.frameGeometry(), settled);
    }
};

QTEST_MAIN(TstWindowGridSnap)
#include "tst_window_grid_snap.moc"
