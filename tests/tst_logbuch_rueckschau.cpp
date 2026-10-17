// Haelt fest, wie das Logbuch fuer die App gelesen wird — und vor allem,
// dass dabei kein Kontakt ERFUNDEN wird.
//
// Hintergrund (2026-10-04): vom Telefon laesst sich seit #184 loggen, aber
// nicht nachsehen. Die naechstliegende Loesung — die ganze Datei parsen und
// die letzten zehn nehmen — waere fuer Martins Logbuch (6,6 MB, 9271
// Datensaetze) ein Vielfaches der Arbeit fuer ein Zehntel des Ergebnisses.
// Gemessen am echten Logbuch:
//
//     voller Durchlauf              4,1 ms
//     Suche nach einem Rufzeichen   3,8 ms   (3 Treffer)
//     letzte 64 kB                  0,04 ms  (78 Datensaetze)
//
// Also wird vom Dateiende gelesen. Genau daraus entsteht die GEFAHR, gegen
// die dieser Stand vor allem steht: ein Schnitt an einer willkuerlichen
// Byte-Stelle beginnt mitten in einem Datensatz, und der ADIF-Parser macht
// aus der hinteren Haelfte eines Kontakts einen eigenen. Das ist schlimmer
// als ein fehlender Kontakt — es ist ein Kontakt, den niemand hatte, in
// einem Logbuch, das Martin fuer Diplome einreicht.
//
// `rohesStueckErfindetEinenDatensatz` fuehrt genau das vor: derselbe
// Schnitt, einmal ohne und einmal mit `abDatensatzGrenze`. Ohne entsteht
// ein Datensatz mit abgeschnittenem Rufzeichen, mit nicht. Ein Pruefstand,
// der nur die Behebung gruen zeigt, belegt nichts.
//
// Die zweite Gefahr ist leiser: eine Dupe-Warnung, die auf Verdacht wahr
// wird. Wer "schon gearbeitet" meldet, obwohl Band oder Betriebsart
// unbekannt sind, verhindert einen Kontakt, den es noch nicht gibt —
// unbemerkt, weil nichts passiert.

#include "core/AdifLog.h"
#include "core/ZeitUtc.h"
#include "core/LogbuchRueckschau.h"
#include "models/LogEntry.h"

#include <QtTest>
#include <QTemporaryDir>

using namespace Longpath;
using namespace Longpath::LogbuchRueckschau;

namespace {

QString datensatz(const QString& ruf, const QDateTime& zeit,
                  const QString& band, const QString& mode,
                  const QString& bemerkung = QString())
{
    LogEntry e;
    e.call    = ruf;
    e.timeOn  = zeit;
    e.band    = band;
    e.mode    = mode;
    e.rstSent = QStringLiteral("59");
    e.rstRcvd = QStringLiteral("59");
    e.comment = bemerkung;
    return e.toAdifRecord();
}

// Eine Datei wie LogbookDatei::anhaengen sie anlegt: Kopf, dann Kontakte,
// aeltester zuerst.
QString schreibeLogbuch(const QString& pfad, const QStringList& datensaetze)
{
    QFile f(pfad);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { return {}; }
    QTextStream aus(&f);
    aus << "Longpath logbook\n"
        << "<ADIF_VER:5>3.1.4 <PROGRAMID:8>Longpath <EOH>\n";
    for (const QString& d : datensaetze) { aus << d << "\n"; }
    aus.flush();
    return pfad;
}

const QDateTime kStart = Longpath::Zeit::utc(QDate(2026, 1, 1), QTime(0, 0));

}  // namespace

class TstLogbuchRueckschau : public QObject
{
    Q_OBJECT

private slots:
    void rohesStueckErfindetEinenDatensatz();
    void grenzeOhneJedeMarkeGibtNichtsZurueck();
    void letzteKommenJuengsterZuerst();
    void fensterWaechstBisGenugDaIst();
    void fehlendeDateiIstEinLeeresLogbuch();
    void vorsiebNimmtNurDieDatensaetzeMitDemRufzeichen();
    void zweiTrefferImSelbenDatensatzKommenEinmal();
    void nurDasGeparsteRufzeichenZaehlt();
    void leeresBandMeldetKeinenDupe();
    void gleicherModeGiltNurAufGleichemBand();
    void rueckschauFindetDenKontaktInDerDatei();
};

