// =================================================================
// tests/tst_ft8_tonwahl.cpp  (Longpath)
// =================================================================
//
// Eine freie Sendestelle suchen. Longpath-original -- weder WSJT-X
// noch MSHV tun das.
//
// Der wichtigste Fall heiszt `ohneNotWirdNichtGewechselt`: wer einen
// ruft, sucht ihn dort, wo er zuletzt war. Eine Automatik, die die
// Sendestelle bei jeder Gelegenheit verschiebt, kostet mehr QSOs, als
// sie bringt.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>

#include "core/Ft8Tonwahl.h"

using namespace Longpath;

class TestFt8Tonwahl : public QObject
{
    Q_OBJECT

private slots:
    void eineFreieStelleIstFrei()
    {
        const QVector<int> belegt{500, 900, 1800};
        QVERIFY(!stelleBelegt(belegt, 1400, 60));
        QVERIFY(stelleBelegt(belegt, 920, 60));
        // Genau am Rand: 60 Hz Abstand ist frei, 59 nicht.
        QVERIFY(!stelleBelegt(belegt, 960, 60));
        QVERIFY(stelleBelegt(belegt, 959, 60));
    }

    // ── Der wichtigste Fall ──────────────────────────────────────────
    void ohneNotWirdNichtGewechselt()
    {
        // Wer einen ruft, sucht ihn dort, wo er zuletzt war. Eine
        // Automatik, die ohne Grund verschiebt, kostet QSOs.
        const QVector<int> belegt{500, 900, 1800};
        QCOMPARE(freieTonlage(belegt, 200, 2800, 60, 1400), 1400);
        QCOMPARE(freieTonlage({}, 200, 2800, 60, 1500), 1500);
    }

    void aufEinerBelegtenStelleWirdAusgewichen()
    {
        const QVector<int> belegt{1500};
        const int neu = freieTonlage(belegt, 200, 2800, 60, 1500);
        QVERIFY2(neu != 1500, "es haette ausweichen muessen");
        QVERIFY2(!stelleBelegt(belegt, neu, 60),
                 qPrintable(QStringLiteral("auf %1 liegt immer noch "
                                           "jemand").arg(neu)));
    }

    void dieNaechstgelegeneFreieStelleGewinnt()
    {
        // Links und rechts frei: die Rechnung darf nicht quer durchs
        // Band springen.
        const QVector<int> belegt{1500};
        const int neu = freieTonlage(belegt, 200, 2800, 60, 1500);
        QVERIFY2(std::abs(neu - 1500) <= 70,
                 qPrintable(QStringLiteral("sprang auf %1").arg(neu)));
    }

    void derBereichWirdEingehalten()
    {
        // Alles unterhalb belegt: die Antwort muss trotzdem im
        // Bereich liegen.
        QVector<int> belegt;
        for (int hz = 200; hz <= 1400; hz += 40) { belegt << hz; }
        const int neu = freieTonlage(belegt, 200, 2800, 60, 800);
        QVERIFY2(neu >= 200 && neu <= 2800,
                 qPrintable(QStringLiteral("%1 liegt ausserhalb").arg(neu)));
        QVERIFY(!stelleBelegt(belegt, neu, 60));
    }

    // Ein Hound darf nicht unter 1000 -- der Bereich kommt von aussen,
    // und die Rechnung haelt ihn ein.
    void fuerDenHoundBleibtEsUeberTausend()
    {
        const QVector<int> belegt{1500, 1560, 1620};
        const int neu = freieTonlage(belegt, 1000, 2800, 60, 1500);
        QVERIFY2(neu >= 1000,
                 qPrintable(QStringLiteral("%1 liegt unter 1000").arg(neu)));
        QVERIFY(!stelleBelegt(belegt, neu, 60));
    }

    // ── Wenn nichts frei ist, lieber nichts tun ──────────────────────
    void einVollesBandAendertNichts()
    {
        // Jede Stelle im Bereich belegt.
        QVector<int> belegt;
        for (int hz = 190; hz <= 2810; hz += 20) { belegt << hz; }
        QCOMPARE(freieTonlage(belegt, 200, 2800, 60, 1500), 1500);
    }

    void unsinnigeVorgabenAendernNichts()
    {
        QCOMPARE(freieTonlage({500}, 2800, 200, 60, 1500), 1500);  // bis<von
        QCOMPARE(freieTonlage({500}, 200, 2800, 0, 1500), 1500);   // Abstand 0
    }

    // ── Die Gegenprobe ───────────────────────────────────────────────
    //
    // Ohne diesen Lauf belegt keiner der obigen, dass die Rechnung
    // ueberhaupt etwas kann: eine Funktion, die IMMER den Wunsch
    // zurueckgibt, waere in sechs von acht Faellen gruen.
    void eineFunktionDieImmerDenWunschGaebeWaereHierRot()
    {
        const QVector<int> belegt{1500};
        const int neu = freieTonlage(belegt, 200, 2800, 60, 1500);
        QVERIFY2(neu != 1500,
                 "wenn hier der Wunsch zurueckkaeme, pruefte dieser "
                 "Prueflauf nichts");
        // Und die Stelle muss WIRKLICH frei sein, nicht nur anders.
        QVERIFY(!stelleBelegt(belegt, neu, 60));
    }
};

QTEST_APPLESS_MAIN(TestFt8Tonwahl)
#include "tst_ft8_tonwahl.moc"
