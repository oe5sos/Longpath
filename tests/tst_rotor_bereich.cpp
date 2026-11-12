// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Erlaubter Bereich des Rotors (2026-11-08) ───────────────────────
//
// Betreiber: „zu hause ... soll der rotor nicht 360 drehen". Geprueft
// wird der WEG, nicht nur das Ziel -- ein Rotor mit Nordanschlag faehrt
// von 280 nach 80 Grad ueber Sueden.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/RotorBereich.h"
#include "core/RundumLauf.h"

using namespace Longpath;

namespace {
RotorBereich bogen(double von, double bis, double anschlag)
{
    RotorBereich b;
    b.an = true;
    b.vonGrad = von;
    b.bisGrad = bis;
    b.anschlagGrad = anschlag;
    return b;
}
} // namespace

class TstRotorBereich : public QObject {
    Q_OBJECT
private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void ohneGrenzeIstAllesErlaubt()
    {
        RotorBereich b;
        QVERIFY(b.erlaubt(180.0));
        QVERIFY(b.wegErlaubt(280.0, 80.0));
        // von == bis heisst: ganz herum.
        const RotorBereich voll = bogen(40.0, 40.0, 0.0);
        QVERIFY(voll.wegErlaubt(10.0, 350.0));
    }

    void derWegZaehltNichtNurDasZiel()
    {
        // Erlaubt 270 -> 90 ueber Nord.
        const RotorBereich nord = bogen(270.0, 90.0, 0.0);     // Anschlag Nord
        QVERIFY(nord.erlaubt(280.0));
        QVERIFY(nord.erlaubt(80.0));
        QVERIFY(!nord.erlaubt(180.0));
        QVERIFY2(!nord.wegErlaubt(280.0, 80.0),
                 "Nordanschlag: 280 -> 80 geht ueber Sueden, also durch die Sperre");
        QVERIFY(nord.wegErlaubt(280.0, 350.0));
        QVERIFY(nord.wegErlaubt(10.0, 80.0));

        const RotorBereich sued = bogen(270.0, 90.0, 180.0);   // Anschlag Sued
        QVERIFY2(sued.wegErlaubt(280.0, 80.0), "Suedanschlag: der Weg geht ueber Nord");
        QVERIFY(!sued.wegErlaubt(280.0, 180.0));

        const RotorBereich frei = bogen(270.0, 90.0, -1.0);    // dreht frei
        QVERIFY(frei.wegErlaubt(280.0, 80.0));                 // kurzer Weg ueber Nord
        QVERIFY(!frei.wegErlaubt(80.0, 100.0));
    }

    void dieReichweiteKenntDenAnschlag()
    {
        double anfang = 0.0, spanne = 0.0;
        // Anschlag im Bogen: zwei Stuecke, je nach Seite.
        const RotorBereich nord = bogen(270.0, 90.0, 0.0);
        QVERIFY(nord.reichweite(300.0, &anfang, &spanne));
        QCOMPARE(anfang, 270.0);
        QCOMPARE(spanne, 90.0);
        QVERIFY(nord.reichweite(30.0, &anfang, &spanne));
        QCOMPARE(anfang, 0.0);
        QCOMPARE(spanne, 90.0);
        QVERIFY(!nord.reichweite(180.0, &anfang, &spanne));
        // Anschlag ausserhalb: ein Stueck.
        const RotorBereich sued = bogen(270.0, 90.0, 180.0);
        QVERIFY(sued.reichweite(30.0, &anfang, &spanne));
        QCOMPARE(anfang, 270.0);
        QCOMPARE(spanne, 180.0);
    }

    void speichernMitDemVorhandenenAnschlag()
    {
        // Der Anschlag ist die schon vorhandene Einstellung "End stop".
        AppSettings::instance().setValue(QStringLiteral("RotorEndStopDeg"), 180.0);
        RotorBereich b = RotorBereich::load();
        QCOMPARE(b.anschlagGrad, 180.0);
        QVERIFY(!b.an);
        b.an = true; b.vonGrad = 300.0; b.bisGrad = 60.0;
        b.save();
        const RotorBereich c = RotorBereich::load();
        QVERIFY(c.an);
        QCOMPARE(c.vonGrad, 300.0);
        QCOMPARE(c.bisGrad, 60.0);
        QVERIFY(c.beschreibung().contains(QStringLiteral("300° → 060°")));
    }

    void derLaufBleibtImBereich()
    {
        const RotorBereich sued = bogen(300.0, 60.0, 180.0);
        const QVector<double> p = RundumLauf::plane(10.0, sued);
        QCOMPARE(p, (QVector<double>{300.0, 330.0, 0.0, 30.0, 60.0, 10.0}));
        double von = 10.0;
        for (double z : p) {
            QVERIFY(sued.erlaubt(z));
            QVERIFY2(sued.wegErlaubt(von, z), qPrintable(QStringLiteral("%1 -> %2").arg(von).arg(z)));
            von = z;
        }
        // Ausserhalb gestartet: kein Plan.
        QVERIFY(RundumLauf::plane(180.0, sued).isEmpty());
        // Ohne Grenze mit Nordanschlag: ab dem Anschlag ganz herum.
        RotorBereich n;
        n.anschlagGrad = 0.0;
        const QVector<double> v = RundumLauf::plane(100.0, n);
        QCOMPARE(v.first(), 0.0);
        QCOMPARE(v.at(v.size() - 2), 359.0);
        QCOMPARE(v.last(), 100.0);
    }
};

QTEST_MAIN(TstRotorBereich)
#include "tst_rotor_bereich.moc"
