// SPDX-License-Identifier: GPL-3.0-or-later
//
// „die flex wird dauernd verloren und wiedergefunden" -- gemessen am
// 2026-10-28 im Protokoll eines Laufs von 3 h 44 min an OE5SOS'
// FLEX-8400 (der Mac auf en0 = WLAN, 172.30.30.110; das Geraet auf
// 172.30.30.12):
//
//   374 Mal „verloren"/„gefunden" in 223 Minuten — alle 36 s eines.
//   Kuerzeste Standzeit 5,02 s — genau die Frist.
//   Kuerzeste Luecke „verloren" → „gefunden": 10 ms.
//   21,2 % der Laufzeit stand die Maschine als „offline" in der Liste.
//
// Zehn Millisekunden heisst: das Geraet war nie weg. Flex ruft per
// Broadcast aus, und Broadcast ueber WLAN wird nicht quittiert — er
// faellt reihenweise aus. Das ist kein Fehler im Netz, sondern die
// Eigenschaft des Mediums, und RadioLinkKind.h haelt sie seit dem
// 2026-08-23 schon fest.
//
// Was es kostet, ist nicht Protokollrauschen: onRadioLost faerbt die
// Zeile grau und schreibt „offline" hinein, und updateButtonStates()
// macht den Verbinden-Knopf an einer solchen Zeile FEST. Jede zwoelfte
// Minute war er tot.
//
// Gegenprobe: mit der alten Fassung (eine Frist von 5 s fuer ALLE
// Wege) faellt `imWlanUeberlebtSieEineLuecke` um — nach zehn Sekunden
// ohne Ruf war die Maschine dort verschwunden.
//
// Longpath-original: kein Thetis- oder AetherSDR-Urtext.

// no-port-check: Longpath-original test file.

#include <QtTest>

#include <QByteArray>
#include <QHostAddress>
#include <QSignalSpy>

#include "core/FlexRadioDiscovery.h"
#include "core/RadioLinkKind.h"

using namespace Longpath;

namespace {

// Kopf und Nutzlast der echten FLEX-8400, am 2026-10-18 vom Draht
// gelesen — dieselben Bytes wie in tst_flex_stufe1.cpp.
const char* kHeader8400Hex =
    "3853009b00000800"
    "00001c2d534cffff"
    "6ad47ab900000000"
    "00000000";

const char* kPayload8400 =
    "discovery_protocol_version=3.1.0.4 model=FLEX-8400 "
    "serial=1025-1213-8400-1196 version=4.3.16.42034 nickname=Martin "
    "callsign=OE5SOS ip=172.30.30.12 port=4992 status=Available "
    "radio_license_id=00-1C-2D-05-34-28 turf_region=EU";

QByteArray echterRuf()
{
    QByteArray out = QByteArray::fromHex(kHeader8400Hex);
    out.append(kPayload8400);
    return out;
}

// Die MAC, unter der die Maschine in der Liste steht.
const char* kMac = "00:1C:2D:05:34:28";

}  // namespace

class TstFlexSucheFrist : public QObject
{
    Q_OBJECT

private:
    // Eine Suche, die nichts vom Netz dieses Rechners weiss: der
    // Eigenruf-Filter bekommt eine fremde Adresse, der Netzweg wird
    // gesetzt. Sonst haengt der Lauf daran, ob der Bauknecht gerade
    // WLAN hat.
    static void aufstellen(FlexRadioDiscovery& suche, RadioLinkKind weg)
    {
        suche.setOwnAddressesForTesting({QHostAddress(QStringLiteral("10.0.0.1"))});
        suche.setLinkKindForTesting(weg);
    }

private slots:
    // ─── Die Frist folgt dem Netzweg ─────────────────────────────────

    void amKabelBleibenEsFuenfSekunden()
    {
        // AetherSDRs Wert. Am Kabel ist ein verlorener Broadcast die
        // Ausnahme, also darf die Frist streng bleiben.
        QCOMPARE(FlexRadioDiscovery::staleTimeoutFor(RadioLinkKind::Wired), 5000);
        QCOMPARE(FlexRadioDiscovery::staleTimeoutFor(RadioLinkKind::Loopback), 5000);
    }

    void imWlanSindEsZwanzig()
    {
        // Aus der Messung abgeleitet, nicht geschaetzt: 98 % der
        // gemessenen Rufluecken liegen bei <= 20,1 s, danach springt
        // die Verteilung auf 196 s.
        QCOMPARE(FlexRadioDiscovery::staleTimeoutFor(RadioLinkKind::Wireless), 20000);
        QCOMPARE(FlexRadioDiscovery::staleTimeoutFor(RadioLinkKind::Virtual), 20000);
    }

