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
    return m_localPort;
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
        return;
    }

    switch (static_cast<FlexVitaClass>(h.packetClass)) {
    case FlexVitaClass::PanadapterFft: {
        const auto rahmen = m_fft.feed(datagram);
        if (!rahmen) {
            return;   // noch nicht vollstaendig, oder nur Wiederholung
        }
        // Der Maszstab kommt von der Stelle, die ihn gesetzt hat — die
        // Maschine meldet y_pixels nicht zurueck (siehe Kopf).
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

    case FlexVitaClass::ReducedBwAudio:
    case FlexVitaClass::Meter:
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
