// no-port-check: Longpath-original test, keine Thetis-Logik.
//
// ── Die Streifen der Kurve (2026-10-08) ─────────────────────────────────
//
// `Kurve::baue` entscheidet, aus wie vielen Baendern die GPU die
// Spektrumkurve zeichnet und mit welcher Deckkraft. Die ganze Vorgabe
// "beide Schalter aus" beruht auf einer Zusage: dann kommt GENAU EIN
// Streifen mit den alten Werten heraus. Hier steht sie als Pruefung,
// damit sie nicht beim naechsten Umbau still zerfaellt.
//
// Gegenprobe im Sinne von feedback-pruefstand-gegen-alte-fassung: der
// erste Prueffall vergleicht nicht gegen sich selbst, sondern gegen die
// Werte, die vor dem Umbau als Zahlenliteral im Malcode standen
// (+halbPx / -halbPx, Deckkraft 0,9 bzw. 0,55).

#include <QtTest/QtTest>

#include "gui/KurvenStreifen.h"

using namespace Longpath;

class TestKurvenStreifen : public QObject
{
    Q_OBJECT

private slots:
    // Die Zusage, auf der die Vorgabe beruht.
    void ohneBeideBleibtEsEinStreifen()
    {
        Kurve::Streifen s[Kurve::kStreifenHoechstens];
        const int n = Kurve::baue(false, false, 1.0f, 2.0f, 0.9f, s);
        QCOMPARE(n, 1);
        // Genau die Werte, die vorher hart im Malcode standen.
        QCOMPARE(s[0].o1,  1.0f);
        QCOMPARE(s[0].a1,  0.9f);
        QCOMPARE(s[0].o2, -1.0f);
        QCOMPARE(s[0].a2,  0.9f);
    }

    // Dieselbe Zusage fuer die Spitzenhaltelinie: andere Deckkraft,
    // andere Breite, aber auch genau ein Streifen.
    void dieSpitzenlinieBleibtAuchEinStreifen()
    {
        Kurve::Streifen s[Kurve::kStreifenHoechstens];
        const int n = Kurve::baue(false, false, 0.75f, 2.0f, 0.55f, s);
        QCOMPARE(n, 1);
        QCOMPARE(s[0].o1,  0.75f);
        QCOMPARE(s[0].a1,  0.55f);
        QCOMPARE(s[0].o2, -0.75f);
        QCOMPARE(s[0].a2,  0.55f);
    }

    void dieWeicheKanteMachtDreiStreifen()
    {
        Kurve::Streifen s[Kurve::kStreifenHoechstens];
        const int n = Kurve::baue(true, false, 1.0f, 2.0f, 0.9f, s);
        QCOMPARE(n, 3);
        // Kern einen halben Geraete-Pixel schmaler als vorher.
        const float kern = 1.0f - 0.5f * Kurve::kKantePixel;
        QCOMPARE(s[1].o1,  kern);
        QCOMPARE(s[1].o2, -kern);
        QCOMPARE(s[1].a1,  0.9f);
        QCOMPARE(s[1].a2,  0.9f);
        // Aussen laeuft es auf 0 aus, und der Auslauf schliesst
        // luecken- und ueberlappungsfrei an den Kern an.
        QCOMPARE(s[0].a1, 0.0f);
        QCOMPARE(s[0].o2, s[1].o1);
        QCOMPARE(s[0].a2, s[1].a1);
        QCOMPARE(s[2].a2, 0.0f);
        QCOMPARE(s[2].o1, s[1].o2);
        QCOMPARE(s[2].a1, s[1].a2);
        // Das Band wird dadurch NICHT schmaler als vorher: aussen steht
        // es jetzt weiter, nur mit Deckkraft 0.
        QVERIFY(s[0].o1 > 1.0f);
        QVERIFY(s[2].o2 < -1.0f);
    }

