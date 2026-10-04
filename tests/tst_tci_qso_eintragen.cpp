// Prüfstand: ein QSO vom Telefon landet im Logbuch — und nur eines, das
// hingehört.
//
// Longpath-eigener TCI-Befehl (wie `spectrum_span`):
//
//     log_qso:<rufzeichen>[,<rst gesendet>[,<rst empfangen>]];
//     -> log_qso_ok:<rufzeichen>;   oder   log_qso_err:<grund>;
//
// Geprüft werden die Zusagen, auf denen das steht:
//   * ohne Anmeldung kommt gar nichts durch,
//   * abgeschaltet (TciAllowRemoteLog=False) wird abgelehnt — mit Antwort,
//     damit die Seite es sagen kann statt zu behaupten,
//   * ein leeres oder unsinnig langes Rufzeichen wird abgelehnt,
//   * ein gültiger Eintrag steht danach wirklich in der Datei.
//
// Die Datei liegt im Prüf-Sandkasten (QStandardPaths-Testmodus), nicht im
// echten Logbuch — das ist hier keine Formalie: der Prüfstand schreibt
// Kontakte.

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include <QFile>

#include "core/AppSettings.h"
#include "core/LogbookDatei.h"
#include "core/TciServer.h"
#include "TciBurstHelfer.h"

using Longpath::AppSettings;
using Longpath::TciServer;
namespace LogbookDatei = Longpath::LogbookDatei;

namespace {
constexpr char kToken[] = "PRUEFTOKEN1234";
}

class TestTciQsoEintragen : public QObject {
    Q_OBJECT

private:
    QStringList m_antworten;

    bool aufbauen(TciServer& server, QWebSocket& client, bool loggenFrei,
                  bool anmelden = true)
    {
        AppSettings::instance().setValue(QStringLiteral("TciAllowRemoteLog"),
            loggenFrei ? QStringLiteral("True") : QStringLiteral("False"));
        if (!TciServer::setRemoteToken(QString::fromLatin1(kToken))) { return false; }
        if (!server.start(0)) { return false; }
        // Jede Verbindung als Netzverbindung behandeln — sonst greift die
        // Freigabe im Testlauf nie, weil alles über Loopback läuft.
        server.setTreatAllClientsAsRemoteForTest(true);

        connect(&client, &QWebSocket::textMessageReceived, this,
                [this](const QString& m) {
                    if (m.startsWith(QStringLiteral("log_qso"))) {
                        m_antworten << m;
                    }
                });

        QSignalSpy verbunden(&client, &QWebSocket::connected);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        if (!verbunden.wait(2000)) { return false; }
        if (!anmelden) { return true; }

        client.sendTextMessage(QStringLiteral("auth:%1;").arg(QLatin1String(kToken)));
        return TciTest::warteAufReady(client);
    }

    /// Wartet auf eine Antwort — auf das EREIGNIS, nicht auf Millisekunden.
    bool warteAufAntwort(int grenzeMs = 4000)
    {
        QElapsedTimer uhr; uhr.start();
        while (m_antworten.isEmpty() && uhr.elapsed() < grenzeMs) {
            QTest::qWait(25);
        }
        return !m_antworten.isEmpty();
    }

    static QString dateiInhalt()
    {
        QFile f(LogbookDatei::pfad());
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return {}; }
        return QString::fromUtf8(f.readAll());
    }

