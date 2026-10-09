// ACOM S-Serie: die Protokollschicht.
//
// Portiert aus AetherSDR tests/acom_protocol_test.cpp [@d58e2b8a] --
// Herkunft und Abweichungen stehen im Kopf von core/AcomProtocol.h. Die
// Form ist umgestellt (nacktes main() mit eigenem Zaehler -> QTest mit
// deutschen Fallnamen); die geprueften Faelle und erwarteten Werte sind
// dieselben, dazu vier eigene.
//
// WARUM DIESER PRUEFSTAND MEHR WERT IST ALS DER DES SPE: hier stehen
// WOERTLICH ABGEDRUCKTE Pruefsummen aus der Herstellerbeschreibung
// dagegen -- 55 92 04 15, 55 91 04 16, 55 90 04 17, 55 93 04 14,
// 55 01 04 A6. Fuenf Rahmen, fuenf unabhaengig nachrechenbare
// Pruefsummen. Beim SPE gab es nichts dergleichen, weil dort die
// Beschreibung selbst nicht aufzufinden war.
//
// Und: AetherSDR hat diesen Treiber gegen ein ECHTES 600S gefahren. Was
// daran hardwarebestaetigt ist, ist unten so gekennzeichnet (die
// Balkenschwellen des 600S, die Wortfolge des Betriebsstundenzaehlers);
// alles andere ist abgeleitet und auch so gekennzeichnet.
//
// Vier eigene Faelle, gegen Fallen, die hier wirklich drohten:
//
//   1. `dieGeraetetabelleWiderlegtDenModellnamen` -- der Modellname ist
//      NICHT die Nennleistung (1200S kann 1000 W, 2020S kann 1500 W).
//      Wer vom Namen auf die Leistung schliesst, skaliert zwei von sechs
//      Balken falsch.
//   2. `dieSelbstskalierungGehtNurNachOben` -- sie springt direkt auf
//      die passende Stufe und fällt nie zurück. Ein Leistungseinbruch
//      ist Betrieb, kein kleinerer Verstaerker.
//   3. `einRahmenJeByteGefuettert` -- ueber RS-232 kommt jede Teilung vor.
//   4. `nurEineUnbekannteKennungBleibtUnbekannt` -- die Beschreibung
//      dokumentiert genau EINEN Typcode. Ein geratener zweiter waere
//      schlimmer als keiner.
//
// Alle Daten sind erfunden oder aus der Beschreibung abgeschrieben --
// kein Geraet noetig.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Portiert fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt mit Claude Code.
// =================================================================

#include <QtTest>

#include <QByteArray>
#include <QList>

#include "core/AcomProtocol.h"

using namespace Longpath::Acom;

namespace {

void setLe16(QByteArray& d, int offset, quint16 value)
{
    d[offset]     = static_cast<char>(value & 0xFF);
    d[offset + 1] = static_cast<char>((value >> 8) & 0xFF);
}

// Baut eine Telemetrie-Nutzlast mit bekannten Werten an jeder Stelle,
// die der Dekodierer liest -- von Hand gesetzt, NICHT in einer Schleife
// und nicht mit der Offsetrechnung des Dekodierers. So pruefen sich die
// beiden gegenseitig.
QByteArray baueTelemetrieNutzlast()
{
    QByteArray p(68, '\0');
    p[0] = static_cast<char>(0x70);   // Betriebsart-Nibble = OperateTx
    setLe16(p, 5, 1234);              // dcPowerPam1_x10W
    setLe16(p, 9, 1);                 // Betriebsstunden, hohes Wort
    setLe16(p, 11, 5000);             // Betriebsstunden, niedriges Wort -> 70536
    setLe16(p, 13, 305);              // paTempRaw
    setLe16(p, 17, 50);               // inputPower_x10W
    setLe16(p, 19, 450);              // forwardPowerW
    setLe16(p, 21, 8);                // reflectedPowerW
    setLe16(p, 23, 130);              // swr_x100 -> 1,30
    setLe16(p, 25, 200);              // dissipationPam1_x10W
    setLe16(p, 33, 5000);             // vcc5_mV
    setLe16(p, 35, 261);              // vcc26_x10V -> 26,1 V
    setLe16(p, 37, 502);              // hv1_x10V -> 50,2 V
    setLe16(p, 41, 9400);             // id1_mA -> 9,4 A
    setLe16(p, 45, 14245);            // carrierFreqKHz
    p[63] = static_cast<char>(0xFF);  // errorCode: kein Fehler
    setLe16(p, 64, 0);                // errorParam
    p[66] = static_cast<char>((2 << 4) | 5);  // Luefter 2, Band 5 (20 m)
    return p;
}

}  // namespace

