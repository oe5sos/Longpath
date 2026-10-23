// Haelt fest, dass ein gescheiterter Versand den GRUND nennt — und den
// richtigen.
//
// Hintergrund (2026-10-04): Anvelina verband nicht. Im Dialog stand "fast
// immer die Netzwerkstrecke, nicht das Geraet: ueber WLAN kommen die Pakete
// oft nicht durch". Das war falsch herum — es ging kein einziges Datagramm
// hinaus. Ping zum selben Geraet lief, und derselbe Versand aus einem
// anderen Programm ging anstandslos; nur Longpath kam nicht ins lokale
// Netz. Ein Hinweis, der in die falsche Richtung zeigt, kostet mehr Zeit
// als gar keiner.
//
// DREI Faelle, die auseinandergehalten werden MUESSEN — jeder mit einer
// anderen Abhilfe:
//
//   EHOSTUNREACH & Co. — es gibt gerade keinen Weg dorthin. Route, ARP,
//                        Schnittstelle, Gegenstelle aus. Ein ping trennt es.
//   EPERM / EACCES     — das Betriebssystem verweigert den Zugriff.
//                        Berechtigung oder Filter.
//   ENOBUFS / EAGAIN   — die Sendewarteschlange ist voll. Warten oder Kabel.
//
// Dass die ersten beiden getrennt gehoeren, zeigte derselbe Tag: der erste
// Entwurf warf sie zusammen und riet bei EHOSTUNREACH zu den
// Systemeinstellungen. Im selben Protokoll steht aber (11:08:25), dass
// rotctld — ein eigenes Programm, eigene Identitaet, anderes Teilnetz — in
// derselben Minute mit "No route to host" scheiterte. Eine Berechtigung, die
// an der Identitaet der App haengt, erklaert das nicht. Der Rat waere
// derselbe Fehler gewesen wie der Dialog, den dieser Zweig ersetzt.
//
// Werden sie vertauscht, schickt die Meldung den Bediener an den falschen
// Schalter. Genau dagegen steht dieser Stand.

#include "core/SendeFehler.h"

#include <QtTest>

#include <cerrno>

// ── Die Nummern kommen aus dem Pruefling, nicht von Hand (2026-10-22) ──
//
// Vorher standen hier die POSIX-Namen fest im Quelltext (EPERM, EACCES,
// EHOSTDOWN ...). Unter MinGW gibt es EHOSTDOWN nicht, und genau daran
// ist diese Datei im Windows-Messlauf vom 2026-10-22 gescheitert:
// "error: 'EHOSTDOWN' was not declared in this scope".
//
// Bemerkenswert dabei: SendeFehler.h loest dasselbe Problem seit dem
// 2026-10-04 und sagt es im Kommentar auch ("EHOSTDOWN fehlt unter
// MinGW -- daran scheiterte der Windows-Bau") -- es fuehrt je System
// eine eigene Tabelle (WSAE* gegen E*). Der Pruefstand hat die Tabelle
// nachgebaut statt sie zu benutzen, und damit genau die Doppelfuehrung
// eingebaut, gegen die der Kommentar dort geschrieben ist.
//
// Jetzt liest er die Tabellen des Prueflings. Zwei Gewinne: er
// uebersetzt auf beiden Systemen, und eine neue Nummer in SendeFehler.h
// wird hier ab sofort mitgeprueft, ohne dass jemand zwei Stellen pflegt.
using Longpath::Netz::kEigenerFehler;
using Longpath::Netz::kKeinWeg;
using Longpath::Netz::kVerweigert;
using Longpath::Netz::kWarteschlange;
using Longpath::Netz::sendeFehlerDeutung;

class TstSendeFehlerDeutung : public QObject
{
    Q_OBJECT

private slots:
    void berechtigungWirdAlsBerechtigungErkannt();
    void keinWegIstKeineVerweigerung();
    void warteschlangeWirdNichtMitBerechtigungVerwechselt();
    void eigenerFehlerWirdNichtDemNetzAngelastet();
    void unbekanntesSchweigtLieber();
};

void TstSendeFehlerDeutung::berechtigungWirdAlsBerechtigungErkannt()
{
    for (int nr : kVerweigert) {
        const QString t = sendeFehlerDeutung(nr);
        QVERIFY2(!t.isEmpty(), qPrintable(QString("errno %1 ohne Deutung").arg(nr)));
        QVERIFY2(t.contains(QStringLiteral("verweigert")),
                 qPrintable(QString("errno %1 nennt die Verweigerung nicht: %2")
                                .arg(nr).arg(t)));
        QVERIFY2(!t.contains(QStringLiteral("Funkstrecke")),
                 qPrintable(QString("errno %1 schickt an den falschen Schalter: %2")
                                .arg(nr).arg(t)));
    }
}

void TstSendeFehlerDeutung::keinWegIstKeineVerweigerung()
{
    // Der wichtigste Punkt dieses Standes. EHOSTUNREACH heisst "ich weiss
    // keinen Weg", nicht "du darfst nicht" — und wer es als Verweigerung
    // meldet, schickt den Bediener in die Systemeinstellungen, wo nichts zu
    // finden ist.
    for (int nr : kKeinWeg) {
        const QString t = sendeFehlerDeutung(nr);
        QVERIFY2(!t.isEmpty(), qPrintable(QString("errno %1 ohne Deutung").arg(nr)));
        QVERIFY2(t.contains(QStringLiteral("keinen Weg")), qPrintable(t));
        QVERIFY2(t.contains(QStringLiteral("ping")), qPrintable(t));
        QVERIFY2(!t.contains(QStringLiteral("Berechtigung")),
                 qPrintable(QString("errno %1 behauptet eine Verweigerung: %2")
                                .arg(nr).arg(t)));
        QVERIFY2(!t.contains(QStringLiteral("Funkstrecke")), qPrintable(t));
    }
}

void TstSendeFehlerDeutung::warteschlangeWirdNichtMitBerechtigungVerwechselt()
{
    for (int nr : kWarteschlange) {
        const QString t = sendeFehlerDeutung(nr);
        QVERIFY(!t.isEmpty());
        QVERIFY2(t.contains(QStringLiteral("Funkstrecke")), qPrintable(t));
        QVERIFY2(!t.contains(QStringLiteral("verweigert")), qPrintable(t));
        QVERIFY2(!t.contains(QStringLiteral("keinen Weg")), qPrintable(t));
    }
}

void TstSendeFehlerDeutung::eigenerFehlerWirdNichtDemNetzAngelastet()
{
    // Ein ungueltiger Socket ist ein Fehler in Longpath. Wer den dem Netz
    // anlastet, sucht tagelang am falschen Ende.
    for (int nr : kEigenerFehler) {
        const QString t = sendeFehlerDeutung(nr);
        QVERIFY(!t.isEmpty());
        QVERIFY2(t.contains(QStringLiteral("Longpath")), qPrintable(t));
    }
}

void TstSendeFehlerDeutung::unbekanntesSchweigtLieber()
{
    // Fuer alles Unbekannte bleibt die Deutung leer — dann steht in der
    // Meldung nur errno samt strerror(). Eine erfundene Erklaerung waere
    // schlimmer als keine.
    QVERIFY(sendeFehlerDeutung(0).isEmpty());
    QVERIFY(sendeFehlerDeutung(ENOENT).isEmpty());
}

QTEST_MAIN(TstSendeFehlerDeutung)
#include "tst_sendefehler_deutung.moc"
