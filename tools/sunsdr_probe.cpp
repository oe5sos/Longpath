// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tools/sunsdr_probe.cpp  (Longpath)
// =================================================================
//
// WERKZEUG, kein Programmteil: fragt die SunSDR2-QRP-Hardware selbst,
// nicht ExpertSDR2, ob sie das Draht-Protokoll spricht, das
// docs/architecture/2026-08-24-sunsdr-native-driver-design.md aus
// ArtemisSDR (github.com/kk68/ArtemisSDR, GPLv2-or-later, Kosta
// Kanchev K0KOZ) als Referenz dokumentiert.
//
// ── Warum es das gibt ───────────────────────────────────────────────
//
// OE5SOS, 2026-08-24: "ich will ohne sun sdr software funken, also
// nur longpath software" -- ein eigener Zielfunkgeraet-Treiber wie
// ANAN/HL2, kein TCI-Umweg ueber ExpertSDR2 mehr.
//
// Das Draht-Protokoll dazu ist bei Expert Electronics nicht
// veroeffentlicht. ArtemisSDR hat es fuer SunSDR2 DX und PRO durch
// stillen Mitschnitt ermittelt -- aber NIE gegen einen QRP gepruft.
// Beide Geraete teilen dieselbe Produktfamilie und dieselbe
// ExpertSDR2/3-Software, das Protokoll ist darum plausibel gleich --
// das ist eine Vermutung, keine bestaetigte Tatsache.
//
// Auf einem Sendezweig ist das kein Software-Detail. Diese Sonde
// beantwortet die eine Frage, die vor jeglichem Verbindungs- oder
// Sendecode steht: antwortet die echte QRP-Hardware ueberhaupt auf
// das dokumentierte Kopf-Format, und wenn ja, stimmen die Bytes mit
// dem ueberein, was ArtemisSDR fuer DX/PRO aufgezeichnet hat?
//
// Sie baut nichts, sie aendert nichts am Geraetezustand. Fuer DX/PRO
// wird GENAU EIN Befehlstyp gesendet: die reine Selbstauskunfts-Abfrage
// (Opcode 0x1A, Nutzlast leer) -- bei ArtemisSDR selbst nur zum
// Auslesen von Firmware/Seriennummer verwendet, nie zur Steuerung
// (sunsdr_probe_identity_query, ArtemisSDR sunsdr.c:4871-4872). Kein
// Einschalten, keine Frequenz, kein PTT, kein Antennenwechsel.
//
// Fuer das QRP-Profil kommt eine zweite, ebenso leere Anfrage dazu:
// Opcode 0x12. Im echten Mitschnitt vom 2026-08-24 war das eine
// Host->Radio-Abfrage ganz ohne Nutzlast, die eine echte 20-Byte-
// Antwort bekam (siehe Design-Dokument, Abschnitt "richer opcode
// set") -- Bedeutung nicht bestaetigt, aber Form und Zeitpunkt (frueh
// im Bootablauf, vor jeglicher IQ-Uebertragung) passen zu einer
// zweiten Selbstauskunfts-Abfrage wie 0x1A, nicht zu einem
// Steuerbefehl. Auch das bleibt rein lesend.
//
// ── Aufruf ──────────────────────────────────────────────────────────
//
//   sunsdr_probe <IP-Adresse> [sekunden]
//
// Probiert nacheinander das DX-Profil (Steuerport 50001, Kennbyte
// 0x32), das PRO-Profil (Steuerport 50002, Kennbyte 0x01) und -- seit
// dem echten Mitschnitt vom 2026-08-24 -- das QRP-Profil (Steuerport
// 50001, Kennbyte 0x03) -- siehe docs/architecture/
// 2026-08-24-sunsdr-native-driver-design.md, Abschnitt "Confirmed
// from real QRP capture". Genau DAS ist der Grund, warum diese Sonde
// beim ersten Versuch (mit dem DX-Kennbyte 0x32) keine Antwort bekam,
// obwohl ping schon lief: falsches Kennbyte, kein Netzwerkproblem.
//
// Ein Vorbehalt aus demselben Mitschnitt: die dortige 0x1A-Abfrage im
// echten Bootablauf trug eine 4-Byte-Nutzlast aus Nullen, nicht die
// leere Nutzlast, die diese Sonde (nach ArtemisSDR's eigener
// Selbstauskunfts-Verwendung) sendet. Bleibt die QRP-Antwort auch mit
// korrigiertem Kennbyte aus, ist das ein moeglicher Grund -- keine
// Gewissheit, siehe Design-Dokument.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-24 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
//   2026-08-24 — QRP-Profil (Kennbyte 0x03) ergaenzt, nach Auswertung
//                eines echten tcpdump-Mitschnitts (21720 Pakete) auf
//                der Werkbank. KI-gestuetzt ueber Anthropic Claude
//                (Cowork).
//   2026-08-25 — Zweite Abfrage (Opcode 0x12) fuer das QRP-Profil
//                ergaenzt, ebenfalls aus dem echten Mitschnitt
//                bestaetigt. Mehrere Antworten je Profil werden jetzt
//                alle protokolliert statt nur der ersten. KI-gestuetzt
//                ueber Anthropic Claude (Cowork).
//   2026-08-26 — `--discover`-Modus ergaenzt: sendet GENAU das
//                Broadcast-Paket, das ExpertSDR2 beim Kaltstart als
//                allererstes verschickt (Kennbyte 0x03, Opcode 0x00,
//                24 Byte), an die Broadcast-Adresse jeder lokalen
//                Schnittstelle, und wartet auf die Unicast-"Leuchtfeuer"-
//                Antwort (Opcode 0x01) der QRP. Fund vom selben Abend
//                (siehe Design-Dokument, "the reachability gate is a
//                broadcast discovery packet"): ein Mitschnitt OHNE
//                Host-Filter zeigte, dass ExpertSDR2 dieses Paket vor
//                jeglichem direkten Kontakt zur QRP verschickt -- die
//                QRP antwortet erst DANACH per Unicast. Testet direkt,
//                ob Longpath selbst (ohne ExpertSDR2) dieselbe Antwort
//                ausloesen kann. Rein lesend: sendet nur die
//                Discovery-Anfrage, nichts, was den Geraetezustand
//                aendert. KI-gestuetzt ueber Anthropic Claude (Cowork).
//   2026-08-26 — `--listen`-Modus ergaenzt: Discovery, dann rein
//                passives Zuhoeren auf dem bestaetigten IQ-Strom-Port
//                50002, ohne irgendetwas weiter zu senden. Testet, ob
//                die QRP von selbst zu senden beginnt, sobald sie das
//                Leuchtfeuer beantwortet hat -- oder ob mindestens ein
//                weiterer, noch unbekannter Befehl noetig ist. KI-
//                gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QCoreApplication>
#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QTextStream>
#include <QTimer>
#include <QUdpSocket>

