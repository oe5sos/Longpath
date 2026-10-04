#pragma once

// Was ein gescheiterter Versand WIRKLICH bedeutet — in einem Satz, den man
// befolgen kann.
//
// Qt meldet JEDEN gescheiterten write()-Syscall als "Unable to send a
// message", und error() sagt nur QAbstractSocket::NetworkError. Das ist
// dieselbe Zeile fuer eine volle Sendewarteschlange, eine fehlende Route und
// eine vom Betriebssystem verweigerte Berechtigung — also fuer drei Faelle
// mit drei voellig verschiedenen Abhilfen.
//
// Dreimal wurde genau an dieser Stelle gesucht (2026-08-27, 2026-09-01,
// 2026-10-04) und dreimal konnte die Zeile den Grund nicht nennen. Am
// 2026-10-04 stand dann im Dialog "fast immer die Netzwerkstrecke, nicht
// das Geraet" — und das war falsch: es ging kein einziges Datagramm hinaus
// (ping zum selben Geraet lief, und derselbe Versand aus einem anderen
// Programm ging anstandslos). Ein Hinweis, der in die falsche Richtung
// zeigt, ist schlimmer als keiner.
//
// `errno` steht nach dem fehlgeschlagenen Aufruf noch und sagt es genau. Er
// muss SOFORT gelesen werden: jeder weitere Aufruf — auch errorString() —
// darf ihn ueberschreiben.

#include <QString>

#include <cerrno>

namespace Longpath {
namespace Netz {

inline QString sendeFehlerDeutung(int nr)
{
    switch (nr) {
    case EHOSTUNREACH:
    case ENETUNREACH:
    case EHOSTDOWN:
        // AUSDRUECKLICH NICHT "Berechtigung". Der erste Entwurf dieser Zeile
        // riet hier zu den Systemeinstellungen, und das waere derselbe
        // Fehler gewesen wie der Dialog, den sie ersetzt: ein Rat in die
        // falsche Richtung.
        //
        // Beleg aus demselben Protokoll (2026-10-04, 11:08:25): rotctld —
        // ein eigenes Programm von Homebrew, eigene Identitaet, anderes
        // Netz (192.168.16.x statt 172.30.30.x) — scheiterte in derselben
        // Minute mit "Network error 65: No route to host". Zwei Programme,
        // zwei Teilnetze, ein Fehlerbild. Eine Berechtigung, die an der
        // Identitaet der App haengt, kann das nicht erklaeren; ein
        // Netzzustand schon.
        //
        // EHOSTUNREACH heisst wortwoertlich: der Kernel weiss gerade keinen
        // Weg dorthin. Abgelaufener Routen- oder ARP-Eintrag, gewechselte
        // Schnittstelle, Gegenstelle aus. Was davon, sagt ein ping.
        return QStringLiteral(
            "Es gibt gerade keinen Weg zu dieser Adresse — der Kernel weist "
            "das Paket ab, es geht gar nicht erst hinaus. Meist ein "
            "abgelaufener Routen- oder ARP-Eintrag, eine gewechselte "
            "Schnittstelle oder eine abgeschaltete Gegenstelle. Ein ping "
            "auf dieselbe Adresse trennt die Faelle.");
    case EPERM:
    case EACCES:
        // Hier und NUR hier ist es wirklich eine Verweigerung: das
        // Betriebssystem sagt nicht "ich weiss keinen Weg", sondern "du
        // darfst nicht".
        return QStringLiteral(
            "Das Betriebssystem verweigert diesem Programm den Zugriff aufs "
            "Netz — eine Berechtigung oder ein Filter, nicht die Strecke.");
    case ENOBUFS:
    case EAGAIN:
        return QStringLiteral(
            "Die Sendewarteschlange der Schnittstelle ist voll — das ist die "
            "Funkstrecke (WLAN) und geht meist von selbst vorbei.");
    case EMSGSIZE:
        return QStringLiteral("Das Datagramm ist groesser als die Strecke erlaubt (MTU).");
    case EBADF:
    case ENOTSOCK:
        return QStringLiteral(
            "Der Socket ist nicht mehr gueltig — ein Fehler in Longpath "
            "selbst, nicht im Netz.");
    case EAFNOSUPPORT:
    case EINVAL:
        return QStringLiteral(
            "Adressfamilie passt nicht zum Socket (IPv4 ueber einen "
            "IPv6-only-Socket?) — ein Fehler in Longpath selbst.");
    default:
        return QString();
    }
}

}  // namespace Netz
}  // namespace Longpath
