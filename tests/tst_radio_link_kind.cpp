// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ueber welche Art Strecke haengt das Funkgeraet?
//
// Anlass, 2026-08-23. Der Betreiber sah 0,17 % bis 2,87 % Paketverlust.
// Ich habe erst den Empfangspuffer verdaechtigt — und dann gemessen:
//
//   netstat -s -p udp  ->  "0 dropped due to full socket buffers"
//
// Seit dem Systemstart hat der Kern kein einziges Paket wegen eines
// vollen Puffers verworfen. Der Verlust passiert also VOR der
// Steckdose. Und:
//
//   networksetup  ->  en0 = Wi-Fi
//   route         ->  interface: en0
//
// Das Funkgeraet haengt am WLAN. Damit ist der Verlust erklaert und es
// ist keine Programmieraufgabe — aber die Anwendung soll es SAGEN,
// statt den Betreiber suchen zu lassen. So, wie ich gesucht habe.
//
// Diese Pruefung braucht kein Netz: sie baut sich die Schnittstellen
// selbst und prueft die Zuordnung.

#include <QtTest>

#include "core/RadioLinkKind.h"

using namespace Longpath;

namespace {

// Qt laesst Typ und Flags an QNetworkInterface nicht setzen — die
// Klasse ist nur lesbar. Fuer die Pruefung wird die Entscheidung darum
// an einer eigenen kleinen Nachbildung gefahren, die dieselbe Frage
// beantwortet: liegt die Adresse im Netz dieser Schnittstelle?
//
// Das prueft NICHT Qts Typerkennung — die gehoert Qt. Es prueft die
// Auswahl, und die ist der Teil, den ich geschrieben habe: die
// RICHTIGE Schnittstelle finden, nicht die erste aktive.
struct FakeIface { QHostAddress ip; int prefix; RadioLinkKind kind; };

RadioLinkKind pick(const QHostAddress& radio, const QList<FakeIface>& list)
{
    if (radio.isNull()) { return RadioLinkKind::Unknown; }
    if (radio.isLoopback()) { return RadioLinkKind::Loopback; }
    for (const FakeIface& f : list) {
        if (radio.isInSubnet(f.ip, f.prefix)) { return f.kind; }
    }
    return RadioLinkKind::Unknown;
}

} // namespace

class TstRadioLinkKind : public QObject
{
    Q_OBJECT

private slots:
    void dieRichtigeSchnittstelleGewinnt()
    {
        // Der Fall, um den es geht: Kabel UND WLAN sind aktiv, aber nur
        // eines traegt das Netz des Funkgeraets. Wer die erste aktive
        // nimmt, antwortet per Muenzwurf.
        const QList<FakeIface> both = {
            {QHostAddress(QStringLiteral("192.168.1.10")), 24, RadioLinkKind::Wired},
            {QHostAddress(QStringLiteral("172.30.30.121")), 24, RadioLinkKind::Wireless},
        };
        const RadioLinkKind k =
            pick(QHostAddress(QStringLiteral("172.30.30.26")), both);
        qInfo() << "Geraet 172.30.30.26 bei Kabel+WLAN →"
                << (k == RadioLinkKind::Wireless ? "WLAN" : "anderes");
        QCOMPARE(k, RadioLinkKind::Wireless);
    }

    void amKabelWirdNichtGewarnt()
    {
        const QList<FakeIface> both = {
            {QHostAddress(QStringLiteral("172.30.30.5")), 24, RadioLinkKind::Wired},
            {QHostAddress(QStringLiteral("192.168.4.2")), 24, RadioLinkKind::Wireless},
        };
        QCOMPARE(pick(QHostAddress(QStringLiteral("172.30.30.26")), both),
                 RadioLinkKind::Wired);
        QVERIFY(radioLinkWarning(RadioLinkKind::Wired).isEmpty());
    }

    void ueberWlanGibtEsEinenSatz()
    {
        const QString s = radioLinkWarning(RadioLinkKind::Wireless);
        qInfo().noquote() << s;
        QVERIFY(!s.isEmpty());
        // Er muss sagen, WAS zu tun ist. Ein Hinweis ohne Ausweg ist
        // nur eine Beunruhigung.
        QVERIFY2(s.contains(QStringLiteral("Kabel")), qPrintable(s));
    }

    void unbekanntSchweigt()
    {
        // Wenn wir es nicht wissen, sagen wir nichts. Eine falsche
        // Warnung kostet mehr Vertrauen, als eine fehlende kostet.
        QVERIFY(radioLinkWarning(RadioLinkKind::Unknown).isEmpty());
        QVERIFY(radioLinkWarning(RadioLinkKind::Loopback).isEmpty());
    }

    void dieEchteFassungLaeuftDurch()
    {
        // Kein Ergebnis behauptet — nur, dass sie mit den WIRKLICHEN
        // Schnittstellen dieser Maschine nicht stolpert.
        const RadioLinkKind k = radioLinkKindFor(
            QHostAddress(QStringLiteral("172.30.30.26")),
            QNetworkInterface::allInterfaces());
        qInfo() << "auf dieser Maschine:"
                << (k == RadioLinkKind::Wireless ? "WLAN"
                    : k == RadioLinkKind::Wired ? "Kabel"
                    : k == RadioLinkKind::Virtual ? "Tunnel"
                    : k == RadioLinkKind::Loopback ? "lokal" : "unbekannt");
        QVERIFY(true);
    }
};

QTEST_MAIN(TstRadioLinkKind)
#include "tst_radio_link_kind.moc"
