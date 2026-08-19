// =================================================================
// tests/tst_applet_panel_below.cpp  (NereusSDR)
// =================================================================
//
// Die Applet-Leiste unter dem Panadapter statt daneben.
//
// Auf Ansage des Betreibers (2026-08-19): „jedes Fenster sollte man frei
// ändern können in der Größe. beim Panadapter sehe ich keine
// Möglichkeit." Die Ursache war nicht ein fehlender Griff, sondern
// seine RICHTUNG — ein waagerechter Splitter hat keinen Griff oben.
//
// Ein ganzes MainWindow im Test aufzubauen zieht Funkgeraet-Modell,
// WDSP und Audio nach sich. Geprueft wird darum die Mechanik an einem
// QSplitter mit denselben Regeln: Richtung wechseln, Mindestmasse
// tauschen, Groessen neu setzen. Faellt hier etwas, faellt es dort auch.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QSplitter>
#include <QWidget>

namespace {

// Dieselben Schritte wie MainWindow::setAppletPanelBelow. Wird die
// Mechanik dort geaendert, gehoert sie hier nachgezogen — der Test haelt
// die REGELN fest, nicht die Zeilen.
void applyBelow(QSplitter* sp, bool below)
{
    const Qt::Orientation want = below ? Qt::Vertical : Qt::Horizontal;
    if (sp->orientation() == want) { return; }
    sp->setOrientation(want);

    if (QWidget* pane = sp->widget(0)) {
        if (below) {
            pane->setMinimumWidth(0);
            pane->setMinimumHeight(200);
        } else {
            pane->setMinimumHeight(0);
            pane->setMinimumWidth(400);
        }
    }
    const int total = below ? sp->height() : sp->width();
    if (total > 0) {
        sp->setSizes({total * 7 / 10, total * 3 / 10});
    }
}

QSplitter* makeSplitter()
{
    auto* sp = new QSplitter(Qt::Horizontal);
    sp->setChildrenCollapsible(false);
    auto* left = new QWidget;
    left->setMinimumWidth(400);
    sp->addWidget(left);
    sp->addWidget(new QWidget);
    sp->resize(1280, 720);
    return sp;
}

} // namespace

class TestAppletPanelBelow : public QObject
{
    Q_OBJECT

private slots:

    void startsBesideThePanadapter()
    {
        QScopedPointer<QSplitter> sp(makeSplitter());
        QCOMPARE(sp->orientation(), Qt::Horizontal);
    }

    void switchingPutsThePanelUnderneath()
    {
        QScopedPointer<QSplitter> sp(makeSplitter());
        applyBelow(sp.data(), true);
        QCOMPARE(sp->orientation(), Qt::Vertical);
    }

    // Der Kern: untereinander muss die MINDESTBREITE weg, sonst kann der
    // Betreiber das Fenster nicht mehr schmal ziehen — 400 Bildpunkte
    // Mindestbreite sind waagerecht sinnvoll und untereinander eine
    // willkuerliche Sperre.
    void theWidthFloorGoesAwayWhenStacked()
    {
        QScopedPointer<QSplitter> sp(makeSplitter());
        QCOMPARE(sp->widget(0)->minimumWidth(), 400);

        applyBelow(sp.data(), true);
        QCOMPARE(sp->widget(0)->minimumWidth(), 0);
    }

    // Und dafuer kommt eine Mindesthoehe: ohne sie laesst sich der
    // Panadapter auf null ziehen, und dann steht da ein leeres Fenster,
    // dessen Ursache niemand findet.
    void aHeightFloorTakesItsPlace()
    {
        QScopedPointer<QSplitter> sp(makeSplitter());
        applyBelow(sp.data(), true);
        QVERIFY2(sp->widget(0)->minimumHeight() >= 200,
                 "ohne Mindesthoehe laesst sich der Panadapter wegziehen");
    }

    // Zurueckschalten stellt beides wieder her — sonst waere der Weg
    // hin und zurueck nicht derselbe, und der Betreiber saesse nach
    // zweimal Klicken vor einem anderen Fenster als vorher.
    void switchingBackRestoresBothFloors()
    {
        QScopedPointer<QSplitter> sp(makeSplitter());
        applyBelow(sp.data(), true);
        applyBelow(sp.data(), false);

        QCOMPARE(sp->orientation(), Qt::Horizontal);
        QCOMPARE(sp->widget(0)->minimumWidth(), 400);
        QCOMPARE(sp->widget(0)->minimumHeight(), 0);
    }

    // Die gemerkten Groessen der einen Richtung sind in der anderen
    // sinnlos — eine Breite als Hoehe gelesen. Nach dem Wechsel steht
    // darum 70 zu 30 in der NEUEN Richtung.
    void sizesAreRecomputedForTheNewDirection()
    {
        QScopedPointer<QSplitter> sp(makeSplitter());
        sp->setSizes({1024, 256});
        applyBelow(sp.data(), true);

        const QList<int> sizes = sp->sizes();
        QCOMPARE(sizes.size(), 2);
        QVERIFY2(sizes[0] > sizes[1],
                 "der Panadapter bekommt den groesseren Teil");
        const int total = sizes[0] + sizes[1];
        QVERIFY2(total <= sp->height() + 8,
                 "die Summe muss in die HOEHE passen, nicht in die Breite");
    }

    void switchingTwiceInARowIsHarmless()
    {
        QScopedPointer<QSplitter> sp(makeSplitter());
        applyBelow(sp.data(), true);
        const QList<int> first = sp->sizes();
        applyBelow(sp.data(), true);
        QCOMPARE(sp->sizes(), first);
    }
};

QTEST_MAIN(TestAppletPanelBelow)
#include "tst_applet_panel_below.moc"
