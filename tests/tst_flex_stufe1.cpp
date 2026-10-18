// =================================================================
// tests/tst_flex_stufe1.cpp  (Longpath)
// =================================================================
//
// Flex-Treiber Stufe 1 — Erkennung und Befehlskanal, gegen ECHTE
// Bytes geprueft, nicht gegen den eigenen Erzeuger.
//
// Die Erkennungsrufe unten sind am 2026-10-18 vom Draht gelesen, auf
// Martins Netz, mit einem Mitleser auf UDP 4992:
//
//   • 172.30.30.12  — die echte FLEX-8400, SmartSDR 4.3.16.42034,
//                     620 Byte je Ruf, im Sekundentakt.
//   • 172.30.30.110 — Longpath selbst. Es ruft auf demselben Port
//                     aus, damit die PGXL es findet
//                     (FlexRadioDiscoveryBroadcaster), und stand
//                     deshalb am Maestro in Martins Geraeteliste als
//                     Flex, das er nicht besitzt. Hier ist derselbe
//                     Ruf der Pruefstein fuer den Eigenruf-Filter.
//
// Damit sind beide Richtungen von echten Bytes gedeckt: was eine
// Maschine wirklich sendet, und was wir selbst senden und verwerfen
// muessen.
//
// Die Saetze des Befehlskanals (V/H/R/S/M) sind aus AetherSDRs
// Satzlehre uebernommen; die Pruefstuecke hier sind die Beispiele, die
// dort im Kopfkommentar stehen (CommandParser.cpp:26-32 [@d58e2b8a]),
// plus die Faelle, an denen ein Leser schweigend falsch liegt: ein
// Ergebniscode mit gesetztem oberstem Bit, ein mehrteiliger
// Objektname, eine unlesbare Frequenz.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file. Die Byte- und
// Zeilenfolgen sind gelesener Draht bzw. Beispiele aus dem Kopf der
// portierten Satzlehre, kein portierter Code.

#include <QtTest>

#include <QHostAddress>
#include <QSignalSpy>

#include "core/FlexRadioConnection.h"
#include "core/FlexRadioDiscovery.h"

using namespace Longpath;

namespace {

// Der Kopf der echten 8400, Byte fuer Byte wie gelesen.
//   38      Typ 3, Kennung vorhanden
//   53      0x50 | 3 — TSI=UTC, TSF=Echtzeit, Zaehler 3
//   00 9b   155 Worte = 620 Byte
//   00 00 08 00   VITA-Mundart
//   00 00 1c 2d   pad + OUI 00-1C-2D (FlexRadio Systems)
//   53 4c ff ff   Info-Klasse "SL", Paketklasse 0xFFFF
//   6a d4 7a b9   Zeitstempel (UNIX-Sekunden)
//   00 00 00 00   Bruchteil
//   00 00 00 00   reserviert
const char* kHeader8400Hex =
    "3853009b00000800"
    "00001c2d534cffff"
    "6ad47ab900000000"
    "00000000";

// Die Nutzlast der echten FLEX-8400, wie gelesen.
const char* kPayload8400 =
    "discovery_protocol_version=3.1.0.4 model=FLEX-8400 "
    "serial=1025-1213-8400-1196 version=4.3.16.42034 nickname=Martin "
    "callsign=OE5SOS ip=172.30.30.12 port=4992 status=Available "
    "inuse_ip= inuse_host= max_licensed_version=v3 "
    "radio_license_id=00-1C-2D-05-34-28 fpc_mac= wan_connected=0 "
    "licensed_clients=2 available_clients=2 max_panadapters=2 "
    "available_panadapters=2 max_slices=2 available_slices=2 "
    "gui_client_ips= gui_client_hosts= gui_client_programs= "
    "gui_client_stations= gui_client_handles= "
    "min_software_version=3.8.0.0 external_port_link=1 "
    "license_is_unknown=0 is_system_model=0 turf_region=EU";

// Longpaths eigener Ruf, ebenfalls wie gelesen. Beachte model=FLEX-6400
// und nickname=Longpath — das war das Phantom am Maestro.
const char* kPayloadEigen =
    "discovery_protocol_version=3.1.0.4 model=FLEX-6400 "
    "serial=7341-1489-4925-4009 version=0.7.0 nickname=Longpath "
    "callsign=OE5SOS ip=172.30.30.110 port=4992 status=Available "
    "inuse_ip= inuse_host= max_licensed_version=v4 "
    "radio_license_id=00-1C-2D-00-00-00 fpc_mac=00:00:00:00:00:00 "
    "wan_connected=1 licensed_clients=2 available_clients=2 "
    "max_panadapters=4 available_panadapters=4 max_slices=4 "
    "available_slices=4 gui_client_ips= gui_client_hosts= "
    "gui_client_programs= gui_client_stations= gui_client_handles= "
    "min_software_version=2.13.0.0 external_port_link=1 "
    "license_is_unknown=0 is_system_model=0 turf_region=USA";

QByteArray beacon(const char* payload)
{
    QByteArray out = QByteArray::fromHex(kHeader8400Hex);
    out.append(payload);
    return out;
}

}  // namespace

