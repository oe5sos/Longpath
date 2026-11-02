// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_flex_socketmeldung.cpp  (Longpath)
// =================================================================
//
// Am 2026-11-01 stand in der Verbindungsmaske des Betreibers:
//
//   Disconnected — Socketfehler ohne Zuordnung (errno unbekannt)
//   (Unknown error, Nr. -1, Ziel 172.30.30.12:4992)
//
// Zwei Fehler in einer Zeile. Erstens deutsch in einer englischen
// Oberflaeche: `tst_oberflaeche_englisch` nimmt Fehlertexte unter
// src/core ausdruecklich aus, weil sie als PROTOKOLLtexte galten —
// dieser hier geht aber ueber `errorOccurred` in die Maske und wird
// gelesen. Zweitens sagte der Satz nichts, was man tun kann.
//
// Beides nagelt dieser Prueflauf fest. Er ist absichtlich stur: er
// kennt die deutschen Merkmale, die in diesen Texten vorkamen, und
// faellt um, sobald eines wiederkehrt.
// =================================================================

#include <QtTest/QtTest>

#include <QAbstractSocket>

#include "core/FlexRadioConnection.h"

using namespace Longpath;

class TstFlexSocketmeldung : public QObject
{
    Q_OBJECT

private slots:
    // Jeder Fall, den die Tabelle kennt, kommt englisch heraus.
    void keinDeutschInDerMaske()
    {
        const QList<QAbstractSocket::SocketError> faelle{
            QAbstractSocket::ConnectionRefusedError,
            QAbstractSocket::RemoteHostClosedError,
            QAbstractSocket::HostNotFoundError,
            QAbstractSocket::SocketTimeoutError,
            QAbstractSocket::NetworkError,
            QAbstractSocket::SocketAccessError,
            QAbstractSocket::SocketResourceError,
            QAbstractSocket::AddressInUseError,
            QAbstractSocket::SocketAddressNotAvailableError,
            QAbstractSocket::UnsupportedSocketOperationError,
            QAbstractSocket::UnfinishedSocketOperationError,
            QAbstractSocket::DatagramTooLargeError,
            QAbstractSocket::UnknownSocketError,
            QAbstractSocket::ProxyAuthenticationRequiredError,  // faellt in den Rest
        };

        // Merkmale, die in genau diesen Texten standen, plus die
        // Umlaute und das scharfe S. Ein Netz, keine Grammatik —
        // dieselbe Einschraenkung wie bei tst_oberflaeche_englisch.
        const QStringList deutsch{
            QStringLiteral("Socketfehler"), QStringLiteral("Verbindung"),
            QStringLiteral("Maschine"),     QStringLiteral("Adresse"),
            QStringLiteral("Netz"),         QStringLiteral("Zuordnung"),
            QStringLiteral("unbekannt"),    QStringLiteral("Betriebssystem"),
            QStringLiteral("Frist"),        QStringLiteral("belegt"),
            QStringLiteral("gross"),        QStringLiteral("laeuft"),
            QStringLiteral("traegt"),       QStringLiteral("hoert"),
            // KEINE Umschriften wie "ae"/"oe"/"ue" in dieser Liste:
            // "does" traegt ein "oe", "queue" ein "ue". Der erste
            // Entwurf fiel genau daran um — ein Netz mit zu feinen
            // Maschen faengt die eigene Hand.
            QStringLiteral("ä"), QStringLiteral("ö"),
            QStringLiteral("ü"), QStringLiteral("ß"),
        };

        for (const auto fall : faelle) {
            const QString text = FlexRadioConnection::socketErrorText(fall);
            QVERIFY2(!text.isEmpty(),
                     qPrintable(QStringLiteral("leer bei %1").arg(int(fall))));
            for (const QString& wort : deutsch) {
                QVERIFY2(!text.contains(wort, Qt::CaseInsensitive),
                         qPrintable(QStringLiteral("%1 steht in: %2")
                                        .arg(wort, text)));
            }
        }
    }

    // Der Fall, der den Betreiber getroffen hat: Qt weisz nichts. Dann
    // muss wenigstens dastehen, was zu tun ist — „ohne Zuordnung" ist
    // ein Schulterzucken, kein Satz.
    void derUnbekannteFallSagtWasZuTunIst()
    {
        const QString text = FlexRadioConnection::socketErrorText(
            QAbstractSocket::UnknownSocketError);
        QVERIFY2(text.contains(QStringLiteral("try again"), Qt::CaseInsensitive),
                 qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("no reason"), Qt::CaseInsensitive),
                 qPrintable(text));
    }

    // Die drei Faelle, die man am haeufigsten liest, im Wortlaut.
    void dieHaeufigenFaelleImWortlaut()
    {
        QCOMPARE(FlexRadioConnection::socketErrorText(
                     QAbstractSocket::ConnectionRefusedError),
                 QStringLiteral("Connection refused — nothing is listening on that port"));
        QCOMPARE(FlexRadioConnection::socketErrorText(
                     QAbstractSocket::SocketTimeoutError),
                 QStringLiteral("No answer — timed out"));
        QCOMPARE(FlexRadioConnection::socketErrorText(
                     QAbstractSocket::NetworkError),
                 QStringLiteral("Network unreachable — cable, Wi-Fi or route"));
    }

    // Ein Fall ausserhalb der Tabelle traegt die Nummer, damit ein
    // Protokoll auch ohne diese Datei auswertbar bleibt.
    void derRestTraegtDieNummer()
    {
        const QString text = FlexRadioConnection::socketErrorText(
            QAbstractSocket::ProxyAuthenticationRequiredError);
        QVERIFY2(text.contains(QString::number(
                     int(QAbstractSocket::ProxyAuthenticationRequiredError))),
                 qPrintable(text));
    }
};

QTEST_MAIN(TstFlexSocketmeldung)
#include "tst_flex_socketmeldung.moc"
