// =================================================================
// src/core/FlexRadioDiscovery.cpp  (Longpath)
// =================================================================
//
// Herkunft, Lizenz und Messprotokoll: siehe FlexRadioDiscovery.h.
// Ported from AetherSDR src/core/RadioDiscovery.cpp at d58e2b8a
// (GPLv3, primary author Jeremy [KK7GWY]) — project-level citation per
// docs/attribution/HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
//   2026-10-28 — Frist nach Netzweg + Zaehler an der blinden Stelle,
//                 Martin Fischer, KI-gestuetzt ueber Anthropic Claude
//                 Code.
// =================================================================

#include "FlexRadioDiscovery.h"

#include <QDateTime>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QStringList>

namespace Longpath {

Q_LOGGING_CATEGORY(lcFlexRx, "longpath.flex.rx", QtInfoMsg)

namespace {

// From AetherSDR RadioDiscovery.cpp:12-27 [@d58e2b8a] —
// cleanDiscoveryValue(). Steuerzeichen und DEL raus, dann trimmen;
// `decodeDelSpaces` macht aus DEL (0x7f) wieder ein Leerzeichen, weil
// die Maschine Leerzeichen innerhalb eines Listenwerts so schuetzt.
QString cleanDiscoveryValue(QString value, bool decodeDelSpaces = false)
{
    if (decodeDelSpaces) {
        value.replace(QChar(0x7f), QLatin1Char(' '));
    }

    QString cleaned;
    cleaned.reserve(value.size());
    for (const QChar ch : value) {
        const char16_t code = ch.unicode();
        if (code < 0x20 || code == 0x7f) {
            continue;
        }
        cleaned.append(ch);
    }
    return cleaned.trimmed();
}

// From AetherSDR RadioDiscovery.cpp:29-41 [@d58e2b8a] —
// splitDiscoveryListValue().
QStringList splitDiscoveryListValue(QString value, bool decodeDelSpaces = false)
{
    value = cleanDiscoveryValue(value, decodeDelSpaces);

    QStringList values;
    for (const QString& item : value.split(',', Qt::SkipEmptyParts)) {
        const QString trimmed = item.trimmed();
        if (!trimmed.isEmpty()) {
            values << trimmed;
        }
    }
    return values;
}

int toIntOrZero(const QString& value)
{
    bool ok = false;
    const int v = value.toInt(&ok);
    return ok ? v : 0;
}

}  // namespace

namespace FlexDiscoveryWire {

FlexBeacon parseBeacon(const QByteArray& datagram)
{
    FlexBeacon beacon;

    // Kennung in Byte 8-15 suchen. From
    // FlexRadioDiscoveryBroadcaster.cpp:289-291 — pad 0x00, OUI
    // 00-1C-2D (FlexRadio Systems), Info-Klasse "SL", Paketklasse 0xFFFF.
    static const QByteArray kClassId =
        QByteArray::fromHex("00001c2d534cffff");

    int payloadStart = 0;
    if (datagram.size() >= kHeaderBytes
        && datagram.mid(kClassIdOffset, kClassIdLength) == kClassId) {
        beacon.valid = true;
        payloadStart = kHeaderBytes;
    } else {
        // Kein Kopf erkannt. AetherSDR liest in diesem Fall einfach das
        // ganze Datagramm als Text (RadioDiscovery.cpp:179-237
        // [@d58e2b8a] kennt ueberhaupt keinen Kopf), und genau dafuer ist
        // dieser Zweig da — ein Ruf ohne Kopf wird gelesen, aber nicht
        // als gueltiger Flex-Ruf gezaehlt, solange `valid` false bleibt.
        payloadStart = 0;
    }

    const QString text =
        QString::fromUtf8(datagram.mid(payloadStart)).trimmed();

    // From AetherSDR RadioDiscovery.cpp:184-235 [@d58e2b8a] — an
    // Leerzeichen zerlegen, jedes Wort an seinem ersten '=' trennen,
    // Woerter ohne '=' uebergehen.
    for (const QString& token : text.split(' ', Qt::SkipEmptyParts)) {
        const int eq = token.indexOf('=');
        if (eq < 0) {
            continue;
        }

        const QString key      = cleanDiscoveryValue(token.left(eq)).toLower();
        const QString rawValue = token.mid(eq + 1);
        const QString value    = cleanDiscoveryValue(rawValue);

        beacon.kvs.insert(key, value);

        // Die Schluessel, die AetherSDR auch liest.
        if (key == QLatin1String("model")) {
            beacon.model = value;
        } else if (key == QLatin1String("serial")) {
            beacon.serial = value;
        } else if (key == QLatin1String("version")) {
            beacon.version = value;
        } else if (key == QLatin1String("nickname")) {
            beacon.nickname = value;
        } else if (key == QLatin1String("callsign")) {
            beacon.callsign = value;
        } else if (key == QLatin1String("ip")) {
            beacon.address = QHostAddress(value);
        } else if (key == QLatin1String("port")) {
            const quint16 p = value.toUShort();
            if (p != 0) {
                beacon.port = p;
            }
        } else if (key == QLatin1String("status")) {
            beacon.status = value;
        } else if (key == QLatin1String("turf_region")) {
            beacon.turfRegion = value;
        } else if (key == QLatin1String("is_system_model")) {
            beacon.isSystemModel = (value == QLatin1String("1"));
        } else if (key == QLatin1String("gui_client_stations")) {
            beacon.guiClientStations = splitDiscoveryListValue(rawValue, true);
        } else if (key == QLatin1String("gui_client_handles")) {
            beacon.guiClientHandles = splitDiscoveryListValue(rawValue);
        } else if (key == QLatin1String("gui_client_programs")) {
            beacon.guiClientPrograms = splitDiscoveryListValue(rawValue, true);
        } else if (key == QLatin1String("gui_client_ips")) {
            beacon.guiClientIps = splitDiscoveryListValue(rawValue);
        } else if (key == QLatin1String("gui_client_hosts")) {
            beacon.guiClientHosts = splitDiscoveryListValue(rawValue, true);

        // Die Schluessel, die eine echte Maschine zusaetzlich sendet.
        // Alle am 2026-10-18 an Martins FLEX-8400 gemessen; AetherSDRs
        // Leser laesst sie fallen (siehe Kopf von FlexRadioDiscovery.h).
        } else if (key == QLatin1String("discovery_protocol_version")) {
            beacon.discoveryProtocolVersion = value;
        } else if (key == QLatin1String("radio_license_id")) {
            beacon.radioLicenseId = value;
        } else if (key == QLatin1String("max_licensed_version")) {
            // „v3" — Zeichenkette. NICHT toInt(): das gibt still 0.
            beacon.maxLicensedVersion = value;
        } else if (key == QLatin1String("min_software_version")) {
            beacon.minSoftwareVersion = value;
        } else if (key == QLatin1String("inuse_ip")) {
            beacon.inUseIp = value;
        } else if (key == QLatin1String("inuse_host")) {
            beacon.inUseHost = value;
        } else if (key == QLatin1String("licensed_clients")) {
            beacon.licensedClients = toIntOrZero(value);
        } else if (key == QLatin1String("available_clients")) {
            beacon.availableClients = toIntOrZero(value);
        } else if (key == QLatin1String("max_panadapters")) {
            beacon.maxPanadapters = toIntOrZero(value);
        } else if (key == QLatin1String("available_panadapters")) {
            beacon.availablePanadapters = toIntOrZero(value);
        } else if (key == QLatin1String("max_slices")) {
            beacon.maxSlices = toIntOrZero(value);
        } else if (key == QLatin1String("available_slices")) {
            beacon.availableSlices = toIntOrZero(value);
        } else if (key == QLatin1String("wan_connected")) {
            beacon.wanConnected = (value == QLatin1String("1"));
        } else if (key == QLatin1String("license_is_unknown")) {
            beacon.licenseIsUnknown = (value == QLatin1String("1"));
        }
    }

    return beacon;
}

QString macFromRadioLicenseId(const QString& licenseId)
{
    const QString trimmed = licenseId.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    const QStringList parts = trimmed.split('-', Qt::SkipEmptyParts);
    if (parts.size() != 6) {
        return QString();
    }
    for (const QString& part : parts) {
        if (part.size() != 2) {
            return QString();
        }
        bool ok = false;
        part.toInt(&ok, 16);
        if (!ok) {
            return QString();
        }
    }
    return parts.join(QLatin1Char(':')).toUpper();
}

RadioInfo toRadioInfo(const FlexBeacon& beacon, const QHostAddress& sender)
{
    RadioInfo info;

    info.protocol  = ProtocolVersion::Flex;
    info.boardType = HPSDRHW::Flex8400;
    info.port      = beacon.port;
    info.address   = beacon.address.isNull() ? sender : beacon.address;

    // Schluessel. Flex kennt die MAC nur als `radio_license_id`; wenn
    // die fehlt oder unplausibel ist, traegt die Seriennummer die
    // Identitaet — anders als bei OpenHPSDR, wo die MAC immer da ist.
    // Ohne diesen Rueckfall waere eine Maschine ohne `radio_license_id`
    // schluessellos und wuerde bei jedem Beacon neu in die Liste
    // gelegt.
    info.macAddress = macFromRadioLicenseId(beacon.radioLicenseId);
    if (info.macAddress.isEmpty()) {
        info.macAddress = beacon.serial;
    }

    // Der Name, den die Geraeteliste zeigt. Spitzname bevorzugt, weil
    // Martins Maschine „Martin" heisst und „FLEX-8400" allein nichts
    // ueber DIESE Maschine sagt, sobald zwei im Netz stehen.
    if (!beacon.nickname.isEmpty() && beacon.nickname != beacon.model) {
        info.name = QStringLiteral("%1 (%2)").arg(beacon.model, beacon.nickname);
    } else {
        info.name = beacon.model;
    }

    info.inUse = beacon.inUse();

    // Bei Flex rechnet das Radio: die Zahl der gleichzeitigen Empfaenger
    // ist nicht von einer Platine abzuleiten, sondern steht im Ruf
    // (`max_slices`, bei Martins 8400 gemessen 2). Siehe auch die
    // Radio-Authoritative Settings Policy in CLAUDE.md — dieselbe
    // Begruendung wie beim NR-Wert der Anvelina-Gateware.
    if (beacon.maxSlices > 0) {
        info.maxReceivers = beacon.maxSlices;
    }

    // Ein SCU in der 8400 (Betreiberangabe 2026-10-18, deckt sich mit
    // `max_panadapters=2` bei einem einzigen Wandler). Diversity braucht
    // zwei SCUs, also hier nein.
    info.adcCount             = 1;
    info.hasDiversityReceiver = false;
    info.hasPureSignal        = false;

    // Firmware: die Maschine sendet „4.3.16.42034", kein Zahlenfeld.
    // firmwareVersion bleibt 0 — eine erfundene Zahl waere schlimmer als
    // keine. Die lesbare Fassung geht als Text mit; sie stammt aus dem
    // RUF, nicht aus dem Befehlskanal: der meldet die API-Fassung
    // („V1.4.0.0"), nicht die der Maschine.
    info.firmwareText = beacon.version;

    return info;
}

}  // namespace FlexDiscoveryWire

// ─── FlexRadioDiscovery ──────────────────────────────────────────────

// Abweichung zu AetherSDR, hergeleitet aus der Messung vom 2026-10-28;
// die Zahlen stehen im Kopf der .h.
int FlexRadioDiscovery::staleTimeoutFor(RadioLinkKind kind)
{
    switch (kind) {
    case RadioLinkKind::Wireless:
        // Broadcast ueber WLAN wird nicht quittiert.
        return kStaleTimeoutWirelessMs;
    case RadioLinkKind::Virtual:
        // Tunnel und Bruecken verwerfen Broadcast genauso gern.
        return kStaleTimeoutWirelessMs;
    case RadioLinkKind::Unknown:
        // Im Zweifel Luft geben, nicht Strenge: siehe die Begruendung
        // an der Deklaration.
        return kStaleTimeoutWirelessMs;
    case RadioLinkKind::Wired:
    case RadioLinkKind::Loopback:
        break;
    }
    return kStaleTimeoutMs;
}

FlexRadioDiscovery::FlexRadioDiscovery(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead,
            this, &FlexRadioDiscovery::onReadyRead);

