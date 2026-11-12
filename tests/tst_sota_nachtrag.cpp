// no-port-check: Longpath-original test, no Thetis logic.
//
// ── MY_SOTA_REF nachtragen: CSV lesen und zuordnen (2026-11-11) ─────
//
// Das Log ist Martins zwei Gipfeltagen nachgebaut: am 21.06. und
// 04.07.2026 auf OE/OO-106, am 04.07. ausserdem morgens ein 20-m-QSO von
// zu Hause (IZ7HNO 09:56). Geprueft:
//   1. SOTA-V2-Format, Kopfzeilen-Format, lose Zeilen; dd/mm, dd/mm/yy,
//      mm/dd (an einem Monat > 12 erkannt), Semikolon.
//   2. Vorschlaege: was die Datei bestaetigt, ist vorausgewaehlt; was an
//      dem Tag nicht in der Datei steht, nicht; ohne Rufzeichen in der
//      Datei alles am Tag; ein gesetzter MY_SOTA_REF bleibt.
//   3. Zwei Gipfel an einem Tag: per Rufzeichen getrennt, sonst nur ein
//      Hinweis.
//   4. Anwenden setzt nur leere Felder, S2S auch SOTA_REF.

#include <QtTest/QtTest>

#include <QTimeZone>

#include "core/SotaNachtrag.h"

using namespace Longpath;
using namespace Longpath::SotaNachtrag;

namespace {

LogEntry qso(const QString& call, const QString& band, int tag, int monat,
             int h, int m)
{
    LogEntry e;
    e.call = call;
    e.band = band;
    e.mode = band == QLatin1String("2m") ? QStringLiteral("SSB") : QStringLiteral("SSB");
    e.myGridSquare = QStringLiteral("JN67VV");
    e.timeOn = QDateTime(QDate(2026, monat, tag), QTime(h, m), QTimeZone::UTC);
    return e;
}

QVector<LogEntry> martinsTage()
{
    QVector<LogEntry> log{
        qso(QStringLiteral("OE5FPL"),   QStringLiteral("6m"),  21, 6,  8, 23),   // 0
        qso(QStringLiteral("OE5PEN/P"), QStringLiteral("2m"),  21, 6,  9, 39),   // 1
        qso(QStringLiteral("OE5PCH"),   QStringLiteral("2m"),  21, 6, 11, 19),   // 2
        qso(QStringLiteral("IZ7HNO"),   QStringLiteral("20m"),  4, 7,  9, 56),   // 3
        qso(QStringLiteral("OM2Y"),     QStringLiteral("2m"),   4, 7, 19,  8),   // 4
        qso(QStringLiteral("SN7L"),     QStringLiteral("2m"),   4, 7, 19, 26),   // 5
        qso(QStringLiteral("DL1ABC"),   QStringLiteral("20m"),  1, 5, 10,  0),   // 6
        qso(QStringLiteral("OK1XYZ"),   QStringLiteral("2m"),   4, 7, 19, 30),   // 7
    };
    log[7].mySotaRef = QStringLiteral("OE/OO-107");   // schon gesetzt
    return log;
}

const QByteArray kV2 =
    "V2,OE5SOS/P,OE/OO-106,04/07/2026,1908,144MHz,SSB,OM2Y,,\r\n"
    "V2,OE5SOS/P,OE/OO-106,04/07/26,1926,144MHz,SSB,SN7L,,\r\n"
    "V2,OE5SOS/P,OE/OO-106,21/06/2026,1119,144MHz,FM,OE5PCH,,\r\n"
    "V2,OE5SOS/P,OE/OO-106,21/06/2026,0939,144MHz,FM,OE5PEN,OE/OO-120,\"s2s, nice\"\r\n"
    "V2,OE5SOS/P,OE/OO-001,15/08/2026,1000,144MHz,FM,OE5XYZ,,\r\n";

const Vorschlag* fuer(const Ergebnis& e, int index)
{
    for (const Vorschlag& v : e.vorschlaege) {
        if (v.index == index) { return &v; }
    }
    return nullptr;
}

} // namespace

class TstSotaNachtrag : public QObject {
    Q_OBJECT
private slots:

