// SPE Expert: die Protokollschicht.
//
// Portiert aus AetherSDR tests/spe_protocol_test.cpp [@d58e2b8a] --
// Herkunft und Abweichungen stehen im Kopf von core/SpeProtocol.h. Die
// Form ist umgestellt: AetherSDR fuehrt einen eigenen Zaehler in einem
// nackten main(), Longpath nimmt QTest; die geprueften Faelle und die
// erwarteten Werte sind dieselben, dazu drei eigene (Bemerkung unten).
//
// Was dieser Pruefstand WIRKLICH belegt -- und was nicht:
//
//   Belegt ist, dass diese Schicht die Byte-Folgen baut und liest, die
//   in der Anleitung von SPE abgedruckt sind: die Tastendruck-Rahmen
//   mit ihrer Pruefsumme, der 67 Zeichen lange Zustandsstring aus §5
//   WOERTLICH, die beiden Pruefsummenformen (ein Byte bei der
//   Quittung, zwei beim Zustand), und die RFC-2217-Rahmen fuer den
//   Einschaltimpuls.
//
//   NICHT belegt ist irgendetwas am echten Geraet. Hier haengt kein
//   SPE am Kabel. Die Zahlen, die AetherSDR an einem 1.5K-FA ueber
//   ser2net gemessen hat -- die Balkenschwellen 450/500/600 usw. und
//   dass die Befehle ohne abschliessendes CR LF manchmal verschluckt
//   werden -- sind hier uebernommene Angaben, nicht nachgeprueft. Der
//   Pruefstand nagelt sie fest, damit sie nicht unbemerkt verrutschen;
//   er bestaetigt sie nicht.
//
// Drei Faelle sind eigen, nicht aus AetherSDR uebernommen:
//
//   1. `dieQuittungstreckeHaeltAmStueck`: fuettert zwanzig Rahmen in
//      einem Stueck und zaehlt sie. Der Parser schneidet den Puffer
//      selbst zurecht; ein Fehler darin faellt bei einem einzelnen
//      Rahmen nicht auf.
//   2. `einRahmenJeByteGefuettert`: dieselben Bytes, aber einzeln
//      zugefuehrt -- die Gegenprobe zum Stueck. Ueber ser2net kommt
//      jede Teilung vor.
//   3. `derZustandUeberlebtEineAushandlung`: Telnet-Aushandlung mitten
//      im Strom. Die Beschreibung behauptet, der Parser steige
//      darueber hinweg; das steht hier als Pruefung.
//
// Alle Daten sind erfunden oder aus der Anleitung abgeschrieben -- kein
// Geraet noetig.

#include <QtTest>

#include <QByteArray>
#include <QList>

#include "core/SpeProtocol.h"

using namespace Longpath::Spe;

namespace {

// Verpackt eine Zustands-Nutzlast so, wie der Verstaerker es tut
// (Anleitung §5): 0xAA x3, CNT, Daten, 16-Bit-Pruefsumme (niedrig, dann
// hoch), CR LF. Absichtlich mit eigener Rechnung, unabhaengig von
// FrameParser -- so pruefen sich die beiden gegenseitig.
QByteArray baueZustandsRahmen(const QByteArray& nutzlast)
{
    quint16 summe = 0;
    for (char c : nutzlast) {
        summe = static_cast<quint16>(summe + static_cast<quint8>(c));
    }

    QByteArray rahmen;
    rahmen.append(3, static_cast<char>(0xAA));
    rahmen.append(static_cast<char>(nutzlast.size()));
    rahmen.append(nutzlast);
    rahmen.append(static_cast<char>(summe & 0xFF));
    rahmen.append(static_cast<char>((summe >> 8) & 0xFF));
    rahmen.append('\r');
    rahmen.append('\n');
    return rahmen;
}

// Das Beispiel aus der Anleitung selbst (§5, woertlich): 67 Zeichen,
// 19 Felder hinter einem fuehrenden Kennzeichen.
const char* kBeispielAusDerAnleitung =
    "C,20K,S,R,x,1,00,1a,0r,L,0000, 0.00, 0.00, 0.0, 0.0, 33,  0,  0,N,N";

}  // namespace

