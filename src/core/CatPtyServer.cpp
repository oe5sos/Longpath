// =================================================================
// src/core/CatPtyServer.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis sources (BEHAVIOURAL MODEL ONLY -- see the
// modification history below: Thetis' serial CAT is Windows-COM
// centric, so the form is carried over, not the code):
//   Project Files/Source/Console/CAT/SIOListenerII.cs   [v2.10.3.15]
//   Project Files/Source/Console/CAT/SDRSerialPortII.cs [v2.10.3.15]
// Original licences from the Thetis sources are included below.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-12 — Neu in C++20/Qt6 fuer Longpath, AI-assisted via
//                 Anthropic Claude Code. Vierter Gang der CAT-Anbindung:
//                 die serielle Haelfte. Protokoll (CatProtocol),
//                 Transport ueber TCP (CatServer) und Verdrahtung
//                 stehen seit 2026-10-11.
//
//                 KEIN Port, sondern ein Verhaltensvorbild -- und das
//                 ist eine Aussage ueber die Quelle, nicht eine
//                 Bequemlichkeit: Thetis' serielle CAT-Kanaele sind
//                 Windows-COM-Ports (`commPort.PortName = "COM" +
//                 portidx`, SDRSerialPortII.cs:89) und setzen auf
//                 System.IO.Ports. Auf macOS und Linux gibt es keinen
//                 COM-Port, den man belegen koennte; die Entsprechung
//                 ist ein Pseudoterminal, das Longpath selbst anlegt
//                 und dessen Geraetepfad das Fremdprogramm oeffnet.
//
//                 Uebernommen ist darum die FORM, nicht der Code:
//                   * vier getrennte Kanaele (Thetis hat SIOListenerII
//                     bis SIO4ListenerII, vier mal dieselbe Klasse)
//                   * je Kanal ein eigener Puffer, Zerlegung am ';'
//                     (SIOListenerII.cs:199-248 ParseString, dort mit
//                     `ParseLeftover` fuer den halben Befehl)
//                   * Antwort zurueck auf denselben Kanal
//                     (SIO.put(answer), SIOListenerII.cs:302)
//                   * 8N1, 9600 Bd als Ausgangslage
//                     (SDRSerialPortII.cs:92-96)
//                   * Handshake aus (SDRSerialPortII.cs:91,
//                     //[2.10.3.9]MW0LGE)
//
//                 Die Zerlegung selbst steht nicht hier, sondern in
//                 CatPuffer -- gemeinsam mit dem TCP-Weg.
// =================================================================
//
//=================================================================
// SIOListener.cs
//=================================================================
// Copyright (C) 2005  Bob Tracy
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
// You may contact the author via email at: k5kdn@arrl.net
//=================================================================
//
//=================================================================
// SDRSerialPort.cs
//=================================================================
// Copyright (C) 2005  Bill Tracey
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
//=================================================================
// Serial port support for PowerSDR support of CAT and serial port control
//=================================================================

// =================================================================
// Der Kopf spiegelt CatPtyServer.h -- dort steht auch, warum dies
// ein Verhaltensvorbild ist und kein Port.
// =================================================================

#include "CatPtyServer.h"