    void v2Format()
    {
        QString fehler;
        const Datei d = leseCsv(kV2, &fehler);
        QCOMPARE(d.format, QStringLiteral("SOTA V2"));
        QCOMPARE(d.zeilen.size(), 5);
        QVERIFY(d.mitQsos);
        QCOMPARE(d.zeilen.at(0).datum, QDate(2026, 7, 4));
        QCOMPARE(d.zeilen.at(1).datum, QDate(2026, 7, 4));      // dd/mm/yy
        QCOMPARE(d.zeilen.at(0).zeit, QTime(19, 8));
        QCOMPARE(d.zeilen.at(0).gipfel, QStringLiteral("OE/OO-106"));
        QCOMPARE(d.zeilen.at(0).gegenCall, QStringLiteral("OM2Y"));
        QCOMPARE(d.zeilen.at(3).gegenGipfel, QStringLiteral("OE/OO-120"));
    }

    void vorschlaegeMitRufzeichen()
    {
        const QVector<LogEntry> log = martinsTage();
        const Ergebnis e = vorschlaege(log, leseCsv(kV2));
        QCOMPARE(e.vorschlaege.size(), 6);
        QCOMPARE(e.schonGesetzt, 1);
        QCOMPARE(e.tageOhneQso, 1);          // 15.08. hat kein QSO im Log
        QVERIFY(e.mehrdeutig.isEmpty());
        QVERIFY(!fuer(e, 6));                // kein Aktivierungstag
        QVERIFY(!fuer(e, 7));                // schon gesetzt

        for (int i : {1, 2, 4, 5}) {
            const Vorschlag* v = fuer(e, i);
            QVERIFY2(v, qPrintable(log.at(i).call));
            QCOMPARE(v->gipfel, QStringLiteral("OE/OO-106"));
            QVERIFY(v->imSotaLog);
            QVERIFY(v->vorgewaehlt);
        }
        // OE5PEN/P im Log, OE5PEN in der Datei: dasselbe Rufzeichen.
        QCOMPARE(fuer(e, 1)->gegenGipfel, QStringLiteral("OE/OO-120"));
        // Morgens von zu Hause, 6 m und 20 m: vorgeschlagen, nicht gewaehlt.
        for (int i : {0, 3}) {
            const Vorschlag* v = fuer(e, i);
            QVERIFY(v);
            QVERIFY(!v->imSotaLog);
            QVERIFY(!v->vorgewaehlt);
            QVERIFY(v->hinweis.contains(QStringLiteral("not in your SOTA log")));
        }
    }

    void kopfzeileOhneRufzeichen()
    {
        const QByteArray csv =
            "Date,Summit,Summit Name,Points,QSOs\n"
            "04/07/2026,OE/OO-106,Gmundnerberg,2,19\n"
            "21/06/2026,OE/OO-106,Gmundnerberg,2,7\n";
        const Datei d = leseCsv(csv);
        QCOMPARE(d.format, QStringLiteral("header"));
        QCOMPARE(d.zeilen.size(), 2);
        QVERIFY(!d.mitQsos);
        const Ergebnis e = vorschlaege(martinsTage(), d);
        QCOMPARE(e.vorschlaege.size(), 6);
        // Ohne Rufzeichen in der Datei laesst sich nichts unterscheiden:
        // alles am Tag ist vorausgewaehlt, der Betreiber hakt ab.
        for (const Vorschlag& v : e.vorschlaege) { QVERIFY(v.vorgewaehlt); }
    }

    void kopfzeileMitGegenstation()
    {
        const QByteArray csv =
            "Date;Time;My Callsign;Summit;Band;Mode;Other Station;Other Summit\n"
            "04/07/2026;19:08;OE5SOS/P;OE/OO-106;2m;SSB;OM2Y;\n"
            "21/06/2026;09:39;OE5SOS/P;OE/OO-106;2m;FM;OE5PEN/P;OE/OO-120\n";
        const Datei d = leseCsv(csv);
        QCOMPARE(d.format, QStringLiteral("header"));
        QVERIFY(d.mitQsos);
        QCOMPARE(d.zeilen.at(0).gegenCall, QStringLiteral("OM2Y"));
        QCOMPARE(d.zeilen.at(0).zeit, QTime(19, 8));
        QCOMPARE(d.zeilen.at(1).gegenGipfel, QStringLiteral("OE/OO-120"));
    }

