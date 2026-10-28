// =================================================================
// tests/tst_dxcluster_wiederanlauf.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. Am 2026-10-14 standen im Protokoll des
// Betreibers 4730 Zeilen vom DX-Cluster -- 680 Versuche zwischen 14:30
// und 17:10, alle 14 Sekunden einer, jedes Mal "The remote host closed
// the connection". Die Treppe des Wiederanlaufs (5 s, 10, 20, 40, …,
// Deckel 60 s) hat nie die zweite Stufe erreicht.
//
// Der Grund: onConnected() setzte den Zaehler zurueck. Ein Cluster NIMMT
// die TCP-Verbindung aber an und wirft erst beim Anmelden hinaus. Jeder
// Zyklus begann damit von vorn.
//
// Dieser Prueflauf haelt die Unterscheidung fest, auf die es ankommt:
// ein offener Socket ist noch keine brauchbare Sitzung.
//
// ── Und am 2026-10-28 war es WIEDER da ──────────────────────────────
//
// 216 Versuche in 223 Minuten, wieder alle 14 Sekunden, wieder "The
// remote host closed the connection". Derselbe Fehler eine Station
// weiter: die ANMELDUNG gelingt jedes Mal. Am Knoten dxc.nc7j.com:7300
// live mitgeschrieben, was er dabei sagt:
//
//   "Reconnected as OE5SOS at 84.113.40.38, this instance is
//    disconnected"
//
// Ein DXSpider-Knoten laesst eine Sitzung je Rufzeichen zu. Fuehrte ein
// zweiter Anschluss dasselbe Rufzeichen, warfen sich beide endlos
// abwechselnd hinaus -- jede Sitzung lebte im Median 8,8 s, und
// markLoggedIn() setzte den Zaehler in JEDEM Zyklus auf 0.
//
// Die Treppe darf also erst fallen, wenn die Sitzung GEHALTEN hat.
// Socket -> Anmeldung -> Standzeit; der Rueckstellpunkt wandert auf die
// letzte Station, weil nur sie Brauchbarkeit beweist.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-14 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
//   2026-10-28 -- Der Rueckstellpunkt wandert von der Anmeldung auf die
//                 Standzeit; dazu die Hinauswurf-Erkennung. Martin
//                 Fischer (OE5SOS), KI-gestuetzt ueber Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QFile>

#include "core/DxClusterClient.h"

using namespace Longpath;

class TstDxClusterWiederanlauf : public QObject {
    Q_OBJECT

private:
    // Ein Zyklus, wie ihn ein abweisender Cluster erzeugt: TCP steht,
    // dann legt die Gegenseite auf -- ohne dass je eine Anmeldung lief.
    static void zyklusOhneAnmeldung(DxClusterClient& c)
    {
        QMetaObject::invokeMethod(&c, "onConnected", Qt::DirectConnection);
        QMetaObject::invokeMethod(&c, "onDisconnected", Qt::DirectConnection);
    }

    // Der Zyklus vom 2026-10-28: TCP steht, die Anmeldung GELINGT, und
    // der Knoten wirft trotzdem hinaus, bevor die Standzeit um ist.
    static void zyklusMitAnmeldungAberKurz(DxClusterClient& c)
    {
        QMetaObject::invokeMethod(&c, "onConnected", Qt::DirectConnection);
        c.markLoggedInForTest();
        QMetaObject::invokeMethod(&c, "onDisconnected", Qt::DirectConnection);
    }

private slots:
    void dieTreppeSteigtAuchWennTcpJedesMalSteht()
    {
        DxClusterClient c;
        QCOMPARE(c.reconnectAttemptsForTest(), 0);
        for (int i = 1; i <= 5; ++i) {
            zyklusOhneAnmeldung(c);
            QCOMPARE(c.reconnectAttemptsForTest(), i);
        }
    }

    // Der Fall vom 2026-10-28. Die Anmeldung gelingt in JEDEM Zyklus,
    // und trotzdem muss die Treppe steigen -- sonst bleibt die
    // Wartezeit bei 5 s und der Knoten bekommt 216 Anmeldungen.
    //
    // Gegenprobe: mit der alten Fassung (markLoggedIn() setzt zurueck)
    // steht hier nach fuenf Zyklen 0 statt 5.
    void dieTreppeSteigtAuchWennDieAnmeldungJedesMalGelingt()
    {
        DxClusterClient c;
        for (int i = 1; i <= 5; ++i) {
            zyklusMitAnmeldungAberKurz(c);
            QCOMPARE(c.reconnectAttemptsForTest(), i);
        }
    }

    // Erst die Standzeit setzt zurueck -- weder der Socket noch die
    // Anmeldung allein. Das ist die eigentliche Regel.
    void erstDieStandzeitSetztZurueck()
    {
        DxClusterClient c;
        zyklusOhneAnmeldung(c);
        zyklusOhneAnmeldung(c);
        QCOMPARE(c.reconnectAttemptsForTest(), 2);

        QMetaObject::invokeMethod(&c, "onConnected", Qt::DirectConnection);
        QCOMPARE(c.reconnectAttemptsForTest(), 2);   // Socket allein zaehlt nicht

        c.markLoggedInForTest();
        QCOMPARE(c.reconnectAttemptsForTest(), 2);   // Anmeldung auch nicht

        c.markSessionStableForTest();
        QCOMPARE(c.reconnectAttemptsForTest(), 0);   // erst die Standzeit
    }

