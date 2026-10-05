// Prüfstand: die Spots, die das Telefon bekommt — und die, die es nicht
// bekommen darf.
//
// Longpath-eigener TCI-Befehl (2026-10-05):
//
//     spots:<rx>;  -> spot_zeile:<nr>,<hz>,<ruf>,<mode>,<quelle>,<alter>;
//                     spots_ok:<anzahl>;
//                  oder spots_err:<grund>;
//
// Die Auswahl selbst steht in `tst_spot_auswahl` — hier geht es um den Weg
// durch den Server, und der hat eigene Zusagen:
//
//   * Ohne Anmeldung kommt gar nichts. Spots sind zwar öffentliche
//     Clustermeldungen, aber die Steuerleitung ist es nicht.
//   * Die Anzahl in der Abschlusszeile stimmt mit den gesendeten Zeilen
//     überein — dieselbe Zusage wie bei `log_last:`, aus demselben Grund:
//     eine kürzere Liste sieht aus wie ein leereres Band.
//   * Ein fehlender Empfänger wird als FEHLER gemeldet, nicht als leere
//     Liste. "Ich kann gerade nicht" und "da ist nichts" sind zwei
//     verschiedene Auskünfte, und nur eine davon heißt: weiterdrehen.
//
// Ohne abonniertes Spektrum kennt der Server die Spanne nicht; dann gilt
// die Vorgabe von 48 kHz (SpotAuswahl::kSpanneVorgabeHz). Genau darauf
// sind die Frequenzen hier gerechnet.

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>

#include "core/TciServer.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/SpotModel.h"
#include "TciBurstHelfer.h"

using namespace Longpath;

namespace {
constexpr char kToken[] = "PRUEFTOKEN1234";
constexpr double kMitteHz = 14'100'000.0;
}

class TestTciSpots : public QObject {
    Q_OBJECT

private:
    QStringList m_antworten;

    bool aufbauen(TciServer& server, QWebSocket& client, bool anmelden = true)
    {
        if (!TciServer::setRemoteToken(QString::fromLatin1(kToken))) { return false; }
        if (!server.start(0)) { return false; }
        server.setTreatAllClientsAsRemoteForTest(true);

        connect(&client, &QWebSocket::textMessageReceived, this,
                [this](const QString& m) {
                    if (m.startsWith(QStringLiteral("spot"))) { m_antworten << m; }
                });

        QSignalSpy verbunden(&client, &QWebSocket::connected);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        if (!verbunden.wait(2000)) { return false; }
        if (!anmelden) { return true; }

        client.sendTextMessage(QStringLiteral("auth:%1;").arg(QLatin1String(kToken)));
        return TciTest::warteAufReady(client);
    }

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

    static void spotten(SpotModel* m, int index, const QString& ruf, double mhz,
                        const QString& quelle = QStringLiteral("CLUSTER"))
    {
        QMap<QString, QString> kvs;
        kvs.insert(QStringLiteral("callsign"), ruf);
        kvs.insert(QStringLiteral("rx_freq"), QString::number(mhz, 'f', 6));
        kvs.insert(QStringLiteral("mode"), QStringLiteral("CW"));
        kvs.insert(QStringLiteral("source"), quelle);
        kvs.insert(QStringLiteral("lifetime_seconds"), QStringLiteral("1800"));
        m->applySpotStatus(index, kvs);
    }

private slots:

    void init() { m_antworten.clear(); }
    void cleanup() { TciServer::setRemoteToken(QString()); }

    /// Der Normalfall: was im Bild liegt, kommt — nach Frequenz, mit
    /// stimmender Anzahl.
    void nurDieSpotsImBild()
    {
        RadioModel model;
        model.addSlice();
        QVERIFY(model.activeSlice());
        model.activeSlice()->setFrequency(kMitteHz);

        SpotModel* sm = model.spotModel();
        QVERIFY(sm);
        spotten(sm, 1, QStringLiteral("OE1DX"), 14.110);
        spotten(sm, 2, QStringLiteral("OE2DX"), 14.090);
        spotten(sm, 3, QStringLiteral("WEIT"),   7.030);   // anderes Band
        spotten(sm, 4, QStringLiteral("FERN"),  14.200);   // zu weit

        TciServer server(&model);
        QWebSocket client;
        QVERIFY2(aufbauen(server, client), "Aufbau gescheitert");

        client.sendTextMessage(QStringLiteral("spots:0;"));
        QVERIFY2(warteAuf(QStringLiteral("spots_ok:")), "Keine Abschlusszeile");

        const QStringList z = zeilenMit(QStringLiteral("spot_zeile:"));
        QCOMPARE(z.size(), 2);
        QCOMPARE(zeilenMit(QStringLiteral("spots_ok:")).first(),
                 QStringLiteral("spots_ok:2;"));

        // Nach Frequenz, und die Nummern beginnen bei 0.
        QVERIFY2(z.at(0).startsWith(QStringLiteral("spot_zeile:0,14090000,OE2DX,CW,CLUSTER,")),
                 qPrintable(z.at(0)));
        QVERIFY2(z.at(1).startsWith(QStringLiteral("spot_zeile:1,14110000,OE1DX,CW,CLUSTER,")),
                 qPrintable(z.at(1)));

        // Sechs Felder, wie die Seite sie liest.
        const QStringList f = z.at(0).mid(QStringLiteral("spot_zeile:").size())
                                  .chopped(1).split(QLatin1Char(','));
        QCOMPARE(f.size(), 6);
    }

