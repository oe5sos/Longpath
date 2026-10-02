// no-port-check: Longpath-eigen. Thetis hat diese Sperre nicht, weil sein
// TCI-Server nur auf Loopback lauscht — es gibt dort keinen Fernzugriff und
// also auch nichts zu sperren.
//
// ── Warum es diesen Prüfstand gibt ──────────────────────────────────────────
//
// Die Durchsicht am 2026-09-30 fand zwei Dinge, die zusammen den schlimmsten
// Fehlerfall dieses Servers ergeben — an Martins Station hängt eine echte
// Antenne:
//
//  1. DIE SPERRE HATTE IHREN EIGENEN PARSER. Sie prüfte mit
//     `trimmed.startsWith("trx:")` — schreibungsabhängig und ohne den Namen
//     zu trimmen. Der Befehlsverteiler dahinter liest denselben Befehl als
//     `parts.at(0).toLower().trimmed()` (TciProtocol.cpp:84). Also:
//
//         TRX:0,true;    Sperre sieht "TRX:" ungleich "trx:" -> greift NICHT
//                        Verteiler macht toLower()          -> führt AUS
//         trx :0,true;   dasselbe über das Leerzeichen
//
//     Aus dem Netz liess sich der Sender damit tasten, obwohl Fernsenden
//     gesperrt war.
//
//  2. KEINE DER DREI SPERREN WURDE GEPRÜFT. Es gab Prüfpunkte für den Getter
//     remoteTxAllowed() — aber keinen, der je einen Sendebefehl durch die
//     Sperre geschickt hätte. Eine Sicherheitsnaht ohne Prüfstand ist eine
//     Behauptung.
//
// Dazu kam der Abstimmträger: `tune:` lief als einziger Sendeweg ohne
// Besitzer, Wachhund und Sendezeit-Deckel. Verschwand der Client, blieb der
// Träger unbegrenzt auf der Antenne stehen.
//
// Geprüft wird deshalb die NAHT, nicht der Getter: echte WebSocket-Clients,
// echte Befehle, in den Schreibweisen, die vorbeiliefen.

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include <QUrl>

#include <QElapsedTimer>
#include "TciBurstHelfer.h"
#include "core/AppSettings.h"
#include "core/CredentialStore.h"
#include "core/TciServer.h"

using namespace Longpath;

namespace {
// Lang und zufällig genug, dass es wie ein echtes Token aussieht; der Inhalt
// spielt keine Rolle, nur dass beide Seiten dasselbe kennen.
const char* kToken = "K7M2PQXR4TWH9NBJ63FDYAVC58EG";
}  // namespace

class TestTciSendesperre : public QObject {
    Q_OBJECT

private:
    // Baut Server + angemeldeten Netz-Client. Gibt false zurück, wenn schon
    // der Aufbau scheitert (dann sagt der Prüfpunkt das, statt stumm grün zu
    // sein — genau der Fehler, den die Durchsicht an anderer Stelle fand).
    bool aufbauen(TciServer& server, QWebSocket& client, bool sendenFrei)
    {
        AppSettings::instance().setValue(QStringLiteral("TciAllowRemoteTx"),
            sendenFrei ? QStringLiteral("True") : QStringLiteral("False"));
        if (!TciServer::setRemoteToken(QString::fromLatin1(kToken))) { return false; }
        if (!server.start(0)) { return false; }

        // Jede Verbindung gilt als Netzverbindung — sonst greift die Sperre
        // im Testlauf nie, weil alles über Loopback läuft.
        server.setTreatAllClientsAsRemoteForTest(true);

        QSignalSpy verbunden(&client, &QWebSocket::connected);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        if (!verbunden.wait(2000)) { return false; }

        client.sendTextMessage(QStringLiteral("auth:%1;").arg(QLatin1String(kToken)));

        // Auf das ENDE des Init-Bursts warten, nicht auf eine Zahl von
        // Millisekunden — Begruendung in TciBurstHelfer.h.
        return TciTest::warteAufReady(client);
    }

    // Schickt einen Befehl und lässt die Ereignisschleife laufen.
    static void schicke(QWebSocket& c, const QString& befehl)
    {
        c.sendTextMessage(befehl);
        QTest::qWait(80);
    }