class TstAcomProtokoll : public QObject
{
    Q_OBJECT

private slots:
    // ── Rahmung ─────────────────────────────────────────────────────
    void fuenfRahmenGegenAbgedruckteWerte();
    void einVollerRahmenSummiertSichAufNull();
    void einBefehlLaeuftDurchDenLeserZurueck();

    // ── Leser ───────────────────────────────────────────────────────
    void rauschenUndEineFalschePruefsummeWerdenUebersprungen();
    void einUnmoeglicheLaengeWirdVerworfenStattGesammelt();
    void einGeteilterRahmenWirdTrotzdemGelesen();
    void einRahmenJeByteGefuettert();
    void mehrereRahmenAmStueck();

    // ── Telemetrie ──────────────────────────────────────────────────
    void dieTelemetrieHatGenauZweiundsiebzigBytes();
    void jedesFeldKommtAnSeinerStelleAn();
    void eineZuKurzeNutzlastWirdAbgelehnt();

    // ── Tabellen ────────────────────────────────────────────────────
    void bandUndFehlerUndBetriebsart();

    // ── Geraete und Selbstskalierung ────────────────────────────────
    void dieGeraetetabelleWiderlegtDenModellnamen();
    void dieStufenfolgeIstAufsteigend();
    void dieSelbstskalierungGehtNurNachOben();

    // ── Systemauskunft ──────────────────────────────────────────────
    void dieAuskunftWirdAngefordertUndGelesen();
    void nurEineUnbekannteKennungBleibtUnbekannt();
};

// ─────────────────────────────────────────────────────────────────────

void TstAcomProtokoll::fuenfRahmenGegenAbgedruckteWerte()
{
    // Die Beschreibung des Herstellers druckt diese Rahmen mit ihren
    // Pruefsummen ab. Das ist die harte Probe auf die Rahmenrechnung:
    // fuenf unabhaengige Werte, nicht einer.
    QCOMPARE(buildTelemetryEnable(),  QByteArray::fromHex("55920415"));
    QCOMPARE(buildTelemetryDisable(), QByteArray::fromHex("55910416"));
    // Zwei Rahmen, die dieses Programm NICHT benutzt -- sie stehen hier
    // nur, weil die Beschreibung ihre Pruefsummen abdruckt und damit die
    // Rechnung ein drittes und viertes Mal belegen.
    QCOMPARE(buildFrame(0x90), QByteArray::fromHex("55900417"));
    QCOMPARE(buildFrame(0x93), QByteArray::fromHex("55930414"));
    QCOMPARE(buildFrame(0x01), QByteArray::fromHex("550104a6"));
}

void TstAcomProtokoll::einVollerRahmenSummiertSichAufNull()
{
    // Die Beschreibung sagt: Pruefsumme = 256 - (Summe der vorherigen
    // Bytes), also summiert sich ein gueltiger Rahmen MIT Pruefsumme auf
    // 0 mod 256. Das ist eine andere Aussage als „die Pruefsumme stimmt"
    // und laesst sich unabhaengig nachrechnen.
    for (const QByteArray& rahmen : {buildTelemetryEnable(),
                                     buildTelemetryDisable(),
                                     buildModeCommand(ModeCommand::Operate),
                                     buildClearFaultsCommand(),
                                     buildRequestMessage(0x11)}) {
        quint8 summe = 0;
        for (char c : rahmen) {
            summe = static_cast<quint8>(summe + static_cast<quint8>(c));
        }
        QVERIFY2(summe == 0,
                 qPrintable(QStringLiteral("Rahmen %1 summiert sich auf %2, "
                                           "nicht auf 0")
                                .arg(QString::fromLatin1(rahmen.toHex()))
                                .arg(summe)));
    }
}