#include "AppSettings.h"
#include "LogCategories.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSocketNotifier>

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace Longpath {

namespace {

/// Die Buchstaben der vier Kanaele, wie das Applet sie zeigt.
QChar kanalName(int k)
{
    return QChar(QLatin1Char('A' + k));
}

} // namespace

bool CatPtyServer::verfuegbar()
{
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    return true;
#else
    // Windows kennt kein Pseudoterminal. Die Entsprechung waere ein
    // Treiberpaar (com0com, VSPE), das einen echten virtuellen COM-Port
    // anlegt -- ein Kerneltreiber, den Longpath nicht mitbringen kann
    // und nicht mitbringen sollte. Dort ist der TCP-Weg der Weg; hamlib
    // und WSJT-X koennen ihn.
    return false;
#endif
}

QString CatPtyServer::festerPfad(int k)
{
    if (k < 0 || k >= kKanaele) { return {}; }
    // Unter dem DATENordner, nicht unter dem Einstellungsordner: eine
    // Sandbox-Instanz (LONGPATH_CONFIG_DIR) darf die Verknuepfungen des
    // Betreibers nicht ueberschreiben, sonst zeigt sein WSJT-X auf das
    // Pseudoterminal eines Prueflaufs.
    return AppSettings::dataDir() + QStringLiteral("/cat/")
           + QString(kanalName(k));
}

CatPtyServer::CatPtyServer(QObject* parent)
    : QObject(parent)
{
}

CatPtyServer::~CatPtyServer()
{
    // Jeden Kanal ausdruecklich abbauen, und zwar als ERSTES: der
    // QSocketNotifier ist ein Kindobjekt, Qt wuerde ihn beim Sterben
    // loeschen -- aber in welcher Reihenfolge gegenueber den
    // Dateigriffen, steht nirgends zugesichert. Ein Wecker, der nach
    // dem close() noch einmal feuert, landete in lies() mit steuerFd
    // == -1 oder, schlimmer, mit einer Nummer, die das System
    // inzwischen neu vergeben hat.
    //
    // Dieselbe Fehlerform hat in diesem Baum schon FUENF Mal
    // zugeschlagen (PgxlConnection, TgxlConnection, CatServer,
    // ~MainWindow, und der Rotor-Abbau am 2026-10-11). Sie kostet hier
    // eine Zeile.
    for (int k = 0; k < kKanaele; ++k) { beende(k); }
}

void CatPtyServer::setzeAntwortgeber(std::function<QString(const QString&)> geber)
{
    m_antwort = std::move(geber);
}

bool CatPtyServer::laeuft(int k) const
{
    return (k >= 0 && k < kKanaele) && m_k[k].steuerFd >= 0;
}

QString CatPtyServer::pfad(int k) const
{
    return (k >= 0 && k < kKanaele) ? m_k[k].pfad : QString();
}

QString CatPtyServer::grund(int k) const
{
    return (k >= 0 && k < kKanaele) ? m_k[k].grund : QString();
}

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)