    /** Wartet, bis der Besitzer den erwarteten Zustand hat.
     *
     *  Warum: `schicke()` wartet feste 80 ms, und das reicht auf diesem
     *  Rechner immer und auf einem belasteten CI-Laeufer nicht. Am
     *  2026-10-02 fiel genau daran
     *  `abfrage_entsichert_den_traeger_nicht` um — der Befehl
     *  `tune:0,false;` war unterwegs, als der Pruefpunkt schon sah.
     *
     *  Das ist dieselbe Krankheit, die in diesem Testbaum schon dreimal
     *  zugeschlagen hat (TciBurstHelfer.h): eine feste Zahl, wo auf ein
     *  Ereignis gewartet werden muss. Ein Pruefstand, der von der
     *  Geschwindigkeit der Maschine abhaengt, misst nicht die Sache,
     *  sondern den Tag.
     *
     *  Nur fuer erwartete WECHSEL. Fuer "es darf sich nichts aendern" taugt
     *  es nicht — dort muss man eine Weile warten und danach pruefen, und
     *  genau das tun die betreffenden Pruefpunkte weiterhin selbst.
     */
    static bool warteAufBesitzer(TciServer& server, bool besetzt,
                                 int grenzeMs = 4000)
    {
        QElapsedTimer uhr;
        uhr.start();
        while (uhr.elapsed() < grenzeMs) {
            if ((server.moxOwnerForTest() != nullptr) == besetzt) { return true; }
            QTest::qWait(20);
        }
        return (server.moxOwnerForTest() != nullptr) == besetzt;
    }

    // Ob ein Sendebefehl AUSGEFÜHRT wurde, erkennt man an der Antwort: der
    // Server bestätigt `trx:`/`tune:` zurück, wenn er sie durchlässt, und
    // schweigt, wenn die Sperre greift (`return` vor dem Verteiler).
    //
    // Der Besitzer taugt hier NICHT als Mass — das war der erste Entwurf
    // dieses Prüfstands und er mass die falsche Sache: dreht man die
    // Normalisierung zurück, überspringt der `trx:`-Block auch das Setzen
    // des Besitzers, und der Prüfpunkt bleibt grün, während der Befehl in
    // Wirklichkeit beim Verteiler landet und sendet. Genau dieses stille
    // Grün war der Fund "Prüfpunkt beweist nichts" an anderer Stelle.
    static bool bestaetigungKam(const QStringList& gesammelt, const QString& name)
    {
        for (const QString& z : gesammelt) {
            for (const QString& teil : z.split(QLatin1Char(';'))) {
                if (teil.trimmed().startsWith(name + QLatin1Char(':'),
                                              Qt::CaseInsensitive)) { return true; }
            }
        }
        return false;
    }

private slots:
    void initTestCase() {
        QVERIFY2(!CredentialStore::isPersistent(),
                 "Im Testbetrieb darf der Schlüsselbund nicht benutzt werden");
    }

    void cleanup() {
        TciServer::setRemoteToken(QString());
        AppSettings::instance().setValue(QStringLiteral("TciAllowRemoteTx"),
                                         QStringLiteral("False"));
    }

    // ── Der eigentliche Fund: Schreibweisen, die vorbeiliefen ───────────────
    void schreibweisen_umgehen_die_sperre_nicht_data()
    {
        QTest::addColumn<QString>("befehl");
        QTest::addColumn<QString>("warum");

        QTest::newRow("klein")        << "trx:0,true;"
            << "die Form, die immer gesperrt war";
        QTest::newRow("gross")        << "TRX:0,true;"
            << "lief bis 2026-09-30 an der Sperre vorbei und tastete den Sender";
        QTest::newRow("gemischt")     << "Trx:0,True;"
            << "dieselbe Lücke, andere Schreibweise";
        QTest::newRow("leerzeichen")  << "trx :0,true;"
            << "der Verteiler trimmt den Namen, die Sperre tat es nicht";
        QTest::newRow("beides")       << "  TRX :0,TRUE;  "
            << "Schreibweise und Leerzeichen zusammen";
        QTest::newRow("tune-klein")   << "tune:0,true;"
            << "der Abstimmträger ist auch Senden";
        QTest::newRow("tune-gross")   << "TUNE:0,TRUE;"
            << "Abstimmträger in Grossschreibung";
        QTest::newRow("tune-leerz")   << "tune :0,true;"
            << "Abstimmträger mit Leerzeichen vor dem Doppelpunkt";
    }

