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
    case EPERM:
    case EACCES:
        // Auf macOS 15+ ist das der Normalfall fuer eine App ohne die
        // Berechtigung "Lokales Netzwerk": das Geraet antwortet auf ping,
        // aber aus DIESEM Programm geht nichts hinaus. Die Berechtigung
        // haengt an der Identitaet der App — bei einer ad-hoc signierten
        // Fassung also am Hash, und der aendert sich mit jedem Neubau.
        return QStringLiteral(
            "Das Betriebssystem laesst dieses Programm nicht ins lokale Netz. "
            "Auf macOS: Systemeinstellungen > Datenschutz & Sicherheit > "
            "Lokales Netzwerk — steht Longpath dort, und ist der Schalter an? "
            "Nach einem Neubau kann die Berechtigung verfallen sein.");
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