private slots:

    void init()
    {
        m_antworten.clear();
        QFile::remove(LogbookDatei::pfad());
    }

    void cleanup()
    {
        TciServer::setRemoteToken(QString());
        QFile::remove(LogbookDatei::pfad());
    }

    /// Der Normalfall: ein Rufzeichen genügt, der Rest kommt vom Gerät.
    void einGueltigesQsoLandetInDerDatei()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY2(aufbauen(server, client, /*loggenFrei=*/true), "Aufbau gescheitert");

        client.sendTextMessage(QStringLiteral("log_qso:oe3abc,59,57;"));
        QVERIFY2(warteAufAntwort(), "Keine Antwort auf log_qso");
        QCOMPARE(m_antworten.size(), 1);
        QVERIFY2(m_antworten.first().startsWith(QStringLiteral("log_qso_ok:")),
                 qPrintable(m_antworten.first()));

        const QString inhalt = dateiInhalt();
        QVERIFY2(inhalt.contains(QStringLiteral("OE3ABC")),
                 "Das Rufzeichen steht nicht in der Datei");
        QVERIFY2(inhalt.contains(QStringLiteral("<EOH>")),
                 "Der ADIF-Kopf fehlt — strenge Importeure lehnen das ab");
        QVERIFY2(inhalt.contains(QStringLiteral("59")), "RST fehlt");
        // Kleinschreibung muss ankommen wie im Logbuch ueblich: GROSS.
        QVERIFY2(!inhalt.contains(QStringLiteral("oe3abc")),
                 "Das Rufzeichen wurde nicht normalisiert");
    }

    /// Ohne Anmeldung passiert NICHTS — auch keine Antwort.
    void ohneAnmeldungKeinEintrag()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*loggenFrei=*/true, /*anmelden=*/false));

        client.sendTextMessage(QStringLiteral("log_qso:OE3ABC;"));
        QTest::qWait(600);   // "es darf nichts passieren" -> warten und sehen
        QVERIFY2(m_antworten.isEmpty(), "Ein nicht angemeldeter Client bekam Antwort");
        QVERIFY2(!QFile::exists(LogbookDatei::pfad()),
                 "Ein nicht angemeldeter Client hat ins Logbuch geschrieben");
    }

    /// Abgeschaltet heisst abgelehnt — aber MIT Antwort. Eine Seite, die
    /// nicht erfaehrt, dass es nicht ging, behauptet sonst den Erfolg.
    void abgeschaltetWirdAbgelehntUndGesagt()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*loggenFrei=*/false));

        client.sendTextMessage(QStringLiteral("log_qso:OE3ABC;"));
        QVERIFY2(warteAufAntwort(), "Keine Antwort auf einen abgelehnten Eintrag");
        QVERIFY2(m_antworten.first().startsWith(QStringLiteral("log_qso_err:")),
                 qPrintable(m_antworten.first()));
        QVERIFY2(!QFile::exists(LogbookDatei::pfad()),
                 "Trotz Ablehnung wurde geschrieben");
    }

    /// Kein Rufzeichen, kein Kontakt.
    void leeresRufzeichenWirdAbgelehnt()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*loggenFrei=*/true));

        client.sendTextMessage(QStringLiteral("log_qso:,59,59;"));
        QVERIFY(warteAufAntwort());
        QVERIFY2(m_antworten.first().startsWith(QStringLiteral("log_qso_err:")),
                 qPrintable(m_antworten.first()));
        QVERIFY(!QFile::exists(LogbookDatei::pfad()));
    }

    /// Ein verirrter Rahmen darf nicht als Rufzeichen in der Datei landen.
    void unsinnigLangesRufzeichenWirdAbgelehnt()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*loggenFrei=*/true));

        client.sendTextMessage(QStringLiteral("log_qso:%1;")
                                   .arg(QString(64, QLatin1Char('X'))));
        QVERIFY(warteAufAntwort());
        QVERIFY2(m_antworten.first().startsWith(QStringLiteral("log_qso_err:")),
                 qPrintable(m_antworten.first()));
        QVERIFY(!QFile::exists(LogbookDatei::pfad()));
    }

    /// Zwei Eintraege haengen aneinander, der Kopf steht nur einmal.
    void zweiEintraegeEinKopf()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*loggenFrei=*/true));

        client.sendTextMessage(QStringLiteral("log_qso:DL1AAA;"));
        QVERIFY(warteAufAntwort());
        m_antworten.clear();
        client.sendTextMessage(QStringLiteral("log_qso:S57BBB;"));
        QVERIFY(warteAufAntwort());

        const QString inhalt = dateiInhalt();
        QVERIFY(inhalt.contains(QStringLiteral("DL1AAA")));
        QVERIFY(inhalt.contains(QStringLiteral("S57BBB")));
        QCOMPARE(inhalt.count(QStringLiteral("<EOH>")), 1);
    }
};

QTEST_MAIN(TestTciQsoEintragen)
#include "tst_tci_qso_eintragen.moc"

#else   // HAVE_WEBSOCKETS

#include <QtTest>
class TestTciQsoEintragen : public QObject { Q_OBJECT };
QTEST_MAIN(TestTciQsoEintragen)
#include "tst_tci_qso_eintragen.moc"

#endif
