// no-port-check: Longpath-eigen. TCI kennt weder Anmeldung noch
// Verschlüsselung — in der Spezifikation kommen auth, password, token und TLS
// kein einziges Mal vor, und weder Thetis noch deskHPSDR noch AetherSDR haben
// etwas dergleichen.
//
// ── Worum es geht ────────────────────────────────────────────────────────────
//
// Solange der Server auf 127.0.0.1 lauscht, ist das vertretbar: wer dort
// verbinden kann, sitzt am Rechner. Sobald er ins Netz geht, ist es das nicht
// mehr — ein Handy kann dann tasten, und jedes andere Gerät im WLAN auch.
//
// Der Schnitt läuft deshalb entlang der HERKUNFT, nicht entlang eines globalen
// Schalters:
//   Loopback  — unverändert. WSJT-X, JTDX, N1MM+, Log4OM und Hamlib kennen
//               kein auth: und sollen es nicht lernen müssen.
//   Netz      — muss sich anmelden, und darf erst senden, wenn der Betreiber
//               es ausdrücklich erlaubt hat.
//
// Der wichtigste Prüfpunkt hier ist deshalb nicht die Sperre, sondern dass
// Loopback sie NICHT zu spüren bekommt: eine Sicherung, die den eigenen
// Logger aussperrt, wird abgeschaltet und schützt dann gar nichts.

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QNetworkInterface>
#include <QWebSocket>

#include "core/AppSettings.h"
#include "core/TciServer.h"

using namespace Longpath;

class TestTciRemoteAuth : public QObject {
    Q_OBJECT

private:
    // Eine echte Nicht-Loopback-Adresse dieses Rechners, falls es eine gibt.
    // Im CI gibt es oft keine — dann wird der Prüfpunkt übersprungen statt
    // falsch grün zu melden.
    static QHostAddress lanAdresse() {
        const auto alle = QNetworkInterface::allAddresses();
        for (const QHostAddress& a : alle) {
            if (a.isLoopback()) { continue; }
            if (a.protocol() != QAbstractSocket::IPv4Protocol) { continue; }
            return a;
        }
        return {};
    }

private slots:
    void init() {
        AppSettings::instance().setValue(QStringLiteral("TciAllowRemoteTx"),
                                         QStringLiteral("False"));
    }

    void cleanup() {
        TciServer::setRemoteToken(QString());   // Schlüsselbund aufräumen
    }

    // ── Das Wichtigste: Loopback merkt von alldem nichts ─────────────────────

    void loopback_bekommt_den_init_burst_ohne_anmeldung() {
        TciServer::setRemoteToken(QStringLiteral("EGAL"));   // auch mit Token …

        TciServer server(nullptr);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy verbunden(&client, &QWebSocket::connected);
        QSignalSpy text(&client, &QWebSocket::textMessageReceived);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY(verbunden.wait(2000));

        // … muss ein Client von Loopback sofort den Init-Burst bekommen.
        QVERIFY2(text.wait(2000),
                 "Loopback darf keine Anmeldung brauchen — sonst steht jeder "
                 "Logger und jedes Digimode-Programm still");
        bool sahReady = false;
        for (const auto& a : text) {
            if (a.at(0).toString().contains(QStringLiteral("ready"))) { sahReady = true; }
        }
        for (int i = 0; i < 20 && !sahReady; ++i) {
            if (!text.wait(300)) { break; }
            for (const auto& a : text) {
                if (a.at(0).toString().contains(QStringLiteral("ready"))) { sahReady = true; }
            }
        }
        QVERIFY2(sahReady, "Der Init-Burst muss mit ready; enden");
    }

    // ── Token ───────────────────────────────────────────────────────────────

    void token_ist_lang_genug_und_ohne_verwechselbare_zeichen() {
        const QString t = TciServer::generateRemoteToken();
        QCOMPARE(t.size(), 32);

        // 0/O und 1/I fehlen absichtlich: das Token soll notfalls abgetippt
        // werden können, ohne dass jemand über eine Null gegen ein O stolpert.
        QVERIFY2(!t.contains(QLatin1Char('0')), "0 ist mit O zu verwechseln");
        QVERIFY2(!t.contains(QLatin1Char('O')), "O ist mit 0 zu verwechseln");
        QVERIFY2(!t.contains(QLatin1Char('1')), "1 ist mit I zu verwechseln");
        QVERIFY2(!t.contains(QLatin1Char('I')), "I ist mit 1 zu verwechseln");

        for (const QChar c : t) {
            QVERIFY2(c.isUpper() || c.isDigit(), "nur Grossbuchstaben und Ziffern");
        }
    }

    void zwei_token_sind_nie_gleich() {
        // 32 Zeichen aus 32 Möglichkeiten sind 160 Bit. Kämen zwei gleiche
        // heraus, zöge die Zufallsquelle nicht.
        QSet<QString> gesehen;
        for (int i = 0; i < 64; ++i) { gesehen.insert(TciServer::generateRemoteToken()); }
        QCOMPARE(gesehen.size(), 64);
    }

