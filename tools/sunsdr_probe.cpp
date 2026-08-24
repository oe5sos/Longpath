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
// Sie baut nichts, sie aendert nichts am Geraetezustand. Es wird
// GENAU EIN Befehlstyp gesendet: die reine Selbstauskunfts-Abfrage
// (Opcode 0x1A, Nutzlast leer) -- bei ArtemisSDR selbst nur zum
// Auslesen von Firmware/Seriennummer verwendet, nie zur Steuerung
// (sunsdr_probe_identity_query, ArtemisSDR sunsdr.c:4871-4872). Kein
// Einschalten, keine Frequenz, kein PTT, kein Antennenwechsel.
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
// =================================================================

#include <QCoreApplication>
#include <QHostAddress>
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
};

// DX/PRO: ArtemisSDR sunsdr.c:2728-2742 (sunsdr_profile_dx /
// sunsdr_profile_pro). QRP: docs/architecture/
// 2026-08-24-sunsdr-native-driver-design.md, "Confirmed from real QRP
// capture" -- Ports wie DX, Kennbyte 0x03, aus einem echten
// tcpdump-Mitschnitt (21720 Pakete, 2026-08-24), nicht geraten.
const SunSdrProfile kProfiles[] = {
    { QStringLiteral("DX"),  50001, 50002, 0x32 },
    { QStringLiteral("PRO"), 50002, 50003, 0x01 },
    { QStringLiteral("QRP"), 50001, 50002, 0x03 },
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

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        out() << "Aufruf: sunsdr_probe <IP-Adresse> [sekunden]\n";
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
    out() << "Sendet NUR die reine Selbstauskunfts-Abfrage (Opcode 0x1A,\n";
    out() << "leere Nutzlast) -- kein Einschalten, keine Frequenz, kein\n";
    out() << "PTT, keine Antenne. Siehe Kopfkommentar dieser Datei.\n\n";
    out().flush();

    struct ProfileResult {
        QString profileName;
        bool gotReply = false;
        QByteArray reply;
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
            results->append({profile.name, false, {}});
            ++profileIndex;
            continue;
        }

        const QByteArray query = buildQueryFrame(profile.magic0, 0x1A);
        out() << "   > 0x1A Selbstauskunft ("  << query.size()
              << " Byte): " << hexDump(query) << "\n";
        out().flush();
        sock->writeDatagram(query, host, profile.ctrlPort);

        const int idx = profileIndex;
        results->append({profile.name, false, {}});

        QObject::connect(sock, &QUdpSocket::readyRead, [sock, idx, results]() {
            while (sock->hasPendingDatagrams()) {
                QByteArray buf;
                buf.resize(int(sock->pendingDatagramSize()));
                QHostAddress sender;
                quint16 senderPort = 0;
                sock->readDatagram(buf.data(), buf.size(), &sender, &senderPort);
                if (!(*results)[idx].gotReply) {
                    out() << "   < Antwort von " << sender.toString() << ":"
                          << senderPort << " (" << buf.size() << " Byte): "
                          << hexDump(buf) << "\n";
                    tryDecodeIdentityReply(buf);
                    out().flush();
                    (*results)[idx].gotReply = true;
                    (*results)[idx].reply = buf;
                }
            }
        });

        ++profileIndex;
    }

    QTimer::singleShot(seconds * 1000, &app, [&app, results]() {
        out() << "\n------------------------------------------------------------\n";
        out() << "BEFUND\n\n";

        bool anyReply = false;
        for (const auto& r : *results) {
            if (r.gotReply) {
                anyReply = true;
                out() << "  Profil " << r.profileName << ": ANTWORT erhalten ("
                      << r.reply.size() << " Byte) -- das Geraet spricht "
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
