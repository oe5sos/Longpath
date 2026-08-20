// =================================================================
// tests/tst_tune_guide_behaviour.cpp  (NereusSDR)
// =================================================================
//
// Die Abstimmhilfe unter echter Maus.
//
// tst_tune_guide prueft den ZUSTAND (Schalter an, Schalter aus). Diese
// Datei prueft, was beim Bewegen der Maus passiert — also die Zeilen
// 1.2, 1.3, 1.6 und 1.7 der Bank-Matrix vom 2026-08-19.
//
// WARUM DAS HIER GEHT UND NICHT AUF DER BANK GEPRUEFT WERDEN MUSS: Qt
// stellt Mausereignisse selbst zu, und der Offscreen-Malweg braucht
// keinen Bildschirm. Was hier steht, muss niemand mehr von Hand
// klicken. In der Matrix bleiben die Zeilen trotzdem stehen: sie
// pruefen zusaetzlich, ob man die Linie SIEHT — und das kann kein Test.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QMouseEvent>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

namespace {

// Die Maus an eine Stelle im Spektrum bewegen. Qt::NoButton, weil es
// ein Hover ist und kein Ziehen.
void hoverAt(SpectrumWidget& w, const QPoint& pos)
{
    QMouseEvent ev(QEvent::MouseMove, QPointF(pos),
                   w.mapToGlobal(QPointF(pos)),
                   Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&w, &ev);
}

void leave(SpectrumWidget& w)
{
    QEvent ev(QEvent::Leave);
    QCoreApplication::sendEvent(&w, &ev);
}

} // namespace

class TestTuneGuideBehaviour : public QObject
{
    Q_OBJECT

private:
    // Ein Fenster in Betriebsgroesse. Ohne Groesse liegt das Spektrum
    // auf null Bildpunkten und jeder Zeigerort waere ausserhalb.
    static void prepare(SpectrumWidget& w)
    {
        w.resize(900, 600);
        w.setFrequencyRange(14200000.0, 192000.0);
    }

private slots:

    // Matrix 1.2: Maus im Spektrum bewegen -> Linie erscheint.
    void movingTheMouseShowsTheGuide()
    {
        SpectrumWidget w;
        prepare(w);
        w.setTuneGuideEnabled(true);
        QVERIFY2(!w.tuneGuideShowing(),
                 "vor der ersten Bewegung liegt nichts");

        hoverAt(w, QPoint(450, 150));
        QVERIFY2(w.tuneGuideShowing(),
                 "eine Bewegung im Spektrum muss die Hilfe zeigen");
    }

    // Bei ausgeschaltetem Schalter darf dieselbe Bewegung nichts tun.
    // Das ist die Gegenprobe zu 1.1 und zugleich der Kostenpunkt: die
    // Zeigerbewegung ist der haeufigste Vorgang ueberhaupt.
    void movingTheMouseDoesNothingWhenOff()
    {
        SpectrumWidget w;
        prepare(w);
        hoverAt(w, QPoint(450, 150));
        QVERIFY(!w.tuneGuideShowing());
    }

    // Matrix 1.7: Zeiger verlaesst den Panadapter -> Linie sofort weg,
    // nicht erst nach vier Sekunden. Eine Linie, die nach dem Verlassen
    // stehen bleibt, zeigt auf eine Frequenz, auf die niemand mehr
    // deutet.
    void leavingTheWidgetHidesItAtOnce()
    {
        SpectrumWidget w;
        prepare(w);
        w.setTuneGuideEnabled(true);
        hoverAt(w, QPoint(450, 150));
        QVERIFY(w.tuneGuideShowing());

        leave(w);
        QVERIFY2(!w.tuneGuideShowing(),
                 "beim Verlassen verschwindet sie sofort");
    }

    // Matrix 1.3: nach vier Sekunden Ruhe blendet sie aus. Statt vier
    // Sekunden zu warten, wird geprueft, dass ein einmal schiessender
    // Zeitgeber laeuft — und dass eine neue Bewegung ihn neu startet.
    void aFreshMoveKeepsItAlive()
    {
        SpectrumWidget w;
        prepare(w);
        w.setTuneGuideEnabled(true);

        hoverAt(w, QPoint(300, 150));
        QVERIFY(w.tuneGuideShowing());
        hoverAt(w, QPoint(500, 200));
        QVERIFY2(w.tuneGuideShowing(),
                 "die zweite Bewegung darf sie nicht loeschen");
    }

    // Matrix 1.9: Schalter aus, waehrend die Linie steht.
    void switchingOffWhileVisibleHidesIt()
    {
        SpectrumWidget w;
        prepare(w);
        w.setTuneGuideEnabled(true);
        hoverAt(w, QPoint(450, 150));
        QVERIFY(w.tuneGuideShowing());

        w.setTuneGuideEnabled(false);
        QVERIFY(!w.tuneGuideShowing());
    }

    // Wieder einschalten zeigt sie NICHT von selbst: erst die naechste
    // Bewegung tut das. Sonst erschiene beim Einschalten im Setup eine
    // Linie an der Stelle, wo die Maus zuletzt zufaellig war.
    void switchingOnAgainWaitsForTheNextMove()
    {
        SpectrumWidget w;
        prepare(w);
        w.setTuneGuideEnabled(true);
        hoverAt(w, QPoint(450, 150));
        w.setTuneGuideEnabled(false);
        w.setTuneGuideEnabled(true);
        QVERIFY2(!w.tuneGuideShowing(),
                 "erst die naechste Bewegung bringt sie zurueck");

        hoverAt(w, QPoint(460, 160));
        QVERIFY(w.tuneGuideShowing());
    }
};

QTEST_MAIN(TestTuneGuideBehaviour)
#include "tst_tune_guide_behaviour.moc"
