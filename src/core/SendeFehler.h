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
#include <QtGlobal>

#include <cstddef>

#if !defined(Q_OS_WIN)
#  include <cstring>
#endif

#if defined(Q_OS_WIN)
// Winsock fuehrt EIGENE Nummern (WSAE*, 10000er-Block) und setzt `errno`
// nicht. Wer dort errno liest, bekommt den Rest irgendeines frueheren
// Aufrufs — eine Deutung davon waere schlimmer als keine.
#  include <winsock2.h>
#else
#  include <cerrno>
#endif

namespace Longpath {
namespace Netz {

// ── Die Nummern, je System ─────────────────────────────────────────────────
//
// Getrennt gefuehrt statt per #ifdef im switch: so gibt es jede Bedeutung
// genau einmal, und ein Name, den ein System nicht kennt (EHOSTDOWN fehlt
// unter MinGW — daran scheiterte der Windows-Bau am 2026-10-04), faellt
// hier auf und nicht erst im Lauf.
#if defined(Q_OS_WIN)
constexpr int kKeinWeg[]        = { WSAEHOSTUNREACH, WSAENETUNREACH, WSAEHOSTDOWN };
constexpr int kVerweigert[]     = { WSAEACCES };
constexpr int kWarteschlange[]  = { WSAENOBUFS, WSAEWOULDBLOCK };
constexpr int kZuGross[]        = { WSAEMSGSIZE };
constexpr int kEigenerFehler[]  = { WSAENOTSOCK, WSAEAFNOSUPPORT, WSAEINVAL };
#else
constexpr int kKeinWeg[]        = { EHOSTUNREACH, ENETUNREACH, EHOSTDOWN };
constexpr int kVerweigert[]     = { EPERM, EACCES };
constexpr int kWarteschlange[]  = { ENOBUFS, EAGAIN };
constexpr int kZuGross[]        = { EMSGSIZE };
constexpr int kEigenerFehler[]  = { EBADF, ENOTSOCK, EAFNOSUPPORT, EINVAL };
#endif

template <std::size_t N>
constexpr bool istDarin(int nr, const int (&liste)[N])
{
    for (std::size_t i = 0; i < N; ++i) {
        if (liste[i] == nr) { return true; }
    }
    return false;
}

/** Die Fehlernummer des zuletzt gescheiterten Netzaufrufs.
 *
 *  SOFORT nach dem fehlgeschlagenen Aufruf lesen: jeder weitere — auch
 *  `errorString()` — darf sie ueberschreiben.
 */
inline int letzterFehler()
{
#if defined(Q_OS_WIN)
    return WSAGetLastError();
#else
    return errno;
#endif
}

/** Der Name der Fehlernummer, lesbar.
 *
 *  `strerror()` kennt die WSAE*-Nummern nicht und liefert dort "Unknown
 *  error" — eine Meldung, die den Grund verschweigt, war genau das Problem,
 *  gegen das diese Datei gebaut ist.
 */
inline QString fehlerName(int nr)
{
#if defined(Q_OS_WIN)
    switch (nr) {
    case WSAEHOSTUNREACH:  return QStringLiteral("WSAEHOSTUNREACH");
    case WSAENETUNREACH:   return QStringLiteral("WSAENETUNREACH");
    case WSAEHOSTDOWN:     return QStringLiteral("WSAEHOSTDOWN");
    case WSAEACCES:        return QStringLiteral("WSAEACCES");
    case WSAENOBUFS:       return QStringLiteral("WSAENOBUFS");
    case WSAEWOULDBLOCK:   return QStringLiteral("WSAEWOULDBLOCK");
    case WSAEMSGSIZE:      return QStringLiteral("WSAEMSGSIZE");
    case WSAENOTSOCK:      return QStringLiteral("WSAENOTSOCK");
    case WSAEAFNOSUPPORT:  return QStringLiteral("WSAEAFNOSUPPORT");
    case WSAEINVAL:        return QStringLiteral("WSAEINVAL");
    case WSAENETDOWN:      return QStringLiteral("WSAENETDOWN");
    default: break;
    }
    return QStringLiteral("WSA-Fehler");
#else
    return QString::fromLatin1(std::strerror(nr));
#endif
}

inline QString sendeFehlerDeutung(int nr)
{
    if (istDarin(nr, kKeinWeg)) {
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
        return QStringLiteral(
            "Es gibt gerade keinen Weg zu dieser Adresse — der Kernel weist "
            "das Paket ab, es geht gar nicht erst hinaus. Meist ein "
            "abgelaufener Routen- oder ARP-Eintrag, eine gewechselte "
            "Schnittstelle oder eine abgeschaltete Gegenstelle. Ein ping "
            "auf dieselbe Adresse trennt die Faelle.");
    }
    if (istDarin(nr, kVerweigert)) {
        // Hier und NUR hier ist es wirklich eine Verweigerung: das
        // Betriebssystem sagt nicht "ich weiss keinen Weg", sondern "du
        // darfst nicht".
        return QStringLiteral(
            "Das Betriebssystem verweigert diesem Programm den Zugriff aufs "
            "Netz — eine Berechtigung oder ein Filter, nicht die Strecke.");
    }
    if (istDarin(nr, kWarteschlange)) {
        return QStringLiteral(
            "Die Sendewarteschlange der Schnittstelle ist voll — das ist die "
            "Funkstrecke (WLAN) und geht meist von selbst vorbei.");
    }
    if (istDarin(nr, kZuGross)) {
        return QStringLiteral("Das Datagramm ist groesser als die Strecke erlaubt (MTU).");
    }
    if (istDarin(nr, kEigenerFehler)) {
        return QStringLiteral(
            "Der Socket ist nicht mehr gueltig oder passt nicht zur Adresse "
            "(IPv4 ueber einen IPv6-only-Socket?) — ein Fehler in Longpath "
            "selbst, nicht im Netz.");
    }
    // Fuer alles Unbekannte bleibt es leer; dann steht in der Meldung nur die
    // Nummer samt strerror(). Eine erfundene Erklaerung waere schlimmer.
    return QString();
}

}  // namespace Netz
}  // namespace Longpath
