// Was vom Netz als Peilung angenommen wird — und was nicht.
//
// Am anderen Ende der Leitung steht ein Mast mit einer Antenne darauf, der
// sich auf Zuruf dreht. Was hier durchrutscht, dreht echtes Metall. Die
// drei Faelle, gegen die dieser Stand steht:
//
//   * `toDouble()` macht aus "" und "abc" eine 0. NULL GRAD IST NORD —
//     eine leere Zeile wuerde die Antenne nach Norden drehen, und niemand
//     haette einen Befehl dazu gegeben.
//   * NaN besteht jeden Bereichsvergleich: `nan < 0` ist falsch, `nan > 360`
//     ist auch falsch. Ein naiver Bereichstest laesst NaN durch.
//   * 360 ist dieselbe Richtung wie 0 und gehoert angenommen; 361 ist ein
//     Tippfehler und darf es nicht.

#include "core/RotorPeilung.h"

#include <QtTest>

using Longpath::RotorPeilung::lies;

class TstRotorPeilung : public QObject
{
    Q_OBJECT

private slots:
    void gueltigeWerteKommenDurch();
    void leeresUndUnfugWerdenNichtZuNord();
    void nanUndUnendlichRutschenNichtDurch();
    void dreihundertsechzigIstNord();
    void ausserhalbWirdAbgelehntUndNichtUmgerechnet();
    void beiAblehnungBleibtDerAlteWertStehen();
};

void TstRotorPeilung::gueltigeWerteKommenDurch()
{
    double g = -1;
    QVERIFY(lies(QStringLiteral("0"), &g));     QCOMPARE(g, 0.0);
    QVERIFY(lies(QStringLiteral("143"), &g));   QCOMPARE(g, 143.0);
    QVERIFY(lies(QStringLiteral("359.9"), &g)); QCOMPARE(g, 359.9);
    QVERIFY(lies(QStringLiteral("  16 "), &g)); QCOMPARE(g, 16.0);
}

void TstRotorPeilung::leeresUndUnfugWerdenNichtZuNord()
{
    // DER Fall: `toDouble()` liefert hier ueberall 0, und 0 ist Nord.
    double g = 42;
    for (const char* s : {"", "   ", "abc", "N", "--", "1,5"}) {
        QVERIFY2(!lies(QString::fromLatin1(s), &g), s);
    }
    QCOMPARE(g, 42.0);   // unberuehrt
}

void TstRotorPeilung::nanUndUnendlichRutschenNichtDurch()
{
    double g = 42;
    for (const char* s : {"nan", "NaN", "inf", "-inf", "Infinity"}) {
        QVERIFY2(!lies(QString::fromLatin1(s), &g), s);
    }
    QCOMPARE(g, 42.0);
}

void TstRotorPeilung::dreihundertsechzigIstNord()
{
    // 360 gilt, wird aber auf 0 gelegt: der Rotor meint dieselbe Richtung,
    // und manche Steuerungen nehmen 360 nicht an.
    double g = -1;
    QVERIFY(lies(QStringLiteral("360"), &g));
    QCOMPARE(g, 0.0);
}

void TstRotorPeilung::ausserhalbWirdAbgelehntUndNichtUmgerechnet()
{
    // Umrechnen waere bequem und falsch: wer -90 schickt, hat sich vertan,
    // und 270 Grad sind eine andere Antwort als "ich habe mich vertan".
    double g = 42;
    for (const char* s : {"-1", "-90", "361", "720", "1000"}) {
        QVERIFY2(!lies(QString::fromLatin1(s), &g), s);
    }
    QCOMPARE(g, 42.0);
}

void TstRotorPeilung::beiAblehnungBleibtDerAlteWertStehen()
{
    // Ein Aufrufer, der die Rueckgabe prueft, darf sich darauf verlassen,
    // dass sein Wert unberuehrt bleibt -- sonst steht nach einer
    // abgelehnten Zeile eine halb geschriebene Zahl im Ziel.
    double g = 143.0;
    QVERIFY(!lies(QStringLiteral("Unfug"), &g));
    QCOMPARE(g, 143.0);
    QVERIFY(lies(QString(), nullptr) == false);   // nullptr vertraegt sich
}

QTEST_MAIN(TstRotorPeilung)
#include "tst_rotor_peilung.moc"
