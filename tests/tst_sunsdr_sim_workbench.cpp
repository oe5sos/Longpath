// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_sunsdr_sim_workbench.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// WERKBANK, keine Pruefung im ueblichen Sinn: faehrt Longpaths nativen
// SunSDR-Treiber gegen ein Messgeraet, das eine SunSDR2 QRP nachstellt
// (`~/Longpath/sunsdr-sim/sunsdr_sim.py` — aus Longpaths eigenen
// Protokollquellen geschrieben, nichts Fremdes darin), und sagt, was
// dabei herauskommt.
//
// Warum es das gibt: der QRP-Treiber ist am echten Geraet bewiesen
// (2026-08-26), aber das Geraet steht nicht immer da. Ohne Messgeraet
// lief hier seit einem Monat nichts mehr — und genau in dieser Zeit
// sind am HL2 drei echte Fehler gefunden worden, weil es dort eines
// gibt.
//
// Laeuft NUR, wenn LONGPATH_SUNSDRSIM gesetzt ist („host:port", z. B.
// 127.0.0.1:50001) — die CI hat kein Messgeraet, und ein Test, der eins
// braucht und keins findet, wuerde dort still scheitern.
//
// Stationen: Suchanfrage → Beacon → Zustandsrahmen → Empfangsstrom →
// Verbindung steht → Frequenz setzen → Lebenszeichen haelt den Strom →
// Trennen.
//
// WICHTIG fuer den Aufbau: das Messgeraet haelt die festen Ports 50001
// und 50002. Der Treiber bindet sie normalerweise selbst — auf einer
// Maschine, auf der beide laufen, geht das nicht. Darum bindet die
// Werkbank fluechtige Ports (setFixedPortBindingEnabledForTest(false));
// das Messgeraet antwortet auf den Absenderport und lernt den Datenport
// aus dem ersten Lebenszeichen.

#include <QtTest>

#include "core/ConnectionState.h"
#include "core/RadioDiscovery.h"
#include "core/SunSdrRadioConnection.h"
#include "core/sunsdr/SunSdrProtocol.h"

#include <QHostAddress>
#include <QSignalSpy>

using namespace Longpath;

namespace {

RadioInfo qrpAt(const QHostAddress& addr, quint16 port)
{
    RadioInfo info;
    info.address    = addr;
    info.port       = port;
    info.boardType  = HPSDRHW::SunSdr2Qrp;
    info.protocol   = ProtocolVersion::SunSdr;
    info.macAddress = QStringLiteral("00:00:00:00:00:00");
    info.name       = QStringLiteral("SunSDR2 QRP (Messgeraet)");
    return info;
}

} // namespace