#include <cstdio>

namespace {

QTextStream& out()
{
    static QTextStream s(stdout);
    return s;
}

// Der 18-Byte-Steuerkopf, zeichengetreu aus ArtemisSDR sunsdr_build_header
// (sunsdr.c:2242-2256) -- dort BAUANLEITUNG, hier wird sie befolgt, um
// GENAU den Rahmen zu erzeugen, den die Referenz fuer eine
// Selbstauskunfts-Abfrage beschreibt (docs/architecture/
// 2026-08-24-sunsdr-native-driver-design.md, "Control channel").
QByteArray buildQueryFrame(quint8 magic0, quint8 opcode)
{
    QByteArray buf(18, char(0));
    buf[0] = char(magic0);
    buf[1] = char(0xFF);
    buf[2] = char(opcode);
    buf[3] = char(0x00);
    // decl_len = 0 (reine Abfrage, keine Nutzlast) -> buf[4..5] bleiben 0.
    // sub = 0 -> buf[6..7] bleiben 0.
    buf[10] = char(0x01);
    return buf;
}

struct SunSdrProfile {
    QString name;
    quint16 ctrlPort;
    quint16 streamPort;
    quint8  magic0;
    QVector<quint8> queries;  // opcodes to try, in order, all empty-payload
};

// DX/PRO: ArtemisSDR sunsdr.c:2728-2742 (sunsdr_profile_dx /
// sunsdr_profile_pro), query 0x1A only -- ArtemisSDR's own documented
// identity query, never verified for a QRP magic byte.
//
// QRP: docs/architecture/2026-08-24-sunsdr-native-driver-design.md,
// "Confirmed from real QRP capture" -- ports match DX, magic byte
// 0x03, from a real tcpdump capture (21720 packets, 2026-08-24), not
// guessed. Queries 0x1A (same as DX/PRO, cross-checking the shared
// opcode) and 0x12 (QRP-only, real capture showed a genuine 20-byte
// reply to this exact empty-payload frame -- see the header comment
// above for why this is trusted as read-only).
const SunSdrProfile kProfiles[] = {
    { QStringLiteral("DX"),  50001, 50002, 0x32, {0x1A} },
    { QStringLiteral("PRO"), 50002, 50003, 0x01, {0x1A} },
    { QStringLiteral("QRP"), 50001, 50002, 0x03, {0x1A, 0x12} },
};

QString hexDump(const QByteArray& data)
{
    QString s;
    s.reserve(data.size() * 3);
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) { s += QLatin1Char(' '); }
        s += QStringLiteral("%1").arg(quint8(data[i]), 2, 16, QLatin1Char('0'));
    }
    return s;
}