class TestFlexStufe1 : public QObject
{
    Q_OBJECT

private slots:
    // ─── Erkennung: der echte Ruf ───────────────────────────────────

    void kopfIstDerBeschriebene()
    {
        const QByteArray header = QByteArray::fromHex(kHeader8400Hex);
        QCOMPARE(header.size(), FlexDiscoveryWire::kHeaderBytes);

        // Byte 0 / 1 / 2-3 nach dem Satzbau in
        // FlexRadioDiscoveryBroadcaster.cpp:281-294.
        QCOMPARE(static_cast<quint8>(header.at(0)), quint8(0x38));
        QCOMPARE(static_cast<quint8>(header.at(1)) & 0xF0, 0x50);
        QCOMPARE(static_cast<quint8>(header.at(1)) & 0x0F, 3);   // Zaehler

        const int words = (static_cast<quint8>(header.at(2)) << 8)
                          | static_cast<quint8>(header.at(3));
        QCOMPARE(words, 155);
        QCOMPARE(words * 4, 620);   // genau die gemessene Rufgroesse
    }

    void echterRufWirdGelesen()
    {
        const FlexBeacon b = FlexDiscoveryWire::parseBeacon(beacon(kPayload8400));

        QVERIFY(b.valid);
        QCOMPARE(b.discoveryProtocolVersion, QStringLiteral("3.1.0.4"));
        QCOMPARE(b.model,    QStringLiteral("FLEX-8400"));
        QCOMPARE(b.serial,   QStringLiteral("1025-1213-8400-1196"));
        QCOMPARE(b.version,  QStringLiteral("4.3.16.42034"));
        QCOMPARE(b.nickname, QStringLiteral("Martin"));
        QCOMPARE(b.callsign, QStringLiteral("OE5SOS"));
        QCOMPARE(b.address,  QHostAddress(QStringLiteral("172.30.30.12")));
        QCOMPARE(b.port,     quint16(4992));
        QCOMPARE(b.status,   QStringLiteral("Available"));
        QCOMPARE(b.radioLicenseId, QStringLiteral("00-1C-2D-05-34-28"));
        QCOMPARE(b.minSoftwareVersion, QStringLiteral("3.8.0.0"));
        QCOMPARE(b.turfRegion, QStringLiteral("EU"));

        // Martins Angaben vom 2026-10-18: 2 Slices, 2 Panadapter.
        QCOMPARE(b.maxSlices,       2);
        QCOMPARE(b.availableSlices, 2);
        QCOMPARE(b.maxPanadapters,       2);
        QCOMPARE(b.availablePanadapters, 2);
        QCOMPARE(b.licensedClients,  2);
        QCOMPARE(b.availableClients, 2);

        QVERIFY(!b.wanConnected);
        QVERIFY(!b.isSystemModel);
        QVERIFY(!b.licenseIsUnknown);
        QVERIFY(b.guiClientStations.isEmpty());
    }