void TstAcomProtokoll::einBefehlLaeuftDurchDenLeserZurueck()
{
    QList<Frame> gelesen;
    FrameParser p;
    p.setFrameCallback([&](const Frame& f) { gelesen.append(f); });

    p.feed(buildModeCommand(ModeCommand::Standby));
    QCOMPARE(gelesen.size(), 1);
    QCOMPARE(gelesen.at(0).address, quint8(0x81));
    // Unterbefehl 0x02 = Betriebsart wechseln, dann der Zielwert.
    QCOMPARE(static_cast<quint8>(gelesen.at(0).data.at(0)), quint8(0x02));
    QCOMPARE(static_cast<quint8>(gelesen.at(0).data.at(2)),
             static_cast<quint8>(ModeCommand::Standby));
}

void TstAcomProtokoll::rauschenUndEineFalschePruefsummeWerdenUebersprungen()
{
    QList<Frame> gelesen;
    FrameParser p;
    p.setFrameCallback([&](const Frame& f) { gelesen.append(f); });

    QByteArray strom;
    strom.append(QByteArray::fromHex("0000ff"));     // Rauschen ohne Startbyte
    strom.append(QByteArray::fromHex("550104ff"));   // Start, aber Pruefsumme falsch
    strom.append(buildTelemetryEnable());            // echter Rahmen
    p.feed(strom);

    QCOMPARE(gelesen.size(), 1);
    QCOMPARE(gelesen.at(0).address, quint8(0x92));
}

void TstAcomProtokoll::einUnmoeglicheLaengeWirdVerworfenStattGesammelt()
{
    // Die Beschreibung deckelt jede Nachricht auf 72 Bytes. Eine Laenge
    // darueber darf nicht geglaubt werden -- der Leser wartete sonst auf
    // Bytes, die nie kommen, und jeder echte Rahmen DAHINTER bliebe
    // liegen, bis die erfundene Laenge voll ist.
    //
    // ── WIE DIESER FALL ENTSTANDEN IST ──────────────────────────────
    //
    // Der erste Anlauf fuetterte `55 ff 00` und hundert Fuellbytes. Das
    // prueft den Deckel NICHT: das Laengenbyte steht an Stelle 2, ist
    // dort 0x00, und die zweite Bedingung (`length < 4`) wirft den
    // Rahmen schon weg, bevor der Deckel ueberhaupt gefragt wird. Die
    // Gegenprobe „Deckel weg" lief darum gruen durch. (AetherSDRs
    // Fassung hat denselben Fall und daneben den Kommentar
    // „length=255" -- der stimmt dort auch nicht.)
    //
    // Richtig geprueft wird er so: eine Laenge, die GROSS genug ist, um
    // die erste Bedingung zu passieren (>= 4) und zugleich ueber dem
    // Deckel liegt -- und dann WENIGER Bytes nachschieben als die
    // erfundene Laenge verlangt. Ohne Deckel wartet der Leser auf 255
    // Bytes, die nie kommen, und meldet den echten Rahmen dahinter
    // nie. Mit Deckel verwirft er das Startbyte sofort und findet ihn.
    QList<Frame> gelesen;
    FrameParser p;
    p.setFrameCallback([&](const Frame& f) { gelesen.append(f); });

    QByteArray strom;
    strom.append(QByteArray::fromHex("552fff"));   // Laenge 255 -- ueber dem Deckel
    strom.append(buildTelemetryEnable());          // echter Rahmen, 4 Bytes
    p.feed(strom);

    QCOMPARE(gelesen.size(), 1);
    QCOMPARE(gelesen.at(0).address, quint8(0x92));

    // ── Und die andere Haelfte: PHANTOM-RAHMEN ──────────────────────
    //
    // Ein Rahmen hat immer wenigstens vier Bytes (Start, Adresse,
    // Laenge, Pruefsumme). Faellt die Untergrenze weg, wird aus zwei
    // Zahlen ein Rahmen, den niemand geschickt hat: bei Laenge 2 oder 3
    // zeigt das Laengenbyte zugleich auf die Stelle, an der die
    // Pruefsumme stehen muesste, und fuer genau zwei Byte-Folgen geht
    // die Rechnung auf. Durchgezaehlt ueber alle 256 Adressen und alle
    // Laengen 1..3 sind es diese beiden:
    //
    //     55 AB 02      55 A8 03
    //
    // Beide wuerden ohne die Untergrenze als gueltige Rahmen gemeldet --
    // mit Adresse 0xAB bzw. 0xA8, die es im Protokoll nicht gibt, und
    // mit negativ gerechneter Nutzlastlaenge (`mid(3, length - 4)`).
    // Ein erfundener Rahmen ist schlimmer als ein verworfener: er sieht
    // aus wie eine Meldung des Geraets.
    //
    // (Die erste Fassung dieses Falls fuetterte `55 2f 02` -- harmlos,
    // weil dort die Pruefsumme NICHT aufgeht und der Leser von selbst
    // weiterspringt. Die Gegenprobe „Untergrenze weg" lief damit gruen
    // durch.)
    for (const char* phantom : {"55ab02", "55a803"}) {
        QList<Frame> gelesen2;
        FrameParser p2;
        p2.setFrameCallback([&](const Frame& f) { gelesen2.append(f); });
        p2.feed(QByteArray::fromHex(phantom));
        QVERIFY2(gelesen2.isEmpty(),
                 qPrintable(QStringLiteral("%1 wurde als Rahmen gemeldet -- "
                                           "den hat niemand geschickt")
                                .arg(QLatin1String(phantom))));
    }
}