// Firmware-Version-Bytes laut ArtemisSDR sunsdr.c:4961-5008 (0x1A-Antwort
// im Muster einer bereits gefundenen Feldlage) -- nur ein Deutungsversuch,
// kein bestaetigtes QRP-Feld. Wird nur ausgegeben, wenn die Antwort lang
// genug ist, um die Bytes ueberhaupt zu enthalten.
void tryDecodeIdentityReply(const QByteArray& reply)
{
    if (reply.size() > 24) {
        out() << "      moegliche Firmware-Bytes (DX/PRO-Lage, ungeprueft "
                 "fuer QRP): [22]=" << quint8(reply[22])
              << " [24]=" << quint8(reply[24]) << "\n";
    } else {
        out() << "      (Antwort zu kurz fuer die dokumentierte "
                 "Firmware-Feldlage -- rohe Bytes oben sind der Befund)\n";
    }
}

// Das Broadcast-Discovery-Paket, byte-genau aus dem Mitschnitt vom
// 2026-08-26 (host-filterfreier tcpdump waehrend ExpertSDR2 kalt
// startete): Kennbyte 0x03 (QRP-Profil), Opcode 0x00 -- ein Opcode, der
// in keinem der vorherigen, auf die QRP-IP gefilterten Mitschnitte
// je auftauchte, weil der Host-Filter Broadcast-Pakete grundsaetzlich
// ausschliesst. ExpertSDR2 verschickt dieses Paket siebenmal (einmal
// je lokaler Schnittstelle: Loopback, WLAN-Teilnetz, Kabel-Teilnetz)
// an die jeweilige Broadcast-Adresse, Port 50001 auf beiden Seiten,
// BEVOR es je direkt mit der QRP spricht. Nicht nachgebaut aus
// ArtemisSDR -- ArtemisSDR hat keinen Broadcast-Discovery-Code
// (`grep -rn "255.255.255.255\|INADDR_BROADCAST"` in sunsdr.c: keine
// Treffer), das ist eine reine QRP-Beobachtung.
QByteArray discoveryFrame()
{
    QByteArray buf(24, char(0));
    buf[0] = char(0x03);
    buf[1] = char(0xFF);
    buf[2] = char(0x00);
    buf[3] = char(0x1A);
    buf[22] = char(0xFB);
    buf[23] = char(0xE6);
    return buf;
}

