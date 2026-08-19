// =================================================================
// tests/tst_nested_splitter_layout.cpp  (NereusSDR)
// =================================================================
//
// Der Panadapter in BEIDEN Achsen veraenderbar.
//
// Auf Ansage des Betreibers (2026-08-19): „der Panadapter soll auf der
// x- und y-Achse verschoben werden koennen, sprich in alle Groessen und
// Richtungen veraenderbar."
//
// Ein einzelner QSplitter hat einen Griff, also EINE Achse. Die Loesung
// ist Schachtelung: aussen senkrecht, innen der bisherige waagerechte.
// Der innere behaelt seine Identitaet, weil ContainerManager einen
// Zeiger darauf haelt.
//
// Ein ganzes MainWindow im Test aufzubauen zieht Funkgeraet-Modell, WDSP
// und Audio nach sich. Geprueft wird darum die MECHANIK an derselben
// Anordnung: dass beide Griffe wirken, dass die untere Flaeche leer
// verborgen bleibt, und dass die Aufteilung sich in der richtigen
// Richtung rechnet.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

namespace {

// Dieselbe Anordnung wie MainWindow::buildUi sie baut.
struct Nest {
    QSplitter* outer{nullptr};   // senkrecht
    QSplitter* inner{nullptr};   // waagerecht
    QWidget*   pan{nullptr};     // Panadapter
    QWidget*   applets{nullptr};
    QWidget*   below{nullptr};
};

Nest makeNest()
{
    Nest n;
    n.inner = new QSplitter(Qt::Horizontal);
    n.inner->setChildrenCollapsible(false);
    n.pan = new QWidget;
    n.pan->setMinimumWidth(400);
    n.applets = new QWidget;
    n.inner->addWidget(n.pan);
    n.inner->addWidget(n.applets);

    n.outer = new QSplitter(Qt::Vertical);
    n.outer->setChildrenCollapsible(false);
    n.outer->addWidget(n.inner);

    n.below = new QWidget;
    auto* col = new QVBoxLayout(n.below);
    col->setContentsMargins(0, 0, 0, 0);
    n.below->setMinimumHeight(120);
    n.below->hide();
    n.outer->addWidget(n.below);

    n.outer->resize(1280, 800);
    return n;
}

} // namespace

class TestNestedSplitterLayout : public QObject
{
    Q_OBJECT

private slots:

    // Die Achsen: aussen senkrecht, innen waagerecht. Faellt das, hat
    // jemand die Schachtelung umgedreht, und dann verstellt der aeussere
    // Griff die Breite statt der Hoehe.
    void theTwoAxesAreDifferent()
    {
        QScopedPointer<QSplitter> outer(makeNest().outer);
        QCOMPARE(outer->orientation(), Qt::Vertical);

        auto* inner = qobject_cast<QSplitter*>(outer->widget(0));
        QVERIFY2(inner, "der innere Splitter muss der erste Eintrag sein");
        QCOMPARE(inner->orientation(), Qt::Horizontal);
    }

    // Der Panadapter liegt IM inneren Splitter, nicht direkt im
    // aeusseren: nur dann wirkt der waagerechte Griff auf ihn.
    void thePanadapterSitsInsideTheInnerSplitter()
    {
        Nest n = makeNest();
        QScopedPointer<QSplitter> keep(n.outer);
        QCOMPARE(n.inner->widget(0), n.pan);
        QCOMPARE(n.outer->widget(0), n.inner);
    }

    // Leer und verborgen: ein sichtbarer leerer Streifen waere ein
    // Versprechen, das niemand eingeloest hat.
    void theAreaBelowStartsHidden()
    {
        Nest n = makeNest();
        QScopedPointer<QSplitter> keep(n.outer);
        QVERIFY(n.below->isHidden());
        QVERIFY2(n.below->layout()->count() == 0,
                 "und leer");
    }

    // Beide Griffe wirken: Breite ueber den inneren, Hoehe ueber den
    // aeusseren. Das ist der ganze Zweck der Schachtelung.
    void bothHandlesMove()
    {
        Nest n = makeNest();
        QScopedPointer<QSplitter> keep(n.outer);

        n.inner->setSizes({900, 380});
        const QList<int> wide = n.inner->sizes();
        QVERIFY2(wide[0] > wide[1], "der Panadapter behaelt die Breite");

        n.outer->setSizes({520, 260});
        const QList<int> tall = n.outer->sizes();
        QVERIFY2(tall[0] > tall[1], "und die Hoehe");
        QVERIFY2(tall[0] + tall[1] <= n.outer->height() + 8,
                 "die Summe muss in die HOEHE passen");
    }

    // Zwei Drittel oben, ein Drittel unten — dieselbe Aufteilung, die
    // setRotorPanelBelow setzt.
    void theDefaultSplitIsTwoThirds()
    {
        Nest n = makeNest();
        QScopedPointer<QSplitter> keep(n.outer);

        const int h = n.outer->height();
        n.below->show();
        n.outer->setSizes({h * 2 / 3, h / 3});

        const QList<int> sizes = n.outer->sizes();
        QVERIFY2(sizes[0] > sizes[1], "oben mehr als unten");
        QVERIFY2(sizes[1] >= 120, "und unten genug fuer den Inhalt");
    }

    // Die Mindestbreite des Panadapters bleibt: sie ist im inneren
    // Splitter zu Hause und hat mit der neuen Achse nichts zu tun.
    void theWidthFloorIsUntouched()
    {
        Nest n = makeNest();
        QScopedPointer<QSplitter> keep(n.outer);
        QCOMPARE(n.pan->minimumWidth(), 400);
    }
};

QTEST_MAIN(TestNestedSplitterLayout)
#include "tst_nested_splitter_layout.moc"
