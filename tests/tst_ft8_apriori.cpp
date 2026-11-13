// =================================================================
// tests/tst_ft8_apriori.cpp  (Longpath)
// =================================================================
//
// ── Was man weiss, bevor man hinhoert (2026-11-13) ──────────────────
//
// Betreiber: FT8 und FT4 sollen laufen „mit allem was dazu gehoert."
//
// Dieser Lauf muss genau EINES zeigen, und zwar in einem Durchgang:
// ein Signal, das der gewoehnliche Dekoder NICHT mehr findet, wird
// mit bekannten Bits gefunden. Ohne die erste Haelfte waere die
// zweite wertlos -- ein gruener Lauf, der auch ohne die Behebung
// gruen waere, belegt nichts.
//
// Dazu kommt die Gegenrichtung, die ebenso wichtig ist: eine FALSCHE
// Gewissheit darf kein falsches QSO erzeugen. Der CRC ist die
// Kontrolle; wenn er traegt, kostet eine danebenliegende Maske nur
// Rechenzeit.
//
// Das Rauschen ist dasselbe wie in tst_ft8_stoerabstand: weiss ueber
// die ganze Bandbreite, Bezug 2500 Hz wie bei WSJT-X.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-13 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>
#include <QVector>
#include <cmath>
#include <random>

#include "core/Ft8Apriori.h"
#include "core/Ft8Decoder.h"

using namespace Longpath;

namespace {

constexpr double kHalbeAbtastrate   = 6000.0;
constexpr double kBezugBandbreiteHz = 2500.0;

QVector<float> mitRauschen(const QVector<float>& rein, double zielDb,
                           uint32_t saat)
{
    double leistung = 0.0;
    for (float v : rein) { leistung += double(v) * double(v); }
    leistung /= double(rein.size());

    const double imBand  = leistung / std::pow(10.0, zielDb / 10.0);
    const double jeProbe = imBand * (kHalbeAbtastrate / kBezugBandbreiteHz);
    const double sigma   = std::sqrt(jeProbe);

    std::mt19937 wuerfel(saat);
    std::normal_distribution<double> glocke(0.0, sigma);

    QVector<float> aus = rein;
    for (float& v : aus) { v = float(double(v) + glocke(wuerfel)); }
    return aus;
}

}  // namespace

class Ft8AprioriTest : public QObject
{
    Q_OBJECT

private slots:

    // ── Die Vorgabe selbst ──────────────────────────────────────────
    //
    // Bevor irgendetwas dekodiert wird: steht in der Maske das, was
    // dort stehen soll? Die Grenzen stammen aus WSJT-X
    // (ft8b.f90:319 und :359), und eine um eins verschobene Grenze
    // faellt im Dekoderlauf nicht auf -- sie macht ihn nur schlechter.
    void dieMaskeTrifftDieRichtigenStellen()
    {
        const auto a = baueApVorgabe(ApTyp::MeinRuf,
                                     QStringLiteral("OE5SOS"));
        QVERIFY(a.brauchbar);

        int gesetzt = 0;
        for (int i = 0; i < kApBits; ++i) { gesetzt += a.maske[size_t(i)]; }
        // 29 Bits Rufzeichen-und-R1, dazu die drei Typbits.
        QCOMPARE(gesetzt, 32);
        for (int i = 0; i < 29; ++i) { QVERIFY(a.maske[size_t(i)] == 1); }
        for (int i = 29; i < 74; ++i) { QVERIFY(a.maske[size_t(i)] == 0); }
        // i3 = 1, also 0,0,1 -- eine Standardnachricht.
        QCOMPARE(int(a.bits[74]), 0);
        QCOMPARE(int(a.bits[75]), 0);
        QCOMPARE(int(a.bits[76]), 1);

        const auto b = baueApVorgabe(ApTyp::MeinRufUndSeiner,
                                     QStringLiteral("OE5SOS"),
                                     QStringLiteral("DL1ABC"));
        QVERIFY(b.brauchbar);
        int gesetztB = 0;
        for (int i = 0; i < kApBits; ++i) { gesetztB += b.maske[size_t(i)]; }
        QCOMPARE(gesetztB, 61);   // 58 + 3
        // Die vorderen 29 muessen dieselben sein wie oben: dasselbe
        // Rufzeichen an derselben Stelle.
        for (int i = 0; i < 29; ++i) {
            QCOMPARE(int(b.bits[size_t(i)]), int(a.bits[size_t(i)]));
        }
    }

    // Ein Rufzeichen, das nicht in die 28 Bit passt, steht gehasht in
    // der Nachricht -- die Masken zeigen dann ins Leere. Lieber keine
    // Vorgabe als eine, die an der falschen Stelle zieht.
    void einSonderrufzeichenGibtKeineVorgabe()
    {
        QVERIFY(!baueApVorgabe(ApTyp::MeinRuf, QString()).brauchbar);
        QVERIFY(!baueApVorgabe(ApTyp::MeinRuf, QStringLiteral("X")).brauchbar);
        QVERIFY(!baueApVorgabe(ApTyp::MeinRufUndSeiner,
                               QStringLiteral("OE5SOS")).brauchbar);
    }

