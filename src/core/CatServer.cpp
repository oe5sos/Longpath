// =================================================================
// src/core/CatServer.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/CAT/TCPIPcatServer.cs
//   [v2.10.3.15-5-g852bf0e] — structural reference only (one TCP
//   listener, several clients, commands separated by ';'); rebuilt on
//   QTcpServer rather than translated line by line. Original licence
//   from the Thetis source is included below.
//
// The CAT protocol itself lives in CatParser; this class only moves
// bytes.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-11 — Written in C++20/Qt6 for Longpath against the upstream
//                shape, with AI-assisted transformation via Anthropic
//                Claude Code. The ';'-split buffering and the 256-byte
//                guard against an unterminated command are Longpath's
//                own.
// =================================================================

/*  TCPIPcatServer.cs

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2012  Bob Tracy

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

You may contact the author via email at: k5kdn@arrl.net
*/
// =================================================================

#include "CatServer.h"

#include "LogCategories.h"

#include <QtNetwork/QTcpSocket>

namespace Longpath {

CatServer::CatServer(QObject* radio, QObject* parent)
    : QObject(parent)
    , m_radio(radio)
    , m_parser(radio)
{
    connect(&m_server, &QTcpServer::newConnection,
            this, &CatServer::onNewConnection);
}

CatServer::~CatServer()
{
    stop();
}

bool CatServer::start(const QHostAddress& address, quint16 port)
{
    if (m_server.isListening()) {
        stop();
    }
    if (!m_server.listen(address, port)) {
        qCWarning(lcCat) << "CatServer: cannot listen on" << address.toString()
                         << port << "—" << m_server.errorString();
        return false;
    }
    qCInfo(lcCat) << "CatServer: listening on" << address.toString()
                  << m_server.serverPort();
    emit listeningChanged(true);
    return true;
}

void CatServer::stop()
{
    const bool wasListening = m_server.isListening();

    for (QTcpSocket* c : std::as_const(m_clients)) {
        c->disconnect(this);
        c->abort();
        c->deleteLater();
    }
    m_clients.clear();
    m_buffers.clear();
    m_server.close();
    m_parser.setPttAllowed(false);

    if (wasListening) {
        qCInfo(lcCat) << "CatServer: stopped";
        emit listeningChanged(false);
        emit clientCountChanged(0);
    }
}

bool    CatServer::isListening() const { return m_server.isListening(); }
quint16 CatServer::port() const        { return m_server.serverPort(); }
int     CatServer::clientCount() const { return int(m_clients.size()); }
bool    CatServer::pttAllowed() const  { return m_parser.pttAllowed(); }

void CatServer::setPttAllowed(bool on)
{
    if (m_parser.pttAllowed() == on) {
        return;
    }
    m_parser.setPttAllowed(on);
    qCInfo(lcCat) << "CatServer: TX over CAT is now"
                  << (on ? "ALLOWED" : "refused");
}

QString CatServer::handleCommand(const QString& command)
{
    const QString reply = m_parser.handleCommand(command);
    if (m_parser.takePttRefused()) {
        emit txRefused();
    }
    emit commandLogged(command, reply);
    return reply;
}

void CatServer::onNewConnection()
{
    while (QTcpSocket* sock = m_server.nextPendingConnection()) {
        m_clients.append(sock);
        m_buffers.insert(sock, QString());
        connect(sock, &QTcpSocket::readyRead,
                this, &CatServer::onClientReadyRead);
        connect(sock, &QTcpSocket::disconnected,
                this, &CatServer::onClientDisconnected);
        qCInfo(lcCat) << "CatServer: client from" << sock->peerAddress().toString();
        emit clientCountChanged(clientCount());
    }
}

// CAT messages end in ';' and arrive glued together, so read everything
// available, cut at every ';', and keep the tail for the next packet.
void CatServer::onClientReadyRead()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) {
        return;
    }
    QString& buffer = m_buffers[sock];
    buffer += QString::fromLatin1(sock->readAll());

    int cut = buffer.indexOf(QLatin1Char(';'));
    while (cut >= 0) {
        const QString command = buffer.left(cut).trimmed();
        buffer.remove(0, cut + 1);
        if (!command.isEmpty()) {
            const QString reply = handleCommand(command);
            if (!reply.isEmpty()) {
                sock->write(reply.toLatin1());
            }
        }
        cut = buffer.indexOf(QLatin1Char(';'));
    }
    // A client that never terminates a command must not grow our buffer
    // without end. 256 bytes is far past the longest CAT command there
    // is (ZZ-prefixed setters top out well below that).
    if (buffer.size() > 256) {
        qCWarning(lcCat) << "CatServer: dropping" << buffer.size()
                         << "bytes of unterminated command";
        buffer.clear();
    }
    sock->flush();
}

void CatServer::onClientDisconnected()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) {
        return;
    }
    m_clients.removeAll(sock);
    m_buffers.remove(sock);
    sock->deleteLater();
    qCInfo(lcCat) << "CatServer: client gone," << clientCount() << "left";
    emit clientCountChanged(clientCount());
}

}  // namespace Longpath