void TstAcomProtokoll::einGeteilterRahmenWirdTrotzdemGelesen()
{
    QList<Frame> gelesen;
    FrameParser p;
    p.setFrameCallback([&](const Frame& f) { gelesen.append(f); });

    const QByteArray rahmen = buildFrame(
        static_cast<quint8>(Address::Telemetry), baueTelemetrieNutzlast());
    p.feed(rahmen.left(30));
    QVERIFY2(gelesen.isEmpty(), "Ein halber Rahmen darf nicht gemeldet werden");
    p.feed(rahmen.mid(30));
    QCOMPARE(gelesen.size(), 1);
    QCOMPARE(gelesen.at(0).data.size(), 68);
}

void TstAcomProtokoll::einRahmenJeByteGefuettert()
{
    // Eigener Fall: ueber RS-232 kommt jede Teilung vor, und der
    // Zustandsautomat muss sie alle ueberleben.
    QList<Frame> gelesen;
    FrameParser p;
    p.setFrameCallback([&](const Frame& f) { gelesen.append(f); });

    const QByteArray rahmen = buildFrame(
        static_cast<quint8>(Address::Telemetry), baueTelemetrieNutzlast());
    for (char c : rahmen) {
        p.feed(QByteArray(1, c));
    }
    QCOMPARE(gelesen.size(), 1);
    QCOMPARE(gelesen.at(0).address, quint8(0x2F));
    QCOMPARE(gelesen.at(0).data, baueTelemetrieNutzlast());
}

void TstAcomProtokoll::mehrereRahmenAmStueck()
{
    // Eigener Fall: der ACOM SCHIEBT seine Telemetrie von selbst, rund
    // zehnmal je Sekunde, und dazwischen kommen Fehlerwoerter. Am Stueck
    // ist der Normalfall, nicht die Ausnahme.
    QList<Frame> gelesen;
    FrameParser p;
    p.setFrameCallback([&](const Frame& f) { gelesen.append(f); });

    const QByteArray tele = buildFrame(
        static_cast<quint8>(Address::Telemetry), baueTelemetrieNutzlast());
    QByteArray strom;
    for (int i = 0; i < 10; ++i) {
        strom.append(tele);
        strom.append(buildFrame(static_cast<quint8>(Address::ErrorCodes),
                                QByteArray(20, '\0')));
    }
    p.feed(strom);

    QCOMPARE(gelesen.size(), 20);
    for (int i = 0; i < 20; ++i) {
        QCOMPARE(gelesen.at(i).address,
                 quint8(i % 2 == 0 ? 0x2F : 0x21));
    }
}

void TstAcomProtokoll::dieTelemetrieHatGenauZweiundsiebzigBytes()
{
    // Die Beschreibung nennt 72 Bytes fuer die Telemetrie. Trifft der
    // Rahmen das nicht, stimmt entweder die Nutzlastlaenge oder die
    // Laengenrechnung nicht.
    const QByteArray rahmen = buildFrame(
        static_cast<quint8>(Address::Telemetry), baueTelemetrieNutzlast());
    QCOMPARE(rahmen.size(), 72);
    QCOMPARE(static_cast<quint8>(rahmen.at(2)), quint8(72));
}

