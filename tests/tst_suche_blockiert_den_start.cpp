// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_suche_blockiert_den_start.cpp  (Longpath)
// =================================================================
// Die Geraetesuche darf die Oberflaeche nicht anhalten.
//
// ── Wie das gefunden wurde ──────────────────────────────────────────
//
// Am 2026-10-16 fielen im vollen Durchgang 18 Laeufe mit
// „qWaitForWindowExposed returned FALSE" um — ein MainWindow brauchte
// rund 5 s, bis es auf dem Schirm stand. tst_settings_are_remembered
// lief 84,5 s bei nur 9,6 s Rechenzeit: der Lauf WARTETE 75 s.
//
// `sample` am laufenden Prozess zeigte, worauf, 682 von 683 Mustern
// im selben Aufruf:
//
//     qWaitForWindowExposed
//       MainWindow::showConnectionPanel()
//         ConnectionPanel::ConnectionPanel()
//           RadioDiscovery::startDiscovery()
//             RadioDiscovery::scanAllNics()
//               RadioDiscovery::quietPollAttempt()
//                 QAbstractSocket::waitForReadyRead()
//                   poll()
//
// scanAllNics() laeuft ohne Faden, direkt in startDiscovery(), und
// wartet je Netzkarte attemptsPerNic × quietPollsBeforeResend ×
// pollTimeoutMs. Auf SafeDefault (3 × 5 × 150 ms) sind das 1800 ms je
// Karte. Dieser Mac hat zwei aktive (en0 am Kabel, en14 mit einer
// 169.254-Adresse, an der nichts haengt) — 3,6 s, in denen die
// Oberflaeche steht.
//
// Der Kommentar in stopDiscovery() benannte die Zahl schon, aber nur
// fuer das BEENDEN: „blocks shutdown for ~4.5 s on a 2-NIC machine".
// Dass derselbe Aufruf beim AUFBAUEN des Fensters genauso blockiert,
// stand nirgends.
//
// ── Was diese Lehre festhaelt ───────────────────────────────────────
//
// Nicht „die Suche ist schnell" — sie darf lange suchen. Sondern:
// startDiscovery() gibt die Ereignisschleife frei, statt sie
// anzuhalten. Gemessen wird darum die Zeit, die der AUFRUF braucht,
// nicht die, die die Suche braucht.
//
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QElapsedTimer>
#include <QNetworkInterface>
#include <QSignalSpy>

#include "core/RadioDiscovery.h"

using namespace Longpath;

namespace {

// Dieselbe Bedingung wie in scanAllNics(): oben, laufend, kein
// Loopback, mindestens eine IPv4-Adresse. Die Zahl entscheidet die
// Erwartung, darum wird sie gemessen und nicht angenommen.
int kandidatenKarten()
{
    int n = 0;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp)
            || !(iface.flags() & QNetworkInterface::IsRunning)
            || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const QNetworkAddressEntry& e : iface.addressEntries()) {
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol) { ++n; break; }
        }
    }
    return n;
}

} // namespace

class TstSucheBlockiertDenStart : public QObject
{
    Q_OBJECT

private slots:
    // Erst die Lage aufnehmen: wie viele Karten, und was die Rechnung
    // daraus erwartet. Ohne diese Zeile im Protokoll ist jede Zahl
    // unten nicht einzuordnen.
    void initTestCase()
    {
        const int karten = kandidatenKarten();
        const DiscoveryTiming t = timingFor(DiscoveryProfile::SafeDefault);
        qInfo() << "Netzkarten (Kandidaten):" << karten
                << " SafeDefault je Karte:"
                << t.attemptsPerNic * t.quietPollsBeforeResend * t.pollTimeoutMs << "ms"
                << " erwartete Blockade bei blockierender Suche:"
                << karten * t.attemptsPerNic * t.quietPollsBeforeResend
                       * t.pollTimeoutMs << "ms";
    }

    // DIE Zusage: der Aufruf kehrt zurueck, bevor die Suche fertig ist.
    //
    // Die Schranke ist nicht knapp gewaehlt — sie liegt bei einem
    // Fuenftel der Zeit, die EINE Karte auf SafeDefault braucht. Damit
    // faellt die Lehre bei jeder Rueckkehr zur blockierenden Suche um,
    // ganz gleich wie viele Karten die Maschine hat, und sie bleibt
    // gruen, solange der Aufruf bloss Sockets aufsetzt und Zeitgeber
    // startet.
    void derAufrufHaeltDieSchleifeNichtAn()
    {
        const DiscoveryTiming t = timingFor(DiscoveryProfile::SafeDefault);
        const int jeKarte = t.attemptsPerNic * t.quietPollsBeforeResend
                            * t.pollTimeoutMs;
        const int schranke = jeKarte / 5;

        RadioDiscovery d;
        d.setProfile(DiscoveryProfile::SafeDefault);

        QElapsedTimer uhr;
        uhr.start();
        d.startDiscovery();
        const qint64 gebraucht = uhr.elapsed();

        qInfo() << "startDiscovery() kehrte nach" << gebraucht
                << "ms zurueck; Schranke" << schranke << "ms";

        QVERIFY2(gebraucht < schranke,
                 qPrintable(QStringLiteral(
                     "startDiscovery() hat die Ereignisschleife %1 ms lang "
                     "angehalten (erlaubt: %2 ms). Genau das laesst jedes "
                     "Fenster, das dabei aufgeht, %1 ms spaeter erscheinen.")
                     .arg(gebraucht).arg(schranke)));
    }

    // Und sie sucht trotzdem: discoveryStarted kommt, und die Suche
    // endet von selbst innerhalb der Zeit, die ihre eigene Rechnung
    // vorgibt. Ohne diesen Fall waere die Lehre oben auch mit einer
    // Suche gruen, die gar nichts tut.
    void undSuchtTrotzdemZuEnde()
    {
        const DiscoveryTiming t = timingFor(DiscoveryProfile::SafeDefault);
        const int jeKarte = t.attemptsPerNic * t.quietPollsBeforeResend
                            * t.pollTimeoutMs;
        // Grosszuegig: die Karten werden nacheinander abgeklopft, dazu
        // Luft fuer eine langsame Maschine.
        const int frist = qMax(1, kandidatenKarten()) * jeKarte * 3 + 5000;

        RadioDiscovery d;
        d.setProfile(DiscoveryProfile::SafeDefault);
        QSignalSpy begonnen(&d, &RadioDiscovery::discoveryStarted);
        QSignalSpy beendet(&d, &RadioDiscovery::discoveryFinished);

        QElapsedTimer uhr;
        uhr.start();
        d.startDiscovery();

        QVERIFY2(!begonnen.isEmpty(),
                 "discoveryStarted muss sofort kommen, nicht erst am Ende");
        QTRY_VERIFY_WITH_TIMEOUT(!beendet.isEmpty(), frist);
        qInfo() << "Suche fertig nach" << uhr.elapsed() << "ms (Frist"
                << frist << "ms)";
    }
};

QTEST_MAIN(TstSucheBlockiertDenStart)
#include "tst_suche_blockiert_den_start.moc"