    // From AetherSDR RadioDiscovery.cpp:49-50 [@d58e2b8a] — gefegt wird
    // doppelt so oft wie die Frist lang ist.
    //
    // Der Takt bleibt an der KURZEN Frist haengen, auch wenn eine
    // Maschine die lange bekommt: er gibt nur die Koernigkeit, mit der
    // eine abgelaufene Frist auffaellt. Am Takt der langen Frist zu
    // fegen hiesse, eine Maschine am Kabel bis zu 20 s zu spaet als
    // verschwunden zu melden.
    m_staleTimer.setInterval(kStaleTimeoutMs / 2);
    connect(&m_staleTimer, &QTimer::timeout,
            this, &FlexRadioDiscovery::onStaleCheck);
}

FlexRadioDiscovery::~FlexRadioDiscovery()
{
    stopListening();
}

bool FlexRadioDiscovery::startListening()
{
    if (m_socket.state() == QAbstractSocket::BoundState) {
        return true;
    }

    // ShareAddress: auf demselben Rechner haengt schon
    // FlexRadioDiscoveryBroadcaster an 4992, und SmartSDR oder ein
    // zweites Longpath koennen es auch. Ohne Teilen scheitert das
    // Binden, sobald irgendetwas davon laeuft.
    const bool bound = m_socket.bind(QHostAddress::AnyIPv4,
                                     FlexDiscoveryWire::kPort,
                                     QUdpSocket::ShareAddress
                                         | QUdpSocket::ReuseAddressHint);
    if (!bound) {
        qCWarning(lcFlexRx) << "bind UDP" << FlexDiscoveryWire::kPort
                            << "fehlgeschlagen:" << m_socket.errorString();
        return false;
    }

    m_staleTimer.start();
    qCInfo(lcFlexRx) << "hoert auf UDP" << FlexDiscoveryWire::kPort;
    return true;
}