    // Die drei Stellen, an denen AetherSDRs Leser etwas anderes
    // erwartet als die Maschine sendet. Steht hier als Pruefstand,
    // damit die Abweichung nicht beim naechsten Abgleich still
    // zurueckfaellt.
    void abweichungenGegenAetherSdrsLeser()
    {
        const FlexBeacon b = FlexDiscoveryWire::parseBeacon(beacon(kPayload8400));

        // 1. Es gibt kein `inuse`. Die Maschine sendet inuse_ip /
        //    inuse_host, beide leer, solange niemand sitzt.
        QVERIFY(!b.kvs.contains(QStringLiteral("inuse")));
        QVERIFY(b.kvs.contains(QStringLiteral("inuse_ip")));
        QVERIFY(b.kvs.contains(QStringLiteral("inuse_host")));
        QVERIFY(b.inUseIp.isEmpty());
        QVERIFY(b.inUseHost.isEmpty());
        QVERIFY(!b.inUse());

        // 2. Es gibt kein `mf_enable`. Stattdessen die Clientzahlen.
        QVERIFY(!b.kvs.contains(QStringLiteral("mf_enable")));

        // 3. max_licensed_version ist „v3" — eine Zeichenkette. Als
        //    Zahl gelesen gaebe das still 0.
        QCOMPARE(b.maxLicensedVersion, QStringLiteral("v3"));
        bool alsZahl = false;
        b.maxLicensedVersion.toInt(&alsZahl);
        QVERIFY(!alsZahl);
    }

    void eigenerRufIstAuchEinGueltigerRuf()
    {
        // Wichtig fuer den Filter: unser Ruf ist nicht kaputt, er ist
        // nur unserer. Er muss an der ADRESSE scheitern, nicht am Satz.
        const FlexBeacon b = FlexDiscoveryWire::parseBeacon(beacon(kPayloadEigen));
        QVERIFY(b.valid);
        QCOMPARE(b.model,    QStringLiteral("FLEX-6400"));
        QCOMPARE(b.nickname, QStringLiteral("Longpath"));
        QCOMPARE(b.maxSlices, 4);
    }

    void fremderVerkehrAufDemPortIstKeinRuf()
    {
        // Auf 4992 liegt auch anderes. Ohne die Kennung in Byte 8-15
        // gilt ein Datagramm nicht als Flex-Ruf.
        QByteArray fremd = QByteArray::fromHex("3853009b0000080000000000534cffff");
        fremd.append(12, '\0');
        fremd.append(kPayload8400);
        QVERIFY(!FlexDiscoveryWire::parseBeacon(fremd).valid);

        QVERIFY(!FlexDiscoveryWire::parseBeacon(QByteArray()).valid);
        QVERIFY(!FlexDiscoveryWire::parseBeacon(QByteArray("hallo")).valid);
    }

    // ─── MAC und Uebersetzung in die Geraetezeile ───────────────────

    void macAusRadioLicenseId()
    {
        QCOMPARE(FlexDiscoveryWire::macFromRadioLicenseId(
                     QStringLiteral("00-1C-2D-05-34-28")),
                 QStringLiteral("00:1C:2D:05:34:28"));
        // Kleinschreibung kommt gross zurueck — Longpath fuehrt MACs
        // als "AA:BB:CC:DD:EE:FF" (RadioDiscovery.h).
        QCOMPARE(FlexDiscoveryWire::macFromRadioLicenseId(
                     QStringLiteral("00-1c-2d-05-34-28")),
                 QStringLiteral("00:1C:2D:05:34:28"));

        QVERIFY(FlexDiscoveryWire::macFromRadioLicenseId(QString()).isEmpty());
        QVERIFY(FlexDiscoveryWire::macFromRadioLicenseId(
                    QStringLiteral("00-1C-2D-05-34")).isEmpty());        // zu kurz
        QVERIFY(FlexDiscoveryWire::macFromRadioLicenseId(
                    QStringLiteral("00-1C-2D-05-34-28-99")).isEmpty());  // zu lang
        QVERIFY(FlexDiscoveryWire::macFromRadioLicenseId(
                    QStringLiteral("ZZ-1C-2D-05-34-28")).isEmpty());     // nicht hex
        QVERIFY(FlexDiscoveryWire::macFromRadioLicenseId(
                    QStringLiteral("0-1C-2D-05-34-28")).isEmpty());      // Gruppe nicht 2
    }

