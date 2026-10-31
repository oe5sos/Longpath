// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_flex_abstimmregler.cpp  (Longpath)
// =================================================================
//
// Betreiber 2026-10-31: „tunen will ich grundsaetzlich mit einem
// watt." Gemessen kamen bei `tunepower=10` zwischen 22,7 und 24,1 W
// heraus -- `tunepower` ist Prozent auf einer unbekannten Kurve.
//
// Die Zahlen hier sind SEINE Zahlen: 23 W bei 10 %, Ziel 1 W.
// =================================================================

#include <QtTest/QtTest>

#include "core/safety/FlexAbstimmRegler.h"

using namespace Longpath::safety;

class TstFlexAbstimmRegler : public QObject
{
    Q_OBJECT

private slots:
    void dieVorgabeIstEinWatt()
    {
        FlexAbstimmRegler r;
        QCOMPARE(r.zielWatt(), 1.0);
        QCOMPARE(r.startProzent(QStringLiteral("20m")),
                 FlexAbstimmRegler::kMinProzent);
    }

    // Der gemessene Fall: 10 % ergaben 23 W, Ziel ist 1 W.
    void zuVielWirdSofortHeruntergeregelt()
    {
        FlexAbstimmRegler r;
        r.setzeZielWatt(1.0);
        const auto neu = r.messung(QStringLiteral("20m"), 10, 23.0);
        QVERIFY2(neu.has_value(), "zu viel Leistung muss JETZT herunter");
        QVERIFY2(*neu < 10, "es muss kleiner werden");
        QVERIFY2(*neu >= FlexAbstimmRegler::kMinProzent, "nie unter eins");
        // Aus dem Verhaeltnis gerechnet, nicht in Einerschritten: 23 W
        // auf 1 W waere sonst zweiundzwanzig Traeger lang.
        QCOMPARE(*neu, 1);
    }

    // ── Waehrend des Traegers NIE hinauf ────────────────────────────
    //
    // Ein Regler, der am laufenden Sender aufdreht, ist ein zweites
    // Risiko -- hier haengt ein ACOM in der Leitung.
    void zuWenigDrehtNichtAufSondernLerntNur()
    {
        FlexAbstimmRegler r;
        r.setzeZielWatt(5.0);
        const auto neu = r.messung(QStringLiteral("20m"), 2, 1.0);
        QVERIFY2(!neu.has_value(),
                 "waehrend des Traegers darf NICHT aufgedreht werden");
        // Fuer das naechste Mal aber gemerkt, einen Schritt hoeher.
        QCOMPARE(r.gelernt(QStringLiteral("20m")).value_or(-1), 3);
        QCOMPARE(r.startProzent(QStringLiteral("20m")), 3);
    }

    void imFensterBleibtEsStehen()
    {
        FlexAbstimmRegler r;
        r.setzeZielWatt(1.0);
        const auto neu = r.messung(QStringLiteral("40m"), 1, 1.1);
        QVERIFY(!neu.has_value());
        QCOMPARE(r.gelernt(QStringLiteral("40m")).value_or(-1), 1);
        QVERIFY(!r.zielUnerreichbar(QStringLiteral("40m")));
    }

    // Ein Prozent ist der Anschlag. Kommt dort schon zu viel heraus,
    // ist das Ziel mit diesem Hebel nicht zu erreichen -- und das muss
    // gesagt werden, nicht wegregelt.
    void amAnschlagWirdDerBefundGemeldet()
    {
        FlexAbstimmRegler r;
        r.setzeZielWatt(1.0);
        const auto neu = r.messung(QStringLiteral("15m"), 1, 4.0);
        QVERIFY2(!neu.has_value(), "unter eins gibt es nichts");
        QVERIFY2(r.zielUnerreichbar(QStringLiteral("15m")),
                 "der Aufrufer muss erfahren, dass der Hebel nicht reicht");
        QCOMPARE(r.letzteLeistung(QStringLiteral("15m")).value_or(0.0), 4.0);
    }

