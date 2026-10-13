// =================================================================
// src/core/CatPtyServer.h  (Longpath)
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

#pragma once

#include "CatPuffer.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class QSocketNotifier;

namespace Longpath {

/// CAT ueber ein Pseudoterminal: vier Geraete, die sich wie serielle
/// Schnittstellen verhalten.
///
/// WOZU, wenn es den TCP-Weg schon gibt: viele Programme koennen CAT
/// nur ueber eine serielle Schnittstelle. hamlib kann beides, aber
/// fldigi fragt nach einem Geraetepfad, Logbuecher und aeltere
/// Contestprogramme ebenso, und wer einen virtuellen COM-Port von
/// Windows gewohnt ist, sucht genau das. Ein Pseudoterminal ist auf
/// macOS und Linux die Entsprechung: Longpath haelt die Steuerseite,
/// das Fremdprogramm oeffnet den Geraetepfad und sieht eine serielle
/// Schnittstelle, die es nicht von einem USB-Kabel unterscheiden kann.
///
/// EIGENE PROTOKOLLSCHICHT GIBT ES HIER NICHT. Beantwortet wird ueber
/// `setzeAntwortgeber`, und der zeigt auf dieselbe CatServer-Instanz,
/// die auch der TCP-Weg fragt -- ein Zustandsgeber, ein Satz Haken,
/// eine AI-Flagge. Thetis macht es umgekehrt (jeder Horcher baut sich
/// `new CATParser(console)`); das geht dort auf, weil der Zustand in
/// genau einem `console`-Objekt liegt. Longpath hat kein solches
/// Allobjekt.
class CatPtyServer : public QObject {
    Q_OBJECT

public:
    /// Vier Kanaele, wie das Applet sie zeigt und wie Thetis sie hat
    /// (SIOListenerII, SIO2ListenerII, SIO3ListenerII, SIO4ListenerII).
    static constexpr int kKanaele = 4;

    explicit CatPtyServer(QObject* parent = nullptr);
    ~CatPtyServer() override;

    /// Wer die Befehle beantwortet. Ohne ihn wird NICHTS gesendet --
    /// nicht einmal `?;`. Ein Kanal ohne Antwortgeber ist ein Fehler in
    /// der Verdrahtung, und ein Fremdprogramm, das darauf `?;` bekommt,
    /// haelt den Befehl fuer nicht unterstuetzt und fragt nie wieder.
    /// Schweigen laesst es in den Zeitablauf laufen, und DAS sieht man.
    void setzeAntwortgeber(std::function<QString(const QString&)> geber);

    /// Kanal `k` (0..3) anlegen. Der Geraetepfad steht danach in
    /// `pfad(k)`. Gibt false, wenn kein Pseudoterminal zu haben ist --
    /// der Aufrufer muss `grund(k)` anzeigen, nicht verschweigen.
    bool starte(int k);
    void beende(int k);
    bool laeuft(int k) const;

    /// Der Geraetepfad, den ein Fremdprogramm oeffnet -- von
    /// `ptsname()`, also z.B. `/dev/ttys012` (macOS) oder `/dev/pts/7`
    /// (Linux). Leer, wenn der Kanal aus ist.
    ///
    /// Der Pfad ist NICHT vorhersagbar und beim naechsten Start ein
    /// anderer; deshalb legt `starte()` zusaetzlich eine feste
    /// Verknuepfung (siehe `festerPfad`).
    QString pfad(int k) const;

    /// Die feste Verknuepfung auf `pfad(k)`:
    /// `<Datenordner>/cat/A` .. `/D`. Die bleibt ueber Neustarts
    /// gleich, und nur sie kann man in WSJT-X oder fldigi dauerhaft
    /// eintragen -- `ptsname()` gibt jedes Mal eine andere Nummer.
    ///
    /// Das ist die Entsprechung zu Thetis, wo der Betreiber einen
    /// festen COM-Port waehlt (`console.CATPort`, SIOListenerII.cs:74)
    /// und der ueber Neustarts derselbe bleibt. Ohne sie muesste man
    /// das Fremdprogramm nach jedem Start von Longpath neu einstellen,
    /// und die Anbindung waere auf dem Papier da und in der Praxis
    /// unbenutzbar.
    static QString festerPfad(int k);

    /// Warum `starte(k)` nicht geklappt hat. Leer, wenn er laeuft.
    QString grund(int k) const;

    /// Gibt es Pseudoterminals auf dieser Plattform? Auf Windows nein
    /// (dort braeuchte es ein Treiberpaar wie com0com, und das kann
    /// Longpath nicht mitbringen) -- dann bleibt der Schalter im Applet
    /// gesperrt, statt sich druecken zu lassen und nichts zu tun.
    static bool verfuegbar();

signals:
    /// Ein Kanal ist dazugekommen oder weggefallen: die Pfade im Applet
    /// stimmen nicht mehr.
    void pfadeGeaendert();
    /// Ein laufender Kanal ist von selbst ausgefallen.
    void kanalAusgefallen(int kanal, QString grund);

private:
    void lies(int k);
    void schreibe(int k, const QString& antwort);
    /// Legt `festerPfad(k)` auf `ziel`. Scheitert still (mit Protokoll):
    /// ein Kanal, der laeuft, soll nicht an einer Verknuepfung
    /// scheitern, die bloss die Bedienung bequemer macht.
    static void setzeVerknuepfung(int k, const QString& ziel);

    struct Kanal {
        /// Die Steuerseite (`posix_openpt`). Longpath liest und
        /// schreibt hier; -1 heisst: Kanal aus.
        int steuerFd{-1};
        /// Die Geraeteseite, die das Fremdprogramm oeffnet. Longpath
        /// haelt sie OFFEN, ohne sie je zu lesen -- aus zwei Gruenden,
        /// die beide beim ersten Versuch zugeschlagen haben:
        ///
        /// 1. Ohne einen offenen Geraete-Dateigriff liefert ein `read`
        ///    auf der Steuerseite sofort EIO (Linux) bzw. 0 (macOS),
        ///    sobald sich das Fremdprogramm trennt. Der QSocketNotifier
        ///    bleibt dabei dauerhaft bereit und dreht eine Endlosrunde.
        /// 2. Die Leitungseinstellungen (`termios`) gehoeren dem Paar.
        ///    Wer die Geraeteseite schliesst, verliert sie beim naechsten
        ///    Oeffnen -- und damit den Rohmodus, ohne den das Echo
        ///    zurueckkommt (siehe `starte()`).
        int geraetFd{-1};
        QString pfad;
        QString grund;
        QSocketNotifier* wecker{nullptr};
        CatPuffer puffer;
    };
    Kanal m_k[kKanaele];

    std::function<QString(const QString&)> m_antwort;
};

} // namespace Longpath