bool CatPtyServer::starte(int k)
{
    if (k < 0 || k >= kKanaele) { return false; }
    beende(k);
    Kanal& ka = m_k[k];
    ka.grund.clear();

    // --- 1. Die Steuerseite anlegen ---------------------------------
    // O_NOCTTY: dieses Pseudoterminal soll NICHT das steuernde Terminal
    // von Longpath werden. Ohne das kann ein Ctrl-C, das jemand in das
    // Geraet tippt, ein SIGINT an Longpath schicken -- das Programm
    // wuerde sich beenden, weil ein Fremdprogramm am CAT-Kanal eine
    // Taste gedrueckt hat.
    const int steuer = ::posix_openpt(O_RDWR | O_NOCTTY);
    if (steuer < 0) {
        ka.grund = QStringLiteral("posix_openpt: %1")
                       .arg(QString::fromLocal8Bit(::strerror(errno)));
        qCWarning(lcConnection).nospace()
            << "CAT-PTY " << kanalName(k) << ": " << ka.grund;
        return false;
    }

    if (::grantpt(steuer) != 0) {
        ka.grund = QStringLiteral("grantpt: %1")
                       .arg(QString::fromLocal8Bit(::strerror(errno)));
        ::close(steuer);
        qCWarning(lcConnection).nospace()
            << "CAT-PTY " << kanalName(k) << ": " << ka.grund;
        return false;
    }
    if (::unlockpt(steuer) != 0) {
        ka.grund = QStringLiteral("unlockpt: %1")
                       .arg(QString::fromLocal8Bit(::strerror(errno)));
        ::close(steuer);
        qCWarning(lcConnection).nospace()
            << "CAT-PTY " << kanalName(k) << ": " << ka.grund;
        return false;
    }

    // --- 2. Den Geraetepfad erfragen --------------------------------
    // ptsname() gibt einen Zeiger auf einen STATISCHEN Puffer, den der
    // naechste Aufruf ueberschreibt. Darum sofort kopieren -- sonst
    // tragen alle vier Kanaele am Ende den Pfad des letzten.
    // (ptsname_r() waere sauberer, gibt es aber auf macOS erst seit
    // 10.13.4 und nicht in jedem SDK; hier laeuft ohnehin alles auf dem
    // Hauptfaden, und der Wert wird in derselben Zeile kopiert.)
    const char* roh = ::ptsname(steuer);
    if (!roh) {
        ka.grund = QStringLiteral("ptsname: %1")
                       .arg(QString::fromLocal8Bit(::strerror(errno)));
        ::close(steuer);
        qCWarning(lcConnection).nospace()
            << "CAT-PTY " << kanalName(k) << ": " << ka.grund;
        return false;
    }
    const QString pfad = QString::fromLocal8Bit(roh);

    // --- 3. Die Geraeteseite oeffnen und offen HALTEN ---------------
    // Begruendung steht am Mitglied `geraetFd` in CatPtyServer.h: ohne
    // einen offenen Geraetegriff dreht der Wecker nach dem Trennen des
    // Fremdprogramms eine Endlosrunde, und die Leitungseinstellungen
    // waeren beim naechsten Oeffnen wieder die Vorgabe.
    const int geraet = ::open(pfad.toLocal8Bit().constData(),
                              O_RDWR | O_NOCTTY);
    if (geraet < 0) {
        ka.grund = QStringLiteral("open(%1): %2")
                       .arg(pfad, QString::fromLocal8Bit(::strerror(errno)));
        ::close(steuer);
        qCWarning(lcConnection).nospace()
            << "CAT-PTY " << kanalName(k) << ": " << ka.grund;
        return false;
    }

    // --- 4. Rohmodus. DAS ist die Stelle, die man nicht auslassen darf.
    //
    // Ein frisches Pseudoterminal kommt mit der Vorgabe einer
    // Textkonsole: ECHO an, ICANON an, OPOST an. Alle drei sind hier
    // falsch, und das erste ist bosartig:
    //
    //   ECHO  -- die Leitungsdisziplin spiegelt alles, was auf der
    //            Steuerseite GESCHRIEBEN wird, auf der Steuerseite
    //            wieder als lesbar zurueck. Longpath liest also seine
    //            EIGENEN Antworten. "FA00014225000;" kaeme als naechster
    //            Befehl herein, wuerde als Setzen von 14.225.000 Hz
    //            gedeutet -- und weil die Antwort darauf wieder
    //            gespiegelt wird, laeuft das Ding im Kreis.
    //   ICANON -- die Geraeteseite bekaeme Zeilen erst beim \n. CAT
    //            schliesst mit ';' ab und schickt kein \n; jede Antwort
    //            blieb hangen, bis zufaellig ein Umbruch kam.
    //   OPOST  -- \n wird zu \r\n aufgeblasen (ONLCR). Fuer Text
    //            richtig, fuer einen Befehlsstrom eine Verschiebung.
    //
    // cfmakeraw() loescht alle drei in einem Griff (POSIX).
    //
    // Gesetzt wird auf der GERAETEseite: dort sitzt die
    // Leitungsdisziplin des Paares, und ein Fremdprogramm, das selbst
    // tcsetattr ruft, aendert dieselbe. Wir setzen die Ausgangslage und
    // reden ihm danach nicht hinein.
    struct termios t{};
    if (::tcgetattr(geraet, &t) == 0) {
        ::cfmakeraw(&t);
        // 8N1 ohne Handshake, wie Thetis es als Ausgangslage setzt:
        // From Thetis CAT/SDRSerialPortII.cs:91-96 [v2.10.3.15]
        //   commPort.Handshake = Handshake.None; //[2.10.3.9]MW0LGE
        //   commPort.Parity = Parity.None;
        //   commPort.StopBits = StopBits.One;
        //   commPort.DataBits = 8;
        //   commPort.BaudRate = 9600;
        //
        // Am Pseudoterminal ist die Baudrate ohne Wirkung -- es gibt
        // keine Leitung, die Bits taktet, und die Bytes gehen so schnell
        // durch, wie beide Seiten lesen. Sie wird trotzdem gesetzt,
        // damit ein Fremdprogramm, das sie ABFRAGT (hamlib tut das),
        // einen plausiblen Wert sieht und nicht 0.
        t.c_cflag &= ~static_cast<tcflag_t>(PARENB | CSTOPB | CRTSCTS);
        t.c_cflag |= static_cast<tcflag_t>(CS8 | CREAD | CLOCAL);
        ::cfsetispeed(&t, B9600);
        ::cfsetospeed(&t, B9600);
        if (::tcsetattr(geraet, TCSANOW, &t) != 0) {
            qCWarning(lcConnection).nospace()
                << "CAT-PTY " << kanalName(k)
                << ": tcsetattr fehlgeschlagen ("
                << QString::fromLocal8Bit(::strerror(errno))
                << ") - das Echo kann zurueckkommen";
        }
    } else {
        qCWarning(lcConnection).nospace()
            << "CAT-PTY " << kanalName(k) << ": tcgetattr fehlgeschlagen ("
            << QString::fromLocal8Bit(::strerror(errno))
            << ") - das Echo kann zurueckkommen";
    }

    // --- 5. Nicht blockierend lesen und schreiben -------------------
    // Ohne O_NONBLOCK kann ein Fremdprogramm, das aufhoert zu lesen,
    // Longpath im write() festhalten -- den Hauptfaden, also die ganze
    // Oberflaeche samt Panadapter.
    const int flaggen = ::fcntl(steuer, F_GETFL, 0);
    if (flaggen < 0 || ::fcntl(steuer, F_SETFL, flaggen | O_NONBLOCK) < 0) {
        ka.grund = QStringLiteral("fcntl O_NONBLOCK: %1")
                       .arg(QString::fromLocal8Bit(::strerror(errno)));
        ::close(geraet);
        ::close(steuer);
        qCWarning(lcConnection).nospace()
            << "CAT-PTY " << kanalName(k) << ": " << ka.grund;
        return false;
    }

    ka.steuerFd = steuer;
    ka.geraetFd = geraet;
    ka.pfad     = pfad;
    ka.puffer.leere();

    ka.wecker = new QSocketNotifier(steuer, QSocketNotifier::Read, this);
    connect(ka.wecker, &QSocketNotifier::activated, this, [this, k]() { lies(k); });

    setzeVerknuepfung(k, pfad);

    qCInfo(lcConnection).nospace()
        << "CAT-PTY " << kanalName(k) << ": " << pfad
        << " (feste Verknuepfung: " << festerPfad(k) << ")";
    emit pfadeGeaendert();
    return true;
}

