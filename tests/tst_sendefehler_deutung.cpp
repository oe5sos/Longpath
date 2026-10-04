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
// Die beiden Faelle, die hier auseinandergehalten werden MUESSEN:
//
//   EHOSTUNREACH / EPERM  — das Betriebssystem laesst dieses Programm nicht
//                           ins lokale Netz. Abhilfe: eine Berechtigung.
//   ENOBUFS / EAGAIN      — die Sendewarteschlange ist voll. Abhilfe: warten
//                           oder Kabel.
//
// Werden die vertauscht, schickt die Meldung den Bediener an den falschen
// Schalter. Genau dagegen steht dieser Stand.

#include "core/SendeFehler.h"

#include <QtTest>

#include <cerrno>

using Longpath::Netz::sendeFehlerDeutung;

class TstSendeFehlerDeutung : public QObject
{
    Q_OBJECT

private slots:
    void berechtigungWirdAlsBerechtigungErkannt();
    void warteschlangeWirdNichtMitBerechtigungVerwechselt();
    void eigenerFehlerWirdNichtDemNetzAngelastet();
    void unbekanntesSchweigtLieber();
};

void TstSendeFehlerDeutung::berechtigungWirdAlsBerechtigungErkannt()
{
    for (int nr : {EHOSTUNREACH, ENETUNREACH, EHOSTDOWN, EPERM, EACCES}) {
        const QString t = sendeFehlerDeutung(nr);
        QVERIFY2(!t.isEmpty(), qPrintable(QString("errno %1 ohne Deutung").arg(nr)));
        QVERIFY2(t.contains(QStringLiteral("Lokales Netzwerk")),
                 qPrintable(QString("errno %1 nennt die Berechtigung nicht: %2")
                                .arg(nr).arg(t)));
        // Und ausdruecklich NICHT in die WLAN-Richtung zeigen.
        QVERIFY2(!t.contains(QStringLiteral("Funkstrecke")),
                 qPrintable(QString("errno %1 schickt an den falschen Schalter: %2")
                                .arg(nr).arg(t)));
    }
}

void TstSendeFehlerDeutung::warteschlangeWirdNichtMitBerechtigungVerwechselt()
{
    for (int nr : {ENOBUFS, EAGAIN}) {
        const QString t = sendeFehlerDeutung(nr);
        QVERIFY(!t.isEmpty());
        QVERIFY2(t.contains(QStringLiteral("Funkstrecke")), qPrintable(t));
        QVERIFY2(!t.contains(QStringLiteral("Lokales Netzwerk")), qPrintable(t));
    }
}

void TstSendeFehlerDeutung::eigenerFehlerWirdNichtDemNetzAngelastet()
{
    // Ein ungueltiger Socket ist ein Fehler in Longpath. Wer den dem Netz
    // anlastet, sucht tagelang am falschen Ende.
    for (int nr : {EBADF, ENOTSOCK, EAFNOSUPPORT, EINVAL}) {
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
