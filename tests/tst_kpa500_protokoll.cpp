// no-port-check: Longpath-eigener Pruefstand, kein Port.
// =================================================================
// tests/tst_kpa500_protokoll.cpp  (Longpath)
// =================================================================
//
// Elecraft KPA500: die Protokollschicht.
//
// Was hier geprueft wird, laesst sich AM DOKUMENT nachlesen -- und das
// ist der Unterschied zum SPE. Elecrafts "KPA500 PROGRAMMERʼS
// REFERENCE" Rev. A2 ist frei herunterladbar, am 2026-10-09 beschafft
// und gelesen; jeder Fall unten nennt seine Fundstelle. Beim SPE stammt
// alles aus AetherSDRs Lesung einer Anleitung, die oeffentlich nicht
// auffindbar ist (siehe den Kopf von core/SpeProtocol.h).
//
// Kein Geraet noetig: die Schicht ist eine reine Funktion ueber Text.
//
// Drei Faelle sind gegen Fallen gebaut, die beim Schreiben wirklich
// drohten:
//
//   1. `dasGedachteKommaSitztRichtig` -- Spannung, Strom und Stehwelle
//      kommen OHNE Komma, mit einer gedachten Stelle hinter der
//      zweiten Ziffer. Wer das uebersieht, zeigt 475 Volt an.
//   2. `derZwischenraumDarfSoOderSoSein` -- das Dokument schreibt
//      "^VIvvv iii;" mit einem Zwischenraum. Ob der auf dem Draht
//      wirklich dasteht, laesst sich am PDF nicht entscheiden und hier
//      haengt kein Geraet. Der Leser muss beides koennen.
//   3. `einFehlerBleibtEineZahl` -- das Dokument gibt die Zuordnung
//      Nummer -> Fehlername NICHT her. Ein Name waere geraten.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 -- Neu. Martin Fischer, AI-assisted via Anthropic
//                 Claude (Claude Code).
// =================================================================

#include <QtTest>

#include <QByteArray>
#include <QList>

#include "core/Kpa500Protocol.h"

using namespace Longpath::Kpa500;

class TstKpa500Protokoll : public QObject
{
    Q_OBJECT

private slots:
    // ── Was hinausgeht ──────────────────────────────────────────────
    void dieAbfragenSindBuchstabenUndEinSemikolon();
    void dieEinstellungenStimmenMitDemDokument();
    void einBandAusserhalbDesBereichsGehtNichtHinaus();
    void derBootLaderHatEinzelneBuchstaben();

    // ── Was hereinkommt ─────────────────────────────────────────────
    void eineAntwortWirdZerlegt();
    void dreiBuchstabenVerbenAuch();
    void mehrereAntwortenAmStueck();
    void einByteNachDemAnderen();
    void rauschenVorDemPraefixWirdVerworfen();
    void derPufferWaechstNichtOhneEnde();
    void dasNackteSemikolonIstEineAussage();
    void dieBootAntwortHatKeinPraefix();

    // ── Zahlen ──────────────────────────────────────────────────────
    void dasGedachteKommaSitztRichtig();
    void derZwischenraumDarfSoOderSoSein();
    void imEmpfangIstDieStehwelleNullNichtEins();
    void unbrauchbareFelderWerdenAbgelehnt();
    void dieTemperaturBleibtImBereich();
    void dieBandtabelleIstDieDesDokuments();
    void derBetriebszustandUndDieVersorgung();
    void einFehlerBleibtEineZahl();
};

// ─────────────────────────────────────────────────────────────────────

void TstKpa500Protokoll::dieAbfragenSindBuchstabenUndEinSemikolon()
{
    // From Elecraft Rev A2, §Command Reference: „the GET format is just
    // the 2 or 3 letters of the command followed by a semicolon."
    QCOMPARE(buildGet(QStringLiteral("WS")), QByteArrayLiteral("^WS;"));
    QCOMPARE(buildGet(QStringLiteral("VI")), QByteArrayLiteral("^VI;"));
    QCOMPARE(buildGet(QStringLiteral("RVM")), QByteArrayLiteral("^RVM;"));
    // Das Geraet nimmt Klein- wie Grossschreibung, antwortet aber in
    // Gross -- wir schicken gleich Gross, damit Hin- und Rueckweg
    // dieselbe Schreibweise haben.
    QCOMPARE(buildGet(QStringLiteral("ws")), QByteArrayLiteral("^WS;"));
}

