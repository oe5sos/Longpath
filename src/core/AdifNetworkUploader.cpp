// =================================================================
// src/core/AdifNetworkUploader.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — see AdifNetworkUploader.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "AdifNetworkUploader.h"

#include <QHostAddress>
#include <QTcpSocket>
#include <QUdpSocket>

namespace NereusSDR {

AdifNetworkUploader::AdifNetworkUploader(QObject* parent)
    : QsoUploader(parent) {}

void AdifNetworkUploader::setTarget(const QString& host, quint16 port,
                                    Transport t)
{
    m_host = host.trimmed();
    m_port = port;
    m_transport = t;
}

QString AdifNetworkUploader::serviceName() const
{
    if (!isConfigured()) { return QStringLiteral("Local logger"); }
    return QStringLiteral("Local logger (%1 %2:%3)")
        .arg(m_transport == Transport::Udp ? QStringLiteral("UDP")
                                           : QStringLiteral("TCP"))
        .arg(m_host).arg(m_port);
}

void AdifNetworkUploader::upload(const LogEntry& entry)
{
    const QString call = entry.call;
    if (!isConfigured()) {
        emit uploadFinished(call, false, false,
            QStringLiteral("No local logger address set"));
        return;
    }

    // The record, terminated by a newline. Listeners split on <EOR>, but
    // several also read line-wise, and a trailing newline costs nothing.
    const QByteArray payload =
        (entry.toAdifRecord() + QLatin1Char('\n')).toUtf8();

    if (m_transport == Transport::Udp) {
        QUdpSocket sock;
        const qint64 sent =
            sock.writeDatagram(payload, QHostAddress(m_host), m_port);
        if (sent < 0) {
            emit uploadFinished(call, false, false, sock.errorString());
            return;
        }
        // Deliberately worded as "sent", not "logged". UDP has no
        // acknowledgement, so this is the strongest true statement
        // available — claiming more would teach the operator to trust a
        // result that means nothing.
        emit uploadFinished(call, true, false,
            QStringLiteral("sent by UDP (no confirmation possible)"));
        return;
    }

    // TCP: blocking, but bounded. A logger on the same desk answers at
    // once, and a short clear failure beats a long stall mid-contact.
    QTcpSocket sock;
    sock.connectToHost(m_host, m_port);
    if (!sock.waitForConnected(kTcpTimeoutMs)) {
        emit uploadFinished(call, false, false,
            QStringLiteral("couldn't reach %1:%2 — %3")
                .arg(m_host).arg(m_port).arg(sock.errorString()));
        return;
    }
    sock.write(payload);
    if (!sock.waitForBytesWritten(kTcpTimeoutMs)) {
        emit uploadFinished(call, false, false,
            QStringLiteral("connected but the logger didn't take it: %1")
                .arg(sock.errorString()));
        return;
    }
    sock.disconnectFromHost();
    emit uploadFinished(call, true, false,
        QStringLiteral("delivered by TCP"));
}

} // namespace NereusSDR
