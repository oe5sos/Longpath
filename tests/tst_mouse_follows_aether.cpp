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
#include <QKeyEvent>
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

        // Und der WEG muss stimmen. 200 Punkte auf 1000 Punkte Breite
        // sind ein Fuenftel der Ansicht — bei 100 kHz also rund
        // 20 kHz, nicht mehr.
        //
        // Am Geraet gemessen (2026-08-22): 100 Punkte ergaben 64 kHz
        // statt 18 — Faktor dreieinhalb, weil gegen einen Startwert
        // gerechnet wurde, den die Nachzentrierung laengst ueberholt
        // hatte. Der Betreiber sah davon nur: "ich clicke und fahre
        // nach rechts, bildschirm geht nach links."
        const double moved = std::abs(w.centerFrequency() - centerBefore);
        const double expect = 0.2 * bwBefore;
        QVERIFY2(moved < expect * 1.6,
                 qPrintable(QStringLiteral(
                     "Die Ansicht wandert %1 Hz, erwartet sind rund "
                     "%2 Hz — sie laeuft der Hand davon")
                     .arg(moved).arg(expect)));
        QVERIFY2(moved > expect * 0.5,
                 qPrintable(QStringLiteral(
                     "Die Ansicht wandert nur %1 Hz statt rund %2 Hz")
                     .arg(moved).arg(expect)));

        // Richtung: nach rechts ziehen holt TIEFERE Frequenzen ins
        // Bild — der Inhalt folgt der Hand (AetherSDR: "Dragging right
        // moves the view right → center shifts left").
        SpectrumWidget w2;
        arm(w2);
        const double c0 = w2.centerFrequency();
        drag(&w2, QPoint(400, 150), QPoint(600, 150));
        QVERIFY2(w2.centerFrequency() < c0,
                 "Nach rechts ziehen erhoeht die Mitte — die Ansicht "
                 "laeuft der Hand entgegen");
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

    void aDoubleClickJumpsThereAtOnce()
    {
        // Das Beispiel des Betreibers, 2026-08-22, woertlich: "wenn ich
        // auf sieben Komma fünf stehe und ich gehe auf sieben Komma
        // zwei und mache einen Doppelklick auf sieben Komma zwei,
        // sollte auch meine Frequenz sofort auf sieben Komma zwei
        // hüpfen."
        //
        // Bis dahin fiel der Doppelklick nur auf mousePressEvent
        // durch — und seit das Ziehen wieder verschiebt, begann er
        // also ein Verschieben, statt abzustimmen.
        SpectrumWidget w;
        w.resize(1000, 600);
        w.setDdcCenterFrequency(7'350'000.0);
        w.setSampleRate(768'000.0);
        w.setFrequencyRange(7'350'000.0, 700'000.0);   // 7,0 .. 7,7
        w.setVfoFrequency(7'500'000.0);
        w.setConnectionState(ConnectionState::Connected);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        // Bildpunkt fuer 7,2 MHz aus der Abbildung des Widgets holen,
        // nicht nachrechnen.
        w.setVfoFrequency(7'200'000.0);
        QCoreApplication::processEvents();
        const int x72 = w.vfoXForTest();
        w.setVfoFrequency(7'500'000.0);
        QCoreApplication::processEvents();

        QSignalSpy tuned(&w, &SpectrumWidget::frequencyClicked);
        const QPoint p(x72, 150);
        QMouseEvent dbl(QEvent::MouseButtonDblClick, p, w.mapToGlobal(p),
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&w, &dbl);
        QCoreApplication::processEvents();

        QVERIFY2(tuned.count() > 0, "Der Doppelklick stimmt nicht ab");
        const double got = tuned.last().at(0).toDouble();
        QVERIFY2(std::abs(got - 7'200'000.0) < 2000.0,
                 qPrintable(QStringLiteral(
                     "Doppelklick auf 7,2 MHz landet bei %1 Hz").arg(got)));
    }

    void aDoubleClickDoesNotSlideTheView()
    {
        // Der Betreiber am 2026-08-22: "bei doppelklick im padadapter
        // ruscht er herum, anstatt den balken dort hinzubringen."
        //
        // Ursache: der erste Klick begann SOFORT ein Verschieben, und
        // die winzige Handbewegung zwischen den beiden Klicks rutschte
        // die Ansicht weg — der Doppelklick landete dann woanders, als
        // man gezielt hatte. Jetzt beginnt das Verschieben erst ab
        // vier Punkten Weg, derselben Schwelle, die das Loslassen fuer
        // den Klick benutzt.
        SpectrumWidget w;
        arm(w);
        const double centerBefore = w.centerFrequency();

        // Druck, zwei Punkte Zittern, Loslassen — wie bei einer Hand.
        const QPoint p0(700, 150);
        const QPoint p1(702, 150);
        QMouseEvent press(QEvent::MouseButtonPress, p0, w.mapToGlobal(p0),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&w, &press);
        QMouseEvent move(QEvent::MouseMove, p1, w.mapToGlobal(p1),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&w, &move);
        QMouseEvent rel(QEvent::MouseButtonRelease, p1, w.mapToGlobal(p1),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&w, &rel);
        QCoreApplication::processEvents();

        QVERIFY2(qFuzzyCompare(w.centerFrequency(), centerBefore),
                 qPrintable(QStringLiteral(
                     "Zwei Punkte Zittern haben die Ansicht um %1 Hz "
                     "verrutscht — dann trifft kein Doppelklick mehr")
                     .arg(std::abs(w.centerFrequency() - centerBefore))));
    }

    void theWholeListeningBandIsAGrip()
    {
        // "ich möchte, wenn ich meinen bereich von 2,7 k, sprich meinen
        // bereich wo ich höre, mit click und click hold verschieben
        // können." (Betreiber, 2026-08-22)
        //
        // Bei LSB liegt der Durchlass LINKS vom VFO-Strich. Die erste
        // Fassung griff nur um den Strich — wer auf die tuerkise
        // Flaeche zielte, traf daneben und verschob die Ansicht. Hier
        // wird genau dort gefasst, wo der Bediener hinschaut: MITTEN
        // in der Flaeche.
        SpectrumWidget w;
        arm(w);
        w.setFilterOffset(-2900, -100);      // LSB, 2,8 kHz
        QCoreApplication::processEvents();

        const int xVfo = w.vfoXForTest();
        // Mitte des Durchlasses liegt bei LSB links vom Strich.
        const double hzPerPx = w.bandwidth() / 964.0;
        const int xBand = xVfo - static_cast<int>(1500.0 / hzPerPx);

        const double centerBefore = w.centerFrequency();
        QSignalSpy tuned(&w, &SpectrumWidget::frequencyClicked);
        drag(&w, QPoint(xBand, 150), QPoint(xBand + 30, 150));

        QVERIFY2(tuned.count() > 0,
                 "Ein Zug MITTEN im Hoerbereich stimmt nicht ab");
        QVERIFY2(qFuzzyCompare(w.centerFrequency(), centerBefore),
                 "Der Zug im Hoerbereich hat die ANSICHT verschoben, "
                 "statt den Bereich zu bewegen");
    }

    void theArrowKeysTuneByOneStep()
    {
        // "oder auch mit dem rechten und linken Cursortaste muss das
        // automatisch rübergezogen werden."
        SpectrumWidget w;
        arm(w);
        w.setStepSize(100);
        QCoreApplication::processEvents();

        QSignalSpy tuned(&w, &SpectrumWidget::frequencyClicked);
        QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QCoreApplication::sendEvent(&w, &right);
        QCoreApplication::processEvents();
        QVERIFY2(tuned.count() > 0, "Die Pfeiltaste bewegt nichts");
        QCOMPARE(tuned.last().at(0).toDouble(), 7'100'100.0);

        QKeyEvent left(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
        QCoreApplication::sendEvent(&w, &left);
        QCoreApplication::processEvents();
        QCOMPARE(tuned.last().at(0).toDouble(), 7'099'900.0);
    }

    void theArrowKeysWorkAfterClickingTheWidget()
    {
        // Der Betreiber am 2026-08-22: "mit dem cursor auf der
        // tastatur kann ich auch nicht nach rechts und links fahren."
        //
        // Der bestehende Fall schickt die Taste DIREKT an das Widget
        // und beweist damit nur, dass der Handler rechnet. Der Weg des
        // Bedieners ist ein anderer: klicken, dann tippen — und dabei
        // entscheidet der FOKUS, wer die Taste bekommt. Genau das wird
        // hier geprueft.
        SpectrumWidget w;
        arm(w);
        w.setStepSize(100);

        const QPoint p(700, 150);
        QMouseEvent press(QEvent::MouseButtonPress, p, w.mapToGlobal(p),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&w, &press);
        QMouseEvent rel(QEvent::MouseButtonRelease, p, w.mapToGlobal(p),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&w, &rel);
        QCoreApplication::processEvents();

        // focusWidget() DES FENSTERS, nicht hasFocus(): letzteres
        // verlangt ein AKTIVES Fenster, und unter Last hat der
        // Pruefstand keins. Der Fall war deshalb unstet — er mass die
        // Fensteraktivierung, nicht unseren Fokus. (Derselbe Fehler
        // steckte schon in tst_zoom_buttons_do_something und wurde
        // dort am 2026-08-22 aus demselben Grund korrigiert.)
        QVERIFY2(w.window() && w.window()->focusWidget() == &w,
                 "Ein Klick ins Spektrum gibt ihm keinen Fokus — dann "
                 "kommt keine Pfeiltaste an");

        QSignalSpy tuned(&w, &SpectrumWidget::frequencyClicked);
        QTest::keyClick(&w, Qt::Key_Right);
        QCoreApplication::processEvents();
        QVERIFY2(tuned.count() > 0,
                 "Die Pfeiltaste kommt nach einem Klick nicht an");
    }

    void everythingWorksWhileDisconnected()
    {
        // Der Betreiber am 2026-08-22: "test selbst, gar nichts
        // funktioniert." Gemessen: beide Geraete antworteten in dem
        // Moment nicht — und GETRENNT schluckte der Panadapter jeden
        // Linksklick und jede Taste (Phase 3Q-8). Alle meine Messungen
        // davor liefen verbunden oder ohne Oberflaeche; deshalb war bei
        // mir alles gruen und bei ihm nichts brauchbar.
        //
        // Die Ansicht gehoert dem Rechner: Frequenz, Zoom, Ausschnitt
        // und Filterkanten sind Werte im Modell.
        SpectrumWidget w;
        w.resize(1000, 600);
        w.setDdcCenterFrequency(7'100'000.0);
        w.setSampleRate(192'000.0);
        w.setFrequencyRange(7'100'000.0, 100'000.0);
        w.setVfoFrequency(7'100'000.0);
        w.setStepSize(100);
        w.setConnectionState(ConnectionState::Disconnected);   // <-- Kern
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        // Klick abseits des DISCONNECTED-Schilds (das liegt mittig).
        QSignalSpy tuned(&w, &SpectrumWidget::frequencyClicked);
        drag(&w, QPoint(700, 60), QPoint(701, 60));
        QVERIFY2(tuned.count() > 0,
                 "Getrennt stimmt ein Klick nicht ab");

        // Pfeiltaste
        QSignalSpy tuned2(&w, &SpectrumWidget::frequencyClicked);
        QTest::keyClick(&w, Qt::Key_Right);
        QCoreApplication::processEvents();
        QVERIFY2(tuned2.count() > 0,
                 "Getrennt bewegt die Pfeiltaste nichts");

        // Verschieben
        const double c0 = w.centerFrequency();
        drag(&w, QPoint(800, 60), QPoint(600, 60));
        QVERIFY2(!qFuzzyCompare(w.centerFrequency(), c0),
                 "Getrennt laesst sich die Ansicht nicht verschieben");
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