void TstKpa500Protokoll::dieEinstellungenStimmenMitDemDokument()
{
    // §Command Format druckt „^OS1;" woertlich ab.
    QCOMPARE(buildOperateSet(true),  QByteArrayLiteral("^OS1;"));
    QCOMPARE(buildOperateSet(false), QByteArrayLiteral("^OS0;"));
    // §Command Format druckt „^BN05;" woertlich ab -- ZWEI Stellen.
    QCOMPARE(buildBandSet(5),  QByteArrayLiteral("^BN05;"));
    QCOMPARE(buildBandSet(0),  QByteArrayLiteral("^BN00;"));
    QCOMPARE(buildBandSet(10), QByteArrayLiteral("^BN10;"));
    // §^FL: „SET format: ^FLC;" -- ein Buchstabe, keine Zahl.
    QCOMPARE(buildFaultClear(), QByteArrayLiteral("^FLC;"));
    // §^ON: „^ON0; turns the KPA500 off."
    QCOMPARE(buildPowerOff(), QByteArrayLiteral("^ON0;"));
    // §Command Format: das nackte Semikolon als Probe.
    QCOMPARE(buildPing(), QByteArrayLiteral(";"));
}

void TstKpa500Protokoll::einBandAusserhalbDesBereichsGehtNichtHinaus()
{
    // §^BN: „All other values are ignored." Was das Geraet
    // stillschweigend verwirft, soll gar nicht erst hinausgehen --
    // sonst wartet der Aufrufer auf eine Antwort, die nie kommt.
    QVERIFY(buildBandSet(11).isEmpty());   // 4 m gibt es beim KPA500 nicht
    QVERIFY(buildBandSet(-1).isEmpty());
    QVERIFY(buildBandSet(99).isEmpty());
}

void TstKpa500Protokoll::derBootLaderHatEinzelneBuchstaben()
{
    // §BootLoader Command Reference: einzelne Grossbuchstaben, KEIN
    // Semikolon.
    QCOMPARE(buildBootIdentify(), QByteArrayLiteral("I"));
    QCOMPARE(buildBootPowerOn(),  QByteArrayLiteral("P"));
}

void TstKpa500Protokoll::eineAntwortWirdZerlegt()
{
    QList<Reply> gelesen;
    ReplyParser p;
    p.setReplyCallback([&](const Reply& r) { gelesen.append(r); });

    p.feed(QByteArrayLiteral("^WS250 015;"));
    QCOMPARE(gelesen.size(), 1);
    QCOMPARE(gelesen.at(0).verb, QStringLiteral("WS"));
    QCOMPARE(gelesen.at(0).data, QStringLiteral("250 015"));
    QVERIFY(!gelesen.at(0).isBootIdentify);
    QVERIFY(!gelesen.at(0).isBareSemicolon);
}

void TstKpa500Protokoll::dreiBuchstabenVerbenAuch()
{
    // §^RVM: „^RVMnn.nn;" -- drei Buchstaben, und in den Daten steht ein
    // Punkt. Wer die Verblaenge auf zwei festnagelt, liest hier "RV" und
    // "Mnn.nn".
    QList<Reply> gelesen;
    ReplyParser p;
    p.setReplyCallback([&](const Reply& r) { gelesen.append(r); });

    p.feed(QByteArrayLiteral("^RVM01.04;"));
    QCOMPARE(gelesen.size(), 1);
    QCOMPARE(gelesen.at(0).verb, QStringLiteral("RVM"));
    QCOMPARE(gelesen.at(0).data, QStringLiteral("01.04"));
}