    void uebersetzungInDieGeraetezeile()
    {
        const FlexBeacon b = FlexDiscoveryWire::parseBeacon(beacon(kPayload8400));
        const RadioInfo info = FlexDiscoveryWire::toRadioInfo(
            b, QHostAddress(QStringLiteral("172.30.30.12")));

        QCOMPARE(info.protocol,  ProtocolVersion::Flex);
        QCOMPARE(info.boardType, HPSDRHW::Flex8400);
        QCOMPARE(info.macAddress, QStringLiteral("00:1C:2D:05:34:28"));
        QCOMPARE(info.address, QHostAddress(QStringLiteral("172.30.30.12")));
        QCOMPARE(info.port, quint16(4992));
        QCOMPARE(info.name, QStringLiteral("FLEX-8400 (Martin)"));
        QVERIFY(!info.inUse);

        // Die Empfaengerzahl kommt aus dem Ruf, nicht aus einer
        // Platinentabelle — bei Flex rechnet das Radio.
        QCOMPARE(info.maxReceivers, 2);
        QCOMPARE(info.adcCount, 1);
        QVERIFY(!info.hasDiversityReceiver);

        // Keine erfundene Firmwarezahl: die Maschine meldet
        // „4.3.16.42034", das passt in kein int.
        QCOMPARE(info.firmwareVersion, 0);
    }

    void serienummerTraegtWennDieMacFehlt()
    {
        QString payload = QString::fromLatin1(kPayload8400);
        payload.replace(QStringLiteral("radio_license_id=00-1C-2D-05-34-28"),
                        QStringLiteral("radio_license_id="));
        const FlexBeacon b =
            FlexDiscoveryWire::parseBeacon(beacon(payload.toLatin1().constData()));
        const RadioInfo info = FlexDiscoveryWire::toRadioInfo(b, QHostAddress());

        // Ohne Rueckfall waere die Zeile schluessellos und wuerde bei
        // jedem Ruf neu angelegt.
        QCOMPARE(info.macAddress, QStringLiteral("1025-1213-8400-1196"));
    }

    void adresseFaelltAufDenAbsenderZurueck()
    {
        QString payload = QString::fromLatin1(kPayload8400);
        payload.replace(QStringLiteral("ip=172.30.30.12"), QStringLiteral("ip="));
        const FlexBeacon b =
            FlexDiscoveryWire::parseBeacon(beacon(payload.toLatin1().constData()));
        QVERIFY(b.address.isNull());

        const RadioInfo info = FlexDiscoveryWire::toRadioInfo(
            b, QHostAddress(QStringLiteral("172.30.30.12")));
        QCOMPARE(info.address, QHostAddress(QStringLiteral("172.30.30.12")));
    }

    // ─── Die Liste: anlegen, nachfuehren, vergessen ─────────────────

    void rufWirdGemeldetUndNachgefuehrt()
    {
        FlexRadioDiscovery disc;
        disc.setOwnAddressesForTesting({QHostAddress(QStringLiteral("203.0.113.7"))});

        QSignalSpy gefunden(&disc, &FlexRadioDiscovery::radioDiscovered);
        QSignalSpy geaendert(&disc, &FlexRadioDiscovery::radioUpdated);

        const QHostAddress von(QStringLiteral("172.30.30.12"));
        disc.ingestDatagramForTesting(beacon(kPayload8400), von);
        QCOMPARE(gefunden.count(), 1);
        QCOMPARE(geaendert.count(), 0);
        QCOMPARE(disc.discoveredRadios().size(), 1);

        // Zweiter Ruf derselben Maschine: nachfuehren, nicht anlegen.
        disc.ingestDatagramForTesting(beacon(kPayload8400), von);
        QCOMPARE(gefunden.count(), 1);
        QCOMPARE(geaendert.count(), 1);
        QCOMPARE(disc.discoveredRadios().size(), 1);
    }

    void eigenerRufStehtNichtInDerListe()
    {
        // Genau der Fehler vom 2026-10-18: Longpath als Flex in der
        // eigenen Liste. Der Ruf ist gueltig (siehe oben), er kommt nur
        // von einer eigenen Adresse.
        FlexRadioDiscovery disc;
        disc.setOwnAddressesForTesting({QHostAddress(QStringLiteral("172.30.30.110"))});

        QSignalSpy gefunden(&disc, &FlexRadioDiscovery::radioDiscovered);
        disc.ingestDatagramForTesting(beacon(kPayloadEigen),
                                      QHostAddress(QStringLiteral("172.30.30.110")));
        QCOMPARE(gefunden.count(), 0);
        QVERIFY(disc.discoveredRadios().isEmpty());

        // Dieselbe Maschine von einer fremden Adresse waere eine
        // fremde Maschine — der Filter haengt an der Adresse, nicht am
        // Spitznamen.
        disc.ingestDatagramForTesting(beacon(kPayloadEigen),
                                      QHostAddress(QStringLiteral("172.30.30.99")));
        QCOMPARE(gefunden.count(), 1);
    }