class TstSpeProtokoll : public QObject
{
    Q_OBJECT

private slots:
    // ── Rahmen zum Verstaerker ───────────────────────────────────────
    void dieTastendruckRahmenStimmenMitDerAnleitung();

    // ── Rahmen vom Verstaerker ───────────────────────────────────────
    void dieQuittungHatEinPruefsummenByte();
    void derZustandHatZweiPruefsummenBytes();
    void rauschenUndEineFalschePruefsummeWerdenUebersprungen();
    void einUnmoeglichesCntWirdVerworfenStattGesammelt();
    void einGeteilterRahmenWirdTrotzdemGelesen();
    void dieQuittungstreckeHaeltAmStueck();
    void einRahmenJeByteGefuettert();
    void derZustandUeberlebtEineAushandlung();

    // ── Zustandsstring ──────────────────────────────────────────────
    void derEmpfangsZustandAusDerAnleitung();
    void einSendenderVerstaerkerMitWarnung();
    void ohneKennzeichenUndZuKurz();

    // ── Tabellen ────────────────────────────────────────────────────
    void bandWarnungAlarmUndStufe();

    // ── Geraeteabhaengige Skalierung ────────────────────────────────
    void dieGeraetetabelle();
    void dieBalkenachseFolgtDerLeistungsstufe();

    // ── Einschaltimpuls (RFC 2217) ──────────────────────────────────
    void dieRfc2217RahmenStimmen();
    void dieAntwortDesVermittlersWirdGelesenNichtGeraten();
};

// ─────────────────────────────────────────────────────────────────────

void TstSpeProtokoll::dieTastendruckRahmenStimmenMitDerAnleitung()
{
    // §3 druckt OPERATE woertlich ab: 0x55 0x55 0x55 0x01 0x0D 0x0D --
    // die Pruefsumme eines einzelnen Bytes ist das Byte selbst. Das
    // CR LF dahinter steht nicht im Rahmenbild der Anleitung, ist am
    // echten Geraet aber noetig (siehe Kopf von SpeProtocol.h).
    QCOMPARE(buildKeyCommand(Key::Operate), QByteArray::fromHex("555555010d0d0d0a"));
    QCOMPARE(buildStatusRequest(), QByteArray::fromHex("5555550190900d0a"));
    QCOMPARE(buildKeyCommand(Key::SwitchOff), QByteArray::fromHex("555555010a0a0d0a"));
    QCOMPARE(buildKeyCommand(Key::Tune), QByteArray::fromHex("5555550109090d0a"));
    QCOMPARE(buildBacklightCommand(true), QByteArray::fromHex("5555550182820d0a"));
    QCOMPARE(buildBacklightCommand(false), QByteArray::fromHex("5555550183830d0a"));
}

void TstSpeProtokoll::dieQuittungHatEinPruefsummenByte()
{
    QList<Frame> empfangen;
    FrameParser parser;
    parser.setFrameCallback([&](const Frame& f) { empfangen.append(f); });

    parser.feed(QByteArray::fromHex("aaaaaa010d0d"));
    QCOMPARE(empfangen.size(), 1);
    QVERIFY(empfangen.at(0).isAck());
    QCOMPARE(static_cast<quint8>(empfangen.at(0).data.at(0)), quint8(0x0D));
}

void TstSpeProtokoll::derZustandHatZweiPruefsummenBytes()
{
    const QByteArray nutzlast(kBeispielAusDerAnleitung);
    QCOMPARE(nutzlast.size(), int(kStatusDataLength));

    QList<Frame> empfangen;
    FrameParser parser;
    parser.setFrameCallback([&](const Frame& f) { empfangen.append(f); });
    parser.feed(baueZustandsRahmen(nutzlast));

    QCOMPARE(empfangen.size(), 1);
    QVERIFY(!empfangen.at(0).isAck());
    QCOMPARE(empfangen.at(0).data, nutzlast);
}