void TstKpa500Protokoll::mehrereAntwortenAmStueck()
{
    // So kommt es wirklich: ein Abfragetakt schickt mehrere Abfragen
    // und bekommt die Antworten in einem Stueck zurueck.
    QList<Reply> gelesen;
    ReplyParser p;
    p.setReplyCallback([&](const Reply& r) { gelesen.append(r); });

    p.feed(QByteArrayLiteral("^WS250 015;^VI475 320;^TM042;^OS1;^BN05;^FL00;"));
    QCOMPARE(gelesen.size(), 6);
    QCOMPARE(gelesen.at(0).verb, QStringLiteral("WS"));
    QCOMPARE(gelesen.at(1).verb, QStringLiteral("VI"));
    QCOMPARE(gelesen.at(2).verb, QStringLiteral("TM"));
    QCOMPARE(gelesen.at(3).verb, QStringLiteral("OS"));
    QCOMPARE(gelesen.at(4).verb, QStringLiteral("BN"));
    QCOMPARE(gelesen.at(5).verb, QStringLiteral("FL"));
}

void TstKpa500Protokoll::einByteNachDemAnderen()
{
    // Die Gegenprobe zum Stueck. Ueber einen seriellen Anschluss kommt
    // jede Teilung vor.
    QList<Reply> gelesen;
    ReplyParser p;
    p.setReplyCallback([&](const Reply& r) { gelesen.append(r); });

    const QByteArray alles = QByteArrayLiteral("^WS250 015;^VI475 320;");
    for (char c : alles) {
        p.feed(QByteArray(1, c));
    }
    QCOMPARE(gelesen.size(), 2);
    QCOMPARE(gelesen.at(0).data, QStringLiteral("250 015"));
    QCOMPARE(gelesen.at(1).data, QStringLiteral("475 320"));
}

void TstKpa500Protokoll::rauschenVorDemPraefixWirdVerworfen()
{
    QList<Reply> gelesen;
    ReplyParser p;
    p.setReplyCallback([&](const Reply& r) { gelesen.append(r); });

    p.feed(QByteArrayLiteral("\x00\xff Mull ^WS250 015;"));
    QCOMPARE(gelesen.size(), 1);
    QCOMPARE(gelesen.at(0).verb, QStringLiteral("WS"));
    QCOMPARE(gelesen.at(0).data, QStringLiteral("250 015"));
}

void TstKpa500Protokoll::derPufferWaechstNichtOhneEnde()
{
    // Ein Geraet, das nur Rauschen schickt und nie ein Semikolon, darf
    // den Speicher nicht volllaufen lassen. Nach dem Rauschen muss eine
    // echte Antwort weiterhin ankommen.
    QList<Reply> gelesen;
    ReplyParser p;
    p.setReplyCallback([&](const Reply& r) { gelesen.append(r); });

    for (int i = 0; i < 200; ++i) {
        p.feed(QByteArray(100, 'x'));
    }
    // Die eigentliche Aussage ist die GROESSE, nicht dass die Antwort
    // danach ankommt: die kommt auch mit vollgelaufenem Puffer richtig
    // an (indexOf findet das Semikolon ja trotzdem). Der erste Anlauf
    // dieses Falls hat nur das geprueft und lief darum auch ohne den
    // Waechter gruen durch -- gefunden bei der Gegenprobe.
    QVERIFY2(p.bufferedForTesting() < 1000,
             qPrintable(QStringLiteral("Der Puffer haelt %1 Bytes Rauschen "
                                       "fest -- 20000 wurden gefuettert")
                            .arg(p.bufferedForTesting())));

    p.feed(QByteArrayLiteral("^TM042;"));
    QCOMPARE(gelesen.size(), 1);
    QCOMPARE(gelesen.at(0).verb, QStringLiteral("TM"));
}

void TstKpa500Protokoll::dasNackteSemikolonIstEineAussage()
{
    // §Command Format: „The KPA500 will respond to a null command,
    // containing only a ';' by echoing the ';' character. This may be
    // useful to make sure the PC is communicating with the KPA500."
    QList<Reply> gelesen;
    ReplyParser p;
    p.setReplyCallback([&](const Reply& r) { gelesen.append(r); });

    p.feed(QByteArrayLiteral(";"));
    QCOMPARE(gelesen.size(), 1);
    QVERIFY2(gelesen.at(0).isBareSemicolon,
             "Das zurueckgeworfene Semikolon ist die Antwort auf die Probe "
             "und darf nicht als Rauschen verschwinden");
    QVERIFY(gelesen.at(0).verb.isEmpty());
}