    void derHofKommtUntenDazu()
    {
        Kurve::Streifen s[Kurve::kStreifenHoechstens];
        const int n = Kurve::baue(true, true, 1.0f, 2.0f, 0.9f, s);
        QCOMPARE(n, Kurve::kStreifenHoechstens);
        // Die ersten beiden sind der Hof -- zuerst gebaut, also auch
        // zuerst gezeichnet, also UNTER der Linie.
        const float hofHalb = 1.0f + Kurve::kHofPixel * 2.0f;
        QCOMPARE(s[0].o1, hofHalb);
        QCOMPARE(s[0].a1, 0.0f);
        QCOMPARE(s[0].a2, Kurve::kHofDeckkraft);
        QCOMPARE(s[1].a1, Kurve::kHofDeckkraft);
        QCOMPARE(s[1].o2, -hofHalb);
        QCOMPARE(s[1].a2, 0.0f);
        // Der Hof reicht weiter als der Kern, sonst waere er keiner.
        QVERIFY(hofHalb > s[3].o1);
    }

    // Der Hof allein, ohne weiche Kante: drei Streifen, und der Kern
    // behaelt seine alte Breite.
    void derHofAlleinLaesstDenKernInRuhe()
    {
        Kurve::Streifen s[Kurve::kStreifenHoechstens];
        const int n = Kurve::baue(false, true, 1.0f, 2.0f, 0.9f, s);
        QCOMPARE(n, 3);
        QCOMPARE(s[2].o1,  1.0f);
        QCOMPARE(s[2].o2, -1.0f);
        QCOMPARE(s[2].a1,  0.9f);
    }

    // Bei Linienbreite 1 auf einem Schirm ohne Verdopplung ist die halbe
    // Breite 0,5 Geraete-Pixel. Ein halber Pixel Abzug liesse 0 uebrig --
    // dann waere die Kurve weg. Die Schranke faengt das.
    void derKernVerschwindetNicht()
    {
        Kurve::Streifen s[Kurve::kStreifenHoechstens];
        Kurve::baue(true, false, 0.5f, 1.0f, 0.9f, s);
        QCOMPARE(s[1].o1, Kurve::kKernMindestens);
        QVERIFY(s[1].o1 > 0.0f);
    }

    // Der Hof skaliert mit der Pixeldichte, der Auslauf nicht: 2,5
    // LOGISCHE Pixel Hof (damit er auf dem Retina-Schirm gleich breit
    // aussieht), 1 GERAETE-Pixel Auslauf (damit er genau eine Zeile
    // deckt, nicht zwei).
    void derHofWaechstMitDerPixeldichteDerAuslaufNicht()
    {
        Kurve::Streifen eins[Kurve::kStreifenHoechstens];
        Kurve::Streifen zwei[Kurve::kStreifenHoechstens];
        Kurve::baue(true, true, 1.0f, 1.0f, 0.9f, eins);
        Kurve::baue(true, true, 1.0f, 2.0f, 0.9f, zwei);
        QCOMPARE(eins[0].o1, 1.0f + 2.5f);
        QCOMPARE(zwei[0].o1, 1.0f + 5.0f);
        // Auslauf: Abstand zwischen Kernkante und Nullkante, beide Male 1.
        QCOMPARE(eins[2].o1 - eins[2].o2, Kurve::kKantePixel);
        QCOMPARE(zwei[2].o1 - zwei[2].o2, Kurve::kKantePixel);
    }

    // Nie mehr Streifen als Plaetze: der Puffer der GPU ist auf
    // kStreifenHoechstens ausgelegt, ein Ueberlauf waere ein Schreiben
    // hinter das Ende.
    void keineKombinationSprengtDenPuffer()
    {
        for (int k = 0; k < 2; ++k) {
            for (int h = 0; h < 2; ++h) {
                Kurve::Streifen s[Kurve::kStreifenHoechstens];
                const int n = Kurve::baue(k != 0, h != 0, 1.5f, 2.0f, 0.9f, s);
                QVERIFY(n >= 1);
                QVERIFY(n <= Kurve::kStreifenHoechstens);
            }
        }
    }
};

QTEST_APPLESS_MAIN(TestKurvenStreifen)
#include "tst_kurven_streifen.moc"