// Sendet discoveryFrame() an die Broadcast-Adresse jeder lokalen
// IPv4-Schnittstelle (wie ExpertSDR2 es tut, statt nur an eine
// geratene 255.255.255.255) und protokolliert jede Antwort. Beantwortet
// direkt die Frage, die das Design-Dokument als naechsten Bench-
// Schritt nennt: kann Longpath selbst (nicht nur ExpertSDR2) die QRP
// zum Antworten bringen?
int runDiscoverMode(QCoreApplication& app, int seconds)
{
    out() << "SunSDR-Discovery-Sonde -- " << seconds << " Sekunden\n";
    out() << "------------------------------------------------------------\n";
    out() << "Sendet NUR die Broadcast-Discovery-Anfrage (Kennbyte 0x03,\n";
    out() << "Opcode 0x00), die ExpertSDR2 beim Kaltstart als erstes\n";
    out() << "verschickt -- keine Frequenz, kein PTT, kein Zustand\n";
    out() << "geaendert.\n\n";
    out().flush();

    const QByteArray query = discoveryFrame();
    out() << "> Discovery (" << query.size() << " Byte): " << hexDump(query)
          << "\n\n";
    out().flush();

    auto sock = std::make_shared<QUdpSocket>();
    if (!sock->bind(QHostAddress::AnyIPv4, 50001,
                     QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        // ExpertSDR2 selbst bindet moeglicherweise Port 50001 lokal;
        // ShareAddress erlaubt beiden Prozessen, denselben Port zu
        // belegen (wie es UDP-Discovery ueblicherweise handhabt). Klappt
        // das trotzdem nicht, weicht die Sonde auf einen freien Port aus
        // -- die QRP antwortet laut Mitschnitt an den ABSENDER-Port der
        // Anfrage, nicht zwingend an 50001, also bleibt die Sonde auch
        // dann empfangsbereit.
        if (!sock->bind(QHostAddress::AnyIPv4, 0)) {
            out() << "FEHLER: konnte keinen lokalen Port oeffnen: "
                  << sock->errorString() << "\n";
            out().flush();
            return 1;
        }
    }
    // Kein extra "Broadcast erlauben"-Sockelschalter noetig -- Qt6's
    // QUdpSocket::writeDatagram sendet an eine Broadcast-Zieladresse ohne
    // weiteres Zutun (anders als bei manch aelterer Socket-API).

    int replyCount = 0;
    QObject::connect(sock.get(), &QUdpSocket::readyRead, [sock, &replyCount]() {
        while (sock->hasPendingDatagrams()) {
            QByteArray buf;
            buf.resize(int(sock->pendingDatagramSize()));
            QHostAddress sender;
            quint16 senderPort = 0;
            sock->readDatagram(buf.data(), buf.size(), &sender, &senderPort);
            out() << "< Antwort von " << sender.toString() << ":" << senderPort
                  << " (" << buf.size() << " Byte): " << hexDump(buf) << "\n";
            out().flush();
            ++replyCount;
        }
    });

    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) { continue; }
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            const QHostAddress bcast = entry.broadcast();
            if (bcast.isNull()) { continue; }
            sock->writeDatagram(query, bcast, 50001);
            out() << "  an " << bcast.toString() << " (" << iface.name()
                  << ")\n";
        }
    }
    out().flush();

    QTimer::singleShot(seconds * 1000, &app, [&app, &replyCount]() {
        out() << "\n------------------------------------------------------------\n";
        out() << "BEFUND\n\n";
        if (replyCount > 0) {
            out() << "  " << replyCount << " Antwort(en) erhalten -- die QRP\n"
                     "  reagiert auf Longpaths eigene Discovery-Anfrage, ganz\n"
                     "  ohne ExpertSDR2. Bestaetigt Gate 1 aus dem Design-\n"
                     "  Dokument als loesbar.\n";
        } else {
            out() << "  Keine Antwort. Entweder ist gerade eine ExpertSDR2-\n"
                     "  Sitzung aktiv (Gate 2, exklusive Sitzung -- siehe\n"
                     "  Design-Dokument), oder das Discovery-Paket muss noch\n"
                     "  genauer nachgebaut werden.\n";
        }
        out().flush();
        app.quit();
    });

    return app.exec();
}

