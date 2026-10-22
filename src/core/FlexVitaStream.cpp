// =================================================================
// src/core/FlexVitaStream.cpp  (Longpath)
// =================================================================
//
// Herkunft, Lizenz und das Messprotokoll: siehe FlexVitaStream.h und
// docs/protocols/flex-vita49-gemessen.md.
// Ported from AetherSDR src/core/PanadapterStream.{h,cpp} at d58e2b8a
// (GPLv3, primary author Jeremy [KK7GWY]) — project-level citation per
// docs/attribution/HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
// =================================================================

#include "FlexVitaStream.h"

#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QStringList>

#include <algorithm>

namespace Longpath {

Q_LOGGING_CATEGORY(lcFlexStream, "longpath.flex.strom", QtInfoMsg)

FlexVitaStream::FlexVitaStream(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead,
            this, &FlexVitaStream::onReadyRead);
}

quint16 FlexVitaStream::start(const QHostAddress& radioAddress)
{
    if (m_socket.state() == QAbstractSocket::BoundState) {
        return m_localPort;
    }
    if (radioAddress.isNull()) {
        qCWarning(lcFlexStream) << "keine Geraeteadresse — nichts zu melden";
        return 0;
    }

    // Das Betriebssystem waehlt den Port; welcher, erfaehrt die Maschine
    // gleich darauf aus der Absenderadresse.
    if (!m_socket.bind(QHostAddress::AnyIPv4, 0)) {
        qCWarning(lcFlexStream) << "UDP-Bind gescheitert:" << m_socket.errorString();
        return 0;
    }
    m_radioAddress = radioAddress;
    m_localPort = m_socket.localPort();

    // From AetherSDR PanadapterStream.cpp:261-268 [@d58e2b8a] — ein
    // Null-Byte an die Maschine; sie lernt IP:Port aus der
    // Absenderadresse. Das ist der Weg, der sicher trägt; der
    // TCP-Befehl `client udpport` kommt daneben und wird vom Aufrufer
    // geschickt, weil nur der den Befehlskanal hat.
    const QByteArray anmeldung(1, '\x00');
    const qint64 gesendet =
        m_socket.writeDatagram(anmeldung, m_radioAddress, kRadioPort);
    if (gesendet != 1) {
        qCWarning(lcFlexStream) << "UDP-Anmeldung nicht hinausgegangen:"
                                << m_socket.errorString();
    } else {
        qCInfo(lcFlexStream) << "angemeldet bei" << m_radioAddress.toString()
                             << ":" << kRadioPort
                             << "— hoere auf eigenem Port" << m_localPort;
    }

    // Alle fuenf Sekunden sagen, WAS ankommt. Ein Strom, der schweigt,
    // und ein Strom, der nur Ton bringt, sehen von aussen gleich aus —
    // und genau diese zwei Faelle hat am 2026-10-21 niemand
    // auseinanderhalten koennen, weil hier nichts protokolliert wurde.
    m_proKlasse.clear();
    m_bericht.setInterval(5000);
    m_bericht.callOnTimeout(this, [this] { berichteKlassen(); });
    m_bericht.start();
    return m_localPort;
}

// Eine Zeile je Bericht, und nur wenn sich seit der letzten etwas
// bewegt hat — ein stehender Strom soll das Protokoll nicht fluten.
void FlexVitaStream::berichteKlassen()
{
    if (m_datagrams == m_zuletztGemeldet) {
        return;
    }
    m_zuletztGemeldet = m_datagrams;

    QStringList teile;
    for (auto it = m_proKlasse.cbegin(); it != m_proKlasse.cend(); ++it) {
        const QString name = [k = it.key()] () -> QString {
            switch (static_cast<FlexVitaClass>(k)) {
            case FlexVitaClass::PanadapterFft:  return QStringLiteral("Bild-FFT");
            case FlexVitaClass::WaterfallTile:  return QStringLiteral("Wasserfall");
            case FlexVitaClass::NarrowAudio:    return QStringLiteral("Ton");
            case FlexVitaClass::ReducedBwAudio: return QStringLiteral("Ton schmal");
            case FlexVitaClass::Meter:          return QStringLiteral("Messwerte");
            default: break;
            }
            return k == 0 ? QStringLiteral("unlesbar")
                          : QStringLiteral("0x%1").arg(k, 4, 16, QLatin1Char('0'));
        }();
        teile << QStringLiteral("%1 %2").arg(name).arg(it.value());
    }
    // Die zweite Zeile ist die wichtigere: Pakete kommen an, lange
    // bevor daraus ein Rahmen wird. Steht hier 0, waehrend oben
    // Tausende stehen, dann liegt es am Zusammensetzen, nicht am Netz.
    qCInfo(lcFlexStream) << "Strom:" << m_datagrams << "Pakete —"
                         << qPrintable(teile.join(QStringLiteral(", ")));
    qCInfo(lcFlexStream) << "Strom: daraus fertig —"
                         << m_fftRahmen << "Bildrahmen,"
                         << m_wasserfallZeilen << "Wasserfallzeilen;"
                         << "Maszstab gesetzt fuer" << m_scales.size()
                         << "Strom-Kennung(en)";
}