    /// Ein leeres Band ist eine Antwort, kein Schweigen.
    void keineSpotsSagtNull()
    {
        RadioModel model;
        model.addSlice();
        model.activeSlice()->setFrequency(kMitteHz);

        TciServer server(&model);
        QWebSocket client;
        QVERIFY(aufbauen(server, client));

        client.sendTextMessage(QStringLiteral("spots:0;"));
        QVERIFY2(warteAuf(QStringLiteral("spots_ok:")),
                 "Ein leeres Band wurde mit Schweigen beantwortet");
        QCOMPARE(zeilenMit(QStringLiteral("spot_zeile:")).size(), 0);
        QCOMPARE(zeilenMit(QStringLiteral("spots_ok:")).first(),
                 QStringLiteral("spots_ok:0;"));
    }

    /// Ohne Anmeldung passiert NICHTS — auch keine Antwort.
    void ohneAnmeldungKeineSpots()
    {
        RadioModel model;
        model.addSlice();
        model.activeSlice()->setFrequency(kMitteHz);
        spotten(model.spotModel(), 1, QStringLiteral("OE1DX"), 14.100);

        TciServer server(&model);
        QWebSocket client;
        QVERIFY(aufbauen(server, client, /*anmelden=*/false));

        client.sendTextMessage(QStringLiteral("spots:0;"));
        QTest::qWait(600);   // "es darf nichts passieren" -> warten und sehen
        QVERIFY2(m_antworten.isEmpty(),
                 qPrintable(m_antworten.join(QLatin1Char(' '))));
    }

    /// "Ich kann gerade nicht" ist nicht "da ist nichts". Nur eine der
    /// beiden Auskuenfte heisst: weiterdrehen.
    void fehlenderEmpfaengerIstEinFehlerKeineLeereListe()
    {
        TciServer server(nullptr);   // kein Modell
        QWebSocket client;
        QVERIFY(aufbauen(server, client));

        client.sendTextMessage(QStringLiteral("spots:0;"));
        QVERIFY2(warteAuf(QStringLiteral("spots_err:")),
                 "Ohne Modell kam keine Fehlermeldung");
        QCOMPARE(zeilenMit(QStringLiteral("spots_ok:")).size(), 0);

        // Und ein Empfaenger, den es nicht gibt, genauso.
        RadioModel model;
        model.addSlice();
        TciServer server2(&model);
        QWebSocket client2;
        m_antworten.clear();
        QVERIFY(aufbauen(server2, client2));
        client2.sendTextMessage(QStringLiteral("spots:7;"));
        QVERIFY(warteAuf(QStringLiteral("spots_err:")));
    }

    /// `spots;` ohne Empfaengernummer meint den ersten.
    void ohneNummerGiltDerErste()
    {
        RadioModel model;
        model.addSlice();
        model.activeSlice()->setFrequency(kMitteHz);
        spotten(model.spotModel(), 1, QStringLiteral("OE1DX"), 14.100);

        TciServer server(&model);
        QWebSocket client;
        QVERIFY(aufbauen(server, client));

        client.sendTextMessage(QStringLiteral("spots;"));
        QVERIFY(warteAuf(QStringLiteral("spots_ok:")));
        QCOMPARE(zeilenMit(QStringLiteral("spot_zeile:")).size(), 1);
    }
};

QTEST_MAIN(TestTciSpots)
#include "tst_tci_spots.moc"

#else   // HAVE_WEBSOCKETS

#include <QtTest>
class TestTciSpots : public QObject { Q_OBJECT };
QTEST_MAIN(TestTciSpots)
#include "tst_tci_spots.moc"

#endif