void CatPtyServer::beende(int k)
{
    if (k < 0 || k >= kKanaele) { return; }
    Kanal& ka = m_k[k];
    if (ka.steuerFd < 0 && !ka.wecker) { return; }

    // Erst den Wecker still legen, dann die Dateigriffe schliessen. Die
    // andere Reihenfolge laesst ein letztes activated() in lies()
    // laufen, wenn steuerFd schon zu ist -- und eine geschlossene
    // Nummer kann das System in derselben Runde neu vergeben haben.
    //
    // EHRLICH DAZU: belegt NOETIG ist diese Reihenfolge nicht. Die
    // Gegenprobe am 2026-10-12 hat setEnabled(false) und das disconnect
    // entfernt und drei Laeufe gefahren -- alle 24 Faelle gruen, auch
    // der mit offener Verbindung. Gefangen wird es von der ersten Zeile
    // in lies() (`if (ka.steuerFd < 0) { return; }`), und Qt nimmt den
    // Wecker beim Schliessen des Dateigriffs offenbar selbst aus dem
    // Rennen.
    //
    // Es bleibt trotzdem, aus demselben Grund wie das flush() im
    // TCP-Weg: es kostet nichts, und die Annahme "ein Wecker auf einem
    // geschlossenen Dateigriff feuert nicht mehr" steht nicht in diesem
    // Code, sondern in Qts Innenleben. Die Fehlerform
    // "Abbau greift in tote Mitglieder" hat in diesem Baum fuenf Mal
    // zugeschlagen, und JEDES Mal war die Begruendung, dass es bisher
    // gutgegangen ist.
    if (ka.wecker) {
        ka.wecker->setEnabled(false);
        disconnect(ka.wecker, nullptr, this, nullptr);
        // deleteLater, nicht delete: wir koennen aus activated() dieses
        // Weckers kommen (ein Kanal, der von selbst ausfaellt, beendet
        // sich in lies()). Ein direktes Loeschen zoege dem Aufrufer den
        // Boden weg -- derselbe Absturz, der im Contestprogramm am
        // 2026-09-30 den SingleInstanceGuard getroffen hat.
        ka.wecker->deleteLater();
        ka.wecker = nullptr;
    }
    if (ka.geraetFd >= 0) { ::close(ka.geraetFd); ka.geraetFd = -1; }
    if (ka.steuerFd >= 0) { ::close(ka.steuerFd); ka.steuerFd = -1; }

    // Die Verknuepfung mit abraeumen: sie zeigte auf ein
    // Pseudoterminal, das es nicht mehr gibt. Stehenlassen hiesse, dass
    // ein Fremdprogramm sie oeffnet, keinen Fehler bekommt (die
    // Nummer ist inzwischen an jemand anderen vergeben) und mit dem
    // falschen Geraet redet.
    const QString fest = festerPfad(k);
    if (!fest.isEmpty() && QFileInfo(fest).isSymLink()) {
        QFile::remove(fest);
    }

    ka.pfad.clear();
    ka.puffer.leere();
    emit pfadeGeaendert();
}