void TstKpa500Protokoll::dieBootAntwortHatKeinPraefix()
{
    // §BootLoader, 'I' Identify: „the amplifier will respond with
    // „KPA500"." Kein '^', kein Semikolon -- ein Leser, der auf das
    // Semikolon wartet, sieht sie nie.
    QList<Reply> gelesen;
    ReplyParser p;
    p.setReplyCallback([&](const Reply& r) { gelesen.append(r); });

    p.feed(QByteArrayLiteral("KPA500"));
    QCOMPARE(gelesen.size(), 1);
    QVERIFY2(gelesen.at(0).isBootIdentify,
             "Die Antwort des Boot-Laders wurde nicht erkannt");

    // Und sie blockiert nichts: danach laeuft der normale Betrieb.
    p.feed(QByteArrayLiteral("^TM042;"));
    QCOMPARE(gelesen.size(), 2);
    QCOMPARE(gelesen.at(1).verb, QStringLiteral("TM"));

    // Geteilt ankommend muss sie auch gehen -- und genau dieser Fall ist
    // der Grund, warum der Rauschen-Waechter fuenf Bytes stehen laesst
    // statt alles wegzuwerfen, was kein '^' enthaelt.
    ReplyParser q;
    QList<Reply> geteilt;
    q.setReplyCallback([&](const Reply& r) { geteilt.append(r); });
    q.feed(QByteArrayLiteral("KPA"));
    QVERIFY(geteilt.isEmpty());
    q.feed(QByteArrayLiteral("500"));
    QCOMPARE(geteilt.size(), 1);
    QVERIFY2(geteilt.at(0).isBootIdentify,
             "Die geteilte Boot-Antwort ging verloren -- der "
             "Rauschen-Waechter haelt zu wenig fest");
}

void TstKpa500Protokoll::dasGedachteKommaSitztRichtig()
{
    // §^VI: „Note the implied decimal point after the second digit for
    // both values." -- 475 heisst 47,5 V, nicht 475 V.
    const auto v = parseVoltsAmps(QStringLiteral("475 320"));
    QVERIFY(v.has_value());
    QCOMPARE(v->volts, 47.5f);
    QCOMPARE(v->amps,  32.0f);

    // §^WS: „There is an implied decimal point after the second s."
    const auto w = parsePowerSwr(QStringLiteral("250 015"));
    QVERIFY(w.has_value());
    QCOMPARE(w->watts, 250);     // Watt sind ganze Watt
    QCOMPARE(w->swr,   1.5f);    // 015 -> 1,5
}

void TstKpa500Protokoll::derZwischenraumDarfSoOderSoSein()
{
    // Das Dokument schreibt „^VIvvv iii;". Ob auf dem Draht wirklich ein
    // Leerzeichen steht oder das nur der Satz des PDF ist, laesst sich
    // am Dokument nicht entscheiden, und hier haengt kein Geraet. Also
    // muss beides gehen.
    const auto mitRaum = parseVoltsAmps(QStringLiteral("475 320"));
    const auto ohneRaum = parseVoltsAmps(QStringLiteral("475320"));
    QVERIFY(mitRaum.has_value());
    QVERIFY2(ohneRaum.has_value(),
             "Ohne Zwischenraum muss es auch gelesen werden");
    QCOMPARE(ohneRaum->volts, 47.5f);
    QCOMPARE(ohneRaum->amps,  32.0f);

    const auto wsOhne = parsePowerSwr(QStringLiteral("250015"));
    QVERIFY(wsOhne.has_value());
    QCOMPARE(wsOhne->watts, 250);
    QCOMPARE(wsOhne->swr,   1.5f);
}

void TstKpa500Protokoll::imEmpfangIstDieStehwelleNullNichtEins()
{
    // §^WS: „sss will return as 000 when not transmitting." Es waere
    // bequem, daraus 1,0 zu machen -- aber das ist eine Messung, die es
    // nicht gab. Hier kommt 0 heraus, und wer anzeigt, entscheidet.
    const auto w = parsePowerSwr(QStringLiteral("000 000"));
    QVERIFY(w.has_value());
    QCOMPARE(w->watts, 0);
    QCOMPARE(w->swr, 0.0f);
}

