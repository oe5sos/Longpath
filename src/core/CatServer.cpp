// =================================================================
// src/core/CatServer.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/CAT/TCPIPcatServer.cs [v2.10.3.15]
// Original licence from the Thetis source is included below.
//
// Der vollstaendige Block steht hier UND in CatServer.h: der Pruefer
// verify-thetis-headers.py geht Datei fuer Datei, und beide stehen in
// THETIS-PROVENANCE.md.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-11 — Reimplemented in C++20/Qt6 for Longpath, AI-assisted
//                 via Anthropic Claude Code. Zweiter Gang der
//                 CAT-Anbindung: der Transport, vier Kanaele,
//                 ereignisgesteuert statt ein Faden je Verbindung.
//   2026-10-13 — Der Zustandsgeber ist jetzt auch der Nachleser, damit
//                 ein Setzen nicht blind quittiert wird.
// =================================================================
//
//=================================================================
// TCPIPcatServer.cs
//=================================================================
// Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "CatServer.h"

#include "LogCategories.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

namespace Longpath {

CatServer::CatServer(QObject* parent)
    : QObject(parent)
{
}

CatServer::~CatServer()
{
    // Jeden Kanal ausdruecklich abbauen. Qt wuerde die Kindobjekte beim
    // Sterben ohnehin loeschen, aber die Reihenfolge waere dann nicht
    // unsere: ein Socket, der im Abbau noch readyRead liefert, landete
    // in `lies()` mit halb abgebauten Mitgliedern. Dieselbe Fehlerform,
    // die in diesem Baum schon viermal zugeschlagen hat (siehe die
    // Destruktoren von PgxlConnection/TgxlConnection).
    for (int k = 0; k < kKanaele; ++k) { beende(k); }
}

void CatServer::setzeZustandsgeber(
    std::function<std::optional<CatProtocol::Zustand>()> geber)
{
    m_geber = std::move(geber);

    // Derselbe Geber ist auch der NACHLESER (2026-10-13): nach einem
    // Setzen fragt die Protokollschicht damit nach, ob der Wunsch
    // angekommen ist, und antwortet sonst mit "?;".
    //
    // Warum das wichtig ist: ohne Nachleser quittiert ein Setzen blind.
    // Die zweite CAT-Fassung dieses Projekts hat es am 2026-10-11 am
    // Draht gemessen -- "RT1 answered silently and the following RT
    // still said RT0": der Haken lief, die Scheibe dahinter fehlte, und
    // ein Contestprogramm glaubte, seine RIT sei an.
    //
    // Meldet der Geber std::nullopt (kein Empfaenger), gilt der letzte
    // bekannte Stand als Nachlese-Ergebnis. Der Befehl selbst wird dann
    // ohnehin schon in verarbeiteFuerTest() abgelehnt, bevor er hierher
    // kommt -- das ist nur die Absicherung fuer den Fall, dass der
    // Empfaenger MITTEN im Befehl wegfaellt.
    if (m_geber) {
        m_protokoll.setzeNachleser([this]() {
            const std::optional<CatProtocol::Zustand> z = m_geber();
            return z ? *z : m_protokoll.zustand();
        });
    } else {
        m_protokoll.setzeNachleser(nullptr);
    }
}

void CatServer::setzeHaken(const CatProtocol::Haken& haken)
{
    m_protokoll.setzeHaken(haken);
}

bool CatServer::starte(int k, quint16 port)
{
    if (k < 0 || k >= kKanaele) { return false; }
    beende(k);

    auto* srv = new QTcpServer(this);
    // Nur die eigene Maschine. CAT kann den Sender tasten (TX;) -- ein
    // Horchen auf allen Schnittstellen hiesse, dass jeder im Netz
    // Longpath senden lassen kann. Wer es ueber das Netz braucht, legt
    // einen Tunnel; das ist eine Entscheidung, die der Betreiber
    // treffen muss, nicht eine Vorgabe.
    if (!srv->listen(QHostAddress::LocalHost, port)) {
        qCWarning(lcConnection).nospace()
            << "CAT-Kanal " << k << ": Port " << port
            << " nicht zu haben - " << srv->errorString();
        delete srv;
        return false;
    }

    m_k[k].server = srv;
    m_k[k].port   = srv->serverPort();
    connect(srv, &QTcpServer::newConnection, this, [this, k]() { nimmVerbindung(k); });

    qCInfo(lcConnection).nospace()
        << "CAT-Kanal " << k << " horcht auf 127.0.0.1:" << m_k[k].port;
    return true;
}

void CatServer::beende(int k)
{
    if (k < 0 || k >= kKanaele) { return; }
    Kanal& ka = m_k[k];
    if (!ka.server) { return; }

    // Erst die Verbindungen loesen, dann den Horcher. Umgekehrt kaeme
    // ein letztes readyRead an, wenn ka.server schon weg ist.
    //
    // Das abort()/deleteLater() darunter ist Ordnung, nicht
    // Notwendigkeit: die Gegenprobe hat beide Zeilen entfernt, und
    // nichts fiel -- QTcpServer ist Elternteil seiner Sockets und
    // nimmt sie beim Sterben mit. Ausdruecklich trennen heisst: der
    // Zeitpunkt gehoert uns, und ein Fremdprogramm merkt das Ende des
    // Kanals sofort statt beim naechsten Schreibversuch.
    const QList<QTcpSocket*> offene = ka.puffer.keys();
    for (QTcpSocket* s : offene) {
        disconnect(s, nullptr, this, nullptr);
        s->abort();
        s->deleteLater();
    }
    ka.puffer.clear();

    disconnect(ka.server, nullptr, this, nullptr);
    ka.server->close();
    ka.server->deleteLater();
    ka.server = nullptr;
    ka.port = 0;
    emit verbindungenGeaendert(k, 0);
}

bool CatServer::laeuft(int k) const
{
    return (k >= 0 && k < kKanaele) && m_k[k].server != nullptr
           && m_k[k].server->isListening();
}

quint16 CatServer::port(int k) const
{
    return (k >= 0 && k < kKanaele) ? m_k[k].port : 0;
}

QHostAddress CatServer::adresse(int k) const
{
    if (k < 0 || k >= kKanaele) { return {}; }
    QTcpServer* srv = m_k[k].server;
    if (!srv || !srv->isListening()) { return {}; }
    return srv->serverAddress();
}

int CatServer::verbindungen(int k) const
{
    return (k >= 0 && k < kKanaele) ? static_cast<int>(m_k[k].puffer.size()) : 0;
}

void CatServer::nimmVerbindung(int k)
{
    Kanal& ka = m_k[k];
    if (!ka.server) { return; }
    while (QTcpSocket* s = ka.server->nextPendingConnection()) {
        ka.puffer.insert(s, QString());
        connect(s, &QTcpSocket::readyRead, this, [this, k, s]() { lies(k, s); });
        connect(s, &QTcpSocket::disconnected, this, [this, k, s]() { raeumeAuf(k, s); });
        qCInfo(lcConnection).nospace()
            << "CAT-Kanal " << k << ": Verbindung von "
            << s->peerAddress().toString() << " (" << ka.puffer.size() << " offen)";
        emit verbindungenGeaendert(k, static_cast<int>(ka.puffer.size()));
    }
}

void CatServer::raeumeAuf(int k, QTcpSocket* s)
{
    Kanal& ka = m_k[k];
    if (ka.puffer.remove(s) == 0) { return; }
    disconnect(s, nullptr, this, nullptr);
    // deleteLater, nicht delete: wir stehen hier im Signal DIESES
    // Sockets. Ein direktes Loeschen zoege den Boden unter dem
    // Aufrufer weg -- genau der Absturz, der im Contestprogramm am
    // 2026-09-30 den SingleInstanceGuard getroffen hat (QLocalSocket
    // in dessen eigenem readyRead geloescht).
    s->deleteLater();
    emit verbindungenGeaendert(k, static_cast<int>(ka.puffer.size()));
}

void CatServer::lies(int k, QTcpSocket* s)
{
    Kanal& ka = m_k[k];
    auto it = ka.puffer.find(s);
    if (it == ka.puffer.end()) { return; }

    const QByteArray roh = s->readAll();
    // ASCII, wie Thetis (Encoding.ASCII.GetString). Ein Byte > 0x7F
    // kommt in CAT nicht vor und wuerde als Ersatzzeichen im Puffer
    // landen; QLatin1 laesst es durch, ohne den Befehl zu verschieben.
    it->append(QString::fromLatin1(roh));
    verarbeitePuffer(k, s);
}

void CatServer::verarbeitePuffer(int k, QTcpSocket* s)
{
    Kanal& ka = m_k[k];
    auto it = ka.puffer.find(s);
    if (it == ka.puffer.end()) { return; }
    QString& puffer = *it;

    // From Thetis CAT/TCPIPcatServer.cs:275-296 [v2.10.3.15] --
    // ParseReceiveBuffer, am 2.10.3.9 (MW0LGE) auf mehrere Befehle in
    // einem Paket gefixt:
    //   m_oneLineBuf.Replace(Environment.NewLine, "");
    //     // so can be used a by raw telnet client
    //   while (lineEndIndex > -1) { msg = ...Substring(0, idx+1); ... }
    //   if (m_oneLineBuf.Length > 255) m_oneLineBuf.Clear();
    //
    // Zeilenumbrueche fliegen raus, DAMIT ein rohes telnet geht -- wer
    // dort "FA;" eintippt, schickt "FA;\r\n", und ohne diese Zeile
    // steckte das \r\n vor dem naechsten Befehl.
    puffer.remove(QLatin1Char('\r'));
    puffer.remove(QLatin1Char('\n'));

    int ende = puffer.indexOf(QLatin1Char(';'));
    while (ende >= 0) {
        const QString befehl = puffer.left(ende + 1);
        puffer.remove(0, ende + 1);

        const QString antwort = verarbeiteFuerTest(befehl);
        if (!antwort.isEmpty() && s->state() == QAbstractSocket::ConnectedState) {
            s->write(antwort.toLatin1());
            // flush() spart eine Runde der Ereignisschleife: write()
            // legt die Bytes nur in den Socketpuffer, hinausgeschrieben
            // werden sie sonst erst beim naechsten Durchlauf. Bei einem
            // Fremdprogramm, das synchron auf die Antwort wartet (hamlib,
            // WSJT-X, fldigi tun das), ist das die Latenz, die es sieht.
            //
            // EHRLICH DAZU: notwendig ist es nicht. Ich habe diese Zeile
            // am 2026-10-11 eingebaut, als acht Prueffaelle rot waren und
            // jede Abfrage leer zurueckkam -- die Ursache lag aber im
            // PRUEFSTAND (waitForReadyRead am Client dreht die
            // Ereignisschleife nicht, also arbeitete der Server nie; siehe
            // die Notiz bei frage() in tst_cat_server.cpp). Die Gegenprobe
            // ohne diese Zeile ist gruen. Sie bleibt, weil sie nichts
            // kostet und die Latenz senkt, nicht weil sie etwas repariert.
            s->flush();
        }
        // Nach dem Schreiben erneut nachsehen: ein Fremdprogramm kann
        // sich auf die Antwort hin trennen, und dann ist der Eintrag
        // (und damit `puffer`) weg.
        //
        // VORSORGE, nicht belegte Notwendigkeit. Die Gegenprobe am
        // 2026-10-11 hat diese drei Zeilen entfernt und zwanzig Runden
        // mit sofortigem abort() nach zwei Befehlen gefahren -- nichts
        // fiel. Qt liefert `disconnected` verzoegert, der Eintrag lebt
        // also waehrend dieser Schleife noch. Die Zeilen bleiben, weil
        // sie nichts kosten und die Annahme "waehrend meiner Schleife
        // aendert sich die Hashtabelle nicht" nicht in diesem Code
        // steht, sondern in Qts Zustellreihenfolge -- und die ist keine
        // Zusicherung, auf die ein Zeiger zeigen sollte.
        it = ka.puffer.find(s);
        if (it == ka.puffer.end()) { return; }
        ende = it->indexOf(QLatin1Char(';'));
    }

    if (it->size() > kPufferDeckel) {
        qCWarning(lcConnection).nospace()
            << "CAT-Kanal " << k << ": " << it->size()
            << " Zeichen ohne Abschluss ';' - Puffer verworfen";
        it->clear();
    }
}

QString CatServer::verarbeiteFuerTest(const QString& befehl)
{
    if (!m_geber) {
        // Kein Zustandsgeber heisst: wir wissen nichts ueber das Geraet.
        // Dann lieber ablehnen als eine Vorgabe melden -- eine
        // erfundene Frequenz ist schlimmer als ein klares "kenne ich
        // nicht".
        return CatProtocol::abgelehnt();
    }
    const std::optional<CatProtocol::Zustand> z = m_geber();
    if (!z) {
        // Der Geber sagt: gerade nichts zu melden (kein Empfaenger
        // angelegt, keine Verbindung). Dieselbe Antwort wie ohne Geber.
        return CatProtocol::abgelehnt();
    }
    m_protokoll.setzeZustand(*z);
    return m_protokoll.antwortAuf(befehl);
}

} // namespace Longpath