    void token_ueberlebt_speichern_und_lesen() {
        const QString t = TciServer::generateRemoteToken();
        QVERIFY(TciServer::setRemoteToken(t));
        QCOMPARE(TciServer::remoteToken(), t);

        QVERIFY(TciServer::setRemoteToken(QString()));
        QVERIFY(TciServer::remoteToken().isEmpty());
    }

    // ── Sendefreigabe ───────────────────────────────────────────────────────

    void senden_aus_dem_netz_ist_ab_werk_gesperrt() {
        AppSettings::instance().remove(QStringLiteral("TciAllowRemoteTx"));
        QVERIFY2(!TciServer::remoteTxAllowed(),
                 "Ohne ausdrückliche Freigabe darf aus dem Netz nicht gesendet werden");
    }

    void freigabe_wirkt_nur_wenn_ausdruecklich_gesetzt() {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("TciAllowRemoteTx"), QStringLiteral("True"));
        QVERIFY(TciServer::remoteTxAllowed());
        s.setValue(QStringLiteral("TciAllowRemoteTx"), QStringLiteral("False"));
        QVERIFY(!TciServer::remoteTxAllowed());
        // Irgendetwas anderes ist kein Ja.
        s.setValue(QStringLiteral("TciAllowRemoteTx"), QStringLiteral("ja"));
        QVERIFY(!TciServer::remoteTxAllowed());
        s.setValue(QStringLiteral("TciAllowRemoteTx"), QStringLiteral("true"));
        QVERIFY2(!TciServer::remoteTxAllowed(),
                 "Gross-/Kleinschreibung: das Programm schreibt True, nichts anderes gilt");
    }

    // ── Der Netzweg, wenn dieser Rechner einen hat ──────────────────────────

    void ohne_hinterlegtes_token_kommt_niemand_aus_dem_netz_herein() {
        const QHostAddress lan = lanAdresse();
        if (lan.isNull()) { QSKIP("kein Nicht-Loopback-Netzweg auf diesem Rechner"); }

        TciServer::setRemoteToken(QString());       // kein Token hinterlegt

        TciServer server(nullptr);
        if (!server.start(QHostAddress::AnyIPv4, 0)) {
            QSKIP("konnte nicht auf alle Schnittstellen binden");
        }

        QWebSocket client;
        QSignalSpy text(&client, &QWebSocket::textMessageReceived);
        client.open(QUrl(QStringLiteral("ws://%1:%2")
                             .arg(lan.toString()).arg(server.port())));

        // Die Verbindung wird angenommen und sofort wieder geschlossen — vor
        // allem darf KEIN Init-Burst kommen.
        QTest::qWait(1500);
        QCOMPARE(text.count(), 0);
    }

    void aus_dem_netz_erst_nach_auth_der_init_burst() {
        const QHostAddress lan = lanAdresse();
        if (lan.isNull()) { QSKIP("kein Nicht-Loopback-Netzweg auf diesem Rechner"); }

        const QString token = TciServer::generateRemoteToken();
        QVERIFY(TciServer::setRemoteToken(token));

        TciServer server(nullptr);
        if (!server.start(QHostAddress::AnyIPv4, 0)) {
            QSKIP("konnte nicht auf alle Schnittstellen binden");
        }

        QWebSocket client;
        QSignalSpy verbunden(&client, &QWebSocket::connected);
        QSignalSpy text(&client, &QWebSocket::textMessageReceived);
        client.open(QUrl(QStringLiteral("ws://%1:%2")
                             .arg(lan.toString()).arg(server.port())));
        QVERIFY(verbunden.wait(2000));

        // Vor der Anmeldung: Stille. Auch auf eine ganz gewöhnliche Abfrage.
        client.sendTextMessage(QStringLiteral("vfo:0,0;"));
        QTest::qWait(800);
        QCOMPARE(text.count(), 0);

        // Falsches Token: ebenfalls Stille, keine Auskunft was fehlt.
        client.sendTextMessage(QStringLiteral("auth:FALSCH;"));
        QTest::qWait(500);
        QCOMPARE(text.count(), 0);

        // Richtiges Token: auth:ok; und danach der Init-Burst.
        client.sendTextMessage(QStringLiteral("auth:%1;").arg(token));
        QVERIFY(text.wait(2000));
        bool sahOk = false, sahReady = false;
        for (int runde = 0; runde < 25 && !sahReady; ++runde) {
            for (const auto& a : text) {
                const QString m = a.at(0).toString();
                if (m.contains(QStringLiteral("auth:ok"))) { sahOk = true; }
                if (m.contains(QStringLiteral("ready")))   { sahReady = true; }
            }
            if (!sahReady) { text.wait(300); }
        }
        QVERIFY2(sahOk, "Die Anmeldung muss bestätigt werden");
        QVERIFY2(sahReady, "Nach der Anmeldung muss der Init-Burst kommen");
    }
};

QTEST_MAIN(TestTciRemoteAuth)
#include "tst_tci_remote_auth.moc"

#else   // !HAVE_WEBSOCKETS
int main() { return 0; }
#endif