void CatPtyServer::lies(int k)
{
    Kanal& ka = m_k[k];
    if (ka.steuerFd < 0) { return; }

    char roh[1024];
    QString angekommen;
    for (;;) {
        const ssize_t n = ::read(ka.steuerFd, roh, sizeof(roh));
        if (n > 0) {
            // ASCII, wie Thetis (ASCIIEncoding). Ein Byte > 0x7F kommt
            // in CAT nicht vor und wuerde als Ersatzzeichen im Puffer
            // landen; QLatin1 laesst es durch, ohne den Befehl zu
            // verschieben.
            angekommen.append(QString::fromLatin1(roh, static_cast<int>(n)));
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;                      // nichts mehr da, der Normalfall
        }
        if (n < 0 && errno == EINTR) {
            continue;                   // Signal dazwischen, nochmal
        }
        // n == 0 oder ein echter Fehler. Beides sollte nicht
        // vorkommen, solange wir die Geraeteseite offen halten (siehe
        // starte()). Wenn es doch passiert, den Kanal abbauen statt den
        // Wecker endlos feuern zu lassen: ein bereiter Dateigriff, der
        // nichts liefert, ist eine Endlosrunde auf dem Hauptfaden.
        const QString grund = (n == 0)
            ? QStringLiteral("Geraeteseite geschlossen")
            : QStringLiteral("read: %1")
                  .arg(QString::fromLocal8Bit(::strerror(errno)));
        qCWarning(lcConnection).nospace()
            << "CAT-PTY " << kanalName(k) << " ausgefallen: " << grund;
        beende(k);
        m_k[k].grund = grund;
        emit kanalAusgefallen(k, grund);
        return;
    }

    if (angekommen.isEmpty()) { return; }

    const QStringList befehle = ka.puffer.nimm(angekommen);
    if (ka.puffer.verworfen()) {
        qCWarning(lcConnection).nospace()
            << "CAT-PTY " << kanalName(k) << ": mehr als "
            << CatPuffer::kDeckel
            << " Zeichen ohne Abschluss ';' - Puffer verworfen";
    }

    for (const QString& befehl : befehle) {
        if (!m_antwort) {
            // Siehe setzeAntwortgeber(): SCHWEIGEN, nicht "?;". Ein
            // Fremdprogramm, dem man einen Befehl ablehnt, haelt ihn
            // fuer nicht unterstuetzt und fragt nie wieder; ein
            // Zeitablauf dagegen faellt auf und fuehrt zur Ursache.
            qCWarning(lcConnection).nospace()
                << "CAT-PTY " << kanalName(k)
                << ": Befehl ohne Antwortgeber verworfen (" << befehl << ")";
            continue;
        }
        // Nach jedem Befehl nachsehen, ob der Kanal noch steht: ein
        // Befehl kann etwas ausloesen, das ihn abbaut (der Antwortgeber
        // greift in das Modell).
        if (m_k[k].steuerFd < 0) { return; }
        const QString antwort = m_antwort(befehl);
        if (!antwort.isEmpty()) { schreibe(k, antwort); }
    }
}

