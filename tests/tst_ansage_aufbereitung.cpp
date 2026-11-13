// =================================================================
// tests/tst_ansage_aufbereitung.cpp  (Longpath)
// =================================================================
//
// Ansagen contesttauglich (core/audio/AnsageAufbereitung): gegen
// kuenstliche Aufnahmen, die aussehen wie am Mac mit eingebautem
// Mikrofon -- Rauschen, ein Tastenklick beim Starten, Sprache in
// Silben, Stille, ein Klick beim Beenden.
//
// Geprueft:
//   * die Stille vorn und hinten faellt weg, der Rand bleibt
//   * die Klicks zaehlen nicht als Sprache und fallen mit weg
//   * nur Rauschen oder nur ein Klick: nichts gefunden, nichts gekuerzt
//   * leise und laute Sprache landen auf demselben Pegel, die Spitze
//     bleibt unter der Grenze
//   * der Hochpass nimmt 50 Hz und laesst 1 kHz
//
// =================================================================
// Modification history (Longpath):
//   2026-11-13 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>

#include "core/audio/AnsageAufbereitung.h"

#include <cmath>
#include <random>

using namespace Longpath;
namespace AA = Longpath::AnsageAufbereitung;

namespace {

constexpr int kRate = 48000;

int ms(int m) { return m * kRate / 1000; }

float db(double v) { return float(20.0 * std::log10(std::max(v, 1e-9))); }

// Rauschen bei etwa -60 dBFS (eingebautes Mikrofon im stillen Raum).
void rauschen(QVector<float>& v, int von, int bis, std::mt19937& zufall)
{
    std::normal_distribution<float> n(0.0f, 0.001f);
    for (int i = von; i < bis; ++i) { v[i] += n(zufall); }
}

// Ein Tastenklick: 15 ms, steiler Anstieg, laut.
void klick(QVector<float>& v, int bei, float hoehe = 0.6f)
{
    for (int i = 0; i < ms(15); ++i) {
        const float abfall = std::exp(-float(i) / float(ms(3)));
        v[bei + i] += hoehe * abfall * ((i % 7 < 3) ? 1.0f : -1.0f);
    }
}

// „Sprache": drei Toene mit Silbenhuelle (4 Silben je Sekunde).
void sprache(QVector<float>& v, int von, int bis, float hoehe)
{
    for (int i = von; i < bis; ++i) {
        const double t = double(i - von) / kRate;
        const double silbe = 0.55 + 0.45 * std::sin(2.0 * M_PI * 4.0 * t);
        const double ton = 0.5 * std::sin(2.0 * M_PI * 220.0 * t)
                         + 0.3 * std::sin(2.0 * M_PI * 660.0 * t)
                         + 0.2 * std::sin(2.0 * M_PI * 1800.0 * t);
        v[i] += float(hoehe * silbe * ton);
    }
}

// Wie am Mac: Klick bei 30 ms, Sprache 600..1800 ms, Klick bei 2600 ms.
QVector<float> aufnahme(float hoehe = 0.2f)
{
    std::mt19937 zufall(7);
    QVector<float> v(ms(2800), 0.0f);
    rauschen(v, 0, v.size(), zufall);
    klick(v, ms(30));
    sprache(v, ms(600), ms(1800), hoehe);
    klick(v, ms(2600));
    return v;
}

double effektiv(const QVector<float>& v, int von, int bis)
{
    double s = 0.0;
    for (int i = von; i < bis; ++i) { s += double(v[i]) * v[i]; }
    return std::sqrt(s / std::max(1, bis - von));
}

} // namespace

class TstAnsageAufbereitung : public QObject
{
    Q_OBJECT

private slots:

    void stilleUndKlicksFallenWeg()
    {
        const QVector<float> roh = aufnahme();
        AA::Befund b;
        const QVector<float> aus = AA::kuerze(roh, kRate, &b);
        QVERIFY(b.sprache);

        // Die Sprache beginnt bei 600 ms. Vorn bleiben kVorMs (+ ein
        // Rahmen Messgenauigkeit), der Startklick bei 30 ms ist weg.
        const double laenge = double(aus.size()) / kRate;
        const double soll = 1.2 + AA::kVorMs / 1000.0 + AA::kNachMs / 1000.0;
        QVERIFY2(std::abs(laenge - soll) < 0.05,
                 qPrintable(QStringLiteral("%1 s statt %2 s").arg(laenge).arg(soll)));

        // Im Ergebnis gibt es keinen Ausschlag ueber der Sprachspitze --
        // der Klick (0,6) waere einer.
        float spitze = 0.0f;
        for (float x : aus) { spitze = std::max(spitze, std::fabs(x)); }
        QVERIFY2(spitze < 0.25f, qPrintable(QString::number(spitze)));

        // Weiche Kanten: der erste und letzte Wert sind null.
        QCOMPARE(aus.first(), 0.0f);
        QCOMPARE(aus.last(), 0.0f);
    }