void FlexRadioDiscovery::stopListening()
{
    m_staleTimer.stop();
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.close();
    }
}

bool FlexRadioDiscovery::isListening() const
{
    return m_socket.state() == QAbstractSocket::BoundState;
}

void FlexRadioDiscovery::ingestDatagramForTesting(const QByteArray& datagram,
                                                  const QHostAddress& sender)
{
    handleDatagram(datagram, sender);
}

void FlexRadioDiscovery::setOwnAddressesForTesting(const QList<QHostAddress>& addresses)
{
    m_ownAddressesOverride = addresses;
}

void FlexRadioDiscovery::setLinkKindForTesting(RadioLinkKind kind)
{
    m_linkKindOverride = kind;
}

void FlexRadioDiscovery::onReadyRead()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket.receiveDatagram();
        if (datagram.isNull()) {
            continue;
        }
        handleDatagram(datagram.data(), datagram.senderAddress());
    }
}

void FlexRadioDiscovery::handleDatagram(const QByteArray& data,
                                        const QHostAddress& sender)
{
    const FlexBeacon beacon = FlexDiscoveryWire::parseBeacon(data);
    if (!beacon.valid) {
        // Kein Flex-Ruf — anderer Verkehr auf 4992, ODER ein Ruf, den
        // dieser Leser nicht versteht. Von hier aus ist das nicht zu
        // unterscheiden, also wird es nur gezaehlt; die
        // „verloren"-Zeile traegt die Zahl dann hinaus. Siehe die
        // Begruendung an m_nichtGelesen.
        ++m_nichtGelesen;
        return;
    }
    if (beacon.serial.isEmpty()) {
        qCWarning(lcFlexRx) << "Ruf ohne Seriennummer von"
                            << sender.toString() << "— verworfen";
        return;
    }

    // Der eigene Ruf. Longpath ruft auf 4992 selbst aus, damit die PGXL
    // es findet; ohne diese Pruefung stuende es in seiner eigenen
    // Liste (am 2026-10-18 live so gesehen: 172.30.30.110,
    // model=FLEX-6400, nickname=Longpath).
    if (isOwnAddress(sender)) {
        qCDebug(lcFlexRx) << "eigener Ruf von" << sender.toString()
                          << "— uebergangen";
        return;
    }

    const RadioInfo info = FlexDiscoveryWire::toRadioInfo(beacon, sender);
    if (info.macAddress.isEmpty()) {
        return;
    }

    // Welche Frist gilt fuer DIESE Maschine? Gemessen wird an der
    // Adresse, unter der Longpath sie auch anrufen wuerde.
    const RadioLinkKind kind =
        m_linkKindOverride ? *m_linkKindOverride
                           : radioLinkKindFor(info.address,
                                              QNetworkInterface::allInterfaces());

    Gesehen& g = m_lastSeen[info.macAddress];
    g.zuletztMs = QDateTime::currentMSecsSinceEpoch();
    g.fristMs   = staleTimeoutFor(kind);

    // Ein gelesener Ruf hebt den Verdacht gegen den Leser auf.
    m_nichtGelesen = 0;

    upsertRadio(info);
}