    void unbekannterWegBekommtLuftUndNichtStrenge()
    {
        // Hier weicht die Frist bewusst von radioLinkWarning() ab, das
        // im Zweifel schweigt und „Kabel" annimmt. Der Schaden zeigt in
        // die andere Richtung: eine Zeile, die zu lange stehenbleibt,
        // kostet nichts — ein falsches „offline" nimmt dem Betreiber
        // den Verbinden-Knopf weg.
        QCOMPARE(FlexRadioDiscovery::staleTimeoutFor(RadioLinkKind::Unknown), 20000);
    }

    // ─── Und sie wirkt auch im Durchlauf ─────────────────────────────

    void imWlanUeberlebtSieEineLuecke()
    {
        FlexRadioDiscovery suche;
        aufstellen(suche, RadioLinkKind::Wireless);

        QSignalSpy gefunden(&suche, &FlexRadioDiscovery::radioDiscovered);
        QSignalSpy verloren(&suche, &FlexRadioDiscovery::radioLost);

        const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
        suche.ingestDatagramForTesting(echterRuf(),
                                       QHostAddress(QStringLiteral("172.30.30.12")));
        QCOMPARE(gefunden.count(), 1);

        // Zehn Sekunden ohne Ruf. Das ist im WLAN gemessener Alltag --
        // der Median der Luecken lag bei 8,6 s.
        suche.sweepStaleForTesting(jetzt + 10000);
        QVERIFY2(verloren.isEmpty(),
                 "nach 10 s ohne Broadcast im WLAN als verschwunden "
                 "gemeldet -- genau das Flattern, das den "
                 "Verbinden-Knopf 374 Mal totgelegt hat");
        QCOMPARE(suche.discoveredRadios().size(), 1);

        // Aber eine echte Abwesenheit faellt weiter auf. Die fuenf
        // langen Luecken im Protokoll lagen bei 175 s und mehr.
        suche.sweepStaleForTesting(jetzt + 25000);
        QCOMPARE(verloren.count(), 1);
        QCOMPARE(verloren.first().first().toString(), QString::fromLatin1(kMac));
        QVERIFY(suche.discoveredRadios().isEmpty());
    }

    void amKabelFaelltDieselbeLueckeAuf()
    {
        // Die Gegenrichtung: die lange Frist darf nicht fuer alle
        // gelten. Am Kabel ist eine Luecke von zehn Sekunden ein
        // Befund, kein Medium.
        FlexRadioDiscovery suche;
        aufstellen(suche, RadioLinkKind::Wired);

        QSignalSpy verloren(&suche, &FlexRadioDiscovery::radioLost);
        const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
        suche.ingestDatagramForTesting(echterRuf(),
                                       QHostAddress(QStringLiteral("172.30.30.12")));

        suche.sweepStaleForTesting(jetzt + 10000);
        QCOMPARE(verloren.count(), 1);
    }

    // ─── Das Messgeraet an der blinden Stelle ────────────────────────

    void ungeleseneDatagrammeWerdenGezaehlt()
    {
        // handleDatagram kehrt bei `!beacon.valid` stillschweigend um.
        // Ein Ruf, den dieser Leser nicht versteht, sah im Protokoll
        // damit genau aus wie ein Ruf, der nie kam -- und diese zwei
        // Ursachen brauchen entgegengesetzte Behebungen.
        FlexRadioDiscovery suche;
        aufstellen(suche, RadioLinkKind::Wireless);
        QCOMPARE(suche.ungeleseneDatagramme(), 0u);

        // Fremder Verkehr auf 4992: zu kurz fuer einen Kopf.
        suche.ingestDatagramForTesting(QByteArray::fromHex("3853009b0000080000000000534cffff"),
                                       QHostAddress(QStringLiteral("172.30.30.99")));
        QCOMPARE(suche.ungeleseneDatagramme(), 1u);

        suche.ingestDatagramForTesting(QByteArray("kein Flex-Ruf"),
                                       QHostAddress(QStringLiteral("172.30.30.99")));
        QCOMPARE(suche.ungeleseneDatagramme(), 2u);
    }

    void einGelesenerRufHebtDenVerdachtAuf()
    {
        // Der Zaehler soll die Frage „kam zwischen zwei Rufen etwas
        // Unlesbares an?" beantworten, nicht ueber den ganzen Lauf
        // summieren -- sonst sagt er nach drei Stunden nichts mehr.
        FlexRadioDiscovery suche;
        aufstellen(suche, RadioLinkKind::Wireless);

        suche.ingestDatagramForTesting(QByteArray("kein Flex-Ruf"),
                                       QHostAddress(QStringLiteral("172.30.30.99")));
        QCOMPARE(suche.ungeleseneDatagramme(), 1u);

        suche.ingestDatagramForTesting(echterRuf(),
                                       QHostAddress(QStringLiteral("172.30.30.12")));
        QCOMPARE(suche.ungeleseneDatagramme(), 0u);
    }
};

QTEST_MAIN(TstFlexSucheFrist)
#include "tst_flex_suche_frist.moc"