    void nurRauschenOderKlickBleibtWieEsIst()
    {
        std::mt19937 zufall(3);
        QVector<float> still(ms(2000), 0.0f);
        rauschen(still, 0, still.size(), zufall);
        AA::Befund b;
        QCOMPARE(AA::kuerze(still, kRate, &b).size(), still.size());
        QVERIFY(!b.sprache);
        QCOMPARE(AA::verstaerkung(b), 1.0f);

        // Ein Klick allein ist keine Sprache.
        QVector<float> nurKlick = still;
        klick(nurKlick, ms(500));
        QCOMPARE(AA::kuerze(nurKlick, kRate, &b).size(), nurKlick.size());
        QVERIFY(!b.sprache);
    }

    void leiseUndLautLandenGleich()
    {
        // 20 dB auseinander aufgenommen.
        const float leiseVerst = AA::verstaerkungFuerSendung(aufnahme(0.02f), kRate);
        const float lautVerst  = AA::verstaerkungFuerSendung(aufnahme(0.2f), kRate);
        QVERIFY2(leiseVerst > lautVerst * 5.0f,
                 qPrintable(QStringLiteral("%1 / %2").arg(leiseVerst).arg(lautVerst)));

        for (const float hoehe : {0.02f, 0.2f}) {
            const QVector<float> roh = AA::kuerze(aufnahme(hoehe), kRate);
            const QVector<float> aus = AA::fuerSendung(roh, kRate);
            const AA::Befund b = AA::miss(aus, kRate);
            QVERIFY(b.sprache);
            QVERIFY2(std::abs(b.spracheDb - AA::kZielSpracheDb) < 1.5f,
                     qPrintable(QStringLiteral("Hoehe %1: Sprache %2 dBFS")
                                    .arg(hoehe).arg(b.spracheDb)));
            QVERIFY2(b.spitzeDb <= AA::kZielSpitzeDb + 0.1f,
                     qPrintable(QStringLiteral("Spitze %1 dBFS").arg(b.spitzeDb)));
        }
    }

    void spitzeBegrenztDieVerstaerkung()
    {
        // Ein einzelner lauter Ausschlag mitten in leiser Sprache: die
        // Spitze darf nicht ueber die Grenze, auch wenn die Sprache dann
        // leiser bleibt als das Ziel.
        QVector<float> roh = aufnahme(0.02f);
        roh[ms(1000)] = 0.9f;
        const QVector<float> aus = AA::fuerSendung(roh, kRate);
        float spitze = 0.0f;
        for (float x : aus) { spitze = std::max(spitze, std::fabs(x)); }
        QVERIFY2(db(spitze) <= AA::kZielSpitzeDb + 0.1f, qPrintable(QString::number(db(spitze))));
    }

    void hochpassNimmtRumpeln()
    {
        auto durch = [](double hz) {
            AA::Hochpass hp;
            hp.setze(kRate);
            QVector<float> v(kRate);
            for (int i = 0; i < v.size(); ++i) {
                v[i] = hp(float(std::sin(2.0 * M_PI * hz * i / kRate)));
            }
            return db(effektiv(v, kRate / 2, kRate) * std::sqrt(2.0));
        };
        QVERIFY2(durch(50.0) < -15.0f, qPrintable(QString::number(durch(50.0))));
        QVERIFY2(std::abs(durch(1000.0)) < 0.5f, qPrintable(QString::number(durch(1000.0))));
        QVERIFY2(std::abs(durch(150.0) + 3.0f) < 0.5f, qPrintable(QString::number(durch(150.0))));
    }
};

QTEST_GUILESS_MAIN(TstAnsageAufbereitung)
#include "tst_ansage_aufbereitung.moc"