    // Abwesenheit ist keine Aussage: unterhalb der Messbarkeit wird
    // weder geregelt noch gemerkt.
    void unterDerMessbarkeitZaehltNichts()
    {
        FlexAbstimmRegler r;
        r.setzeZielWatt(1.0);
        const auto neu = r.messung(QStringLiteral("20m"), 5, 0.2);
        QVERIFY(!neu.has_value());
        QVERIFY2(!r.gelernt(QStringLiteral("20m")).has_value(),
                 "das Ausklingen ist keine Messung");
        QVERIFY(!r.letzteLeistung(QStringLiteral("20m")).has_value());
    }

    void jedesBandLerntFuerSich()
    {
        FlexAbstimmRegler r;
        r.setzeZielWatt(1.0);
        (void) r.messung(QStringLiteral("20m"), 10, 23.0);
        QCOMPARE(r.startProzent(QStringLiteral("20m")), 1);
        QVERIFY2(!r.gelernt(QStringLiteral("40m")).has_value(),
                 "40m hat nichts gelernt — ein anderer Antennenweg");
        QCOMPARE(r.startProzent(QStringLiteral("40m")),
                 FlexAbstimmRegler::kMinProzent);
    }

    void dieDeckeWirdNieUeberschritten()
    {
        FlexAbstimmRegler r;
        r.setzeZielWatt(50.0);
        r.setzeDecke(3);
        for (int i = 0; i < 10; ++i) {
            (void) r.messung(QStringLiteral("20m"), 3, 1.0);
        }
        QCOMPARE(r.gelernt(QStringLiteral("20m")).value_or(-1), 3);
        QCOMPARE(r.startProzent(QStringLiteral("20m")), 3);
    }

    void ausgeschaltetRegeltNie()
    {
        FlexAbstimmRegler r;
        r.setzeZielWatt(1.0);
        r.setzeAn(false);
        QVERIFY(!r.messung(QStringLiteral("20m"), 10, 23.0).has_value());
        // Ausgeschaltet gilt die Einstellung des Betreibers, also die
        // Decke -- nicht das Kleinste.
        r.setzeDecke(10);
        QCOMPARE(r.startProzent(QStringLiteral("20m")), 10);
    }
    // ── Und er haengt wirklich im Sendeweg (2026-10-31) ──────────
    //
    // Derselbe Waechter wie beim Riegel: ein Regler, den niemand
    // fragt, ist gebaut und unerreichbar -- der Fehler, der an
    // diesem und dem Vortag vier Mal vorkam.
    void derReglerHaengtImAbstimmweg()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/models/RadioModel.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QString src = QString::fromUtf8(f.readAll());

        const int a = src.indexOf(QStringLiteral("bool RadioModel::flexAbstimmen("));
        QVERIFY(a > 0);
        const int e = src.indexOf(QStringLiteral("\n}\n"), a);
        QVERIFY(e > a);
        const QString rumpf = src.mid(a, e - a);
        QVERIFY2(rumpf.contains(QStringLiteral("m_flexAbstimmRegler.startProzent")),
                 "der Traeger faengt ohne den Regler an — dann steht wieder "
                 "irgendein Prozentwert von irgendwann am Geraet");
        QVERIFY2(rumpf.contains(QStringLiteral("setzeFlexAbstimmleistung")),
                 "die Leistung wird vor dem Traeger nicht gesetzt");

        // Und nachgeregelt wird er auch -- sonst bleibt der Startwert
        // stehen, egal was herauskommt.
        QVERIFY2(src.contains(QStringLiteral("m_flexAbstimmRegler.messung(")),
                 "niemand speist Messungen in den Regler");
        QVERIFY2(src.contains(QStringLiteral("flexAbstimmZielUnerreichbar")),
                 "der Befund 'Hebel reicht nicht' wird nicht gemeldet");
    }
};

QTEST_MAIN(TstFlexAbstimmRegler)
#include "tst_flex_abstimmregler.moc"
