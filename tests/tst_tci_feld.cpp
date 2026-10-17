// Pruefstand fuer TciFeld::sicher.
//
// Longpaths eigene Antworten sind zeilenweise und durch Kommas geteilt, und
// die Felder darin kommen NICHT von uns: aus einem Telnet-Strom von einem
// fremden Rechner (Cluster, RBN) und aus einer ADIF-Datei, in der auch
// Einfuhren aus anderen Programmen liegen.
//
// Zwei Fehler sind damit moeglich, und beide sind unsichtbar:
//
//   * Ein Komma im Feld verschiebt alle folgenden Felder -- aus dem
//     Rufzeichen wird die Betriebsart.
//   * Ein Semikolon beendet die Zeile mitten im Feld, und was danach kommt,
//     liest die Gegenseite als NEUEN BEFEHL.

#include <QtTest>
#include <QString>

#include "core/TciFeld.h"

using namespace Longpath;

class TstTciFeld : public QObject {
    Q_OBJECT

private slots:
    void einEchtesRufzeichenBleibtWieEsIst();
    void dasKommaGehtHinaus();
    void dasSemikolonGehtHinaus();
    void derEinbruchGehtNichtMehr();
    void unsichtbaresGehtHinaus();
    void einUnfallWirdGekuerzt();
    void einSatzDarfLaengerSein();
};

void TstTciFeld::einEchtesRufzeichenBleibtWieEsIst()
{
    // Der erste Pruefpunkt ist der wichtigste: eine Sicherung, die auch
    // richtige Werte veraendert, ist keine Sicherung, sondern ein Fehler.
    QCOMPARE(TciFeld::sicher(QStringLiteral("OE5SOS")), QStringLiteral("OE5SOS"));
    QCOMPARE(TciFeld::sicher(QStringLiteral("DL1YCF/P")), QStringLiteral("DL1YCF/P"));
    QCOMPARE(TciFeld::sicher(QStringLiteral("VP2E/W5XYZ")), QStringLiteral("VP2E/W5XYZ"));
    QCOMPARE(TciFeld::sicher(QStringLiteral("20M")), QStringLiteral("20M"));
    QCOMPARE(TciFeld::sicher(QStringLiteral("SSB")), QStringLiteral("SSB"));
    QCOMPARE(TciFeld::sicher(QStringLiteral("59")), QStringLiteral("59"));
    QCOMPARE(TciFeld::sicher(QStringLiteral("599 TU")), QStringLiteral("599 TU"));
    QCOMPARE(TciFeld::sicher(QString()), QString());
}

void TstTciFeld::dasKommaGehtHinaus()
{
    QCOMPARE(TciFeld::sicher(QStringLiteral("OE5,SOS")), QStringLiteral("OE5SOS"));
    QCOMPARE(TciFeld::sicher(QStringLiteral(",,,")), QString());
}

void TstTciFeld::dasSemikolonGehtHinaus()
{
    QCOMPARE(TciFeld::sicher(QStringLiteral("OE5SOS;")), QStringLiteral("OE5SOS"));
}

void TstTciFeld::derEinbruchGehtNichtMehr()
{
    // DER Punkt. Ein Clusterbetreiber (oder ein kaputter Cluster) schickt ein
    // Rufzeichen, das die Zeile beendet und einen eigenen Befehl anhaengt.
    // Ohne Sicherung steht das so in der Antwort, und die App fuehrt es aus:
    // der Sender geht an.
    const QString boese = QStringLiteral("OE5SOS;trx:0,true;");
    const QString rein  = TciFeld::sicher(boese);

    QVERIFY2(!rein.contains(QLatin1Char(';')), qPrintable(rein));
    QVERIFY2(!rein.contains(QLatin1Char(',')), qPrintable(rein));
    // Die Zeichen sind weg, der Text bleibt sichtbar -- stillschweigend
    // verschlucken waere schlechter: so sieht man im Protokoll, dass etwas
    // Seltsames ankam.
    QCOMPARE(rein, QStringLiteral("OE5SOStrx:0true"));

    // Und zusammengebaut bleibt die Zeile eine Zeile mit genau sechs Feldern.
    const QString zeile = QStringLiteral("spot_zeile:%1,%2,%3,%4,%5,%6;")
                              .arg(0).arg(14074000)
                              .arg(rein, TciFeld::sicher(QStringLiteral("SSB")),
                                   TciFeld::sicher(QStringLiteral("CLUSTER")))
                              .arg(12);
    QCOMPARE(zeile.count(QLatin1Char(';')), 1);
    QVERIFY(zeile.endsWith(QLatin1Char(';')));
    QCOMPARE(zeile.chopped(1).section(QLatin1Char(':'), 1)
                 .split(QLatin1Char(',')).size(), 6);
}

void TstTciFeld::unsichtbaresGehtHinaus()
{
    QCOMPARE(TciFeld::sicher(QStringLiteral("OE5\nSOS")), QStringLiteral("OE5SOS"));
    QCOMPARE(TciFeld::sicher(QStringLiteral("OE5\r\nSOS")), QStringLiteral("OE5SOS"));
    QCOMPARE(TciFeld::sicher(QStringLiteral("OE5\tSOS")), QStringLiteral("OE5SOS"));
    QCOMPARE(TciFeld::sicher(QStringLiteral("  OE5SOS  ")), QStringLiteral("OE5SOS"));
}

void TstTciFeld::einUnfallWirdGekuerzt()
{
    const QString lang(5000, QLatin1Char('X'));
    QCOMPARE(TciFeld::sicher(lang).size(), TciFeld::kMaxZeichen);
    // Das laengste echte Rufzeichen der Welt hat elf Zeichen -- die Grenze
    // schneidet also nur, was ohnehin kein Rufzeichen ist.
    QVERIFY(TciFeld::kMaxZeichen > 11);
}


void TstTciFeld::einSatzDarfLaengerSein()
{
    // Die wenigen Felder, die kein Rufzeichen sind, sondern ein Satz --
    // ein Fehlertext etwa, der am Zeilenende allein steht. 64 Zeichen
    // schnitten ihn mitten im Wort ab, und eine halbe Fehlermeldung ist
    // schlechter als keine.
    const QString meldung = QStringLiteral(
        "Das Logbuch laesst sich nicht oeffnen: /Users/jemand/Library/"
        "Preferences/Longpath/logbuch.adi, Zugriff verweigert");
    const QString raus = TciFeld::sicher(meldung, 200);

    QVERIFY2(raus.contains(QStringLiteral("Zugriff verweigert")), qPrintable(raus));
    // Das Komma muss trotzdem weg: `tci.js` teilt an Kommas und nimmt das
    // erste Stueck -- mit Komma saehe der Bediener nur die halbe Meldung.
    QVERIFY2(!raus.contains(QLatin1Char(',')), qPrintable(raus));
    QVERIFY2(!raus.contains(QLatin1Char(';')), qPrintable(raus));

    // Und die Vorgabe bleibt die kurze: ein Rufzeichenfeld wird nicht
    // heimlich laenger.
    QCOMPARE(TciFeld::sicher(QString(500, QLatin1Char('X'))).size(),
             TciFeld::kMaxZeichen);
    QCOMPARE(TciFeld::sicher(QString(500, QLatin1Char('X')), 200).size(), 200);
}

QTEST_MAIN(TstTciFeld)
#include "tst_tci_feld.moc"