void TstSpeProtokoll::rauschenUndEineFalschePruefsummeWerdenUebersprungen()
{
    QList<Frame> empfangen;
    FrameParser parser;
    parser.setFrameCallback([&](const Frame& f) { empfangen.append(f); });

    QByteArray strom;
    strom.append(QByteArray::fromHex("0000aa41ff"));    // Rauschen, darin ein einzelnes 0xAA
    strom.append(QByteArray::fromHex("aaaaaa010d0e"));  // voller Vorlauf, aber Pruefsumme falsch
    strom.append(QByteArray::fromHex("aaaaaa010909"));  // echte Quittung (TUNE)
    parser.feed(strom);

    QCOMPARE(empfangen.size(), 1);
    QCOMPARE(static_cast<quint8>(empfangen.at(0).data.at(0)), quint8(0x09));
}

void TstSpeProtokoll::einUnmoeglichesCntWirdVerworfenStattGesammelt()
{
    // Ein CNT jenseits der Obergrenze darf nicht als Laenge geglaubt
    // werden: der Parser wartete sonst auf 255 Bytes und schluckte dabei
    // jeden echten Rahmen, der danach kommt.
    QList<Frame> empfangen;
    FrameParser parser;
    parser.setFrameCallback([&](const Frame& f) { empfangen.append(f); });

    QByteArray strom;
    strom.append(QByteArray::fromHex("aaaaaaff"));      // CNT = 255
    strom.append(QByteArray(100, '\0'));
    strom.append(QByteArray::fromHex("aaaaaa010d0d"));  // echte Quittung
    parser.feed(strom);

    QCOMPARE(empfangen.size(), 1);
    QCOMPARE(static_cast<quint8>(empfangen.at(0).data.at(0)), quint8(0x0D));
}

void TstSpeProtokoll::einGeteilterRahmenWirdTrotzdemGelesen()
{
    QList<Frame> empfangen;
    FrameParser parser;
    parser.setFrameCallback([&](const Frame& f) { empfangen.append(f); });

    const QByteArray rahmen = baueZustandsRahmen(QByteArray(kBeispielAusDerAnleitung));
    parser.feed(rahmen.left(10));
    QVERIFY2(empfangen.isEmpty(),
             "Ein halber Rahmen darf nicht gemeldet werden");
    parser.feed(rahmen.mid(10));
    QCOMPARE(empfangen.size(), 1);
    QCOMPARE(empfangen.at(0).data.size(), int(kStatusDataLength));
}

void TstSpeProtokoll::dieQuittungstreckeHaeltAmStueck()
{
    // Eigener Fall: zwanzig Rahmen in EINEM Stueck. Bei einem einzelnen
    // Rahmen faellt ein Schnittfehler im Puffer nicht auf.
    QList<Frame> empfangen;
    FrameParser parser;
    parser.setFrameCallback([&](const Frame& f) { empfangen.append(f); });

    QByteArray strom;
    const QByteArray zustand = baueZustandsRahmen(QByteArray(kBeispielAusDerAnleitung));
    for (int i = 0; i < 10; ++i) {
        strom.append(QByteArray::fromHex("aaaaaa010d0d"));  // Quittung
        strom.append(zustand);                              // Zustand
    }
    parser.feed(strom);

    QCOMPARE(empfangen.size(), 20);
    for (int i = 0; i < 20; ++i) {
        if (i % 2 == 0) {
            QVERIFY(empfangen.at(i).isAck());
        } else {
            QCOMPARE(empfangen.at(i).data.size(), int(kStatusDataLength));
        }
    }
}

void TstSpeProtokoll::einRahmenJeByteGefuettert()
{
    // Die Gegenprobe zum Stueck: dieselben Bytes, einzeln zugefuehrt.
    QList<Frame> empfangen;
    FrameParser parser;
    parser.setFrameCallback([&](const Frame& f) { empfangen.append(f); });

    const QByteArray rahmen = baueZustandsRahmen(QByteArray(kBeispielAusDerAnleitung));
    for (char c : rahmen) {
        parser.feed(QByteArray(1, c));
    }

    QCOMPARE(empfangen.size(), 1);
    QCOMPARE(empfangen.at(0).data, QByteArray(kBeispielAusDerAnleitung));
}