    void rufOhneSeriennummerWirdVerworfen()
    {
        QString payload = QString::fromLatin1(kPayload8400);
        payload.replace(QStringLiteral("serial=1025-1213-8400-1196"),
                        QStringLiteral("serial="));

        FlexRadioDiscovery disc;
        disc.setOwnAddressesForTesting({QHostAddress(QStringLiteral("203.0.113.7"))});
        QSignalSpy gefunden(&disc, &FlexRadioDiscovery::radioDiscovered);
        disc.ingestDatagramForTesting(beacon(payload.toLatin1().constData()),
                                      QHostAddress(QStringLiteral("172.30.30.12")));
        QCOMPARE(gefunden.count(), 0);
    }

    // ─── Befehlskanal: die Satzlehre ────────────────────────────────

    void fassungUndKennung()
    {
        FlexLine v = FlexCommandWire::parseLine(QStringLiteral("V3.3.28.0"));
        QCOMPARE(v.type, FlexLineType::Version);
        QCOMPARE(v.object, QStringLiteral("3.3.28.0"));

        FlexLine h = FlexCommandWire::parseLine(QStringLiteral("H0A1B2C3D"));
        QCOMPARE(h.type, FlexLineType::Handle);
        QCOMPARE(h.handle, quint32(0x0A1B2C3D));
    }

    void antwortMitUndOhneFehler()
    {
        FlexLine ok = FlexCommandWire::parseLine(QStringLiteral("R1|0|"));
        QCOMPARE(ok.type, FlexLineType::Response);
        QCOMPARE(ok.sequence, quint32(1));
        QCOMPARE(ok.resultCode, quint32(0));

        FlexLine err = FlexCommandWire::parseLine(
            QStringLiteral("R2|50001001|No Such Object"));
        QCOMPARE(err.sequence, quint32(2));
        QCOMPARE(err.resultCode, quint32(0x50001001));
        QCOMPARE(err.object, QStringLiteral("No Such Object"));
    }

    void schwererCodeBehaeltSeinOberstesBit()
    {
        // Der Fall, an dem ein int schweigend luegt: F3000001 hat das
        // oberste Bit gesetzt. Als int gelesen gibt QString::toInt()
        // einen Ueberlauf und damit 0 — eine schwere Ablehnung saehe
        // dann wie Erfolg aus. (AetherSDR schreibt denselben Hinweis an
        // dieselbe Stelle, CommandParser.cpp:63-70 [@d58e2b8a].)
        FlexLine fatal = FlexCommandWire::parseLine(
            QStringLiteral("R7|F3000001|Fatal"));
        QCOMPARE(fatal.resultCode, quint32(0xF3000001));
        QVERIFY(fatal.resultCode != 0);

        // Gegenprobe, dass die Gefahr echt ist und nicht behauptet:
        bool ok = false;
        QStringLiteral("F3000001").toInt(&ok, 16);
        QVERIFY(!ok);
    }

    void zustandsschubUndDieObjektgrenze()
    {
        FlexLine s = FlexCommandWire::parseLine(
            QStringLiteral("S0A1B2C3D|slice 0 RF_frequency=14.225000 mode=USB"));
        QCOMPARE(s.type, FlexLineType::Status);
        QCOMPARE(s.handle, quint32(0x0A1B2C3D));
        QCOMPARE(s.object, QStringLiteral("slice 0"));
        QCOMPARE(s.kvs.value(QStringLiteral("RF_frequency")),
                 QStringLiteral("14.225000"));
        QCOMPARE(s.kvs.value(QStringLiteral("mode")), QStringLiteral("USB"));

        // Mehrteiliger Objektname: die Grenze ist das letzte Leerzeichen
        // VOR dem ersten '=', nicht das erste Leerzeichen.
        FlexLine pan = FlexCommandWire::parseLine(
            QStringLiteral("S0A1B2C3D|display pan 0x40000000 center=14.100 bandwidth=0.2"));
        QCOMPARE(pan.object, QStringLiteral("display pan 0x40000000"));
        QCOMPARE(pan.kvs.value(QStringLiteral("center")), QStringLiteral("14.100"));

        // Ganz ohne Schluesselpaare ist der Rumpf nur ein Objektname.
        FlexLine nackt = FlexCommandWire::parseLine(
            QStringLiteral("S0A1B2C3D|slice 1 removed"));
        QCOMPARE(nackt.object, QStringLiteral("slice 1 removed"));
        QVERIFY(nackt.kvs.isEmpty());
    }