void FlexVitaStream::stop()
{
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.close();
    }
    m_localPort = 0;
}

bool FlexVitaStream::isRunning() const
{
    return m_socket.state() == QAbstractSocket::BoundState;
}

void FlexVitaStream::setYPixels(quint32 streamId, int yPixels)
{
    if (yPixels <= 1) {
        return;
    }
    m_scales[streamId].yPixels = yPixels;
}

void FlexVitaStream::setDbmRange(quint32 streamId, double minDbm, double maxDbm)
{
    if (maxDbm <= minDbm) {
        return;
    }
    Scale& s = m_scales[streamId];
    s.minDbm = minDbm;
    s.maxDbm = maxDbm;
}

void FlexVitaStream::ingestDatagramForTesting(const QByteArray& datagram)
{
    handleDatagram(datagram);
}

void FlexVitaStream::onReadyRead()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram d = m_socket.receiveDatagram();
        if (d.isNull()) {
            continue;
        }
        handleDatagram(d.data());
    }
}

// From AetherSDR PanadapterStream::processDatagram [@d58e2b8a] — an der
// Paketklasse verteilen, alles Unbekannte still fallen lassen.
void FlexVitaStream::handleDatagram(const QByteArray& datagram)
{
    ++m_datagrams;

    const FlexVita::Header h = FlexVita::parseHeader(datagram);
    if (!h.valid) {
        ++m_ignored;
        ++m_proKlasse[0];   // 0 = unlesbarer Kopf
        return;
    }
    ++m_proKlasse[static_cast<quint16>(h.packetClass)];

    switch (static_cast<FlexVitaClass>(h.packetClass)) {
    case FlexVitaClass::PanadapterFft: {
        const auto rahmen = m_fft.feed(datagram);
        if (!rahmen) {
            return;   // noch nicht vollstaendig, oder nur Wiederholung
        }
        // Der Maszstab kommt von der Stelle, die ihn gesetzt hat — die
        // Maschine meldet y_pixels nicht zurueck (siehe Kopf).
        ++m_fftRahmen;
        // Einmal die ROHWERTE melden. Bis hierher war alles Modell:
        // „ein Bin ist eine Bildzeile 0…y_pixels-1". Stimmt das nicht,
        // nuetzt der richtigste Teiler nichts (2026-10-22: mit der von
        // der Maschine gemeldeten 20 blieb das Bild voll ausgeschlagen).
        if (m_fftRahmen == 1 && !rahmen->bins.isEmpty()) {
            quint16 kleinster = rahmen->bins.first();
            quint16 groesster = rahmen->bins.first();
            double summe = 0.0;
            for (const quint16 b : rahmen->bins) {
                kleinster = std::min(kleinster, b);
                groesster = std::max(groesster, b);
                summe += b;
            }
            qCInfo(lcFlexStream).nospace()
                << "Strom: Rohwerte des ersten Bildrahmens — "
                << rahmen->bins.size() << " Bins, kleinster " << kleinster
                << ", groesster " << groesster << ", Mittel "
                << (summe / rahmen->bins.size());
        }
        const Scale s = m_scales.value(rahmen->streamId);
        emit spectrumReady(rahmen->streamId,
                           FlexVita::binsToDbm(rahmen->bins,
                                               s.minDbm, s.maxDbm, s.yPixels));
        break;
    }

    case FlexVitaClass::WaterfallTile: {
        const auto zeile = m_tiles.feed(datagram);
        if (!zeile) {
            return;
        }
        ++m_wasserfallZeilen;
        emit waterfallRowReady(zeile->streamId, zeile->values,
                               zeile->lowFreqMhz, zeile->highFreqMhz,
                               zeile->autoBlackLevel);
        break;
    }

    case FlexVitaClass::NarrowAudio: {
        const QVector<float> werte = FlexVita::narrowAudioSamples(datagram);
        if (werte.isEmpty()) {
            return;
        }
        emit audioSamplesReady(h.streamId, werte);
        break;
    }

    case FlexVitaClass::Meter: {
        // Seit 2026-10-21 gemessen und damit baubar (§5b des
        // Messblatts). Der Strom liefert nur Index und Rohwert; was
        // ein Index heisst und wie er zu teilen ist, weiss erst der
        // Aufrufer aus den `meter`-Statuszeilen.
        const auto werte = FlexVita::meterSamples(datagram);
        if (!werte.isEmpty()) {
            emit meterSamplesReady(h.streamId, werte);
        }
        break;
    }

    case FlexVitaClass::ReducedBwAudio:
        // Nicht gemessen, also nicht gebaut. Die schmale Tonklasse
        // kommt erst mit `client set send_reduced_bw_dax=1`, und das
        // schickt Longpath nicht; Messwerte hat der Messlauf nie
        // ausgeloest. Abwesenheit ist keine Aussage — hier ist nur
        // nichts darueber belegt, und raten waere schlimmer als
        // fallen lassen.
        ++m_ignored;
        break;

    default:
        ++m_ignored;
        break;
    }
}

}  // namespace Longpath
