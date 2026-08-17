// =================================================================
// tests/tst_frequency_instrument.cpp  (NereusSDR)
// =================================================================
//
// Das Frequenz-Widget BEDIENT, es zeigt nicht nur.
//
// Auflage des Betreibers, 2026-08-17: „Das Frequenz-Widget muss Rad
// und Klick-Eingabe tragen, nicht nur anzeigen — sonst nimmt Punkt 3
// das Abstimmen weg." Punkt 3 ist das Ausblenden der VFO-Flagge; wenn
// dieses Widget nur anzeigte, waere danach das Abstimmen mit der Maus
// verschwunden.
//
// Geprueft wird deshalb genau das: dass das Rad ueber einer Ziffer
// DIESE Dekade dreht, und dass die Aenderung als Wunsch nach aussen
// geht statt hier geschrieben zu werden.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>
#include <QLabel>
#include <QSignalSpy>
#include <QWheelEvent>

#include "gui/instruments/FrequencyInstrument.h"

using namespace NereusSDR;

namespace {

/// Die Ziffernschilder in Anzeigefolge: die Trennpunkte und die
/// Einheit tragen keinen Mauszeiger fuer senkrechtes Ziehen.
QList<QLabel*> digitsOf(FrequencyInstrument& w)
{
    QList<QLabel*> out;
    for (QLabel* l : w.findChildren<QLabel*>()) {
        if (l->cursor().shape() == Qt::SizeVerCursor) { out.append(l); }
    }
    // findChildren gibt keine Reihenfolge zu — nach x sortieren.
    std::sort(out.begin(), out.end(), [](QLabel* a, QLabel* b) {
        return a->mapToGlobal(QPoint(0, 0)).x()
             < b->mapToGlobal(QPoint(0, 0)).x();
    });
    return out;
}

void wheelOn(QWidget* w, int notches)
{
    QWheelEvent ev(QPointF(4, 4), w->mapToGlobal(QPointF(4, 4)),
                   QPoint(0, 0), QPoint(0, notches * 120),
                   Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(w, &ev);
}

} // namespace

class TestFrequencyInstrument : public QObject
{
    Q_OBJECT

private slots:

    // ── Die Gruppierung, nicht nur der Wert ──────────────────────────
    //
    // Befund des Betreibers, 2026-08-18: oben stand 0.713.9700 MHz,
    // in der VFO-Zeile darunter richtig 7.139.700. Der WERT stimmte
    // die ganze Zeit — nur die Trennpunkte sassen an festen Stellen
    // von links, waehrend die Gruppierung vom kleinsten Hertz her
    // zaehlt. Ein Test auf den Wert allein waere gruen geblieben.
    void groupingCountsFromTheRight()
    {
        FrequencyInstrument w;
        w.resize(400, 120);

        w.setFrequency(7'139'700.0);
        QCOMPARE(w.groupedText(), QStringLiteral("7.139.700"));

        w.setFrequency(14'225'000.0);
        QCOMPARE(w.groupedText(), QStringLiteral("14.225.000"));
    }

    // Unter 10 MHz bleibt die vorderste Stelle leer. Eine fuehrende
    // Null liest sich als Teil der Zahl — genau so ist der Fehler oben
    // ueberhaupt entstanden.
    void belowTenMegahertzThereIsNoLeadingZero()
    {
        FrequencyInstrument w;
        w.resize(400, 120);

        w.setFrequency(7'139'700.0);
        QVERIFY2(!w.groupedText().startsWith(QLatin1Char('0')),
                 qPrintable(QStringLiteral("fuehrende Null: %1")
                                .arg(w.groupedText())));

        w.setFrequency(3'650'000.0);
        QCOMPARE(w.groupedText(), QStringLiteral("3.650.000"));

        // Ab 10 MHz steht die Stelle wieder da.
        w.setFrequency(14'225'000.0);
        QVERIFY(w.groupedText().startsWith(QStringLiteral("14")));
    }

    // Die leere Stelle behaelt ihre Trefferflaeche: 7 MHz muss sich auf
    // 17 MHz drehen lassen, auch wenn dort nichts steht.
    void theBlankLeadingDigitStillTunes()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(7'139'700.0);
        QSignalSpy spy(&w, &FrequencyInstrument::frequencyEdited);
        wheelOn(digitsOf(w).at(0), +1);
        QCOMPARE(spy.count(), 1);
        QVERIFY(qAbs(spy.at(0).at(0).toDouble() - 17'139'700.0) < 1e-6);
    }

