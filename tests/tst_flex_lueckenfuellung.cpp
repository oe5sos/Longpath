// =================================================================
// tests/tst_flex_lueckenfuellung.cpp  (Longpath)
// =================================================================
//
// Die Lueckenfuellung im Flex-Tonweg: fehlende Pakete als
// ausgeblendete Stille wieder einschieben.
//
// Der entscheidende Fall heiszt
// `ohneFuellungSpringtDieWelle` — er fahrt dieselbe Luecke EINMAL
// ohne und EINMAL mit Fuellung und vergleicht den groeszten
// Probensprung. Gruen mit der Behebung belegt nichts; erst die
// Gegenprobe zeigt, dass es ueberhaupt etwas zu beheben gab.
//
// Longpath-original (der Prueflauf; die Fuellung selbst ist ein Port,
// siehe src/core/FlexLueckenfuellung.h).
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>

#include "core/FlexLueckenfuellung.h"

#include <cmath>

using namespace Longpath::FlexVita;

namespace {

// Ein Paket aus 128 Stereorahmen, als Sinus — eine echte Welle, damit
// ein Sprung messbar ist. `phase` setzt fort, wo das vorige endete.
QVector<float> paket(int rahmen, double& phase, double schrittRad)
{
    QVector<float> p(rahmen * 2);
    for (int i = 0; i < rahmen; ++i) {
        const float w = float(std::sin(phase));
        p[i * 2]     = w;
        p[i * 2 + 1] = w;
        phase += schrittRad;
    }
    return p;
}

// Der groeszte Sprung von einer Probe zur naechsten, im linken Kanal.
double groessterSprung(const QVector<float>& a)
{
    double max = 0.0;
    for (int i = 2; i < a.size(); i += 2) {
        max = std::max(max, double(std::abs(a[i] - a[i - 2])));
    }
    return max;
}

}  // namespace

class TestFlexLueckenfuellung : public QObject
{
    Q_OBJECT

private slots:
    void ohneOffeneLueckeBleibtAllesGleich()
    {
        double ph = 0.0;
        const QVector<float> ton = paket(128, ph, 0.05);
        Lueckenzustand z;
        const QVector<float> aus = fuelleLuecke(ton, z, true);
        QCOMPARE(aus, ton);
        QCOMPARE(z.letzteRahmen, 128);
        QCOMPARE(z.offeneLuecken, 0);
    }

    void ausgeschaltetWirdNichtGefuellt()
    {
        double ph = 0.0;
        Lueckenzustand z;
        fuelleLuecke(paket(128, ph, 0.05), z, false);
        z.offeneLuecken = 3;
        const QVector<float> ton = paket(128, ph, 0.05);
        const QVector<float> aus = fuelleLuecke(ton, z, false);
        QCOMPARE(aus.size(), ton.size());
        // Und der Zaehler wird trotzdem geleert: eine Luecke, die
        // niemand fuellt, darf nicht auf das naechste Paket warten.
        QCOMPARE(z.offeneLuecken, 0);
    }

    void dasErstePaketWirdNichtGefuellt()
    {
        // Ohne Vorgaenger gibt es keinen Rand, von dem herunterzublenden
        // waere. Abwesenheit ist keine Aussage.
        double ph = 0.0;
        Lueckenzustand z;
        z.offeneLuecken = 5;
        const QVector<float> ton = paket(128, ph, 0.05);
        QCOMPARE(fuelleLuecke(ton, z, true).size(), ton.size());
    }

    void eineLueckeWirdEingeschoben()
    {
        double ph = 0.0;
        Lueckenzustand z;
        fuelleLuecke(paket(128, ph, 0.05), z, true);
        z.offeneLuecken = 1;
        const QVector<float> aus = fuelleLuecke(paket(128, ph, 0.05), z, true);
        // 128 eingeschoben + 128 neu.
        QCOMPARE(aus.size(), 256 * 2);
        // Die Mitte der eingeschobenen Strecke ist still.
        QCOMPARE(aus[64 * 2], 0.0f);
        // Und sie beginnt beim gemerkten Rand, nicht bei null.
        QVERIFY(std::abs(aus[0]) > 0.01f);
    }

    void dieDeckeHaelt()
    {
        double ph = 0.0;
        Lueckenzustand z;
        fuelleLuecke(paket(128, ph, 0.05), z, true);
        // Mehr als die Decke kann gar nicht offen sein (der Aufrufer
        // begrenzt), aber die Fuellung muss auch dann rechnen koennen.
        z.offeneLuecken = kMaxGefuellteLuecken;
        const QVector<float> aus = fuelleLuecke(paket(128, ph, 0.05), z, true);
        QCOMPARE(aus.size(), (kMaxGefuellteLuecken * 128 + 128) * 2);
    }