void TstSpeProtokoll::derZustandUeberlebtEineAushandlung()
{
    // Eigener Fall: die Telnet-Aushandlung des Vermittlers liegt mitten
    // im Strom. Der Kopf von SpeProtocol.h behauptet, der Vorlauf-Suchlauf
    // steige darueber hinweg -- hier steht es als Pruefung.
    QList<Frame> empfangen;
    FrameParser parser;
    parser.setFrameCallback([&](const Frame& f) { empfangen.append(f); });

    QByteArray strom;
    strom.append(Rfc2217::buildWillComPortOption());
    strom.append(QByteArray::fromHex("fffd2c"));  // IAC DO COM-PORT-OPTION
    strom.append(baueZustandsRahmen(QByteArray(kBeispielAusDerAnleitung)));
    parser.feed(strom);

    QCOMPARE(empfangen.size(), 1);
    QCOMPARE(empfangen.at(0).data, QByteArray(kBeispielAusDerAnleitung));
}

void TstSpeProtokoll::derEmpfangsZustandAusDerAnleitung()
{
    const auto s = parseStatus(QByteArray(kBeispielAusDerAnleitung));
    QVERIFY(s.has_value());
    QCOMPARE(s->id, QStringLiteral("20K"));
    QVERIFY2(!s->operate, "S heisst STANDBY");
    QVERIFY2(!s->transmitting, "R heisst Empfang");
    QCOMPARE(s->bank, QChar(u'x'));          // 2K-FA kennt keine Speicherbaenke
    QCOMPARE(s->input, 1);
    QCOMPARE(s->bandIndex, 0);               // 00 = 160 m
    QCOMPARE(s->txAntenna, 1);               // Ziffer und ATU-Buchstabe getrennt
    QCOMPARE(s->atuState, QChar(u'a'));      // a = ATU eingeschaltet
    QCOMPARE(s->rxAntenna, QStringLiteral("0r"));
    QCOMPARE(s->powerLevel, QChar(u'L'));
    QCOMPARE(s->outputPowerW, 0.0f);         // im Empfang null
    QCOMPARE(s->tempUpper, 33);
    QCOMPARE(s->warning, QChar(u'N'));
    QCOMPARE(s->alarm, QChar(u'N'));
}

void TstSpeProtokoll::einSendenderVerstaerkerMitWarnung()
{
    // Ein sendender 1.5K-FA: jedes Zahlenfeld, das das Beispiel oben auf
    // null stehen laesst, traegt hier einen Bruchwert.
    const QByteArray nutzlast(
        "C,15K,O,T,A,2,05,2b,0r,H,1350, 1.10, 1.25, 47.5, 32.0, 45, 40, 38,S,N");
    const auto s = parseStatus(nutzlast);
    QVERIFY(s.has_value());
    QCOMPARE(s->id, QStringLiteral("15K"));
    QVERIFY2(s->operate, "O heisst OPERATE");
    QVERIFY2(s->transmitting, "T heisst Senden");
    QCOMPARE(s->bank, QChar(u'A'));
    QCOMPARE(s->input, 2);
    QCOMPARE(s->bandIndex, 5);               // 05 = 20 m
    QCOMPARE(s->atuState, QChar(u'b'));      // b = ATU ueberbrueckt
    QCOMPARE(s->powerLevel, QChar(u'H'));
    QCOMPARE(s->outputPowerW, 1350.0f);
    QCOMPARE(s->swrAtu, 1.10f);
    QCOMPARE(s->swrAnt, 1.25f);
    QCOMPARE(s->paVoltageV, 47.5f);
    QCOMPARE(s->paCurrentA, 32.0f);
    QCOMPARE(s->tempUpper, 45);
    QCOMPARE(s->tempLower, 40);
    QCOMPARE(s->tempCombiner, 38);
    QCOMPARE(s->warning, QChar(u'S'));
}

void TstSpeProtokoll::ohneKennzeichenUndZuKurz()
{
    QVERIFY2(parseStatus(QByteArray(
                 "20K,S,R,x,1,00,1a,0r,L,0000, 0.00, 0.00, 0.0, 0.0, 33,  0,  0,N,N"))
                 .has_value(),
             "19 Felder ohne fuehrendes Kennzeichen muessen auch gelesen werden");
    QVERIFY2(!parseStatus(QByteArray("C,20K,S,R,x,1,00")).has_value(),
             "Ein abgeschnittener String muss abgelehnt werden, nicht "
             "falsch indiziert");
}