class TstSunSdrSimWorkbench : public QObject { Q_OBJECT
private slots:
    void connect_stream_tune_disconnect()
    {
        const QString target = qEnvironmentVariable("LONGPATH_SUNSDRSIM");
        if (target.isEmpty()) {
            QSKIP("LONGPATH_SUNSDRSIM nicht gesetzt — Werkbank, kein CI-Test.");
        }
        const QStringList hp = target.split(QLatin1Char(':'));
        const QHostAddress addr(hp.value(0, QStringLiteral("127.0.0.1")));
        const quint16 port = static_cast<quint16>(
            hp.value(1, QStringLiteral("50001")).toUInt());

        SunSdrRadioConnection conn;
        // Feste Ports gehoeren dem Messgeraet, siehe Kopf.
        conn.setFixedPortBindingEnabledForTest(false);
        conn.init();

        QSignalSpy state(&conn, &RadioConnection::connectionStateChanged);
        QSignalSpy failed(&conn, &RadioConnection::connectFailed);
        QSignalSpy iq(&conn, &RadioConnection::iqDataReceived);

        // ── 1. Verbinden: Suchanfrage → Beacon → Zustandsrahmen ──────
        conn.connectToRadio(qrpAt(addr, port));
        QTRY_VERIFY_WITH_TIMEOUT(conn.state() == ConnectionState::Connected
                                 || failed.count() > 0, 12000);
        if (failed.count() > 0) {
            const auto reason = failed.first().at(0).value<ConnectFailure>();
            QFAIL(qPrintable(QStringLiteral(
                "Der Treiber kam nicht durch (Grund %1) — laeuft das "
                "Messgeraet? python3 ~/Longpath/sunsdr-sim/sunsdr_sim.py")
                .arg(int(reason))));
        }
        qInfo() << "STATE" << int(conn.state()) << "Wechsel" << state.count();

        // ── 2. Empfangsstrom ─────────────────────────────────────────
        QTRY_VERIFY_WITH_TIMEOUT(iq.count() >= 20, 10000);
        const int blocksAtStart = iq.count();
        qInfo() << "IQ-Bloecke" << blocksAtStart;

        // Die Proben muessen etwas enthalten: das Messgeraet legt einen
        // Ton bei +10 kHz und Rauschen darunter. Ein Block voll Nullen
        // waere ein stiller Ausfall, den die Blockzahl allein nicht sieht.
        const QVector<float> samples =
            iq.last().at(1).value<QVector<float>>();
        double peak = 0.0;
        for (const float v : samples) { peak = std::max(peak, double(std::abs(v))); }
        qInfo() << "Proben je Block" << samples.size() << "Spitze" << peak;
        QVERIFY2(samples.size() == SunSdr::kIqComplexPerPkt * 2,
                 "Ein Block traegt nicht 200 Probenpaare");
        QVERIFY2(peak > 0.001,
                 "Der Empfangsstrom ist still — das Messgeraet sendet einen "
                 "Ton, der Treiber gibt ihn nicht weiter");

        // ── 3. Frequenz setzen ───────────────────────────────────────
        // Der Treiber schickt sie als Kandidaten-Kodierung (Wert mal
        // zehn, niederwertig zuerst); das Messgeraet liest sie zurueck
        // und schreibt sie in sein Protokoll.
        conn.setReceiverFrequency(0, 14'200'000ULL);
        QTest::qWait(500);

        // ── 4. Das Lebenszeichen haelt den Strom ─────────────────────
        // Das echte Geraet laesst den Strom nach ~8 s ohne Lebenszeichen
        // verstummen; das Messgeraet tut dasselbe. Zehn Sekunden ohne
        // Abriss sind also der Beweis, dass der 2-s-Takt des Treibers
        // wirklich hinausgeht.
        const int blocksBeforeWait = iq.count();
        QTest::qWait(10'000);
        const int blocksAfterWait = iq.count();
        qInfo() << "IQ-Bloecke nach 10 s:" << blocksBeforeWait << "->"
                << blocksAfterWait;
        QVERIFY2(blocksAfterWait > blocksBeforeWait + 100,
                 qPrintable(QStringLiteral(
                     "Der Strom ist waehrend der zehn Sekunden verstummt "
                     "(%1 -> %2) — das Lebenszeichen erreicht das Geraet nicht")
                     .arg(blocksBeforeWait).arg(blocksAfterWait)));
        QCOMPARE(conn.state(), ConnectionState::Connected);

        // ── 4b. Wiederholte Bloecke ──────────────────────────────────
        // Das Messgeraet schickt jeden Block achtmal, so wie das Geraet
        // (an der Bank gemessen, 2026-09-23). Sieben davon muessen
        // verworfen werden — sonst bekommt die Signalverarbeitung jede
        // Probe achtmal.
        qInfo() << "DOPPELT verworfen:" << conn.duplicateBlocksDroppedForTest()
                << "bei" << iq.count() << "weitergereichten Bloecken";
        QVERIFY2(conn.duplicateBlocksDroppedForTest() > quint64(iq.count()) * 5,
                 qPrintable(QStringLiteral(
                     "Von acht Kopien je Block kamen zu wenige als Wiederholung "
                     "an (%1 verworfen, %2 weitergereicht) — laeuft das "
                     "Messgeraet mit --repeat 8?")
                     .arg(conn.duplicateBlocksDroppedForTest()).arg(iq.count())));

        // ── 5. Trennen ───────────────────────────────────────────────
        conn.disconnect();
        QTest::qWait(300);
        QVERIFY(conn.state() != ConnectionState::Connected);
    }
};

QTEST_MAIN(TstSunSdrSimWorkbench)
#include "tst_sunsdr_sim_workbench.moc"