    void meldungTraegtIhreSchwereInBit24Und25()
    {
        struct Fall { const char* zahl; FlexSeverity schwere; };
        const Fall faelle[] = {
            {"00000001", FlexSeverity::Info},
            {"01000001", FlexSeverity::Warning},
            {"02000001", FlexSeverity::Error},
            {"03000001", FlexSeverity::Fatal},
        };
        for (const Fall& f : faelle) {
            const FlexLine m = FlexCommandWire::parseLine(
                QStringLiteral("M%1|etwas ist passiert").arg(QLatin1String(f.zahl)));
            QCOMPARE(m.type, FlexLineType::Message);
            QCOMPARE(m.severity, f.schwere);
            QCOMPARE(m.object, QStringLiteral("etwas ist passiert"));
        }
    }

    void unbekannteSatzartUndLeereZeile()
    {
        QCOMPARE(FlexCommandWire::parseLine(QStringLiteral("X1|2")).type,
                 FlexLineType::Unknown);
        QCOMPARE(FlexCommandWire::parseLine(QString()).type,
                 FlexLineType::Unknown);
    }

    void befehlWirdRichtigEingepackt()
    {
        QCOMPARE(FlexCommandWire::buildCommand(5, QStringLiteral("sub slice all")),
                 QByteArray("C5|sub slice all\n"));
    }

    // ─── Die Frequenz, das Ziel der Stufe 1 ────────────────────────

    void scheibennummerAusDemObjektnamen()
    {
        QCOMPARE(FlexCommandWire::sliceIdFromObject(QStringLiteral("slice 0")), 0);
        QCOMPARE(FlexCommandWire::sliceIdFromObject(QStringLiteral("slice 3")), 3);
        QCOMPARE(FlexCommandWire::sliceIdFromObject(QStringLiteral("slice x")), -1);
        QCOMPARE(FlexCommandWire::sliceIdFromObject(QStringLiteral("slice")), -1);
        QCOMPARE(FlexCommandWire::sliceIdFromObject(QStringLiteral("display pan 0x4")), -1);
        QCOMPARE(FlexCommandWire::sliceIdFromObject(QString()), -1);
    }

    void frequenzKommtInMegahertzUndWirdZuHertz()
    {
        QMap<QString, QString> kvs;
        kvs.insert(QStringLiteral("RF_frequency"), QStringLiteral("14.074000"));
        const auto hz = FlexCommandWire::frequencyHzFromSliceKvs(kvs);
        QVERIFY(hz.has_value());
        QCOMPARE(*hz, 14'074'000.0);
    }

    void unlesbareFrequenzWirdNichtZuNull()
    {
        // Der Grund fuer die Regel: eine verstuemmelte RF_frequency
        // wuerde sonst auf 0 Hz stimmen. FlexLib faellt hier zu, und
        // AetherSDR schreibt genau das an dieselbe Stelle
        // (FlexBackend.cpp:731-736 [@d58e2b8a]).
        QMap<QString, QString> kaputt;
        kaputt.insert(QStringLiteral("RF_frequency"), QStringLiteral("NOT_A_NUMBER"));
        QVERIFY(!FlexCommandWire::frequencyHzFromSliceKvs(kaputt).has_value());

        // Fehlt der Schluessel ganz, kommt auch nichts.
        QMap<QString, QString> ohne;
        ohne.insert(QStringLiteral("mode"), QStringLiteral("USB"));
        QVERIFY(!FlexCommandWire::frequencyHzFromSliceKvs(ohne).has_value());
    }

    // ─── Die Verbindung als Ganzes, ohne Netz ──────────────────────

