// Prüfstand: ins Logbuch SEHEN, vom Telefon aus.
//
// Zwei Longpath-eigene TCI-Befehle (2026-10-04), Gegenstück zu `log_qso:`:
//
//     log_last:<n>;   -> log_qso_zeile:<nr>,<datum>,<zeit>,<ruf>,<band>,
//                            <mode>,<rst_s>,<rst_r>;   je Kontakt
//                        log_last_ok:<anzahl>;
//     log_dup:<ruf>;  -> log_dup_ok:<ruf>,<anzahl>,<datum>,<zeit>,<band>,
//                            <mode>,<gleiches band 0|1>,<dupe 0|1>;
//
// Geprüft werden die Zusagen, auf denen die Seite aufbaut:
//
//   * Die ANZAHL in der Abschlusszeile stimmt mit den gesendeten Zeilen
//     überein. Daran hängt alles: die Seite zeigt eine Liste nur, wenn
//     beide gleich sind, weil eine kürzere Liste wie ein kürzeres Logbuch
//     aussieht und niemand es merkt.
//   * Der jüngste Kontakt steht ZUERST. Auf einem Telefon sieht man die
//     ersten drei Zeilen; stünden dort die ältesten, wäre die Liste
//     nutzlos und trotzdem nicht falsch.
//   * Ohne Anmeldung kommt nichts — auch keine Antwort.
//   * Abgeschaltet (TciAllowRemoteLog=False) wird abgelehnt, und zwar MIT
//     Grund. Wer im Logbuch lesen darf, sieht jedes Rufzeichen und jede
//     Zeit darin; das ist nicht weniger heikel als anhängen, und ein
//     stummes Nein ist von einem Defekt nicht zu unterscheiden.
//   * `log_last:` ist gedeckelt. `log_last:100000` würde die Steuerleitung
//     mit Zeilen füllen, hinter denen jede Bedienung wartet.
//
// Die Datei liegt im Prüf-Sandkasten (QStandardPaths-Testmodus), nicht im
// echten Logbuch: der Stand schreibt Kontakte, um sie danach zu lesen.

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