void CatPtyServer::schreibe(int k, const QString& antwort)
{
    Kanal& ka = m_k[k];
    if (ka.steuerFd < 0) { return; }

    const QByteArray roh = antwort.toLatin1();
    qsizetype ab = 0;
    while (ab < roh.size()) {
        const ssize_t n = ::write(ka.steuerFd, roh.constData() + ab,
                                  static_cast<size_t>(roh.size() - ab));
        if (n > 0) { ab += n; continue; }
        if (n < 0 && errno == EINTR) { continue; }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // Die Eingangsschlange der Geraeteseite ist voll: das
            // Fremdprogramm fragt und liest nicht mehr ab. Die Antwort
            // wird VERWORFEN, nicht gepuffert -- sie traegt den Zustand
            // von jetzt, und wenn sie den Leser erst in zehn Sekunden
            // erreicht, meldet sie eine Frequenz, auf der nichts mehr
            // steht. Ein Zustandssatz hat eine Haltbarkeit.
            qCWarning(lcConnection).nospace()
                << "CAT-PTY " << kanalName(k)
                << ": Geraeteseite liest nicht ab, Antwort verworfen ("
                << antwort.trimmed() << ")";
            return;
        }
        qCWarning(lcConnection).nospace()
            << "CAT-PTY " << kanalName(k) << ": write: "
            << QString::fromLocal8Bit(::strerror(errno));
        return;
    }
}

void CatPtyServer::setzeVerknuepfung(int k, const QString& ziel)
{
    const QString fest = festerPfad(k);
    if (fest.isEmpty()) { return; }

    QDir().mkpath(QFileInfo(fest).absolutePath());
    // Eine alte Verknuepfung muss weg, bevor symlink() geht --
    // symlink() legt nie ueber etwas Bestehendes. Nur wenn es
    // tatsaechlich eine Verknuepfung ist: falls dort eine ECHTE Datei
    // liegt (jemand hat sie hingelegt, oder ein frueherer Fehlgriff),
    // wird sie nicht geloescht. Longpath raeumt seine eigenen Spuren
    // weg, nicht fremde Dateien.
    const QFileInfo vorher(fest);
    if (vorher.isSymLink()) {
        QFile::remove(fest);
    } else if (vorher.exists()) {
        qCWarning(lcConnection).nospace()
            << "CAT-PTY " << kanalName(k) << ": " << fest
            << " ist keine Verknuepfung - bleibt unberuehrt, der Pfad "
            << ziel << " muss von Hand eingetragen werden";
        return;
    }

    if (::symlink(ziel.toLocal8Bit().constData(),
                  fest.toLocal8Bit().constData()) != 0) {
        // Still scheitern, aber im Protokoll: der Kanal LAEUFT, die
        // Verknuepfung macht nur die Bedienung bequemer. Daran soll
        // nichts hangen.
        qCWarning(lcConnection).nospace()
            << "CAT-PTY " << kanalName(k) << ": symlink " << fest << " -> "
            << ziel << " fehlgeschlagen ("
            << QString::fromLocal8Bit(::strerror(errno)) << ")";
    }
}

#else // ── Windows ──────────────────────────────────────────────────────

bool CatPtyServer::starte(int k)
{
    if (k < 0 || k >= kKanaele) { return false; }
    m_k[k].grund = QStringLiteral(
        "Pseudoterminals gibt es auf Windows nicht. Fuer einen "
        "virtuellen COM-Port braucht es ein Treiberpaar (com0com); "
        "WSJT-X, fldigi und hamlib koennen stattdessen den TCP-Weg.");
    qCWarning(lcConnection).nospace()
        << "CAT-PTY " << kanalName(k) << ": " << m_k[k].grund;
    return false;
}

void CatPtyServer::beende(int) {}
void CatPtyServer::lies(int) {}
void CatPtyServer::schreibe(int, const QString&) {}
void CatPtyServer::setzeVerknuepfung(int, const QString&) {}

#endif

} // namespace Longpath