    void schreibweisen_umgehen_die_sperre_nicht()
    {
        QFETCH(QString, befehl);
        QFETCH(QString, warum);

        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY2(aufbauen(server, client, /*sendenFrei=*/false),
                 "Aufbau gescheitert — der Prüfpunkt hätte nichts bewiesen");

        // Ab hier mitschreiben: der Init-Burst ist durch, alles Weitere ist
        // Antwort auf unseren Befehl.
        QStringList antworten;
        connect(&client, &QWebSocket::textMessageReceived,
                [&antworten](const QString& s) { antworten << s; });

        schicke(client, befehl);

        const QString name = befehl.trimmed().startsWith(QLatin1String("tu"),
                                 Qt::CaseInsensitive)
                             ? QStringLiteral("tune") : QStringLiteral("trx");
        QVERIFY2(!bestaetigungKam(antworten, name),
                 qPrintable(QStringLiteral(
                     "»%1« wurde ausgeführt — %2 (Antwort: %3)")
                     .arg(befehl, warum, antworten.join(QLatin1Char(' ')))));
        QVERIFY2(server.moxOwnerForTest() == nullptr,
                 qPrintable(QStringLiteral("»%1« hat einen Besitzer gesetzt")
                                .arg(befehl)));

        client.close();
        server.stop();
    }

