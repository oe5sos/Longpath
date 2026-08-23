#pragma once

// =================================================================
// src/core/RadioLinkKind.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Ueber welche Art Strecke haengt das Funkgeraet? ─────────────────
//
// Anlass, 2026-08-23. Der Betreiber sah auf seiner Anvelina zwischen
// 0,17 % und 2,87 % Paketverlust und fragte, woran das liegt. Ich habe
// erst den Empfangspuffer verdaechtigt und dann nachgemessen:
//
//   netstat -s -p udp  ->  "0 dropped due to full socket buffers"
//
// Seit dem Systemstart hat der Kern kein einziges Paket wegen eines
// vollen Puffers verworfen. Die Verluste passieren also VOR der
// Steckdose. Der naechste Blick:
//
//   networksetup -listallhardwareports  ->  en0 = Wi-Fi
//   route -n get 172.30.30.26           ->  interface: en0
//
// Damit ist es erklaert. HPSDR Protokoll 2 schiebt einen
// ununterbrochenen UDP-Strom von rund 9 Mbit/s, und WEDER Protokoll 1
// NOCH 2 senden ein verlorenes Paket nach. WLAN ist ein geteiltes
// Medium mit eigenen Wiederholungen auf der Sicherungsschicht; ein
// Kanalwechsel, ein Nachbar, eine Mikrowelle — und ein Paket ist weg.
// Ein Verlust in dieser Groessenordnung ist dort das Normale, nicht
// das Aussergewoehnliche.
//
// ── Warum das in die Anwendung gehoert ──────────────────────────────
//
// Weil es keine Programmieraufgabe ist und man das SEHEN muss. Ohne
// Hinweis sucht der Betreiber den Fehler im Programm — so, wie ich es
// eine Stunde lang getan habe. Ein Wort an der Verlustanzeige
// beantwortet die Frage, bevor sie gestellt wird.
//
// Die Erkennung steht hier fuer sich, damit sie sich OHNE Funkgeraet
// pruefen laesst: sie bekommt eine Adresse und eine Liste von
// Schnittstellen und liefert eine Art. Kein Netzzugriff, keine
// Abhaengigkeit vom Betriebszustand.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-23 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QHostAddress>
#include <QList>
#include <QNetworkInterface>
#include <QString>

namespace Longpath {

enum class RadioLinkKind {
    Unknown,     ///< Keine passende Schnittstelle gefunden.
    Wired,       ///< Kabel — der Normalfall fuer ein SDR.
    Wireless,    ///< WLAN — hier ist Paketverlust erwartbar.
    Loopback,    ///< Auf demselben Rechner (Emulator, Aufzeichnung).
    Virtual,     ///< VPN, Tunnel, virtuelle Bruecke.
};

/// Welche Schnittstelle traegt diese Adresse? Gesucht wird die, deren
/// Netz die Adresse enthaelt — NICHT die erste aktive. Auf einem
/// Rechner mit Kabel UND WLAN im selben Haus waere sonst die Antwort
/// eine Muenzwurf-Frage.
inline RadioLinkKind radioLinkKindFor(const QHostAddress& radio,
                                      const QList<QNetworkInterface>& ifaces)
{
    if (radio.isNull()) { return RadioLinkKind::Unknown; }
    if (radio.isLoopback()) { return RadioLinkKind::Loopback; }

    for (const QNetworkInterface& iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) { continue; }
        if (!(iface.flags() & QNetworkInterface::IsRunning)) { continue; }

        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            const QHostAddress& ip = entry.ip();
            if (ip.protocol() != radio.protocol()) { continue; }
            const int prefix = entry.prefixLength();
            if (prefix < 0) { continue; }
            if (!radio.isInSubnet(ip, prefix)) { continue; }

            switch (iface.type()) {
            // Wifi und Ieee80211 sind in Qt6 derselbe Wert (8);
            // beide zu nennen ist ein Uebersetzungsfehler.
            case QNetworkInterface::Wifi:
                return RadioLinkKind::Wireless;
            case QNetworkInterface::Loopback:
                return RadioLinkKind::Loopback;
            case QNetworkInterface::Virtual:
            case QNetworkInterface::Ppp:
                return RadioLinkKind::Virtual;
            case QNetworkInterface::Ethernet:
                return RadioLinkKind::Wired;
            default:
                // Thunderbolt-Bruecken, USB-Adapter und aehnliches
                // meldet Qt als Unknown. Sie sind praktisch immer
                // Kabel — und "Kabel" ist die Antwort, die KEINE
                // Warnung ausloest. Im Zweifel lieber schweigen als
                // jemandem ein WLAN andichten, das er nicht hat.
                return RadioLinkKind::Wired;
            }
        }
    }
    return RadioLinkKind::Unknown;
}

/// Ein Satz fuer den Betreiber, oder leer, wenn es nichts zu sagen gibt.
inline QString radioLinkWarning(RadioLinkKind kind)
{
    switch (kind) {
    case RadioLinkKind::Wireless:
        return QStringLiteral(
            "Das Funkgerät hängt über WLAN. Weder Protokoll 1 noch 2 "
            "senden verlorene Pakete nach — jedes fehlende ist ein Loch "
            "im Ton. Ein Kabel behebt das.");
    case RadioLinkKind::Virtual:
        return QStringLiteral(
            "Das Funkgerät hängt über einen Tunnel oder ein VPN. "
            "Paketverlust ist dort erwartbar und wird nicht nachgesendet.");
    case RadioLinkKind::Wired:
    case RadioLinkKind::Loopback:
    case RadioLinkKind::Unknown:
        break;
    }
    return QString();
}

} // namespace Longpath