    void everyDigitIsItsOwnTarget()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        QCOMPARE(digitsOf(w).size(), 8);
    }

    // ── Drei Bloecke, nicht acht Zeichen ─────────────────────────────
    //
    // Befund des Betreibers, 2026-08-18: „Der Trenner steht genauso
    // weit ab wie die Ziffern untereinander, damit sieht das Auge acht
    // Einzelzeichen statt drei Bloecke — und die Gruppierung, die wir
    // gerade repariert haben, wird optisch wieder aufgehoben."
    //
    // Ursache: Monospace gibt dem Punkt dieselbe Zelle wie einer
    // Ziffer. Der Test misst die WIRKLICHEN Abstaende auf dem Schirm,
    // nicht die Konstanten — waere er gegen kGroupGapOfCell gerechnet,
    // pruefte er nur, dass eine Zahl gleich sich selbst ist.
    void theGapBetweenGroupsBeatsTheGapWithin()
    {
        FrequencyInstrument w;
        w.resize(520, 140);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        const QList<QLabel*> d = digitsOf(w);
        QCOMPARE(d.size(), 8);

        auto gapAfter = [&d](int i) {
            const QRect a = d.at(i)->geometry();
            const QRect b = d.at(i + 1)->geometry();
            return b.left() - a.right() - 1;
        };

        // Innerhalb einer Gruppe: 2-3, 3-4 und 5-6, 6-7.
        const int within = qMax(qMax(gapAfter(2), gapAfter(3)),
                                qMax(gapAfter(5), gapAfter(6)));
        // Zwischen zwei Gruppen: hinter Stelle 1 und hinter Stelle 4.
        const int between = qMin(gapAfter(1), gapAfter(4));

        // Beide Enden festnageln, nicht nur ihr Verhaeltnis: „innen
        // eng" ist eine eigene Zusicherung, sonst waere die Zeile auch
        // dann gruen, wenn beide Abstaende gross sind und nur der eine
        // groesser. Bezug ist die Zeichenzelle, damit die Pruefung
        // ueber die Schriftstufen haelt.
        const int cell = d.at(0)->width();
        QVERIFY2(within <= cell / 6,
                 qPrintable(QStringLiteral(
                     "innerhalb der Gruppe %1 px bei Zelle %2 — nicht eng")
                     .arg(within).arg(cell)));
        QVERIFY2(between >= cell / 2,
                 qPrintable(QStringLiteral(
                     "zwischen den Gruppen %1 px bei Zelle %2 — das Auge "
                     "sieht acht Zeichen statt drei Bloecke")
                     .arg(between).arg(cell)));
    }

    // Der Kern der Auflage: das Rad ueber der Kilohertz-Stelle dreht
    // Kilohertz, nicht die eingestellte Schrittweite.
    void theWheelTurnsThatDigitsDecade()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(7'139'700.0);
        const QList<QLabel*> d = digitsOf(w);
        QCOMPARE(d.size(), 8);

        QSignalSpy spy(&w, &FrequencyInstrument::frequencyEdited);

        // Stelle 0 = 10 MHz, 4 = 1 kHz, 7 = 1 Hz.
        const QList<QPair<int, double>> cases = {
            {0, 1e7}, {1, 1e6}, {2, 1e5}, {3, 1e4},
            {4, 1e3}, {5, 1e2}, {6, 1e1}, {7, 1e0},
        };
        for (const auto& c : cases) {
            spy.clear();
            wheelOn(d.at(c.first), +1);
            QCOMPARE(spy.count(), 1);
            const double got = spy.at(0).at(0).toDouble();
            QVERIFY2(qAbs((got - 7'139'700.0) - c.second) < 1e-6,
                     qPrintable(QStringLiteral(
                         "Stelle %1 muss %2 Hz drehen, drehte %3")
                             .arg(c.first).arg(c.second)
                             .arg(got - 7'139'700.0)));
        }
    }

    void theWheelTurnsBothWays()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(7'139'700.0);
        const QList<QLabel*> d = digitsOf(w);
        QSignalSpy spy(&w, &FrequencyInstrument::frequencyEdited);
        wheelOn(d.at(4), -1);
        QCOMPARE(spy.count(), 1);
        QVERIFY(qAbs(spy.at(0).at(0).toDouble() - 7'138'700.0) < 1e-6);
    }

    // Das Widget schreibt NICHT selbst. Es meldet den Wunsch; wer ihn
    // ausfuehrt, entscheidet der Empfaenger — so laeuft die Aenderung
    // ueber denselben Weg wie jede andere Abstimmung, samt allem, was
    // daran haengt.
    void theWidgetDoesNotMoveItself()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(7'139'700.0);
        wheelOn(digitsOf(w).at(4), +1);
        QCOMPARE(w.frequency(), 7'139'700.0);
    }

    void negativeFrequenciesAreNotOffered()
    {
        FrequencyInstrument w;
        w.resize(400, 120);
        w.setFrequency(5.0);
        QSignalSpy spy(&w, &FrequencyInstrument::frequencyEdited);
        // Zehn MHz abwaerts von 5 Hz waere negativ.
        wheelOn(digitsOf(w).at(0), -1);
        QCOMPARE(spy.count(), 0);
    }

    void theThreeFormsAreSelectable()
    {
        FrequencyInstrument w;
        QCOMPARE(w.form(), FrequencyInstrument::Form::BandStrip);
        w.setForm(FrequencyInstrument::Form::FlatArc);
        QCOMPARE(w.form(), FrequencyInstrument::Form::FlatArc);
        w.setForm(FrequencyInstrument::Form::NumberOnly);
        QCOMPARE(w.form(), FrequencyInstrument::Form::NumberOnly);
        // Die blosse Zahl braucht weniger Hoehe als der Bogen — sonst
        // stuende in der knappsten Fassung die meiste leere Flaeche.
        const int hNumber = w.sizeHint().height();
        w.setForm(FrequencyInstrument::Form::FlatArc);
        QVERIFY(w.sizeHint().height() > hNumber);
    }
};

QTEST_MAIN(TestFrequencyInstrument)
#include "tst_frequency_instrument.moc"
