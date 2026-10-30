// =================================================================
// tests/tst_window_placement_oversized.cpp  (Longpath)
// =================================================================
//
// ensureOnVisibleScreen() mit einem Fenster, das groesser ist als die
// verfuegbare Bildschirmflaeche.
//
// Befund 2026-09-25 (voller Debug/ASAN-Lauf): qBound(min, x, max) bekam
// max < min, sobald die Mindestgroesse die Flaeche uebersteigt -- Qts
// Zusicherung "!(max < min)" brach tst_pan_floating_window,
// tst_panadapter_stack_layouts und tst_real_pan_float_state ab. Im
// Release-Bau kam still die linke/obere Kante heraus; das ist jetzt das
// ausdrueckliche Verhalten und wird hier festgehalten. Im Debug-Bau
// bricht dieser Test ohne die Behebung ab.
// =================================================================

#include <QtTest/QtTest>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

#include "gui/WindowPlacement.h"

using namespace Longpath;

class TstWindowPlacementOversized : public QObject {
    Q_OBJECT

private slots:
    void aWindowLargerThanTheScreenStartsAtItsTopLeft()
    {
        QScreen* screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect avail = screen->availableGeometry();

        QWidget w;
        const QSize tooBig(avail.width() + 400, avail.height() + 300);
        ensureOnVisibleScreen(&w, nullptr, tooBig);

        qDebug() << "Flaeche" << avail << "Fenster" << w.geometry();
        QCOMPARE(w.geometry().topLeft(), avail.topLeft());
        QVERIFY(w.width()  >= tooBig.width());
        QVERIFY(w.height() >= tooBig.height());
    }

    // Nur zu breit, nicht zu hoch: waagrecht an die Kante, senkrecht
    // weiter mittig -- die Achsen sind unabhaengig.
    void onlyTooWideKeepsTheVerticalCentring()
    {
        QScreen* screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect avail = screen->availableGeometry();

        QWidget w;
        const QSize wide(avail.width() + 200, avail.height() / 2);
        ensureOnVisibleScreen(&w, nullptr, wide);

        QCOMPARE(w.geometry().left(), avail.left());
        QCOMPARE(w.geometry().top(), avail.y() + (avail.height() - w.height()) / 2);
    }

    // ── Ein Bildpunkt genuegt nicht mehr (2026-10-30) ────────────────
    //
    // Der Sichtbarkeitstest war `intersects()`: ein Fenster, das zu
    // 99 % ueber die Schirmkante hing, galt als in Ordnung und wurde
    // bei jedem Start wieder genau dorthin gesetzt. Am Geraet gesehen:
    // das FT8-Panel stand links drueber, der Titel war auf „T8
    // DECODER" abgeschnitten.
    //
    // Die Groeszenordnung stand als „Schwaeche 2" seit 2026-08-16 im
    // Kopf von WindowPlacement.h -- 80 x 40 px, genug, um das Fenster
    // mit der Maus wieder greifen zu koennen.
    void einFensterFastGanzUeberDerKanteWirdZurueckgeholt()
    {
        QScreen* screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect avail = screen->availableGeometry();

        QWidget anker;
        anker.setGeometry(avail.x() + 200, avail.y() + 100, 800, 600);

        QWidget w;
        // 600 breit, aber nur 20 Punkte ragen in den Schirm hinein.
        w.setGeometry(avail.x() - 580, avail.y() + 300, 600, 400);
        Longpath::ensureOnVisibleScreen(&w, &anker, QSize(260, 300));

        const QRect sichtbar = avail.intersected(w.geometry());
        QVERIFY2(sichtbar.width() >= 80 && sichtbar.height() >= 40,
                 qPrintable(QStringLiteral("nur %1x%2 sichtbar, Fenster %3")
                                .arg(sichtbar.width()).arg(sichtbar.height())
                                .arg(QStringLiteral("%1,%2 %3x%4")
                                         .arg(w.geometry().x())
                                         .arg(w.geometry().y())
                                         .arg(w.geometry().width())
                                         .arg(w.geometry().height()))));
    }

    // GEGENPROBE: mit dem alten Test waere derselbe Fall durchgegangen.
    // Ohne diesen Lauf belegt der Fall darueber nicht, dass sich etwas
    // geaendert hat.
    void derAlteTestHaetteDasDurchgelassen()
    {
        QScreen* screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect avail = screen->availableGeometry();
        const QRect fastDraussen(avail.x() - 580, avail.y() + 300, 600, 400);

        // So sah die Pruefung bis zum 2026-10-30 aus.
        QVERIFY2(avail.intersects(fastDraussen),
                 "wenn das schon frueher falsch war, prueft der Fall "
                 "darueber nichts");
        // Und so sieht sie jetzt aus.
        const QRect sichtbar = avail.intersected(fastDraussen);
        QVERIFY(sichtbar.width() < 80);
    }

    // Ein Fenster, das ordentlich auf dem Schirm liegt, wird NICHT
    // angefasst -- sonst zoege diese Aenderung jedes freistehende
    // Fenster bei jedem Start in die Mitte.
    void einOrdentlichLiegendesFensterBleibtWoEsIst()
    {
        QScreen* screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect avail = screen->availableGeometry();

        QWidget anker;
        anker.setGeometry(avail.x() + 100, avail.y() + 100, 900, 700);

        QWidget w;
        const QRect wunsch(avail.x() + 300, avail.y() + 250, 500, 400);
        w.setGeometry(wunsch);
        Longpath::ensureOnVisibleScreen(&w, &anker, QSize(260, 300));
        QCOMPARE(w.geometry(), wunsch);
    }

    // Und ein Fenster, das SELBST schmaler als 80 Punkte ist, darf die
    // Bedingung nicht unerfuellbar finden -- sonst wanderte es bei
    // jedem Start in die Mitte.
    void einSchmalesFensterWirdNichtBestraft()
    {
        QScreen* screen = QGuiApplication::primaryScreen();
        QVERIFY(screen);
        const QRect avail = screen->availableGeometry();

        QWidget anker;
        anker.setGeometry(avail.x() + 100, avail.y() + 100, 900, 700);

        QWidget w;
        const QRect wunsch(avail.x() + 300, avail.y() + 250, 60, 30);
        w.setGeometry(wunsch);
        Longpath::ensureOnVisibleScreen(&w, &anker, QSize(0, 0));
        QCOMPARE(w.geometry(), wunsch);
    }

};

QTEST_MAIN(TstWindowPlacementOversized)
#include "tst_window_placement_oversized.moc"
