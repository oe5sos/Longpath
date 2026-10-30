#pragma once

// =================================================================
// tests/PortProbe.h  (Longpath)
// =================================================================
//
// „Lauscht auf diesem Port schon jemand?" — und zwar so, dass die
// Antwort stimmt.
//
// Drei Prüfstände hingen an dieser Frage und stellten sie alle auf
// dieselbe Weise: einen TCP-Connect nach 127.0.0.1 aufbauen und
// schauen, ob er gelingt. Am 2026-10-30 log genau das auf dem
// macOS-Läufer der CI (Lauf 41020438997, Scherbe 1/2) — drei
// Programme auf einen Streich:
//
//   tst_whisper_server_launcher        3 Fälle, alle „Port belegt"
//   tst_asr_against_live_server        sah einen Server, der Anruf
//                                      lief dann in „Connection refused"
//   tst_tci_client_against_live_device  dasselbe auf 40001
//
// Der Connect meldete also Erfolg, wo nichts lauschte. Die ctest-Läufe
// auf dem Mac laufen SERIELL (ci.yml: `ctest --output-on-failure`, kein
// -j), es kann also kein zweiter Prüfstand gewesen sein, und ein
// Ephemeral-Port, der Mikrosekunden nach dem Schließen schon wieder
// belegt ist, ist keine Erklärung. Welche Eigenheit des Läufers es war,
// bleibt offen — die Frage lässt sich aber ohne ihn beantworten.
//
// **Binden statt verbinden.** Wer einen Port binden kann, hat bewiesen,
// dass dort niemand lauscht; wer es nicht kann, ebenso das Gegenteil.
// Das ist eine örtliche Entscheidung im Kern, ohne Paket auf der
// Leitung, ohne Zeitgrenze und ohne Netz — und damit auch verträglich
// mit netzdichten Prüfläufen (PR #339).
//
// Nebenbei: SO_REUSEADDR (das Qt beim Lauschen setzt) erlaubt KEIN
// zweites Binden auf einen aktiv lauschenden Port, auch nicht, wenn der
// andere mit SO_REUSEPORT gebunden hat. Das Verfahren erkennt also auch
// genau den Fall, um den es bei whisper-server geht.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original für Longpath von Martin Fischer,
//                 KI-gestützt über Anthropic Claude (Cowork).
// =================================================================

#include <QHostAddress>
#include <QTcpServer>
#include <QCoreApplication>

namespace LongpathTest {

/// Lauscht auf 127.0.0.1:port jemand? Entschieden durch einen
/// Bindeversuch, nicht durch einen Verbindungsversuch.
inline bool portHasListener(quint16 port)
{
    QTcpServer probe;
    if (probe.listen(QHostAddress::LocalHost, port)) {
        probe.close();
        return false;
    }
    return true;
}

/// Ein Port, auf dem gerade niemand lauscht — und auf den auch kein
/// zweiter Prüflauf gleichzeitig greift: der Bereich kommt aus der
/// Prozessnummer, jeder Kandidat wird gebunden, bevor er genannt wird.
///
/// Auf dem Rechner des Betreibers steht ein Login-Dienst auf 8080,
/// darum nie fest 8080.
inline quint16 freeLocalPort()
{
    const quint16 basis =
        quint16(20000 + (QCoreApplication::applicationPid() % 20000));
    for (quint16 i = 0; i < 400; ++i) {
        const int kandidat = int(basis) + int(i);
        if (kandidat > 65535) { break; }
        QTcpServer s;
        if (s.listen(QHostAddress::LocalHost, quint16(kandidat))) {
            s.close();
            return quint16(kandidat);
        }
    }
    // Als letzte Rettung die Vergabe des Systems.
    QTcpServer s;
    if (!s.listen(QHostAddress::LocalHost, 0)) { return 0; }
    const quint16 p = s.serverPort();
    s.close();
    return p;
}

} // namespace LongpathTest