// ── Die Gegenprobe: ohne Grenzschnitt entsteht ein Kontakt aus nichts ──────
void TstLogbuchRueckschau::rohesStueckErfindetEinenDatensatz()
{
    // WICHTIG fuer das Verstaendnis: in einer von Longpath geschriebenen
    // Datei steht CALL als ERSTES Feld. Ein Schnitt mitten im Datensatz
    // zerstoert dort immer das Rufzeichen, und `LogEntry::isValid()` wirft
    // den Rest weg. Dieser Fall ist also zufaellig harmlos — zufaellig,
    // nicht beabsichtigt.
    //
    // ADIF schreibt die Feldreihenfolge aber NICHT vor, und fremde Logger
    // halten sich nicht an Longpaths. Martins Datei enthaelt zusammengefuehrte
    // Importe. Steht das Datum vor dem Rufzeichen, behaelt ein Schnitt
    // hinter dem Datum das Rufzeichen und verliert die Zeit: ein Kontakt
    // mit richtigem Rufzeichen, ohne Zeit, den es nicht gibt. Er erscheint
    // in "die letzten zehn" und zaehlt in der Dupe-Antwort mit.
    const QByteArray fremderDatensatz =
        QByteArrayLiteral("<QSO_DATE:8>20260101 <TIME_ON:6>120000 "
                          "<CALL:6>OE2BBB <BAND:3>20m <MODE:2>CW "
                          "<RST_SENT:2>59 <RST_RCVD:2>59 <EOR>\n");

    // Der Schnitt: hinter "<QSO_DATE:8>2026" — genau so trifft ein Fenster
    // vom Dateiende her irgendeinen Datensatz.
    const qsizetype schnitt = fremderDatensatz.indexOf("0101");
    QVERIFY(schnitt > 0);
    const QByteArray roh = fremderDatensatz.mid(schnitt);

    // ALTE Fassung (kein Grenzschnitt): ein vollwertig aussehender Kontakt.
    const QVector<LogEntry> ohne = AdifLog::parse(roh);
    QCOMPARE(ohne.size(), 1);
    QCOMPARE(ohne.at(0).call, QStringLiteral("OE2BBB"));
    QCOMPARE(ohne.at(0).band, QStringLiteral("20m"));
    QVERIFY2(!ohne.at(0).timeOn.isValid(), "Zeit muesste beim Schnitt weg sein");

    // NEUE Fassung: derselbe Schnitt, erst zurechtgeschnitten. Nichts bleibt
    // uebrig — lieber ein Kontakt zu wenig als einer erfunden.
    QCOMPARE(AdifLog::parse(abDatensatzGrenze(roh)).size(), 0);

    // Und zur Vollstaendigkeit die Longpath-eigene Reihenfolge: dort faengt
    // `isValid()` den halben Datensatz schon ab. Der Grenzschnitt ist die
    // zweite Sicherung, nicht die einzige — festgehalten, damit niemand ihn
    // spaeter als ueberfluessig wegnimmt.
    const QByteArray eigen =
        datensatz(QStringLiteral("OE2BBB"), kStart, QStringLiteral("20m"),
                  QStringLiteral("CW")).toUtf8();
    const qsizetype s2 = eigen.indexOf("OE2BBB");
    QVERIFY(s2 > 0);
    QCOMPARE(AdifLog::parse(eigen.mid(s2 + 3)).size(), 0);
    QCOMPARE(AdifLog::parse(abDatensatzGrenze(eigen.mid(s2 + 3))).size(), 0);
}