    // ── Freigegeben muss es durchgehen, sonst prüft das obige nichts ────────
    void mit_freigabe_geht_senden_durch()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*sendenFrei=*/true));

        QStringList antworten;
        connect(&client, &QWebSocket::textMessageReceived,
                [&antworten](const QString& s) { antworten << s; });

        schicke(client, QStringLiteral("trx:0,true;"));
        QVERIFY2(warteAufBesitzer(server, true),
                 "Mit Freigabe muss der Sendewunsch durchgehen — sonst sperrt "
                 "der Prüfpunkt oben aus einem anderen Grund als der Sperre");
        // Aktiv auf das Echo warten, nicht auf die 80 ms aus schicke():
        // der Besitzer stand auf dem CI-Laeufer bereits, das Echo war noch
        // unterwegs (2026-10-02).
        QVERIFY2(TciTest::warteAufAntwort(antworten, QStringLiteral("trx")),
                 "Mit Freigabe muss der Server den Sendewunsch auch bestätigen "
                 "— sonst misst der Prüfpunkt oben nichts");

        client.close();
        server.stop();
    }

    // ── Der Abstimmträger bekommt einen Besitzer ────────────────────────────
    //
    // Das ist die zweite Hälfte des Fundes: `tune:` lief ohne Besitzer, also
    // griff weder das Entkeyen beim Trennen (prüft m_moxOwner == ws) noch der
    // Sendezeit-Deckel (steigt bei m_moxOwner.isNull() aus). Ein Träger vom
    // Telefon stand unbegrenzt auf der Antenne, sobald das WLAN abriss.
    void abstimmtraeger_bekommt_einen_besitzer()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*sendenFrei=*/true));

        QVERIFY(server.moxOwnerForTest() == nullptr);
        schicke(client, QStringLiteral("tune:0,true;"));
        QVERIFY2(warteAufBesitzer(server, true),
                 "Der Abstimmträger muss einen Besitzer eintragen — sonst "
                 "nimmt ihn niemand zurück, wenn der Client verschwindet");

        schicke(client, QStringLiteral("tune:0,false;"));
        QVERIFY2(warteAufBesitzer(server, false),
                 "Nach tune-off muss der Besitzer wieder frei sein");

        client.close();
        server.stop();
    }

    // ── Verschwindet der Client, wird der Träger zurückgenommen ─────────────
    void traeger_wird_beim_trennen_zurueckgenommen()
    {
        TciServer server(nullptr);
        QSignalSpy freigegeben(&server, &TciServer::moxReleasedOnClientLoss);
        {
            QWebSocket client;
            QVERIFY(aufbauen(server, client, /*sendenFrei=*/true));
            schicke(client, QStringLiteral("tune:0,true;"));
            QVERIFY(warteAufBesitzer(server, true));
            client.close();
            QTest::qWait(200);
        }
        // Ohne RadioModel meldet releaseMoxHeldBy() die Buchführung trotzdem;
        // entscheidend ist, dass der Besitzer weg ist und das Signal fiel.
        QVERIFY2(warteAufBesitzer(server, false),
                 "Nach dem Trennen darf kein Besitzer mehr eingetragen sein");
        QVERIFY2(freigegeben.count() >= 1,
                 "Das Trennen muss die Rücknahme auslösen — sonst bleibt der "
                 "Träger auf der Antenne");
        server.stop();
    }

    // ── Eine Abfrage darf den Traeger nicht entsichern ──────────────────────
    //
    // Der Fund, der diesen Pruefpunkt erzwungen hat, steckte in der Reparatur
    // vom selben Tag. Der else-Zweig griff bei JEDEM tune-Rahmen, der nicht
    // `,true` war — also auch bei der reinen Statusabfrage `tune:0;`, die ein
    // fremder Client voellig zu Recht schickt. Und die Bedingung
    // `mox() && !isTune()` war waehrend des Traegers zwangslaeufig falsch.
    //
    // Zusammen: nach einem `tune:0;` mitten im Traeger lief der Sender ohne
    // Besitzer, ohne Wachhund und ohne Sendezeit-Deckel weiter. Genau der
    // Zustand, den der Fix eine Stunde vorher geschlossen hatte.
    void abfrage_entsichert_den_traeger_nicht()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*sendenFrei=*/true));

        schicke(client, QStringLiteral("tune:0,true;"));
        QVERIFY2(warteAufBesitzer(server, true),
                 "Aufbau: der Traeger muss einen Besitzer haben");

        // Die reine Abfrage. Der Verteiler laesst den Traeger stehen, also
        // muss auch die Buchfuehrung stehen bleiben.
        schicke(client, QStringLiteral("tune:0;"));
        QVERIFY2(server.moxOwnerForTest() != nullptr,
                 "Eine Abfrage »tune:0;« hat dem laufenden Traeger den "
                 "Besitzer genommen — damit faellt auch Wachhund und "
                 "Sendezeit-Deckel weg");

        // Ein Tippfehler ebenso wenig.
        schicke(client, QStringLiteral("tune:0,1;"));
        QVERIFY2(server.moxOwnerForTest() != nullptr,
                 "»tune:0,1;« ist kein Abschalten und darf nichts freigeben");

        // Erst das ausdrueckliche Abschalten gibt frei.
        schicke(client, QStringLiteral("tune:0,false;"));
        QVERIFY2(warteAufBesitzer(server, false),
                 "Nach »tune:0,false;« muss der Besitzer frei sein");

        client.close();
        server.stop();
    }

    // ── Ohne Anmeldung geht gar nichts ──────────────────────────────────────
    void ohne_anmeldung_kein_senden()
    {
        AppSettings::instance().setValue(QStringLiteral("TciAllowRemoteTx"),
                                         QStringLiteral("True"));
        QVERIFY(TciServer::setRemoteToken(QString::fromLatin1(kToken)));

        TciServer server(nullptr);
        QVERIFY(server.start(0));
        server.setTreatAllClientsAsRemoteForTest(true);

        QWebSocket client;
        QSignalSpy verbunden(&client, &QWebSocket::connected);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY(verbunden.wait(2000));

        // Kein auth: — und trotz gesetzter Sendefreigabe darf nichts passieren.
        schicke(client, QStringLiteral("trx:0,true;"));
        QVERIFY2(server.moxOwnerForTest() == nullptr,
                 "Ohne Anmeldung darf auch bei freigegebenem Fernsenden nicht "
                 "gesendet werden");

        client.close();
        server.stop();
    }
};

QTEST_MAIN(TestTciSendesperre)
#include "tst_tci_sendesperre.moc"

#else  // !HAVE_WEBSOCKETS
int main() { return 0; }
#endif
