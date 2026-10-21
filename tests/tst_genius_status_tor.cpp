// =================================================================
// tests/tst_genius_status_tor.cpp  (Longpath)
// =================================================================
// Longpath-eigener Prüfstand. Kein Thetis-Gegenstück: die Strecke zu
// den 4O3A-Geräten ist eine Longpath/AetherSDR-Angelegenheit.
//
// Eine `status`-Abfrage zur Zeit auf der Strecke zum Power Genius XL
// und zum Tuner Genius XL (src/core/GeniusStatusGate.h). Der Befund
// stammt vom Upstream NereusSDR (KG4VCF, 2026-10-12): der Verstärker
// legt die Verbindung auf, wenn eine zweite Abfrage hinausgeht, bevor
// die erste beantwortet ist — unter Windows typisch 90 s nach dem
// Verbinden, weil dort Keepalive und Meterabfrage zusammenfallen.
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Angelegt. Longpath, KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/GeniusStatusGate.h"
#include "core/PgxlConnection.h"
#include "core/TgxlConnection.h"

class TestGeniusStatusTor : public QObject {
    Q_OBJECT
private slots:
    // Das Tor selbst.
    void tor_haelt_bis_zur_antwort()
    {
        Longpath::GeniusStatusGate tor;
        QVERIFY(!tor.answerPending());
        tor.sent(7);
        QVERIFY(tor.answerPending());
        // Eine FREMDE Folgenummer oeffnet nichts.
        tor.answered(6);
        QVERIFY(tor.answerPending());
        tor.answered(7);
        QVERIFY(!tor.answerPending());
    }

    void tor_gibt_nach_der_frist_auf()
    {
        Longpath::GeniusStatusGate tor;
        tor.setReplyWaitMsForTesting(30);
        tor.sent(1);
        QVERIFY(tor.answerPending());
        QTest::qWait(60);
        // Eine verschluckte Antwort darf die Anzeigen nicht fuer immer
        // stillstellen — das ist der Unterschied zwischen einem Tor und
        // einem Riegel.
        QVERIFY(!tor.answerPending());
    }

    void neue_verbindung_oeffnet_das_tor()
    {
        Longpath::GeniusStatusGate tor;
        tor.sent(42);
        QVERIFY(tor.answerPending());
        tor.reset();
        QVERIFY(!tor.answerPending());
    }

    // Und die Strecke: zwei `status` hintereinander ergeben EINEN Rahmen.
    void verstaerker_schickt_den_zweiten_status_nicht()
    {
        Longpath::PgxlConnection conn;
        QSignalSpy rahmen(&conn,
            &Longpath::PgxlConnection::testFrameWrittenForTesting);

        const quint32 ersteSeq = conn.sendCommand(QStringLiteral("status"));
        QVERIFY(ersteSeq != 0);
        QCOMPARE(rahmen.count(), 1);

        // Der zweite geht NICHT hinaus, solange die Antwort fehlt.
        QCOMPARE(conn.sendCommand(QStringLiteral("status")), 0u);
        QCOMPARE(rahmen.count(), 1);

        // Ein anderer Befehl ist davon unberuehrt — es wird nur `status`
        // gestaffelt, nicht die Strecke angehalten.
        QVERIFY(conn.sendCommand(QStringLiteral("info")) != 0);
        QCOMPARE(rahmen.count(), 2);

        // Antwort des Geraets: jetzt darf der naechste.
        conn.injectLineForTesting(
            QStringLiteral("R%1|0|fwd=0.0 swr=1.0").arg(ersteSeq));
        QVERIFY(conn.sendCommand(QStringLiteral("status")) != 0);
        QCOMPARE(rahmen.count(), 3);
    }

    void tuner_antwortet_mit_einem_s_rahmen()
    {
        Longpath::TgxlConnection conn;
        QSignalSpy rahmen(&conn,
            &Longpath::TgxlConnection::testFrameWrittenForTesting);

        const quint32 ersteSeq = conn.sendCommand(QStringLiteral("status"));
        QVERIFY(ersteSeq != 0);
        QCOMPARE(conn.sendCommand(QStringLiteral("status")), 0u);
        QCOMPARE(rahmen.count(), 1);

        // Der Tuner beantwortet `status` mit `S<seq>|status …`, nicht mit
        // einem R-Rahmen. Wer nur den R-Zweig anfasst, laesst den Tuner
        // nach der ersten Abfrage stillstehen.
        conn.injectLineForTesting(
            QStringLiteral("S%1|status fwd=0.0000 swr=1.0000").arg(ersteSeq));
        QVERIFY(conn.sendCommand(QStringLiteral("status")) != 0);
        QCOMPARE(rahmen.count(), 2);
    }
};

QTEST_MAIN(TestGeniusStatusTor)
#include "tst_genius_status_tor.moc"