void TstAcomProtokoll::jedesFeldKommtAnSeinerStelleAn()
{
    QList<Frame> gelesen;
    FrameParser p;
    p.setFrameCallback([&](const Frame& f) { gelesen.append(f); });
    p.feed(buildFrame(static_cast<quint8>(Address::Telemetry),
                      baueTelemetrieNutzlast()));
    QCOMPARE(gelesen.size(), 1);

    const auto t = decodeTelemetry(gelesen.at(0).data);
    QVERIFY(t.has_value());
    QVERIFY(t->mode == Mode::OperateTx);
    QCOMPARE(t->dcPowerPam1_x10W, quint16(1234));
    // Hohes Wort << 16 | niedriges Wort. Die Reihenfolge ist am echten
    // 600S bestaetigt: dessen Front zeigte 127:43:17, dekodiert 459.797 s
    // -- eine Verdrehung der Woerter waere dort aufgefallen.
    QCOMPARE(t->systemClockSec, quint32(70536));
    QCOMPARE(t->paTempRaw, quint16(305));
    QCOMPARE(t->inputPower_x10W, quint16(50));
    QCOMPARE(t->forwardPowerW, quint16(450));
    QCOMPARE(t->reflectedPowerW, quint16(8));
    QCOMPARE(t->swr_x100, quint16(130));
    QCOMPARE(t->dissipationPam1_x10W, quint16(200));
    QCOMPARE(t->vcc5_mV, quint16(5000));
    QCOMPARE(t->vcc26_x10V, quint16(261));
    QCOMPARE(t->hv1_x10V, quint16(502));
    QCOMPARE(t->id1_mA, quint16(9400));
    QCOMPARE(t->carrierFreqKHz, quint16(14245));
    QCOMPARE(t->errorCode, quint8(0xFF));
    QCOMPARE(t->errorParam, quint16(0));
    // Ein Byte, zwei Felder -- Nibble oben, Nibble unten.
    QCOMPARE(t->fanSpeed, quint8(2));
    QCOMPARE(t->activeBand, quint8(5));
}

void TstAcomProtokoll::eineZuKurzeNutzlastWirdAbgelehnt()
{
    const QByteArray p = baueTelemetrieNutzlast();
    QVERIFY2(!decodeTelemetry(p.left(67)).has_value(),
             "Eine zu kurze Nutzlast muss abgelehnt werden, nicht ueber das "
             "Ende hinaus gelesen");
    QVERIFY(!decodeTelemetry(QByteArray()).has_value());
}

void TstAcomProtokoll::bandUndFehlerUndBetriebsart()
{
    QCOMPARE(bandName(5), QStringLiteral("20m"));
    QCOMPARE(bandName(1), QStringLiteral("160m"));
    QCOMPARE(bandName(3), QStringLiteral("40/60m"));
    QVERIFY2(bandName(0) == QStringLiteral("?m"),
             "Index 0 ist beim ACOM reserviert, kein Band");
    QCOMPARE(bandName(99), QStringLiteral("?m"));
    QCOMPARE(bandName(-1), QStringLiteral("?m"));

    QCOMPARE(errorCodeName(0xFF), QStringLiteral("No fault"));
    QCOMPARE(errorCodeName(0x39), QStringLiteral("Excessive PAM current"));
    QCOMPARE(errorCodeName(0x70), QStringLiteral("CAT error"));
    // Ein unbekannter Code verweist auf die Anzeige des Geraets statt
    // einen Namen zu erfinden -- dieselbe Enthaltsamkeit wie beim KPA500.
    QVERIFY2(errorCodeName(0xAB).contains(QStringLiteral("amplifier display")),
             "Ein unbekannter Fehlercode darf keinen Namen bekommen");

    QCOMPARE(modeName(Mode::Standby),   QStringLiteral("STANDBY"));
    QCOMPARE(modeName(Mode::OperateRx), QStringLiteral("OPR/RX"));
    QCOMPARE(modeName(Mode::OperateTx), QStringLiteral("OPR/TX"));
    QCOMPARE(modeName(Mode::PowerOff),  QStringLiteral("OFF"));
    QCOMPARE(modeName(Mode::Unknown),   QStringLiteral("UNKNOWN"));
}