class TestTciLogbuchLesen : public QObject {
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
                    if (m.startsWith(QStringLiteral("log_"))) {
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

    /// Wartet auf eine Zeile, die so anfängt — auf das EREIGNIS, nicht auf
    /// Millisekunden.
    bool warteAuf(const QString& anfang, int grenzeMs = 4000)
    {
        QElapsedTimer uhr; uhr.start();
        while (uhr.elapsed() < grenzeMs) {
            for (const QString& a : m_antworten) {
                if (a.startsWith(anfang)) { return true; }
            }
            QTest::qWait(25);
        }
        return false;
    }

    QStringList zeilenMit(const QString& anfang) const
    {
        QStringList l;
        for (const QString& a : m_antworten) {
            if (a.startsWith(anfang)) { l << a; }
        }
        return l;
    }

    /// Legt `n` Kontakte über `log_qso:` an — derselbe Weg, den das Telefon
    /// nimmt. Kein zweiter Schreiber im Prüfstand.
    bool eintragen(QWebSocket& client, const QStringList& rufe)
    {
        for (const QString& r : rufe) {
            m_antworten.clear();
            client.sendTextMessage(QStringLiteral("log_qso:%1;").arg(r));
            if (!warteAuf(QStringLiteral("log_qso_ok:"))) { return false; }
            // Eine Sekunde Abstand, damit die Zeiten unterscheidbar sind:
            // TIME_ON hat Sekundenauflösung, und bei gleicher Zeit könnte
            // die Reihenfolge nicht belegt werden.
            QTest::qWait(1100);
        }
        m_antworten.clear();
        return true;
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

    /// Der Normalfall: drei von fünf, jüngster zuerst, Anzahl stimmt.
    void dieLetztenKommenJuengsterZuerst()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY2(aufbauen(server, client, /*loggenFrei=*/true), "Aufbau gescheitert");
        QVERIFY(eintragen(client, {QStringLiteral("OE1AAA"),
                                   QStringLiteral("OE2BBB"),
                                   QStringLiteral("OE3CCC")}));

        client.sendTextMessage(QStringLiteral("log_last:2;"));
        QVERIFY2(warteAuf(QStringLiteral("log_last_ok:")), "Keine Abschlusszeile");

        const QStringList zeilen = zeilenMit(QStringLiteral("log_qso_zeile:"));
        const QStringList ende   = zeilenMit(QStringLiteral("log_last_ok:"));
        QCOMPARE(ende.size(), 1);
        QCOMPARE(zeilen.size(), 2);
        // Die Zusage, auf der die Seite aufbaut.
        QCOMPARE(ende.first(), QStringLiteral("log_last_ok:2;"));

        // Jüngster zuerst, und die Nummern beginnen bei 0.
        QVERIFY2(zeilen.at(0).contains(QStringLiteral("OE3CCC")),
                 qPrintable(zeilen.at(0)));
        QVERIFY2(zeilen.at(1).contains(QStringLiteral("OE2BBB")),
                 qPrintable(zeilen.at(1)));
        QVERIFY2(zeilen.at(0).startsWith(QStringLiteral("log_qso_zeile:0,")),
                 qPrintable(zeilen.at(0)));

        // Acht Felder, wie die Seite sie liest.
        const QStringList f =
            zeilen.at(0).mid(QStringLiteral("log_qso_zeile:").size())
                .chopped(1).split(QLatin1Char(','));
        QCOMPARE(f.size(), 8);
        QCOMPARE(f.at(3), QStringLiteral("OE3CCC"));
        QVERIFY2(f.at(1).size() == 8, qPrintable(f.at(1)));   // yyyymmdd
        QVERIFY2(f.at(2).size() == 6, qPrintable(f.at(2)));   // hhmmss
    }

    /// Ein leeres Logbuch ist eine Antwort, kein Schweigen.
    void leeresLogbuchSagtNull()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*loggenFrei=*/true));
        QVERIFY(!QFile::exists(LogbookDatei::pfad()));

        client.sendTextMessage(QStringLiteral("log_last:10;"));
        QVERIFY2(warteAuf(QStringLiteral("log_last_ok:")),
                 "Ein leeres Logbuch wurde mit Schweigen beantwortet");
        QCOMPARE(zeilenMit(QStringLiteral("log_qso_zeile:")).size(), 0);
        QCOMPARE(zeilenMit(QStringLiteral("log_last_ok:")).first(),
                 QStringLiteral("log_last_ok:0;"));
    }

    /// Ohne Anmeldung passiert NICHTS — auch keine Antwort.
    void ohneAnmeldungKeinBlickInsLogbuch()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*loggenFrei=*/true, /*anmelden=*/false));

        client.sendTextMessage(QStringLiteral("log_last:10;"));
        client.sendTextMessage(QStringLiteral("log_dup:OE1AAA;"));
        QTest::qWait(600);   // "es darf nichts passieren" -> warten und sehen
        QVERIFY2(m_antworten.isEmpty(),
                 qPrintable(m_antworten.join(QLatin1Char(' '))));
    }

    /// Abgeschaltet heisst abgelehnt — aber MIT Grund, für beide Befehle.
    void abgeschaltetWirdAbgelehntUndGesagt()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*loggenFrei=*/false));

        client.sendTextMessage(QStringLiteral("log_last:10;"));
        QVERIFY2(warteAuf(QStringLiteral("log_last_err:")),
                 "Eine abgelehnte Abfrage blieb stumm");
        QCOMPARE(zeilenMit(QStringLiteral("log_qso_zeile:")).size(), 0);

        m_antworten.clear();
        client.sendTextMessage(QStringLiteral("log_dup:OE1AAA;"));
        QVERIFY2(warteAuf(QStringLiteral("log_dup_err:")),
                 "Eine abgelehnte Dupe-Frage blieb stumm");
    }

    /// Die Obergrenze. `log_last:100000` darf die Steuerleitung nicht
    /// fluten — und `log_last:0` muss trotzdem etwas liefern, statt mit
    /// einer leeren Liste wie ein leeres Logbuch zu wirken.
    void dieAnzahlIstGedeckelt()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*loggenFrei=*/true));
        QVERIFY(eintragen(client, {QStringLiteral("OE1AAA"),
                                   QStringLiteral("OE2BBB")}));

        client.sendTextMessage(QStringLiteral("log_last:100000;"));
        QVERIFY(warteAuf(QStringLiteral("log_last_ok:")));
        // Zwei Kontakte, also zwei Zeilen — der Deckel greift erst über 50,
        // belegt ist hier, dass die Zahl nicht durchgereicht wird.
        QCOMPARE(zeilenMit(QStringLiteral("log_qso_zeile:")).size(), 2);
        QCOMPARE(zeilenMit(QStringLiteral("log_last_ok:")).first(),
                 QStringLiteral("log_last_ok:2;"));

        m_antworten.clear();
        client.sendTextMessage(QStringLiteral("log_last:0;"));
        QVERIFY(warteAuf(QStringLiteral("log_last_ok:")));
        QCOMPARE(zeilenMit(QStringLiteral("log_qso_zeile:")).size(), 2);

        m_antworten.clear();
        client.sendTextMessage(QStringLiteral("log_last:Unfug;"));
        QVERIFY2(warteAuf(QStringLiteral("log_last_ok:")),
                 "Eine unlesbare Anzahl liess den Server schweigen");
        QCOMPARE(zeilenMit(QStringLiteral("log_qso_zeile:")).size(), 2);
    }

    /// Die Dupe-Frage: ein eingetragenes Rufzeichen ist bekannt, ein
    /// anderes nicht. Das ist der Satz, der einen Anruf entscheidet.
    void dupeFrageKenntDasEigeneLogbuch()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*loggenFrei=*/true));
        QVERIFY(eintragen(client, {QStringLiteral("OE1AAA"),
                                   QStringLiteral("OE1AAA"),
                                   QStringLiteral("OE2BBB")}));

        client.sendTextMessage(QStringLiteral("log_dup:OE1AAA;"));
        QVERIFY(warteAuf(QStringLiteral("log_dup_ok:")));
        QStringList f = zeilenMit(QStringLiteral("log_dup_ok:")).first()
                            .mid(QStringLiteral("log_dup_ok:").size())
                            .chopped(1).split(QLatin1Char(','));
        QCOMPARE(f.size(), 8);
        QCOMPARE(f.at(0), QStringLiteral("OE1AAA"));
        QCOMPARE(f.at(1), QStringLiteral("2"));          // zweimal gearbeitet
        QVERIFY2(f.at(2).size() == 8, qPrintable(f.at(2)));

        // Kleinschreibung fragt dasselbe.
        m_antworten.clear();
        client.sendTextMessage(QStringLiteral("log_dup:oe1aaa;"));
        QVERIFY(warteAuf(QStringLiteral("log_dup_ok:")));
        QVERIFY(zeilenMit(QStringLiteral("log_dup_ok:")).first()
                    .startsWith(QStringLiteral("log_dup_ok:OE1AAA,2,")));

        // Ein nie gearbeitetes Rufzeichen: bekannt als UNBEKANNT, mit 0 —
        // nicht mit Schweigen, das sich wie ein Fehler liest.
        m_antworten.clear();
        client.sendTextMessage(QStringLiteral("log_dup:OE9ZZZ;"));
        QVERIFY(warteAuf(QStringLiteral("log_dup_ok:")));
        QVERIFY2(zeilenMit(QStringLiteral("log_dup_ok:")).first()
                     .startsWith(QStringLiteral("log_dup_ok:OE9ZZZ,0,")),
                 qPrintable(zeilenMit(QStringLiteral("log_dup_ok:")).first()));
    }

    /// Kein Rufzeichen, keine Auskunft — und ein verirrter Rahmen wird
    /// nicht als Rufzeichen gesucht.
    void unsinnWirdAbgelehnt()
    {
        TciServer server(nullptr);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*loggenFrei=*/true));

        client.sendTextMessage(QStringLiteral("log_dup:;"));
        QVERIFY(warteAuf(QStringLiteral("log_dup_err:")));

        m_antworten.clear();
        client.sendTextMessage(QStringLiteral("log_dup:%1;")
                                   .arg(QString(64, QLatin1Char('X'))));
        QVERIFY(warteAuf(QStringLiteral("log_dup_err:")));
    }
};

QTEST_MAIN(TestTciLogbuchLesen)
#include "tst_tci_logbuch_lesen.moc"

#else   // HAVE_WEBSOCKETS

#include <QtTest>
class TestTciLogbuchLesen : public QObject { Q_OBJECT };
QTEST_MAIN(TestTciLogbuchLesen)
#include "tst_tci_logbuch_lesen.moc"

#endif
