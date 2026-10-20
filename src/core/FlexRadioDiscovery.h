#pragma once

// =================================================================
// src/core/FlexRadioDiscovery.h  (Longpath)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/core/RadioDiscovery.{h,cpp} at d58e2b8a
//     — struct RadioInfo (the discovery field set),
//       RadioDiscovery::parseDiscoveryPacket (key names + cleaning),
//       upsertRadio / onStaleCheck (the serial-keyed upsert and the
//       5-second staleness sweep),
//       DISCOVERY_PORT / STALE_TIMEOUT_MS.
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 section 5.
//
// Die 28 Byte Kopf stammen NICHT von AetherSDR. AetherSDRs Leser wirft
// das ganze Datagramm durch `QString::fromUtf8` und zerlegt es an
// Leerzeichen; die Kopfbytes fallen dabei durch `cleanDiscoveryValue`
// (Steuerzeichen werden verworfen) und durch die Regel „ein Wort ohne
// '=' wird uebersprungen" von selbst heraus. Das geht, ist aber nicht
// der Satzbau. Der steht schon im eigenen Baum, Bank gegengeprueft:
// FlexRadioDiscoveryBroadcaster.cpp:281-297 beschreibt den
// VITA-49-artigen Kopf Byte fuer Byte (aus einer FLEX-8600-Aufzeichnung
// vom 2026-05-19). Hier wird er gelesen statt ueberlesen — die
// Kennung in Byte 8-15 (OUI 00-1C-2D „FlexRadio Systems", Info-Klasse
// „SL") ist die einzige Stelle, an der ein Flex-Beacon sich von
// irgendeinem anderen Verkehr auf UDP 4992 unterscheiden laesst.
//
// Am 2026-10-18 gegen Martins eigene FLEX-8400 gemessen (172.30.30.12,
// SmartSDR 4.3.16.42034, 620 Byte je Beacon, 1 Hz):
//
//   Kopf:  38 53 00 9b 00 00 08 00 00 00 1c 2d 53 4c ff ff
//          6a d4 7a b9 00 00 00 00 00 00 00 00
//   Nutzlast (gekuerzt):
//          discovery_protocol_version=3.1.0.4 model=FLEX-8400
//          serial=1025-1213-8400-1196 version=4.3.16.42034 ...
//
// Byte 0 = 0x38, Byte 1 = 0x50|Zaehler, Byte 2-3 = 0x009b = 155 Worte
// = 620 Byte. Der 2026-05 beschriebene Satzbau stimmt also auch fuer
// eine 4.3.16-Maschine, nicht nur fuer die 8600 von damals.
//
// Drei Felder, die AetherSDR anders liest als die echte Maschine sendet
// — gemessen, nicht vermutet (siehe parseBeacon()):
//   • `inuse` gibt es nicht. Die Maschine sendet `inuse_ip=` und
//     `inuse_host=`, beide leer, solange niemand verbunden ist.
//   • `mf_enable` gibt es nicht. Stattdessen `licensed_clients=2` und
//     `available_clients=2`.
//   • `max_licensed_version=v3` ist eine Zeichenkette mit „v" davor.
//     AetherSDRs `value.toInt()` macht daraus still eine 0.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 Longpath. Stufe 1 des Flex-Treibers: nur Erkennung,
//                 keine Stroeme. Kopfpruefung neu (siehe oben);
//                 Schluesselsatz gegen die echte 8400 erweitert;
//                 Ergebnis wird in Longpaths eigenes RadioInfo
//                 uebersetzt, damit die Geraeteliste keine zweite
//                 Zeilenart braucht.
// =================================================================

#include "RadioDiscovery.h"   // RadioInfo, ProtocolVersion

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUdpSocket>

namespace Longpath {

// Ein gelesener Flex-Erkennungsruf. Die Felder sind die, welche eine
// echte Maschine wirklich sendet (gemessen 2026-10-18, siehe Kopf);
// alles Uebrige bleibt in `kvs` stehen, damit Stufe 2 nichts neu
// aufzeichnen muss.
struct FlexBeacon {
    bool    valid{false};         // Kennung in Byte 8-15 stimmte

    QString discoveryProtocolVersion;  // "3.1.0.4"
    QString model;                     // "FLEX-8400"
    QString serial;                    // "1025-1213-8400-1196" (Schluessel bei Flex)
    QString version;                   // "4.3.16.42034" (SmartSDR)
    QString nickname;                  // "Martin"
    QString callsign;                  // "OE5SOS"
    QHostAddress address;              // aus `ip=`, sonst Absender
    quint16 port{4992};
    QString status;                    // "Available"
    QString radioLicenseId;            // "00-1C-2D-05-34-28" — die MAC
    QString maxLicensedVersion;        // "v3" — Zeichenkette, nicht Zahl
    QString minSoftwareVersion;        // "3.8.0.0"
    QString turfRegion;                // "EU"

    // Wer gerade darauf sitzt. Beide leer = frei.
    QString inUseIp;
    QString inUseHost;