void TstAcomProtokoll::dieGeraetetabelleWiderlegtDenModellnamen()
{
    // DIE FALLE: der Modellname ist NICHT die Nennleistung. Wer vom
    // Namen auf die Leistung schliesst, skaliert zwei von sechs Balken
    // falsch -- und auch die Konstanten des oeffentlichen
    // Vergleichsprogramms sind dort falsch (1200S=1200 W, 2020S=1800 W).
    // Quelle sind ACOMs eigene Datenblattseiten.
    const auto& s600 = modelSpec(QStringLiteral("600S"));
    QCOMPARE(s600.nominalForwardW, 600.0f);
    QCOMPARE(s600.maxForwardW, 700.0f);   // am echten Geraet bestaetigt
    QVERIFY(!s600.hasPam2);

    QCOMPARE(modelSpec(QStringLiteral("500S")).nominalForwardW, 500.0f);
    QCOMPARE(modelSpec(QStringLiteral("500S")).maxForwardW, 600.0f);
    QCOMPARE(modelSpec(QStringLiteral("700S")).nominalForwardW, 700.0f);
    QCOMPARE(modelSpec(QStringLiteral("700S")).maxForwardW, 800.0f);

    QVERIFY2(modelSpec(QStringLiteral("1200S")).nominalForwardW == 1000.0f,
             "Ein 1200S kann 1000 W, nicht 1200 -- der Name ist nicht die "
             "Nennleistung");
    QCOMPARE(modelSpec(QStringLiteral("1200S")).maxForwardW, 1200.0f);
    QCOMPARE(modelSpec(QStringLiteral("1400S")).nominalForwardW, 1200.0f);
    QCOMPARE(modelSpec(QStringLiteral("1400S")).maxForwardW, 1400.0f);
    QVERIFY2(modelSpec(QStringLiteral("2020S")).nominalForwardW == 1500.0f,
             "Ein 2020S kann 1500 W, nicht 2020");
    QVERIFY2(modelSpec(QStringLiteral("2020S")).maxForwardW == 1750.0f,
             "Beim 2020S passt der Name auf kein Leistungsmuster -- die "
             "Decke kommt aus dem Verhaeltnis des 600S, nicht aus '2020'");

    QVERIFY(modelSpec(QStringLiteral("1200S")).hasPam2);
    QVERIFY(modelSpec(QStringLiteral("1400S")).hasPam2);
    QVERIFY2(modelSpec(QStringLiteral("GibtsNicht")).name == QStringLiteral("600S"),
             "Eine unbekannte Kennung faellt auf das einzige am Geraet "
             "gepruefte Modell zurueck");
    QCOMPARE(modelNames().size(), 6);
}

void TstAcomProtokoll::dieStufenfolgeIstAufsteigend()
{
    const QStringList stufen = orderedModelTiers();
    QCOMPARE(stufen, QStringList({QStringLiteral("500S"), QStringLiteral("600S"),
                                  QStringLiteral("700S"), QStringLiteral("1200S"),
                                  QStringLiteral("1400S"), QStringLiteral("2020S")}));
    // Und sie ist wirklich aufsteigend -- sowohl nach Nenn- als auch nach
    // Hoechstleistung. Die Selbstskalierung laeuft diese Liste von vorn
    // ab; waere sie nicht sortiert, waehlte sie die falsche Stufe.
    for (int i = 1; i < stufen.size(); ++i) {
        const auto& a = modelSpec(stufen.at(i - 1));
        const auto& b = modelSpec(stufen.at(i));
        QVERIFY2(a.nominalForwardW < b.nominalForwardW
                     && a.maxForwardW < b.maxForwardW,
                 qPrintable(QStringLiteral("%1 steht vor %2, ist aber nicht "
                                           "kleiner").arg(a.name, b.name)));
    }
}