void TstSpeProtokoll::bandWarnungAlarmUndStufe()
{
    QCOMPARE(bandName(0), QStringLiteral("160m"));
    QCOMPARE(bandName(11), QStringLiteral("4m"));
    QCOMPARE(bandName(12), QStringLiteral("?m"));
    QCOMPARE(bandName(-1), QStringLiteral("?m"));

    QVERIFY2(warningText(u'N').isEmpty(),
             "Keine Warnung heisst leer, damit das Band sich ausblenden kann");
    QCOMPARE(warningText(u'P'), QStringLiteral("Power limit exceeded"));
    QVERIFY2(warningText(u'Z').contains(u'Z'),
             "Ein unbekannter Buchstabe wird durchgereicht, nicht geraten");
    QVERIFY(alarmText(u'N').isEmpty());
    QCOMPARE(alarmText(u'D'), QStringLiteral("Input overdriving"));

    QCOMPARE(powerLevelName(u'L'), QStringLiteral("LOW"));
    QCOMPARE(powerLevelName(u'M'), QStringLiteral("MID"));
    QCOMPARE(powerLevelName(u'H'), QStringLiteral("HIGH"));
    QVERIFY2(powerLevelName(QChar()).isEmpty(),
             "Noch nicht gemeldet heisst leer, nicht ein NUL-Zeichen");
}

void TstSpeProtokoll::dieGeraetetabelle()
{
    const auto& s15 = modelSpec(QStringLiteral("15K"));
    QCOMPARE(s15.nominalPowerW, 1500.0f);
    QCOMPARE(s15.warnPowerW, 1450.0f);
    QCOMPARE(s15.maxPowerW, 1600.0f);
    QVERIFY(s15.hasMemoryBanks);
    QVERIFY(!s15.hasCombiner);

    const auto& s20 = modelSpec(QStringLiteral("20K"));
    QVERIFY2(s20.hasCombiner, "Nur der 2K-FA hat einen Kombinierer");
    QVERIFY2(!s20.hasMemoryBanks, "Der 2K-FA kennt keine Speicherbaenke");

    QCOMPARE(modelSpec(QStringLiteral("13K")).displayName, QStringLiteral("1.3K-FA"));
    QVERIFY2(modelSpec(QStringLiteral("99K")).displayName == QStringLiteral("1.5K-FA"),
             "Eine unbekannte Kennung faellt auf den am Geraet geprueften "
             "1.5K-FA zurueck");
    QCOMPARE(modelIds().size(), 3);

    QCOMPARE(levelNominalW(s15, u'L'), 500.0f);
    QCOMPARE(levelNominalW(s15, u'M'), 1000.0f);
    QCOMPARE(levelNominalW(s15, u'H'), 1500.0f);
    QCOMPARE(levelNominalW(s15, u'?'), 1500.0f);
}

void TstSpeProtokoll::dieBalkenachseFolgtDerLeistungsstufe()
{
    // Die Achse kommt ganz aus levelGaugeRange(), damit eine Korrektur in
    // der Geraetetabelle den Balken wirklich erreicht statt von einer
    // zweiten Rechnung im Bedienfeld ueberschrieben zu werden. Darum hier
    // gegen die Felder der Tabelle selbst geprueft, nicht gegen Zahlen.
    for (const QString& id : modelIds()) {
        const auto& spec = modelSpec(id);
        const auto hoch = levelGaugeRange(spec, u'H');
        QVERIFY2(hoch.nominalW == spec.nominalPowerW
                     && hoch.warnW == spec.warnPowerW
                     && hoch.maxW == spec.maxPowerW,
                 qPrintable(QStringLiteral("%1: HIGH-Achse weicht von der "
                                           "Tabellenzeile ab").arg(spec.displayName)));
        QVERIFY2(spec.warnPowerW == spec.nominalPowerW - 50.0f
                     && spec.maxPowerW == spec.nominalPowerW + 100.0f,
                 qPrintable(QStringLiteral("%1: die Tabellenzeile folgt nicht "
                                           "der Form nominal-50/+100")
                                .arg(spec.displayName)));
    }

    const auto& s15 = modelSpec(QStringLiteral("15K"));
    const auto niedrig = levelGaugeRange(s15, u'L');
    QCOMPARE(niedrig.warnW, 450.0f);
    QCOMPARE(niedrig.nominalW, 500.0f);
    QCOMPARE(niedrig.maxW, 600.0f);

    const auto mittel = levelGaugeRange(s15, u'M');
    QCOMPARE(mittel.warnW, 950.0f);
    QCOMPARE(mittel.nominalW, 1000.0f);
    QCOMPARE(mittel.maxW, 1100.0f);

    const auto unbekannt = levelGaugeRange(s15, u'?');
    QCOMPARE(unbekannt.nominalW, 1500.0f);
    QCOMPARE(unbekannt.warnW, 1450.0f);
    QCOMPARE(unbekannt.maxW, 1600.0f);
}

