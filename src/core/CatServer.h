// =================================================================
// src/core/CatServer.h  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/CAT/TCPIPcatServer.cs [v2.10.3.15]
// Original licence from the Thetis source is included below.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-11 — Reimplemented in C++20/Qt6 for Longpath, AI-assisted
//                 via Anthropic Claude Code. Zweiter Gang der
//                 CAT-Anbindung: der Transport. Die Protokollschicht
//                 (CatProtocol) ist der erste, die Verdrahtung an
//                 RadioModel und das CatApplet folgen.
//                 Thetis faehrt einen Faden je Verbindung mit
//                 50-ms-Schlaf und Available-Abfrage; hier
//                 ereignisgesteuert ueber QTcpServer/QTcpSocket auf
//                 dem Hauptfaden -- die Antworten lesen Modellzustand,
//                 und der gehoert diesem Faden.
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

#pragma once

#include "CatProtocol.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

#include <QHostAddress>

class QTcpServer;
class QTcpSocket;

namespace Longpath {

/// Der CAT-Server: nimmt Kenwood-Befehle ueber TCP entgegen und
/// antwortet mit dem, was CatProtocol sagt.
///
/// Vier Kanaele (A..D), jeder mit eigenem Port und eigenem Schalter --
/// so wie das CatApplet es seit der Skelettphase zeigt und wie Thetis
/// es hat. Mehrere Kanaele, damit WSJT-X, fldigi und ein Logbuch nicht
/// um einen Port streiten.
///
/// Der Zustand kommt bei jedem Befehl frisch aus einem Rueckruf
/// (`setzeZustandsgeber`), nicht aus einer Kopie: ein Server, der eine
/// veraltete Frequenz meldet, laesst ein Fremdprogramm auf der falschen
/// senden. Genau derselbe Grund, aus dem die Protokollschicht ihren
/// Zustand nicht selbst haelt.
class CatServer : public QObject {
    Q_OBJECT

public:
    /// Vier Kanaele, wie das Applet sie zeigt (A B C D).
    static constexpr int kKanaele = 4;

    explicit CatServer(QObject* parent = nullptr);
    ~CatServer() override;

    /// Der Zustandsgeber wird VOR jedem Befehl gefragt.
    ///
    /// Er gibt `std::nullopt`, wenn es gerade nichts zu melden gibt --
    /// kein Empfaenger angelegt, keine Verbindung zum Geraet. Dann
    /// antwortet der Server mit `?;`, und zwar aus demselben Grund, aus
    /// dem er es ohne Geber tut: eine gemeldete 0 Hz ist eine erfundene
    /// Frequenz, und ein Fremdprogramm glaubt sie.
    ///
    /// Am 2026-10-11 im Prueffall am echten Fenster gefunden: ein frisch
    /// gebautes MainWindow hat noch keinen aktiven Slice, und der erste
    /// Entwurf dieses Gebers meldete dafuer brav "FA00000000000;".
    void setzeZustandsgeber(std::function<std::optional<CatProtocol::Zustand>()> geber);
    /// Die Haken gehen unveraendert an die Protokollschicht.
    void setzeHaken(const CatProtocol::Haken& haken);

    /// Kanal `k` (0..3) auf `port` horchen lassen. Gibt false, wenn der
    /// Port nicht zu haben ist (belegt, oder unter 1024 ohne Rechte) --
    /// der Aufrufer muss das anzeigen, nicht verschweigen.
    bool starte(int k, quint16 port);
    void beende(int k);
    bool laeuft(int k) const;
    quint16 port(int k) const;

    /// Die Adresse, an der Kanal `k` wirklich haengt.
    ///
    /// Nicht Bequemlichkeit, sondern die EINZIGE direkte Messung der
    /// sicherheitsrelevanten Eigenschaft dieser Datei: CAT kann den
    /// Sender tasten, also darf der Server nur auf der eigenen Maschine
    /// horchen. Der Prueffall hat das bis zum 2026-10-14 MITTELBAR
    /// gemessen -- er band eine Sonde auf 0.0.0.0 denselben Port und
    /// schloss aus dem Erfolg auf eine lokale Bindung. Das gilt auf
    /// macOS und **nicht auf Linux**: dort verbietet der Kern eine
    /// Platzhalterbindung auf einen Port, der schon lokal gebunden ist,
    /// also schlug die Sonde fehl und der Fall war rot, obwohl der Code
    /// stimmte (CI Linux x64 2/4, PR #222).
    ///
    /// Gibt eine ungueltige Adresse, wenn der Kanal nicht horcht.
    QHostAddress adresse(int k) const;

    /// Wie viele Fremdprogramme an diesem Kanal haengen.
    int verbindungen(int k) const;

    /// Fuer Prueffaelle: einen Befehl so verarbeiten, wie er aus dem
    /// Netz kaeme, ohne Netz. Gibt die Antwort.
    QString verarbeiteFuerTest(const QString& befehl);

signals:
    /// Ein Fremdprogramm hat sich verbunden oder getrennt (Kanal 0..3).
    void verbindungenGeaendert(int kanal, int anzahl);
    /// Ein Kanal ist von selbst ausgefallen (Port verloren).
    void kanalAusgefallen(int kanal, QString grund);

private:
    void nimmVerbindung(int k);
    void lies(int k, QTcpSocket* s);
    void raeumeAuf(int k, QTcpSocket* s);
    /// Zerlegt den Strom in Befehle. From Thetis
    /// CAT/TCPIPcatServer.cs:275-296 [v2.10.3.15] -- ParseReceiveBuffer.
    void verarbeitePuffer(int k, QTcpSocket* s);

    struct Kanal {
        QTcpServer* server{nullptr};
        quint16     port{0};
        QHash<QTcpSocket*, QString> puffer;   // je Verbindung ein Puffer
    };
    Kanal m_k[kKanaele];

    CatProtocol m_protokoll;
    std::function<std::optional<CatProtocol::Zustand>()> m_geber;

    /// From Thetis CAT/TCPIPcatServer.cs:295 [v2.10.3.15]:
    ///   if (m_oneLineBuf.Length > 255) m_oneLineBuf.Clear();
    ///   // not likely to be a cat message if it got this long
    /// Ohne diesen Deckel haelt ein Programm, das nie ein ';' schickt,
    /// den Puffer offen, bis der Speicher ausgeht.
    static constexpr int kPufferDeckel = 255;
};

} // namespace Longpath