void TstAcomProtokoll::dieSelbstskalierungGehtNurNachOben()
{
    // Die Idee: ein 500S KANN keine 900 W melden. Wer 900 W sieht, hat
    // mindestens einen 1200S am Kabel -- unabhaengig davon, was die
    // Typkennung sagt oder nicht sagt.
    QCOMPARE(tierForForwardPower(400.0f),  QStringLiteral("500S"));
    QCOMPARE(tierForForwardPower(650.0f),  QStringLiteral("600S"));
    QCOMPARE(tierForForwardPower(750.0f),  QStringLiteral("700S"));
    QVERIFY2(tierForForwardPower(1100.0f) == QStringLiteral("1200S"),
             "Sie muss DIREKT auf die passende Stufe springen, nicht "
             "schrittweise durch jede dazwischen");
    QCOMPARE(tierForForwardPower(1300.0f), QStringLiteral("1400S"));
    QVERIFY2(tierForForwardPower(5000.0f) == QStringLiteral("2020S"),
             "Jenseits aller Stufen wird gedeckelt, nicht hoeher geraten");

    // Der Spielraum gegen das Flattern genau an einer Stufengrenze: 700 W
    // ist die Decke des 600S, und knapp darueber soll die Stufe NICHT
    // schon wechseln.
    QCOMPARE(tierForForwardPower(700.0f), QStringLiteral("600S"));
    QCOMPARE(tierForForwardPower(710.0f), QStringLiteral("600S"));
}

void TstAcomProtokoll::dieAuskunftWirdAngefordertUndGelesen()
{
    // Die Systemauskunft wird NICHT von selbst geschickt -- sie muss
    // angefordert werden (0x02 mit der gewuenschten Adresse).
    QCOMPARE(buildRequestMessage(0x11).size(), 5);
    QList<Frame> gelesen;
    FrameParser p;
    p.setFrameCallback([&](const Frame& f) { gelesen.append(f); });
    p.feed(buildRequestMessage(0x11));
    QCOMPARE(gelesen.size(), 1);
    QCOMPARE(gelesen.at(0).address, quint8(0x02));
    QCOMPARE(static_cast<quint8>(gelesen.at(0).data.at(0)), quint8(0x11));

    QByteArray nutz(26, '\0');
    nutz[0]  = static_cast<char>(0x01);   // Typ 1 = A600S, der einzige dokumentierte
    nutz[1]  = static_cast<char>(0x02);
    nutz[2]  = static_cast<char>(0x09);
    for (int i = 0; i < 12; ++i) {
        nutz[13 + i] = static_cast<char>(0xA0 + i);
    }
    nutz[25] = static_cast<char>(0x03);

    const auto cfg = decodeSystemConfig(nutz);
    QVERIFY(cfg.has_value());
    QCOMPARE(cfg->amplifierType, quint8(1));
    QCOMPARE(cfg->fwVersion, quint8(2));
    QCOMPARE(cfg->fwSubVersion, quint8(9));
    // Die Seriennummer kommt als HEX-Abzug, nicht als Klartext: ob die
    // zwoelf Bytes ASCII oder Binaer sind, ist unbestaetigt, und ein
    // falsch gedeuteter Klartext waere schlimmer als ein Hex-Abzug.
    QCOMPARE(cfg->serialNumberHex, QStringLiteral("a0a1a2a3a4a5a6a7a8a9aaab"));
    QCOMPARE(cfg->hardFaultCount, quint8(3));
    QVERIFY2(!decodeSystemConfig(nutz.left(25)).has_value(),
             "Eine zu kurze Auskunft muss abgelehnt werden");
}

void TstAcomProtokoll::nurEineUnbekannteKennungBleibtUnbekannt()
{
    // Die Beschreibung dokumentiert GENAU EINEN Typcode („1 - A600S").
    // Fuer die anderen fuenf Modelle gibt es keine Quelle. Ein geratener
    // Code -- auch ein plausibel aussehender wie „nach Erscheinungsjahr"
    // -- waere schlimmer als keiner: er sieht aus wie Wissen.
    QCOMPARE(modelNameForAmplifierType(1), QStringLiteral("600S"));
    QVERIFY2(modelNameForAmplifierType(2).isEmpty(),
             "Typ 2 ist NICHT dokumentiert -- hier darf kein Name stehen");
    QVERIFY2(modelNameForAmplifierType(0).isEmpty(), "Typ 0 ebenso");
    QVERIFY2(modelNameForAmplifierType(255).isEmpty(), "Typ 255 ebenso");

    // Und genau deshalb traegt die Selbstskalierung die Last: sie
    // braucht die Typkennung gar nicht.
    QVERIFY(!tierForForwardPower(1100.0f).isEmpty());
}

QTEST_MAIN(TstAcomProtokoll)
#include "tst_acom_protokoll.moc"