void TstLogbuchRueckschau::grenzeOhneJedeMarkeGibtNichtsZurueck()
{
    // Ein Stueck ohne <EOH> und ohne <EOR> ist die Mitte eines einzigen
    // Datensatzes. Nichts darin ist verlaesslich.
    QVERIFY(abDatensatzGrenze(QByteArrayLiteral("<BAND:3>20m <MODE:3>CW ")).isEmpty());
    QVERIFY(abDatensatzGrenze(QByteArray()).isEmpty());

    // Mit Kopf: alles davor ist Kopf, nicht Kontakt.
    const QByteArray k = QByteArrayLiteral("Longpath logbook\n<ADIF_VER:5>3.1.4 <EOH>\nREST");
    QCOMPARE(abDatensatzGrenze(k).trimmed(), QByteArrayLiteral("REST"));

    // Kleinschreibung gilt genauso — ADIF ist dort gleichgueltig, und
    // fremde Logger schreiben <eor>.
    QCOMPARE(abDatensatzGrenze(QByteArrayLiteral("halb<eor>\nganz")).trimmed(),
             QByteArrayLiteral("ganz"));
}

// ── Die letzten n ──────────────────────────────────────────────────────────
void TstLogbuchRueckschau::letzteKommenJuengsterZuerst()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString p = dir.filePath(QStringLiteral("logbook.adi"));

    QStringList d;
    for (int i = 0; i < 40; ++i) {
        d << datensatz(QStringLiteral("OE%1TST").arg(i, 3, 10, QLatin1Char('0')),
                       kStart.addSecs(i * 600), QStringLiteral("40m"),
                       QStringLiteral("SSB"));
    }
    QVERIFY(!schreibeLogbuch(p, d).isEmpty());

    const QVector<LogEntry> l = letzte(5, p);
    QCOMPARE(l.size(), 5);
    QCOMPARE(l.at(0).call, QStringLiteral("OE039TST"));
    QCOMPARE(l.at(4).call, QStringLiteral("OE035TST"));
    // Absteigend, ohne Ausnahme.
    for (int i = 1; i < l.size(); ++i) {
        QVERIFY(l.at(i - 1).timeOn >= l.at(i).timeOn);
    }

    // Mehr verlangt als vorhanden: alles, nicht leer.
    QCOMPARE(letzte(500, p).size(), 40);
    // Nichts verlangt: nichts bekommen, kein Dateizugriff.
    QCOMPARE(letzte(0, p).size(), 0);
}

void TstLogbuchRueckschau::fensterWaechstBisGenugDaIst()
{
    // Das erste Fenster ist 64 kB. Mit genug Bemerkungstext passen darin
    // weniger Datensaetze als verlangt — dann MUSS das Fenster wachsen,
    // sonst kaeme eine kurze Liste zurueck und niemand merkte es.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString p = dir.filePath(QStringLiteral("logbook.adi"));

    const QString fuellung(1500, QLatin1Char('x'));   // ~1,5 kB je Datensatz
    QStringList d;
    for (int i = 0; i < 120; ++i) {
        d << datensatz(QStringLiteral("OE%1FUL").arg(i, 3, 10, QLatin1Char('0')),
                       kStart.addSecs(i * 600), QStringLiteral("20m"),
                       QStringLiteral("CW"), fuellung);
    }
    QVERIFY(!schreibeLogbuch(p, d).isEmpty());
    QVERIFY(QFileInfo(p).size() > 64 * 1024);

    const QVector<LogEntry> l = letzte(100, p);
    QCOMPARE(l.size(), 100);
    QCOMPARE(l.at(0).call, QStringLiteral("OE119FUL"));
}

void TstLogbuchRueckschau::fehlendeDateiIstEinLeeresLogbuch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fehlt = dir.filePath(QStringLiteral("gibtesnicht.adi"));
    QCOMPARE(letzte(10, fehlt).size(), 0);
    QVERIFY(!rueckschau(QStringLiteral("OE5SOS"), QStringLiteral("40m"),
                        QStringLiteral("SSB"), fehlt).kennt());
}