void TstSpeProtokoll::dieRfc2217RahmenStimmen()
{
    QCOMPARE(Rfc2217::buildWillComPortOption(), QByteArray::fromHex("fffb2c"));
    QCOMPARE(Rfc2217::buildSetControl(Rfc2217::kRtsOn),
             QByteArray::fromHex("fffa2c050bfff0"));
    QCOMPARE(Rfc2217::buildSetControl(Rfc2217::kRtsOff),
             QByteArray::fromHex("fffa2c050cfff0"));
    QCOMPARE(Rfc2217::buildSetControl(Rfc2217::kDtrOn),
             QByteArray::fromHex("fffa2c0508fff0"));
    QCOMPARE(Rfc2217::buildSetControl(Rfc2217::kDtrOff),
             QByteArray::fromHex("fffa2c0509fff0"));
}

void TstSpeProtokoll::dieAntwortDesVermittlersWirdGelesenNichtGeraten()
{
    // Ohne diesen Lauf meldet ein Vermittler im Rohmodus Erfolg, obwohl er
    // jeden SET-CONTROL verschluckt hat.
    QVERIFY(Rfc2217::scanComPortOptionReply(QByteArray::fromHex("fffd2c"))
            == Rfc2217::OptionReply::Accepted);
    QVERIFY(Rfc2217::scanComPortOptionReply(QByteArray::fromHex("fffe2c"))
            == Rfc2217::OptionReply::Refused);
    QVERIFY2(Rfc2217::scanComPortOptionReply(QByteArray(kBeispielAusDerAnleitung))
                 == Rfc2217::OptionReply::None,
             "Ein Rohmodus-Vermittler antwortet nichts -- und nichts wird "
             "gemeldet");
    QVERIFY2(Rfc2217::scanComPortOptionReply(QByteArray::fromHex("fffd01fffd03"))
                 == Rfc2217::OptionReply::None,
             "Die Aushandlung einer fremden Wahlmoeglichkeit ist nicht unsere");
    QVERIFY(Rfc2217::scanComPortOptionReply(
                QByteArray::fromHex("aaaaaa010d0d") + QByteArray::fromHex("fffd2c")
                + QByteArray::fromHex("aaaaaa010909"))
            == Rfc2217::OptionReply::Accepted);
    QVERIFY2(Rfc2217::scanComPortOptionReply(QByteArray::fromHex("fffd2cfffe2c"))
                 == Rfc2217::OptionReply::Refused,
             "Die spaetere Antwort gilt");
    QVERIFY2(Rfc2217::scanComPortOptionReply(QByteArray::fromHex("ffff2c"))
                 == Rfc2217::OptionReply::None,
             "Ein verdoppeltes 0xFF ist kein Aushandlungswort");
    QVERIFY2(Rfc2217::scanComPortOptionReply(QByteArray::fromHex("fffd"))
                 == Rfc2217::OptionReply::None,
             "Eine am Stueckende abgeschnittene Antwort wird nicht erraten");
}

QTEST_MAIN(TstSpeProtokoll)
#include "tst_spe_protokoll.moc"