    void kennungSchaltetAufVerbundenUndFrequenzKommtDurch()
    {
        FlexRadioConnection conn;
        QSignalSpy fassung(&conn, &FlexRadioConnection::versionReceived);
        QSignalSpy verbunden(&conn, &FlexRadioConnection::connected);
        QSignalSpy frequenz(&conn, &FlexRadioConnection::sliceFrequencyChanged);

        QCOMPARE(conn.state(), FlexRadioConnection::State::Disconnected);

        conn.ingestLineForTesting(QStringLiteral("V4.3.16.42034"));
        QCOMPARE(fassung.count(), 1);
        QCOMPARE(conn.radioVersion(), QStringLiteral("4.3.16.42034"));
        // Die Fassung allein macht noch keine Verbindung — erst die
        // Kennung. (So setzt es auch AetherSDR,
        // RadioConnection.cpp:552-559 [@d58e2b8a].)
        QCOMPARE(verbunden.count(), 0);

        conn.ingestLineForTesting(QStringLiteral("H0A1B2C3D"));
        QCOMPARE(verbunden.count(), 1);
        QCOMPARE(conn.state(), FlexRadioConnection::State::Connected);
        QCOMPARE(conn.clientHandle(), quint32(0x0A1B2C3D));

        conn.ingestLineForTesting(
            QStringLiteral("S0A1B2C3D|slice 0 RF_frequency=14.074000 mode=DIGU"));
        QCOMPARE(frequenz.count(), 1);
        QCOMPARE(frequenz.at(0).at(0).toInt(), 0);
        QCOMPARE(frequenz.at(0).at(1).toDouble(), 14'074'000.0);
        QCOMPARE(conn.sliceFrequenciesHz().value(0), 14'074'000.0);
    }

    void gleicheFrequenzZweimalMeldetEinmal()
    {
        // Die Maschine schiebt denselben Zustand mehrfach. Ein
        // unveraenderter Wert ist keine Nachricht.
        FlexRadioConnection conn;
        conn.ingestLineForTesting(QStringLiteral("H1"));
        QSignalSpy frequenz(&conn, &FlexRadioConnection::sliceFrequencyChanged);

        conn.ingestLineForTesting(
            QStringLiteral("S1|slice 0 RF_frequency=14.074000"));
        conn.ingestLineForTesting(
            QStringLiteral("S1|slice 0 RF_frequency=14.074000"));
        QCOMPARE(frequenz.count(), 1);

        conn.ingestLineForTesting(
            QStringLiteral("S1|slice 0 RF_frequency=14.075000"));
        QCOMPARE(frequenz.count(), 2);
    }

    void zweiScheibenWerdenGetrenntGefuehrt()
    {
        FlexRadioConnection conn;
        conn.ingestLineForTesting(QStringLiteral("H1"));

        conn.ingestLineForTesting(
            QStringLiteral("S1|slice 0 RF_frequency=14.074000"));
        conn.ingestLineForTesting(
            QStringLiteral("S1|slice 1 RF_frequency=7.074000"));

        QCOMPARE(conn.sliceFrequenciesHz().size(), 2);
        QCOMPARE(conn.sliceFrequenciesHz().value(0), 14'074'000.0);
        QCOMPARE(conn.sliceFrequenciesHz().value(1),  7'074'000.0);
    }

    void zustandsschubOhneFrequenzMeldetNichts()
    {
        FlexRadioConnection conn;
        conn.ingestLineForTesting(QStringLiteral("H1"));
        QSignalSpy frequenz(&conn, &FlexRadioConnection::sliceFrequencyChanged);

        conn.ingestLineForTesting(QStringLiteral("S1|slice 0 mode=USB audio_level=50"));
        conn.ingestLineForTesting(QStringLiteral("S1|display pan 0x40000000 center=14.1"));
        conn.ingestLineForTesting(QStringLiteral("S1|slice 0 RF_frequency=kaputt"));
        QCOMPARE(frequenz.count(), 0);
        QVERIFY(conn.sliceFrequenciesHz().isEmpty());
    }

    void befehlOhneVerbindungWirdNichtGesendet()
    {
        FlexRadioConnection conn;
        QCOMPARE(conn.sendCommand(QStringLiteral("sub slice all")), quint32(0));
    }
};

QTEST_MAIN(TestFlexStufe1)
#include "tst_flex_stufe1.moc"
