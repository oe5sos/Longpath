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
// =================================================================
// Modification history (Longpath):
//   2026-10-14 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>

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

    // Die Gegenprobe zum Fehler: erst die ANMELDUNG darf die Treppe
    // zuruecksetzen, nicht der offene Socket.
    void erstDieAnmeldungSetztZurueck()
    {
        DxClusterClient c;
        zyklusOhneAnmeldung(c);
        zyklusOhneAnmeldung(c);
        QCOMPARE(c.reconnectAttemptsForTest(), 2);

        QMetaObject::invokeMethod(&c, "onConnected", Qt::DirectConnection);
        QCOMPARE(c.reconnectAttemptsForTest(), 2);   // Socket allein zaehlt nicht

        c.markLoggedInForTest();
        QCOMPARE(c.reconnectAttemptsForTest(), 0);
    }
};

QTEST_MAIN(TstDxClusterWiederanlauf)
#include "tst_dxcluster_wiederanlauf.moc"