bool FlexRadioDiscovery::isOwnAddress(const QHostAddress& sender) const
{
    const QList<QHostAddress> own = m_ownAddressesOverride.isEmpty()
                                        ? QNetworkInterface::allAddresses()
                                        : m_ownAddressesOverride;
    for (const QHostAddress& addr : own) {
        if (addr.isEqual(sender, QHostAddress::TolerantConversion)) {
            return true;
        }
    }
    return false;
}

// From AetherSDR RadioDiscovery.cpp:268-280 [@d58e2b8a] —
// upsertRadio(), auf Longpaths MAC-Schluessel umgestellt (AetherSDR
// fuehrt seine Liste nach Seriennummer).
void FlexRadioDiscovery::upsertRadio(const RadioInfo& info)
{
    for (int i = 0; i < m_radios.size(); ++i) {
        if (m_radios[i].macAddress == info.macAddress) {
            m_radios[i] = info;
            emit radioUpdated(info);
            return;
        }
    }
    m_radios.append(info);
    qCInfo(lcFlexRx) << "gefunden:" << info.name
                     << "auf" << info.address.toString()
                     << "— MAC" << info.macAddress;
    emit radioDiscovered(info);
}

// From AetherSDR RadioDiscovery.cpp:282-298 [@d58e2b8a] — onStaleCheck().
void FlexRadioDiscovery::onStaleCheck()
{
    sweepStale(QDateTime::currentMSecsSinceEpoch());
}

void FlexRadioDiscovery::sweepStale(qint64 now)
{
    QStringList lost;

    for (auto it = m_lastSeen.cbegin(); it != m_lastSeen.cend(); ++it) {
        if (now - it.value().zuletztMs > it.value().fristMs) {
            lost.append(it.key());
        }
    }

    for (const QString& mac : lost) {
        const int frist = m_lastSeen.value(mac).fristMs;
        m_lastSeen.remove(mac);
        m_radios.removeIf([&](const RadioInfo& r) { return r.macAddress == mac; });
        qCInfo(lcFlexRx) << "verloren:" << mac
                         << "— Frist" << frist << "ms,"
                         << m_nichtGelesen
                         << "ungelesene Datagramme auf 4992 seit dem letzten Ruf";
        emit radioLost(mac);
    }
}

}  // namespace Longpath
