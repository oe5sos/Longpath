// no-port-check: Longpath-original test, no Thetis logic.
//
// Der Messkern hinter den vier Durchlass-Entwuerfen (2026-11-08), ohne
// Fenster und ohne GPU: Texte, Rast, Signalerkennung, Luecken, Haltung,
// Lupen-Bins. Siehe src/gui/spectrum/Durchlass.h.

#include <QtTest/QtTest>

#include "gui/spectrum/Durchlass.h"

using namespace Longpath::Durchlass;

namespace {
QVector<float> rauschen(int n, float boden = -135.0f)
{
    QVector<float> v(n, boden);
    // ein leichtes, festes Muster statt Zufall: +/-1 dB
    for (int i = 0; i < n; ++i) { v[i] += static_cast<float>((i * 7) % 3) - 1.0f; }
    return v;
}
void setze(QVector<float>& v, int a, int e, float dbm)
{
    for (int i = a; i < e; ++i) { v[i] = dbm; }
}
} // namespace

class TstDurchlass : public QObject {
    Q_OBJECT
private slots:
    void dieBandbreiteAlsText()
    {
        QCOMPARE(bandbreiteText(150, 2850), QStringLiteral("2.70 kHz"));
        QCOMPARE(bandbreiteText(-2850, -150), QStringLiteral("2.70 kHz"));
        QCOMPARE(bandbreiteText(-250, 250), QStringLiteral("500 Hz"));
        QCOMPARE(bandbreiteKurz(150, 2850), QStringLiteral("USB 2.7k"));
        QCOMPARE(bandbreiteKurz(-2850, -150), QStringLiteral("LSB 2.7k"));
        QCOMPARE(bandbreiteKurz(-3000, 3000), QStringLiteral("6.0k"));
        QCOMPARE(bandbreiteKurz(400, 900), QStringLiteral("USB 500"));
    }

    void dieKanteRastetAufFuenfzigHertz()
    {
        QCOMPARE(raste(2837), 2850);
        QCOMPARE(raste(2824), 2800);
        QCOMPARE(raste(-2837), -2850);
        QCOMPARE(raste(150), 150);
        QCOMPARE(raste(2837, 1), 2837);
    }

    // Eine SSB-Aussendung mit einem Silbenloch bleibt EIN Signal; ein
    // einzelner Traeger ist auch eines; Rauschen ist keines.
    void signaleWerdenErkanntUndGemessen()
    {
        QVector<float> v = rauschen(400);
        setze(v, 100, 111, -110.0f);
        v[105] = -130.0f;                      // Silbenloch, 1 Punkt
        v[200] = -100.0f;                      // Traeger
        const float boden = rauschboden(v);
        QVERIFY(std::abs(boden - (-135.0f)) <= 1.0f);

        const QVector<Signal> s = erkenneSignale(v, 220.0, boden);
        QCOMPARE(s.size(), 2);
        QCOMPARE(s[0].a, 100);
        QCOMPARE(s[0].e, 111);
        QVERIFY(std::abs(s[0].breiteHz - 11 * 220.0) < 1.0);
        QVERIFY(std::abs(s[0].snrDb - 25.0f) <= 1.0f);
        QVERIFY(s[0].mittePx > 104.0 && s[0].mittePx < 106.0);
        QCOMPARE(s[1].a, 200);
        QCOMPARE(s[1].e, 201);
    }

    void freieStellenZwischenDenSignalen()
    {
        QVector<float> v = rauschen(400);
        setze(v, 50, 61, -110.0f);
        setze(v, 150, 161, -112.0f);
        setze(v, 175, 186, -112.0f);           // nur 14 Punkte nach dem zweiten
        v[100] = -120.0f;                      // einzelne Rauschspitze: frei
        const float boden = rauschboden(v);
        // 220 Hz je Punkt: Schutz 500 Hz = 2 Punkte, mindestens 3 kHz.
        const QVector<Luecke> l = findeLuecken(v, 220.0, 40, 200, boden);
        QCOMPARE(l.size(), 1);                 // 161..175 = 3,1 kHz - 1 kHz zu schmal
        QCOMPARE(l[0].a, 61 + 2);
        QCOMPARE(l[0].e, 150 - 2);
        // Der Suchbereich beginnt frei: dort kein Schutzabstand.
        const QVector<Luecke> l2 = findeLuecken(v, 220.0, 0, 50, boden);
        QCOMPARE(l2.size(), 1);
        QCOMPARE(l2[0].a, 0);
    }

    // Erst beim zweiten Mal zeigen, erst nach dem dritten Fehlen loslassen.
    void klammernFlackernNicht()
    {
        Signal s;
        s.a = 100; s.e = 110; s.mittePx = 105.0; s.snrDb = 20.0f;
        Haltung h;
        QVERIFY(h.einspielen({s}).isEmpty());
        QCOMPARE(h.einspielen({s}).size(), 1);
        QCOMPARE(h.einspielen({}).size(), 1);
        QCOMPARE(h.einspielen({}).size(), 1);
        QVERIFY(h.einspielen({}).isEmpty());
        // Ein leicht verschobenes Signal ist dasselbe.
        h.leeren();
        h.einspielen({s});
        s.mittePx = 107.0;
        QCOMPARE(h.einspielen({s}).size(), 1);
    }

    // Bandplan-Beschriftung weicht den Merkpunkten aus: Mitte, links,
    // rechts -- und wenn nichts frei ist, -1 (dann gewinnt die Schrift).
    void dieBeschriftungWeichtDenPunktenAus()
    {
        QCOMPARE(bandplanTextX(0, 100, 40, {}), 30);
        QCOMPARE(bandplanTextX(0, 100, 40, {50}), 4);
        QCOMPARE(bandplanTextX(0, 100, 40, {20, 50}), 56);
        QCOMPARE(bandplanTextX(0, 100, 40, {20, 50, 80}), -1);
        QCOMPARE(bandplanTextX(0, 30, 40, {}), -1);     // passt gar nicht
    }

    void dieLupeNimmtDieRichtigenBins()
    {
        const BinBereich b = lupenBins(4096, 10.0, 0.0, 1000.0, 2000.0);
        QCOMPARE(b.erster, 100);
        QCOMPARE(b.letzter, 200);
        const BinBereich k = lupenBins(4096, 10.0, 0.0, -500.0, 300.0);
        QCOMPARE(k.erster, 0);
        QCOMPARE(k.letzter, 30);
        QVERIFY(lupenBins(4096, 10.0, 0.0, 50000.0, 60000.0).leer());
    }
};

QTEST_APPLESS_MAIN(TstDurchlass)
#include "tst_durchlass.moc"