    // ── Der Gewinn, in einem Durchgang ──────────────────────────────
    void wasOhneVorwissenUntergehtWirdDamitGefunden()
    {
        const QString satz = QStringLiteral("OE5SOS DL1ABC RR73");
        QVector<float> rein;
        QVERIFY(Ft8Decoder::encodeBlock(satz, 1500.0f,
                                        Ft8Decoder::Protocol::Ft8, &rein));

        const QVector<ApVorgabe> wissen = {
            baueApVorgabe(ApTyp::MeinRufUndSeiner,
                          QStringLiteral("OE5SOS"),
                          QStringLiteral("DL1ABC")),
            baueApVorgabe(ApTyp::MeinRuf, QStringLiteral("OE5SOS")),
        };
        QVERIFY(wissen.at(0).brauchbar);

        // ── Beide Grenzen messen, nicht eine behaupten ──────────
        //
        // Wo hoert der gewoehnliche Weg auf, wo der mit Vorwissen?
        // Erst der Abstand zwischen beiden sagt, ob sich die Sache
        // lohnt. Eine einzelne Schwelle waere eine Behauptung.
        //
        // Von laut nach leise in halben Dezibel; je Pegel dieselbe
        // Saat, damit nicht das Rauschen den Unterschied macht.
        const auto enthaelt = [&satz](const QVector<Ft8Decode>& f) {
            for (const Ft8Decode& d : f) {
                if (d.text.trimmed() == satz) { return true; }
            }
            return false;
        };

        double letzteOhne = 0.0;
        double letzteMit  = 0.0;
        QString spur;
        for (double db = -10.0; db >= -30.0; db -= 0.5) {
            const QVector<float> ton = mitRauschen(rein, db, 4242u);

            const bool ohne = enthaelt(Ft8Decoder::decodeBlock(
                ton.constData(), int(ton.size()),
                Ft8Decoder::Protocol::Ft8, 1));
            const bool mit = enthaelt(Ft8Decoder::decodeBlock(
                ton.constData(), int(ton.size()),
                Ft8Decoder::Protocol::Ft8, 1, wissen));

            if (ohne && letzteOhne == 0.0) { /* noch nicht die Grenze */ }
            if (ohne) { letzteOhne = db; }
            if (mit)  { letzteMit  = db; }
            spur += QStringLiteral("%1 dB  ohne=%2  mit=%3\n")
                        .arg(db, 6, 'f', 1)
                        .arg(ohne ? "ja  " : "nein")
                        .arg(mit ? "ja" : "nein");
        }

        qInfo().noquote()
            << QStringLiteral("\nGrenze ohne Vorwissen: %1 dB\n"
                              "Grenze mit Vorwissen:  %2 dB\n"
                              "Gewinn: %3 dB tiefer\n%4")
                   .arg(letzteOhne, 0, 'f', 1)
                   .arg(letzteMit, 0, 'f', 1)
                   .arg(letzteOhne - letzteMit, 0, 'f', 1)
                   .arg(spur);

        QVERIFY2(letzteOhne < 0.0 && letzteMit < 0.0,
                 "Das Signal wurde ueberhaupt nicht dekodiert -- dann "
                 "misst dieser Lauf nichts.");
        QVERIFY2(letzteMit < letzteOhne,
                 qPrintable(QStringLiteral(
                     "Vorwissen reicht nicht tiefer als der gewoehnliche "
                     "Weg (ohne %1 dB, mit %2 dB) -- dann bringt es "
                     "nichts:\n%3")
                                .arg(letzteOhne, 0, 'f', 1)
                                .arg(letzteMit, 0, 'f', 1)
                                .arg(spur)));

        // Und ein Fund mit Vorwissen muss sich als solcher zeigen.
        const QVector<float> tief = mitRauschen(rein, letzteMit, 4242u);
        bool markiert = false;
        for (const Ft8Decode& d : Ft8Decoder::decodeBlock(
                 tief.constData(), int(tief.size()),
                 Ft8Decoder::Protocol::Ft8, 1, wissen)) {
            if (d.text.trimmed() == satz && d.mitVorwissen) { markiert = true; }
        }
        QVERIFY2(markiert, "Fund mit Vorwissen ist nicht gekennzeichnet");
    }

    // ── Die Gegenrichtung: falsches Wissen erfindet nichts ──────────
    //
    // Wenn eine danebenliegende Maske ein QSO erzeugen koennte, waere
    // das Verfahren unbrauchbar -- schlimmer als gar keines, weil ein
    // erfundenes Rufzeichen ins Logbuch wandert. Der CRC muss das
    // auffangen.
    void falschesVorwissenErfindetKeinQso()
    {
        QVector<float> rein;
        QVERIFY(Ft8Decoder::encodeBlock(QStringLiteral("CQ DL9XYZ JO62"),
                                        1500.0f,
                                        Ft8Decoder::Protocol::Ft8, &rein));

        // Vorwissen ueber ein QSO, das gar nicht laeuft.
        const QVector<ApVorgabe> falsch = {
            baueApVorgabe(ApTyp::MeinRufUndSeiner,
                          QStringLiteral("OE5SOS"),
                          QStringLiteral("DL1ABC")),
        };

        for (double db = -18.0; db >= -28.0; db -= 1.0) {
            const QVector<float> ton = mitRauschen(rein, db, 99u);
            const auto funde = Ft8Decoder::decodeBlock(
                ton.constData(), int(ton.size()),
                Ft8Decoder::Protocol::Ft8, 1, falsch);

            for (const Ft8Decode& d : funde) {
                const QString t = d.text.trimmed();
                QVERIFY2(!t.contains(QStringLiteral("DL1ABC")),
                         qPrintable(QStringLiteral(
                             "Bei %1 dB aus falschem Vorwissen erfunden: %2")
                                        .arg(db, 0, 'f', 1).arg(t)));
            }
        }
    }
};

QTEST_MAIN(Ft8AprioriTest)
#include "tst_ft8_apriori.moc"