    // ── Die Gegenprobe ───────────────────────────────────────────────
    void ohneFuellungSpringtDieWelle()
    {
        // Dieselbe Luecke zweimal: einmal so, wie es bis zum
        // 2026-10-30 war (Paket fehlt, naechstes schlieszt direkt an),
        // einmal mit Fuellung.
        //
        // Als Signal ein Pegelwechsel, nicht ein Sinus. Beim ersten
        // Versuch stand hier ein Sinus mit 0,6 rad je Probe, und die
        // alte Fassung sprang nur 0,591 — die Luecke von 128 Rahmen
        // traf zufaellig fast einen ganzen Zyklus. Ein Prueflauf, der
        // vom Zufall der Phase abhaengt, belegt nichts; ein
        // Pegelwechsel ist die Form, gegen die die Blende wirklich
        // arbeitet.
        constexpr float kPegel = 0.8f;
        const QVector<float> erstes(128 * 2, kPegel);
        const QVector<float> zweites(128 * 2, -kPegel);

        Lueckenzustand zOhne;
        Lueckenzustand zMit;
        fuelleLuecke(erstes, zOhne, false);
        fuelleLuecke(erstes, zMit, true);

        // ALT: das fehlende Paket hinterlaesst nichts, das naechste
        // schlieszt direkt an.
        QVector<float> alt = erstes;
        alt.append(zweites);

        // NEU: eine Luecke offen, Fuellung an.
        zMit.offeneLuecken = 1;
        QVector<float> neu = erstes;
        neu.append(fuelleLuecke(zweites, zMit, true));

        const double sprungAlt = groessterSprung(alt);
        const double sprungNeu = groessterSprung(neu);

        // Die alte Fassung springt den ganzen Pegelwechsel: 1,6.
        QVERIFY2(sprungAlt > 1.5,
                 qPrintable(QStringLiteral("alte Fassung sprang nur %1 — "
                                           "dann gab es nichts zu beheben")
                                .arg(sprungAlt)));
        // Mit Fuellung bleibt der groeszte Einzelschritt die Blende
        // selbst: 0,8 ueber 48 Rahmen als Kosinus, also hoechstens
        // 0,8 * pi/2 / 48 = 0,027.
        QVERIFY2(sprungNeu < 0.05,
                 qPrintable(QStringLiteral("mit Fuellung sprang es %1")
                                .arg(sprungNeu)));
        // Und die Zeitachse: 128 Rahmen fehlten, 128 sind wieder da.
        QCOMPARE(neu.size(), (128 + 128 + 128) * 2);
        QCOMPARE(alt.size(), (128 + 128) * 2);
    }

    // Dass die Blende auch eine laufende Welle nicht reiszt — hier
    // zaehlt nicht die Hoehe des alten Sprungs (die haengt an der
    // Phase), sondern dass die neue Fassung nicht mehr springt als die
    // Welle selbst.
    void dieWelleBleibtStetig()
    {
        const double schritt = 0.3;
        double ph = 0.0;
        Lueckenzustand z;
        const QVector<float> erstes = paket(128, ph, schritt);
        fuelleLuecke(erstes, z, true);
        for (int i = 0; i < 128; ++i) { ph += schritt; }

        z.offeneLuecken = 1;
        QVector<float> neu = erstes;
        neu.append(fuelleLuecke(paket(128, ph, schritt), z, true));

        // Steigung der Welle selbst, grosszuegig: sin' <= 1.
        QVERIFY2(groessterSprung(neu) <= schritt * 1.05,
                 qPrintable(QStringLiteral("sprang %1, Welle selbst %2")
                                .arg(groessterSprung(neu)).arg(schritt)));
    }

    void derRandWirdNachgezogen()
    {
        double ph = 0.0;
        Lueckenzustand z;
        const QVector<float> ton = paket(128, ph, 0.05);
        fuelleLuecke(ton, z, true);
        QCOMPARE(z.randL, ton[127 * 2]);
        QCOMPARE(z.randR, ton[127 * 2 + 1]);
    }

    void einLeeresPaketLoeschtDieOffeneLuecke()
    {
        Lueckenzustand z;
        z.offeneLuecken = 4;
        z.letzteRahmen  = 128;
        const QVector<float> leer;
        QCOMPARE(fuelleLuecke(leer, z, true).size(), 0);
        QCOMPARE(z.offeneLuecken, 0);
    }
};

QTEST_MAIN(TestFlexLueckenfuellung)
#include "tst_flex_lueckenfuellung.moc"