// ── Das Vorsieb ───────────────────────────────────────────────────────────
void TstLogbuchRueckschau::vorsiebNimmtNurDieDatensaetzeMitDemRufzeichen()
{
    const QByteArray inhalt =
        QByteArrayLiteral("<EOH>\n")
        + datensatz(QStringLiteral("OE1AAA"), kStart, QStringLiteral("40m"),
                    QStringLiteral("SSB")).toUtf8() + "\n"
        + datensatz(QStringLiteral("OE2BBB"), kStart.addSecs(60),
                    QStringLiteral("20m"), QStringLiteral("CW")).toUtf8() + "\n"
        + datensatz(QStringLiteral("OE3CCC"), kStart.addSecs(120),
                    QStringLiteral("15m"), QStringLiteral("FT8")).toUtf8() + "\n";

    const QByteArray teil = datensaetzeMit(inhalt, QByteArrayLiteral("OE2BBB"));
    const QVector<LogEntry> e = AdifLog::parse(teil);
    QCOMPARE(e.size(), 1);
    QCOMPARE(e.at(0).call, QStringLiteral("OE2BBB"));

    // Kleinschreibung findet genauso.
    QCOMPARE(AdifLog::parse(datensaetzeMit(inhalt, QByteArrayLiteral("oe2bbb"))).size(), 1);
    // Nichts Gesuchtes, nichts zurueck.
    QVERIFY(datensaetzeMit(inhalt, QByteArrayLiteral("OE9ZZZ")).isEmpty());
    QVERIFY(datensaetzeMit(inhalt, QByteArray()).isEmpty());
}

void TstLogbuchRueckschau::zweiTrefferImSelbenDatensatzKommenEinmal()
{
    // Rufzeichen im CALL-Feld UND in der Bemerkung: ein Datensatz, nicht
    // zwei. Sonst zaehlte ein Kontakt doppelt und die Dupe-Meldung
    // behauptete mehr, als dasteht.
    const QByteArray inhalt =
        QByteArrayLiteral("<EOH>\n")
        + datensatz(QStringLiteral("OE2BBB"), kStart, QStringLiteral("20m"),
                    QStringLiteral("CW"),
                    QStringLiteral("Skedansage von OE2BBB")).toUtf8() + "\n";

    const QVector<LogEntry> e =
        AdifLog::parse(datensaetzeMit(inhalt, QByteArrayLiteral("OE2BBB")));
    QCOMPARE(e.size(), 1);
    QCOMPARE(fasseZusammen(e, QStringLiteral("OE2BBB"), QString(), QString()).anzahl, 1);
}

void TstLogbuchRueckschau::nurDasGeparsteRufzeichenZaehlt()
{
    // Das Vorsieb siebt absichtlich grosszuegig: ein Rufzeichen in einer
    // Bemerkung kommt mit. Zaehlen darf es nicht.
    QVector<LogEntry> treffer;
    LogEntry fremd;
    fremd.call    = QStringLiteral("OE1AAA");
    fremd.timeOn  = kStart;
    fremd.band    = QStringLiteral("40m");
    fremd.mode    = QStringLiteral("SSB");
    fremd.comment = QStringLiteral("gehoert von OE2BBB");
    treffer << fremd;

    const Befund b = fasseZusammen(treffer, QStringLiteral("OE2BBB"),
                                   QStringLiteral("40m"), QStringLiteral("SSB"));
    QCOMPARE(b.anzahl, 0);
    QVERIFY(!b.kennt());
    QVERIFY(!b.gleichesBand);
}

// ── Die Dupe-Frage ────────────────────────────────────────────────────────
void TstLogbuchRueckschau::leeresBandMeldetKeinenDupe()
{
    // Der eine Fehler, der einen Kontakt verhindert: "schon gearbeitet",
    // obwohl das Band unbekannt ist. Dann steht die Warnung da, der Bediener
    // ruft nicht, und niemand erfaehrt je davon.
    QVector<LogEntry> treffer;
    LogEntry e;
    e.call   = QStringLiteral("OE2BBB");
    e.timeOn = kStart;
    e.band   = QStringLiteral("20m");
    e.mode   = QStringLiteral("CW");
    treffer << e;

    const Befund ohneBand =
        fasseZusammen(treffer, QStringLiteral("OE2BBB"), QString(), QString());
    QCOMPARE(ohneBand.anzahl, 1);          // gekannt: ja
    QVERIFY(!ohneBand.gleichesBand);       // Dupe: nein
    QVERIFY(!ohneBand.gleicherMode);
    QCOMPARE(ohneBand.letztesBand, QStringLiteral("20m"));

    const Befund mitBand = fasseZusammen(treffer, QStringLiteral("OE2BBB"),
                                         QStringLiteral("20m"), QStringLiteral("CW"));
    QVERIFY(mitBand.gleichesBand);
    QVERIFY(mitBand.gleicherMode);
}