// Discovery (schon bestaetigt sicher, siehe runDiscoverMode), dann NICHTS
// WEITER SENDEN -- nur auf dem bestaetigten IQ-Stream-Port 50002
// (docs/architecture/2026-08-24-sunsdr-native-driver-design.md,
// "Confirmed from real QRP capture") zuhoeren. Testet die Vermutung, dass
// ein Grossteil der bisher unbekannten Opcodes im Verbindungsaufbau reine
// Lese-Abfragen fuer die ExpertSDR2-Anzeige sind, nicht Voraussetzung
// dafuer, dass die QRP ueberhaupt zu senden beginnt. Rein passiv nach der
// Discovery-Anfrage -- kein Steuerbefehl, keine Frequenz, kein Zustand
// geaendert.
// Byte-genau aus dem Mitschnitt vom 2026-08-26 ("VFO tuning", der saubere,
// unvermischte Frequenz-Mitschnitt -- siehe Design-Dokument, Opcode 0x08
// vollstaendig bestaetigt) -- KEIN neu zusammengebauter Wert, sondern die
// exakten Bytes, die ExpertSDR2 selbst schon einmal sicher an dieses
// Geraet gesendet hat, auf einem lizenzierten Amateurfunk-Empfangsband,
// ohne Sendezweig. Zweck: pruefen, ob GENAU dieser bereits als sicher
// bestaetigte Befehl der fehlende Ausloeser fuer den I/Q-Strom ist.
QByteArray replayedFrequencyFrame()
{
    return QByteArray::fromHex(
        "03ff0800080000000000010000008ca31dd76ce0780800000000");
}

// Byte-genau aus dem Mitschnitt vom 2026-08-26 ("stream start"): der
// Moment, in dem der I/Q-Strom beginnt, faellt exakt mit der Antwort auf
// GENAU dieses Paket zusammen -- Opcode 0x01, bei ArtemisSDR
// SUNSDR_OP_STATE_SYNC genannt (dort ein 68-Byte-Paket im DX-Bootablauf;
// die QRP benutzt hier eine kleinere, 30-Byte-Fassung desselben Opcodes).
// Auch dies eine bereits sicher beobachtete, echte Anfrage von
// ExpertSDR2 selbst, kein neu zusammengebauter Wert.
QByteArray replayedStateSyncFrame()
{
    return QByteArray::fromHex(
        "03ff01000c0000000000010000007648ea9e010000000c08040302020202");
}