    void loseZeilenMonatZuerst()
    {
        // Amerikanisch: 06/21 verraet es, also ist 07/04 der 4. Juli.
        const QByteArray csv = "07/04/2026;OE/OO-106;x\n06/21/2026;oe/oo-106;y\nkaputt\n";
        const Datei d = leseCsv(csv);
        QCOMPARE(d.format, QStringLiteral("loose"));
        QCOMPARE(d.zeilen.size(), 2);
        QCOMPARE(d.uebersprungen, 1);
        QCOMPARE(d.zeilen.at(0).datum, QDate(2026, 7, 4));
        QCOMPARE(d.zeilen.at(1).datum, QDate(2026, 6, 21));
        QCOMPARE(d.zeilen.at(1).gipfel, QStringLiteral("OE/OO-106"));

        const Datei iso = leseCsv("2026-07-04,OE/OO-106\n20260621,OE/OO-106\n");
        QCOMPARE(iso.zeilen.size(), 2);
        QCOMPARE(iso.zeilen.at(1).datum, QDate(2026, 6, 21));

        QString fehler;
        QVERIFY(leseCsv("nichts,hier\n", &fehler).zeilen.isEmpty());
        QVERIFY(fehler.contains(QStringLiteral("summit reference")));
    }

    void zweiGipfelAnEinemTag()
    {
        const QByteArray csv =
            "V2,OE5SOS/P,OE/OO-106,04/07/2026,1000,14MHz,SSB,IZ7HNO,,\n"
            "V2,OE5SOS/P,OE/OO-107,04/07/2026,1908,144MHz,SSB,OM2Y,,\n";
        const Ergebnis e = vorschlaege(martinsTage(), leseCsv(csv));
        QCOMPARE(fuer(e, 3)->gipfel, QStringLiteral("OE/OO-106"));
        QCOMPARE(fuer(e, 4)->gipfel, QStringLiteral("OE/OO-107"));
        QVERIFY(!fuer(e, 5));                // SN7L: welcher Gipfel? keiner
        QCOMPARE(e.mehrdeutig.size(), 1);
        QVERIFY(e.mehrdeutig.first().startsWith(QStringLiteral("2026-07-04")));

        // Ohne Rufzeichen: nur der Hinweis.
        const Ergebnis lose = vorschlaege(martinsTage(),
            leseCsv("04/07/2026,OE/OO-106\n04/07/2026,OE/OO-107\n"));
        QVERIFY(!fuer(lose, 4) && !fuer(lose, 5) && !fuer(lose, 3));
        QCOMPARE(lose.mehrdeutig.size(), 1);
    }

    void anwendenSetztNurLeeres()
    {
        QVector<LogEntry> log = martinsTage();
        log[2].sotaRef = QStringLiteral("OE/OO-999");
        const Ergebnis e = vorschlaege(log, leseCsv(kV2));
        QVector<Vorschlag> gewaehlt;
        for (const Vorschlag& v : e.vorschlaege) {
            if (v.vorgewaehlt) { gewaehlt << v; }
        }
        // Ein Vorschlag fuer ein schon gesetztes QSO aendert nichts.
        Vorschlag fremd;
        fremd.index = 7;
        fremd.gipfel = QStringLiteral("OE/OO-106");
        gewaehlt << fremd;

        QCOMPARE(anwenden(log, gewaehlt), 4);
        QCOMPARE(log.at(5).mySotaRef, QStringLiteral("OE/OO-106"));
        QCOMPARE(log.at(1).sotaRef, QStringLiteral("OE/OO-120"));
        QCOMPARE(log.at(2).sotaRef, QStringLiteral("OE/OO-999"));   // nicht ueberschrieben
        QCOMPARE(log.at(7).mySotaRef, QStringLiteral("OE/OO-107"));
        QVERIFY(log.at(3).mySotaRef.isEmpty());                     // nicht gewaehlt
        // Das Feld landet im ADIF.
        QVERIFY(log.at(5).toAdifRecord().contains(QStringLiteral("<MY_SOTA_REF:9>OE/OO-106")));
    }

    void basisRufzeichen()
    {
        QCOMPARE(SotaNachtrag::basisRufzeichen(QStringLiteral("OE5PEN/P")), QStringLiteral("OE5PEN"));
        QCOMPARE(SotaNachtrag::basisRufzeichen(QStringLiteral("DL/OE5SOS/P")), QStringLiteral("OE5SOS"));
        QCOMPARE(SotaNachtrag::basisRufzeichen(QStringLiteral("ol73")), QStringLiteral("OL73"));
    }
};

QTEST_GUILESS_MAIN(TstSotaNachtrag)
#include "tst_sota_nachtrag.moc"