    // Eine Sitzung, die schon getrennt ist, darf nicht nachtraeglich
    // als „hat gehalten" gelten -- sonst setzt ein verspaeteter
    // Taktschlag die Treppe doch noch zurueck.
    void nachDemTrennenSetztDieStandzeitNichtMehrZurueck()
    {
        DxClusterClient c;
        zyklusMitAnmeldungAberKurz(c);
        zyklusMitAnmeldungAberKurz(c);
        QCOMPARE(c.reconnectAttemptsForTest(), 2);

        c.markSessionStableForTest();                // kommt zu spaet
        QCOMPARE(c.reconnectAttemptsForTest(), 2);
    }

    // ─── Der Knoten sagt, warum er auflegt ──────────────────────────

    void derHinauswurfWirdErkannt()
    {
        // Wortlaut vom 2026-10-28, dxc.nc7j.com:7300.
        QVERIFY(DxClusterClient::istHinauswurfForTest(
            QStringLiteral("Reconnected as OE5SOS at 84.113.40.38, "
                           "this instance is disconnected")));
    }

    // ─── Das Mitschreiben ueberlebt den Sturm ───────────────────────

    void nurEineVerbindungDesBetreibersLeertDasMitschreiben()
    {
        // Bis zum 2026-10-28 wurde bei JEDER Verbindung geleert. Bei
        // 216 Neuverbindungen loeschte das Mitschreiben damit genau
        // die Belege, die den Sturm erklaert haetten.
        const auto neu  = DxClusterClient::protokollOeffnungsartForTest(false);
        const auto fort = DxClusterClient::protokollOeffnungsartForTest(true);

        QVERIFY(neu.testFlag(QIODevice::Truncate));
        QVERIFY(!neu.testFlag(QIODevice::Append));

        QVERIFY(fort.testFlag(QIODevice::Append));
        QVERIFY2(!fort.testFlag(QIODevice::Truncate),
                 "ein selbsttaetiger Wiederanlauf leert das Mitschreiben "
                 "-- dann ist beim naechsten Sturm wieder nichts zu holen");

        // Beide schreiben, und beide als Text.
        for (const auto& m : {neu, fort}) {
            QVERIFY(m.testFlag(QIODevice::WriteOnly));
            QVERIFY(m.testFlag(QIODevice::Text));
        }
    }

    void einWiederanlaufWeissDassErEinerIst()
    {
        DxClusterClient c;
        c.setLogFileName(QStringLiteral("tst_wiederanlauf.log"));

        // Die erste Verbindung ist keine Fortsetzung.
        QVERIFY(!c.istWiederanlaufForTest());

        QMetaObject::invokeMethod(&c, "onConnected", Qt::DirectConnection);
        QMetaObject::invokeMethod(&c, "onDisconnected", Qt::DirectConnection);
        QVERIFY2(c.istWiederanlaufForTest(),
                 "nach einem unbeabsichtigten Abriss steht ein Wiederanlauf "
                 "an -- der darf das Mitschreiben nicht leeren");

        // Und das naechste onConnected verbraucht die Fahne wieder.
        QMetaObject::invokeMethod(&c, "onConnected", Qt::DirectConnection);
        QVERIFY(!c.istWiederanlaufForTest());

        // Der Beleg auf Dateiebene: nach Verbindung -> Abriss ->
        // Wiederanlauf muessen BEIDE Kopfzeilen in der Datei stehen.
        // Mit der alten Fassung stand nur die letzte darin, und genau
        // deshalb war vom Sturm des Vormittags nichts mehr zu holen.
        QFile f(c.logFilePath());
        QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("Mitschreibdatei nicht lesbar: %1")
                                .arg(c.logFilePath())));
        const QString inhalt = QString::fromUtf8(f.readAll());
        QCOMPARE(inhalt.count(QStringLiteral("--- Connected to")), 2);
    }

    void gewolltesTrennenBeendetDieSitzung()
    {
        // Wer selbst trennt, faengt beim naechsten Mal neu an -- dann
        // SOLL geleert werden.
        DxClusterClient c;
        c.setLogFileName(QStringLiteral("tst_wiederanlauf.log"));
        QMetaObject::invokeMethod(&c, "onConnected", Qt::DirectConnection);
        QMetaObject::invokeMethod(&c, "onDisconnected", Qt::DirectConnection);
        QVERIFY(c.istWiederanlaufForTest());

        c.disconnect();
        QVERIFY(!c.istWiederanlaufForTest());
    }

    void einSpotIstKeinHinauswurf()
    {
        // Die Gegenprobe: eine gewoehnliche Spotzeile und die
        // Begruessung duerfen die Warnung nicht ausloesen.
        QVERIFY(!DxClusterClient::istHinauswurfForTest(
            QStringLiteral("DX de SP3POB:     3721.0  SP1550SK     CQ CQ   1609Z")));
        QVERIFY(!DxClusterClient::istHinauswurfForTest(
            QStringLiteral("Hello OE5SOS, this is NG7M-1 in Syracuse,Utah")));
        QVERIFY(!DxClusterClient::istHinauswurfForTest(QString()));
    }
};

QTEST_MAIN(TstDxClusterWiederanlauf)
#include "tst_dxcluster_wiederanlauf.moc"
