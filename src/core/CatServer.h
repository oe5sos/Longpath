// =================================================================
// src/core/CatServer.h  (Longpath)
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

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpServer>

#include "CatParser.h"

class QTcpSocket;

namespace Longpath {

// CatServer — Kenwood-style CAT over TCP.
//
// Unlike a newline protocol, CAT messages end in ';' and arrive glued
// together ("FA;IF;MD;"), so the per-client buffer splits on ';' and
// keeps the remainder.
class CatServer : public QObject {
    Q_OBJECT

public:
    explicit CatServer(QObject* radio, QObject* parent = nullptr);
    ~CatServer() override;

    bool start(const QHostAddress& address, quint16 port);
    void stop();

    bool    isListening() const;
    quint16 port() const;
    int     clientCount() const;

    /// Transmit gate. Off by default, off again after stop(), and `RX`
    /// (unkey) is allowed either way -- same contract as the rigctld
    /// bridge, so the two cannot disagree about what is safe.
    void setPttAllowed(bool on);
    bool pttAllowed() const;

    /// Protocol seam for the tests and for TCI's run_cat_ex: one command
    /// in, the answer out (empty when the command is a silent setter).
    QString handleCommand(const QString& command);

signals:
    void listeningChanged(bool listening);
    void clientCountChanged(int count);
    void commandLogged(const QString& command, const QString& reply);
    void txRefused();

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    QObject*   m_radio{nullptr};
    CatParser  m_parser;
    QTcpServer m_server;
    QList<QTcpSocket*> m_clients;
    QHash<QTcpSocket*, QString> m_buffers;   // partial command per client
};

}  // namespace Longpath