void TstLogbuchRueckschau::gleicherModeGiltNurAufGleichemBand()
{
    // Derselbe Mode auf einem ANDEREN Band ist kein Dupe. Wer das
    // zusammenwirft, meldet 40 m SSB als gearbeitet, weil es 20 m SSB gab.
    QVector<LogEntry> treffer;
    LogEntry e;
    e.call   = QStringLiteral("OE2BBB");
    e.timeOn = kStart;
    e.band   = QStringLiteral("20m");
    e.mode   = QStringLiteral("SSB");
    treffer << e;

    const Befund b = fasseZusammen(treffer, QStringLiteral("OE2BBB"),
                                   QStringLiteral("40m"), QStringLiteral("SSB"));
    QCOMPARE(b.anzahl, 1);
    QVERIFY(!b.gleichesBand);
    QVERIFY(!b.gleicherMode);
}

void TstLogbuchRueckschau::rueckschauFindetDenKontaktInDerDatei()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString p = dir.filePath(QStringLiteral("logbook.adi"));

    QStringList d;
    for (int i = 0; i < 30; ++i) {
        d << datensatz(QStringLiteral("OE%1TST").arg(i, 3, 10, QLatin1Char('0')),
                       kStart.addSecs(i * 600), QStringLiteral("40m"),
                       QStringLiteral("SSB"));
    }
    // Dreimal dasselbe Rufzeichen, zweimal auf 20 m CW.
    d << datensatz(QStringLiteral("OE2BBB"), kStart.addSecs(100000),
                   QStringLiteral("20m"), QStringLiteral("CW"))
      << datensatz(QStringLiteral("OE2BBB"), kStart.addSecs(200000),
                   QStringLiteral("20m"), QStringLiteral("CW"))
      << datensatz(QStringLiteral("OE2BBB"), kStart.addSecs(300000),
                   QStringLiteral("15m"), QStringLiteral("SSB"));
    QVERIFY(!schreibeLogbuch(p, d).isEmpty());

    const Befund auf20 = rueckschau(QStringLiteral("OE2BBB"),
                                    QStringLiteral("20m"),
                                    QStringLiteral("CW"), p);
    QCOMPARE(auf20.anzahl, 3);
    QVERIFY(auf20.gleichesBand);
    QVERIFY(auf20.gleicherMode);
    QCOMPARE(auf20.zuletzt, kStart.addSecs(300000));
    QCOMPARE(auf20.letztesBand, QStringLiteral("15m"));

    // Dasselbe Rufzeichen auf einem noch nicht gearbeiteten Band.
    const Befund auf10 = rueckschau(QStringLiteral("OE2BBB"),
                                    QStringLiteral("10m"),
                                    QStringLiteral("SSB"), p);
    QCOMPARE(auf10.anzahl, 3);
    QVERIFY(!auf10.gleichesBand);

    // Ein nie gearbeitetes Rufzeichen.
    QVERIFY(!rueckschau(QStringLiteral("OE9ZZZ"), QStringLiteral("20m"),
                        QStringLiteral("CW"), p).kennt());
    // Leeres Rufzeichen fragt die Datei gar nicht.
    QVERIFY(!rueckschau(QString(), QStringLiteral("20m"),
                        QStringLiteral("CW"), p).kennt());
}

QTEST_MAIN(TstLogbuchRueckschau)
#include "tst_logbuch_rueckschau.moc"
