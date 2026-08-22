// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Maus im Panadapter verhaelt sich wie im Original.
//
// AetherSDR teilt es so auf (SpectrumWidget.cpp:10055-10066 und
// 9324 [@0cd4559]):
//
//     ziehen im Spektrum          -> Ansicht verschieben
//     kurzer Klick                -> abstimmen (Bewegungsschwelle)
//     ziehen im Durchlassbereich  -> abstimmen, relativ zum Griffpunkt
//
// Die Vorgeschichte dieser Datei ist die Lehre daran. Sie hiess
// tst_pan_drag_is_deliberate und verlangte das GEGENTEIL: "ein Zug
// im Spektrum verschiebt NICHT". Das war meine Antwort am Vormittag
// des 2026-08-22 auf einen richtigen Befund des Betreibers — ein
// unabsichtlicher Klick verruecke alles um Megahertz. Falsche
// Antwort: schuld war die absolute Rechnung im VFO-Zug, die sich mit
// der Nachzentrierung aufschaukelte. Am Abend kam die Quittung:
// "wenn man den balken verloren hat kann man nicht mit klick and
// hold den bereich verändern."
//
// Schlimmer noch: der alte Test war die ganze Zeit GRUEN OHNE ETWAS
// ZU PRUEFEN. Er liess das Widget getrennt, und getrennt schluckt
// mousePressEvent JEDEN Linksklick (Phase 3Q-8) — der Zeiger kam nie
// bis zu dem Zweig, ueber den der Test etwas behauptete. Er blieb
// gruen, als das Verhalten umgedreht wurde. Deshalb setzt diese
// Fassung als ERSTES den Verbindungszustand.

#include <QtTest>
#include "gui/SpectrumWidget.h"

#include <QMouseEvent>
#include <QSignalSpy>

namespace {

void drag(QWidget* w, QPoint from, QPoint to)
{
    QMouseEvent press(QEvent::MouseButtonPress, from, w->mapToGlobal(from),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &press);
    const int steps = 8;
    for (int i = 1; i <= steps; ++i) {
        const QPoint p = from + (to - from) * i / steps;
        QMouseEvent move(QEvent::MouseMove, p, w->mapToGlobal(p),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(w, &move);
    }
    QMouseEvent release(QEvent::MouseButtonRelease, to, w->mapToGlobal(to),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &release);
    QCoreApplication::processEvents();
}
} // namespace

using namespace Longpath;

class TstMouseFollowsAether : public QObject { Q_OBJECT

private:
    static void arm(SpectrumWidget& w)
    {
        w.resize(1000, 600);
        w.setDdcCenterFrequency(7'100'000.0);
        w.setSampleRate(192'000.0);
        w.setFrequencyRange(7'100'000.0, 100'000.0);
        w.setVfoFrequency(7'100'000.0);
        // OHNE DIESE ZEILE PRUEFT DER TEST NICHTS. Siehe Kopf.
        w.setConnectionState(ConnectionState::Connected);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
    }

private slots:
    void aDragInTheSpectrumMovesTheView()
    {
        SpectrumWidget w;
        arm(w);
        const double centerBefore = w.centerFrequency();
        const double bwBefore     = w.bandwidth();

        // Weit weg vom Durchlassbereich in der Mitte.
        drag(&w, QPoint(800, 150), QPoint(600, 150));

        QVERIFY2(!qFuzzyCompare(w.centerFrequency(), centerBefore),
                 "Ein Zug im Spektrum verschiebt die Ansicht nicht — "
                 "dann kommt man an einen VFO ausserhalb des Bildes "
                 "nicht mehr heran");
        QVERIFY2(qFuzzyCompare(w.bandwidth(), bwBefore),
                 "Der Zug zoomt, statt zu verschieben");
    }

    void aShortClickStillTunes()
    {
        SpectrumWidget w;
        arm(w);
        QSignalSpy tuned(&w, &SpectrumWidget::frequencyClicked);
        drag(&w, QPoint(700, 150), QPoint(701, 150));
        QVERIFY2(tuned.count() > 0,
                 "Ein kurzer Klick stimmt nicht mehr ab");
    }

    void draggingTheVfoBarStaysProportional()
    {
        // Der Kern des Ausreissers: frueher rechnete der VFO-Zug
        // ABSOLUT gegen eine Ansicht, die sich nach jeder Aenderung
        // neu auf den VFO zentriert. Damit rutschte die Skala unter
        // dem Zeiger weg und die Frequenz lief davon — beim Betreiber
        // bis 7,82 MHz aus einer Handbewegung.
        //
        // Gemessen wird deshalb das VERHAELTNIS: doppelter Weg,
        // hoechstens doppelte Frequenzaenderung.
        //
        // EHRLICH DAZU, damit niemand mehr Sicherheit unterstellt als
        // hier ist: die Gegenprobe ist NEGATIV. Setzt man die alte,
        // absolute Rechnung wieder ein, bleibt dieser Fall gruen.
        // Grund: die Rueckkopplung braucht die Nachzentrierung, und
        // die entsteht erst im vollen Hauptfenster, wo
        // centerChanged das Geraet nachfuehrt. Headless fehlt sie,
        // also sind absolut und relativ hier dasselbe.
        //
        // Der Fall bewacht damit die PROPORTIONALITAET, nicht die
        // Behebung des Ausreissers. Belegt ist die am Geraet:
        // 40 Punkte Zug ergaben 5,9 kHz (7,7888 -> 7,7947 MHz),
        // waehrend dieselbe Geste vorher bis 7,82 MHz sprang.
        SpectrumWidget w;
        arm(w);

        w.setFrequencyRange(7'100'000.0, 100'000.0);
        w.setVfoFrequency(7'100'000.0);
        QCoreApplication::processEvents();
        const int xVfo = w.vfoXForTest();

        QSignalSpy tuned(&w, &SpectrumWidget::frequencyClicked);
        drag(&w, QPoint(xVfo, 150), QPoint(xVfo + 40, 150));
        QVERIFY2(tuned.count() > 0, "Der Zug am Balken stimmt nicht ab");
        const double d1 =
            std::abs(tuned.last().at(0).toDouble() - 7'100'000.0);

        w.setFrequencyRange(7'100'000.0, 100'000.0);
        w.setVfoFrequency(7'100'000.0);
        QCoreApplication::processEvents();
        QSignalSpy tuned2(&w, &SpectrumWidget::frequencyClicked);
        drag(&w, QPoint(xVfo, 150), QPoint(xVfo + 80, 150));
        QVERIFY2(tuned2.count() > 0, "Der zweite Zug stimmt nicht ab");
        const double d2 =
            std::abs(tuned2.last().at(0).toDouble() - 7'100'000.0);

        QVERIFY2(d1 > 1.0 && d2 > 1.0,
                 qPrintable(QStringLiteral("Kein Frequenzweg: %1 / %2")
                     .arg(d1).arg(d2)));
        QVERIFY2(d2 < d1 * 2.5,
                 qPrintable(QStringLiteral(
                     "Doppelter Zeigerweg gibt %1 Hz statt hoechstens "
                     "%2 Hz — die Frequenz laeuft davon, genau der "
                     "Ausreisser vom 2026-08-22")
                     .arg(d2).arg(d1 * 2.5)));
    }
};

QTEST_MAIN(TstMouseFollowsAether)
#include "tst_mouse_follows_aether.moc"