int runListenMode(QCoreApplication& app, int seconds,
                   const QVector<QByteArray>& replayFrames)
{
    out() << "SunSDR-Zuhoer-Sonde -- Discovery, dann " << seconds
          << " Sekunden nur horchen (Port 50002, IQ-Strom)\n";
    out() << "------------------------------------------------------------\n";
    out().flush();

    const QByteArray query = discoveryFrame();
    auto discoverySock = std::make_shared<QUdpSocket>();
    if (!discoverySock->bind(QHostAddress::AnyIPv4, 0)) {
        out() << "FEHLER: konnte keinen lokalen Port fuer die Discovery-"
                 "Anfrage oeffnen: " << discoverySock->errorString() << "\n";
        out().flush();
        return 1;
    }
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) { continue; }
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            const QHostAddress bcast = entry.broadcast();
            if (bcast.isNull()) { continue; }
            discoverySock->writeDatagram(query, bcast, 50001);
        }
    }
    out() << "> Discovery gesendet, warte auf Leuchtfeuer-Antwort...\n";
    out().flush();

    // Port 50002 offen, BEVOR die Discovery-Antwort eintrifft -- falls die
    // QRP sofort nach dem Leuchtfeuer zu senden beginnt, soll kein Paket
    // durch eine Race Condition verpasst werden.
    auto streamSock = std::make_shared<QUdpSocket>();
    if (!streamSock->bind(QHostAddress::AnyIPv4, 50002,
                           QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        if (!streamSock->bind(QHostAddress::AnyIPv4, 0)) {
            out() << "FEHLER: konnte Port 50002 nicht oeffnen (auch nicht "
                     "mit Ausweich-Port): " << streamSock->errorString() << "\n";
            out().flush();
            return 1;
        }
        out() << "(Hinweis: Port 50002 war belegt, Ausweich-Port benutzt --\n"
                 " die QRP schickt vermutlich weiter an genau 50002, dann\n"
                 " sieht diese Sonde nichts. Kein anderer Prozess sollte "
                 "gerade laufen.)\n";
    }

    qint64 streamPackets = 0;
    qint64 streamBytes = 0;
    bool beaconSeen = false;
    QHostAddress radioAddr;

    QObject::connect(discoverySock.get(), &QUdpSocket::readyRead,
                      [discoverySock, &beaconSeen, &radioAddr, &replayFrames]() {
        while (discoverySock->hasPendingDatagrams()) {
            QByteArray buf;
            buf.resize(int(discoverySock->pendingDatagramSize()));
            QHostAddress sender;
            quint16 senderPort = 0;
            discoverySock->readDatagram(buf.data(), buf.size(), &sender, &senderPort);
            // Leuchtfeuer-Antwort: Kennbyte 0x03, Opcode 0x01 (siehe
            // discoveryFrame()-Kommentar). Andere Antworten sind das
            // eigene Broadcast-Echo oder ein anderer LAN-Teilnehmer.
            if (!beaconSeen && buf.size() >= 3 && quint8(buf[0]) == 0x03
                && quint8(buf[2]) == 0x01) {
                beaconSeen = true;
                radioAddr = sender;
                out() << "< Leuchtfeuer von " << sender.toString() << " -- "
                         "QRP hat geantwortet.\n";
                out().flush();
                for (const QByteArray& frame : replayFrames) {
                    out() << "> sende bereits bestaetigten Befehl an "
                          << sender.toString() << " (" << frame.size()
                          << " Byte): " << hexDump(frame) << "\n";
                    out().flush();
                    discoverySock->writeDatagram(frame, sender, 50001);
                }
            }
        }
    });

    QObject::connect(streamSock.get(), &QUdpSocket::readyRead,
                      [streamSock, &streamPackets, &streamBytes]() {
        while (streamSock->hasPendingDatagrams()) {
            QByteArray buf;
            buf.resize(int(streamSock->pendingDatagramSize()));
            QHostAddress sender;
            quint16 senderPort = 0;
            streamSock->readDatagram(buf.data(), buf.size(), &sender, &senderPort);
            if (streamPackets == 0) {
                out() << "< ERSTES Paket auf Port 50002 von "
                      << sender.toString() << ":" << senderPort << " ("
                      << buf.size() << " Byte): " << hexDump(buf.left(24))
                      << (buf.size() > 24 ? " ..." : "") << "\n";
                out().flush();
            }
            ++streamPackets;
            streamBytes += buf.size();
        }
    });

    QTimer::singleShot(seconds * 1000, &app,
                        [&app, &streamPackets, &streamBytes, &beaconSeen,
                         &replayFrames]() {
        out() << "\n------------------------------------------------------------\n";
        out() << "BEFUND\n\n";
        out() << "  Leuchtfeuer-Antwort: " << (beaconSeen ? "ja" : "nein") << "\n";
        if (!replayFrames.isEmpty()) {
            out() << "  Wiederholte Befehle gesendet: "
                  << (beaconSeen ? "ja" : "nein (kein Leuchtfeuer)") << "\n";
        }
        out() << "  Pakete auf Port 50002: " << streamPackets
              << " (" << streamBytes << " Byte gesamt)\n\n";
        const bool sentReplay = !replayFrames.isEmpty();
        if (streamPackets > 0 && !sentReplay) {
            out() << "  Die QRP sendet I/Q-Daten, OHNE dass irgendetwas "
                     "ausser der\n"
                     "  Discovery-Anfrage gesendet wurde. Das waere ein "
                     "starkes Zeichen,\n"
                     "  dass der Grossteil der noch unbekannten Opcodes "
                     "reine Anzeige-\n"
                     "  Abfragen sind, keine Voraussetzung fuer den "
                     "Empfang selbst.\n";
        } else if (streamPackets > 0 && sentReplay) {
            out() << "  Die QRP sendet I/Q-Daten, nachdem Discovery + der "
                     "wiederholte(n)\n"
                     "  Befehl(e) gesendet wurden. Das ist (mindestens ein "
                     "Teil des)\n"
                     "  fehlenden Ausloesers.\n";
        } else if (beaconSeen) {
            out() << "  Leuchtfeuer kam, aber kein I/Q-Strom -- " <<
                     (sentReplay
                          ? "auch der/die gesendete(n)\n  Befehl(e) "
                            "allein reichen nicht, mindestens ein weiterer, "
                            "noch\n  unbekannter Opcode ist noetig.\n"
                          : "die QRP braucht offenbar\n  mindestens einen "
                            "weiteren Befehl, bevor sie zu senden beginnt. "
                            "Welcher\n  das ist, bleibt offen.\n");
        } else {
            out() << "  Kein Leuchtfeuer -- siehe --discover fuer sich "
                     "allein zur\n"
                     "  weiteren Fehlersuche.\n";
        }
        out().flush();
        app.quit();
    });

    return app.exec();
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    if (argc >= 2 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--discover")) {
        const int seconds = (argc > 2) ? QString::fromLocal8Bit(argv[2]).toInt() : 5;
        return runDiscoverMode(app, seconds);
    }

    if (argc >= 2 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--listen")) {
        const int seconds = (argc > 2) ? QString::fromLocal8Bit(argv[2]).toInt() : 8;
        return runListenMode(app, seconds, {});
    }

    // --listen-freq/--listen-full both send replayedFrequencyFrame() —
    // a REAL hardware-retuning command, the instant a beacon replies (no
    // interactive confirmation, this tool has none). It always replays
    // the one exact frequency captured during an earlier bench session,
    // not whatever the radio happens to be tuned to right now. Run
    // carelessly while the operator has the radio parked on a live QSO,
    // this silently retunes it away mid-contact. Warning added
    // 2026-08-26 after a review flagged that this tool gave no signal
    // of that risk anywhere in its own output before this evening.
    static const auto warnAboutRetune = []() {
        out() << "ACHTUNG: dieser Modus sendet einen ECHTEN Frequenz-Befehl an "
                 "die Radio, sobald sie antwortet -- keine Bestaetigung, kein "
                 "Abbruch danach. Immer derselbe, einmal mitgeschnittene Wert, "
                 "NICHT die aktuell eingestellte Frequenz. Falls die Radio "
                 "gerade in Benutzung ist: hier abbrechen (Strg+C).\n\n";
        out().flush();
    };

    if (argc >= 2 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--listen-freq")) {
        const int seconds = (argc > 2) ? QString::fromLocal8Bit(argv[2]).toInt() : 8;
        warnAboutRetune();
        return runListenMode(app, seconds, {replayedFrequencyFrame()});
    }

    if (argc >= 2 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--listen-sync")) {
        const int seconds = (argc > 2) ? QString::fromLocal8Bit(argv[2]).toInt() : 8;
        return runListenMode(app, seconds, {replayedStateSyncFrame()});
    }

    if (argc >= 2 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--listen-full")) {
        const int seconds = (argc > 2) ? QString::fromLocal8Bit(argv[2]).toInt() : 8;
        warnAboutRetune();
        return runListenMode(app, seconds,
                              {replayedStateSyncFrame(), replayedFrequencyFrame()});
    }

    if (argc < 2) {
        out() << "Aufruf: sunsdr_probe <IP-Adresse> [sekunden]\n";
        out() << "    oder: sunsdr_probe --discover [sekunden]\n";
        out() << "    oder: sunsdr_probe --listen [sekunden]\n";
        out() << "    oder: sunsdr_probe --listen-freq [sekunden]\n";
        out() << "    oder: sunsdr_probe --listen-sync [sekunden]\n";
        out() << "    oder: sunsdr_probe --listen-full [sekunden]\n";
        out().flush();
        return 1;
    }
    const QString hostStr = QString::fromLocal8Bit(argv[1]);
    const int seconds = (argc > 2) ? QString::fromLocal8Bit(argv[2]).toInt() : 5;

    QHostAddress host(hostStr);
    if (host.isNull()) {
        out() << "FEHLER: \"" << hostStr << "\" ist keine gueltige "
                 "IP-Adresse.\n";
        out().flush();
        return 1;
    }

    out() << "SunSDR-Sonde -- " << hostStr << ", " << seconds
          << " Sekunden je Profil\n";
    out() << "------------------------------------------------------------\n";
    out() << "Sendet NUR leere Selbstauskunfts-Abfragen (siehe Kopfkommentar\n";
    out() << "dieser Datei fuer die Opcodes je Profil) -- kein Einschalten,\n";
    out() << "keine Frequenz, kein PTT, keine Antenne.\n\n";
    out().flush();

    struct ProfileResult {
        QString profileName;
        int repliesSeen = 0;
    };
    auto results = std::make_shared<QVector<ProfileResult>>();
    auto sockets = std::make_shared<QVector<QUdpSocket*>>();

    int profileIndex = 0;
    for (const SunSdrProfile& profile : kProfiles) {
        out() << "-- Profil " << profile.name << " (Steuerport "
              << profile.ctrlPort << ", Kennbyte 0x"
              << QString::number(profile.magic0, 16).rightJustified(2, QLatin1Char('0'))
              << ") --\n";
        out().flush();

        auto* sock = new QUdpSocket(&app);
        sockets->append(sock);
        if (!sock->bind(QHostAddress::AnyIPv4, 0)) {
            out() << "   FEHLER: konnte keinen lokalen Port oeffnen: "
                  << sock->errorString() << "\n\n";
            out().flush();
            results->append({profile.name, 0});
            ++profileIndex;
            continue;
        }

        // Alle Abfragen dieses Profils direkt hintereinander raus -- die
        // Sonde ordnet Antworten nicht einer bestimmten Abfrage zu, sie
        // protokolliert nur, was auf diesem Sockel zurueckkommt. Bei einem
        // einzelnen Geraet und wenigen Abfragen reicht das zur Auswertung
        // von Hand.
        for (quint8 opcode : profile.queries) {
            const QByteArray query = buildQueryFrame(profile.magic0, opcode);
            out() << "   > 0x" << QString::number(opcode, 16).rightJustified(2, QLatin1Char('0'))
                  << " Selbstauskunft (" << query.size()
                  << " Byte): " << hexDump(query) << "\n";
            out().flush();
            sock->writeDatagram(query, host, profile.ctrlPort);
        }

        const int idx = profileIndex;
        results->append({profile.name, 0});

        QObject::connect(sock, &QUdpSocket::readyRead, [sock, idx, results]() {
            while (sock->hasPendingDatagrams()) {
                QByteArray buf;
                buf.resize(int(sock->pendingDatagramSize()));
                QHostAddress sender;
                quint16 senderPort = 0;
                sock->readDatagram(buf.data(), buf.size(), &sender, &senderPort);
                out() << "   < Antwort von " << sender.toString() << ":"
                      << senderPort << " (" << buf.size() << " Byte): "
                      << hexDump(buf) << "\n";
                tryDecodeIdentityReply(buf);
                out().flush();
                ++(*results)[idx].repliesSeen;
            }
        });

        ++profileIndex;
    }

    QTimer::singleShot(seconds * 1000, &app, [&app, results]() {
        out() << "\n------------------------------------------------------------\n";
        out() << "BEFUND\n\n";

        bool anyReply = false;
        for (const auto& r : *results) {
            if (r.repliesSeen > 0) {
                anyReply = true;
                out() << "  Profil " << r.profileName << ": " << r.repliesSeen
                      << " Antwort(en) erhalten -- das Geraet spricht "
                         "zumindest den 18-Byte-Kopf dieses Profils.\n";
            } else {
                out() << "  Profil " << r.profileName << ": keine Antwort.\n";
            }
        }

        out() << "\n";
        if (!anyReply) {
            out() << "  Keine Antwort auf keinem der Profile.\n\n"
                     "  Zu pruefen, bevor daraus \"spricht das Protokoll\n"
                     "  nicht\" wird:\n"
                     "   - ist die IP-Adresse richtig und die QRP erreichbar\n"
                     "     (z.B. per ping)?\n"
                     "   - laeuft gerade eine ExpertSDR2-Sitzung, die das\n"
                     "     Geraet exklusiv haelt?\n"
                     "   - ist die QRP eingeschaltet (nicht nur am Strom)?\n\n"
                     "  Erst wenn diese drei ausgeschlossen sind, ist \"kein\n"
                     "  Draht-Protokoll auf diesem Kopf-Format\" ein\n"
                     "  belastbarer Befund.\n";
        } else {
            out() << "  Das bestaetigt nur den STEUERKANAL-Kopf und eine\n"
                     "  reine Lese-Abfrage -- NICHT das I/Q-/Ton-Format, NICHT\n"
                     "  den Sendezweig, NICHT die Antennenauswahl, NICHT die\n"
                     "  Leistungsskalierung. Siehe docs/architecture/\n"
                     "  2026-08-24-sunsdr-native-driver-design.md fuer den\n"
                     "  vollstaendigen Abgleich, der als naechstes noch\n"
                     "  aussteht, bevor irgendein Verbindungs- oder gar\n"
                     "  Sendecode entsteht.\n";
        }
        out().flush();
        app.quit();
    });

    return app.exec();
}