void TstKpa500Protokoll::unbrauchbareFelderWerdenAbgelehnt()
{
    // Zu wenige Gruppen, falsche Laengen, leer -- alles abgelehnt statt
    // halb gelesen. Ein halb gelesenes Feld ist schlimmer als keines:
    // es sieht aus wie ein Messwert.
    QVERIFY(!parseVoltsAmps(QStringLiteral("475")).has_value());
    QVERIFY(!parseVoltsAmps(QStringLiteral("47 320")).has_value());
    QVERIFY(!parseVoltsAmps(QStringLiteral("")).has_value());
    QVERIFY(!parsePowerSwr(QStringLiteral("250")).has_value());
    QVERIFY(!parsePowerSwr(QStringLiteral("2500 015")).has_value());
    QVERIFY(!parseTemperature(QStringLiteral("42")).has_value());
    QVERIFY(!parseBand(QStringLiteral("5")).has_value());
    QVERIFY(!parseOperate(QStringLiteral("2")).has_value());
    QVERIFY(!parseFault(QStringLiteral("0")).has_value());
}

void TstKpa500Protokoll::dieTemperaturBleibtImBereich()
{
    // §^TM: „range of 0 - 150 degrees C."
    const auto t = parseTemperature(QStringLiteral("042"));
    QVERIFY(t.has_value());
    QCOMPARE(*t, 42);
    QVERIFY(parseTemperature(QStringLiteral("150")).has_value());
    QVERIFY(parseTemperature(QStringLiteral("000")).has_value());
    QVERIFY2(!parseTemperature(QStringLiteral("151")).has_value(),
             "Jenseits des Bereichs ist es kein Messwert");
}

void TstKpa500Protokoll::dieBandtabelleIstDieDesDokuments()
{
    // §^BN, woertlich: 00 = 160m … 10 = 6m. Der KPA500 hat KEIN 4 m --
    // der SPE hat dort eine 11, und die beiden Tabellen zu verwechseln
    // waere ein Band daneben.
    QCOMPARE(bandName(0),  QStringLiteral("160m"));
    QCOMPARE(bandName(1),  QStringLiteral("80m"));
    QCOMPARE(bandName(2),  QStringLiteral("60m"));
    QCOMPARE(bandName(5),  QStringLiteral("20m"));
    QCOMPARE(bandName(9),  QStringLiteral("10m"));
    QCOMPARE(bandName(10), QStringLiteral("6m"));
    QCOMPARE(bandName(11), QStringLiteral("?m"));
    QCOMPARE(bandName(-1), QStringLiteral("?m"));

    const auto b = parseBand(QStringLiteral("05"));
    QVERIFY(b.has_value());
    QCOMPARE(*b, 5);
    QVERIFY(!parseBand(QStringLiteral("11")).has_value());
}

void TstKpa500Protokoll::derBetriebszustandUndDieVersorgung()
{
    // §^OS: 0 = Standby, 1 = Operate.
    QCOMPARE(*parseOperate(QStringLiteral("1")), true);
    QCOMPARE(*parseOperate(QStringLiteral("0")), false);

    // §^ON: „where n = 1. No response if off." Eine 0 kommt nur als
    // Nachhall eines gerade gesendeten ^ON0; -- auch gueltig.
    QCOMPARE(*parsePowerState(QStringLiteral("1")), true);
    QCOMPARE(*parsePowerState(QStringLiteral("0")), false);
    QVERIFY(!parsePowerState(QStringLiteral("x")).has_value());
}

void TstKpa500Protokoll::einFehlerBleibtEineZahl()
{
    // §^FL: „nn = current fault identifier. nn = 00 indicates no faults
    // are active." Welche Nummer welchen Namen hat, sagt das Dokument
    // NICHT -- also wird kein Name erfunden.
    const auto keiner = parseFault(QStringLiteral("00"));
    QVERIFY(keiner.has_value());
    QVERIFY2(!keiner->isFault(), "00 heisst: kein Fehler");

    const auto drei = parseFault(QStringLiteral("03"));
    QVERIFY(drei.has_value());
    QVERIFY(drei->isFault());
    QCOMPARE(drei->code, 3);

    // Es gibt hier mit Absicht KEINE Funktion, die aus 3 einen Namen
    // macht. Faellt dieser Fall, weil jemand eine eingebaut hat, gehoert
    // zuerst geklaert, WOHER die Zuordnung stammt.
}

QTEST_MAIN(TstKpa500Protokoll)
#include "tst_kpa500_protokoll.moc"