    int licensedClients{0};
    int availableClients{0};
    int maxPanadapters{0};
    int availablePanadapters{0};
    int maxSlices{0};
    int availableSlices{0};

    bool wanConnected{false};
    bool isSystemModel{false};
    bool licenseIsUnknown{false};

    // Verbundene GUI-Clients (Multi-Flex). Leer, solange keiner sitzt.
    QStringList guiClientStations;
    QStringList guiClientHandles;
    QStringList guiClientPrograms;
    QStringList guiClientIps;
    QStringList guiClientHosts;

    QMap<QString, QString> kvs;   // alles, roh

    bool inUse() const { return !inUseIp.isEmpty() || !inUseHost.isEmpty(); }
};

namespace FlexDiscoveryWire {

constexpr quint16 kPort        = 4992;
constexpr int     kHeaderBytes = 28;
// Byte 8-15: OUI 00-1C-2D (FlexRadio Systems) + Info-Klasse "SL" +
// Paketklasse 0xFFFF. From FlexRadioDiscoveryBroadcaster.cpp:289-291.
constexpr int     kClassIdOffset = 8;
constexpr int     kClassIdLength = 8;

// Liest einen Erkennungsruf. Kennt den Kopf, faellt aber auf „Nutzlast
// beginnt bei Byte 0" zurueck, wenn das Datagramm ohne Kopf kommt —
// so verhaelt sich AetherSDRs Leser, und es kostet hier nichts.
FlexBeacon parseBeacon(const QByteArray& datagram);

// Dashed-uppercase („00-1C-2D-05-34-28") → Longpaths Schreibweise
// („00:1C:2D:05:34:28"). Leere oder unplausible Eingabe → leer.
QString macFromRadioLicenseId(const QString& licenseId);

// Uebersetzt den Ruf in Longpaths Geraetezeile. `sender` fuellt die
// Adresse, wenn der Ruf selbst keine mitbringt.
RadioInfo toRadioInfo(const FlexBeacon& beacon, const QHostAddress& sender);

}  // namespace FlexDiscoveryWire

// Hoert passiv auf UDP 4992 und meldet jede FlexRadio-Maschine, die
// sich dort ausruft. Kein Senden: Flex-Maschinen rufen von selbst, im
// Sekundentakt — anders als die OpenHPSDR-Erkennung, die fragen muss
// (RadioDiscovery, UDP 1024).
//
// Longpath ruft auf demselben Port selbst aus, damit die PGXL es in
// ihrer Liste findet (FlexRadioDiscoveryBroadcaster). Dieser Ruf wird
// hier verworfen: jeder Beacon, dessen Absenderadresse eine eigene
// Adresse dieses Rechners ist, zaehlt nicht. Ohne das stuende Longpath
// in seiner eigenen Geraeteliste — am 2026-10-18 live so gesehen
// (172.30.30.110, model=FLEX-6400, nickname=Longpath).
class FlexRadioDiscovery : public QObject {
    Q_OBJECT

public:
    // From AetherSDR RadioDiscovery.h:91-96 [@d58e2b8a] — STALE_TIMEOUT_MS.
    static constexpr int kStaleTimeoutMs = 5000;

    explicit FlexRadioDiscovery(QObject* parent = nullptr);
    ~FlexRadioDiscovery() override;

    bool startListening();
    void stopListening();
    bool isListening() const;

    QList<RadioInfo> discoveredRadios() const { return m_radios; }

    // Nur fuer Pruefstaende: schiebt ein Datagramm durch denselben Weg,
    // den der Socket nimmt, ohne Netz.
    void ingestDatagramForTesting(const QByteArray& datagram,
                                  const QHostAddress& sender);

    // Nur fuer Pruefstaende: setzt die Liste der „eigenen" Adressen
    // fest, statt sie bei QNetworkInterface zu erfragen. Sonst haengt
    // die Pruefung des Eigenruf-Filters daran, welche Adressen der
    // Rechner gerade hat — auf dem Bauknecht andere als hier.
    void setOwnAddressesForTesting(const QList<QHostAddress>& addresses);

signals:
    void radioDiscovered(const Longpath::RadioInfo& info);
    void radioUpdated(const Longpath::RadioInfo& info);
    void radioLost(const QString& macAddress);

private slots:
    void onReadyRead();
    void onStaleCheck();

private:
    void handleDatagram(const QByteArray& data, const QHostAddress& sender);
    bool isOwnAddress(const QHostAddress& sender) const;
    void upsertRadio(const RadioInfo& info);

    QUdpSocket m_socket;
    QTimer     m_staleTimer;
    QList<RadioInfo>      m_radios;
    QMap<QString, qint64> m_lastSeen;   // MAC → ms seit Epoche
    // Leer = bei QNetworkInterface erfragen. Im Pruefstand gesetzt.
    QList<QHostAddress> m_ownAddressesOverride;
};

}  // namespace Longpath
