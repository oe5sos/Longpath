// SPDX-License-Identifier: GPL-3.0-or-later
//
// tests/tst_sunsdr_messlauf.cpp  (Longpath)
//
// Ein MESSLAUF am echten Geraet, kein Pruefpunkt.
//
// Die Frage, die er beantwortet, steht in
// docs/architecture/2026-10-02-sunsdr-verbindungsablauf.md: ExpertSDR2
// bekommt an derselben QRP 480 Pakete/s mit ZWEI VERSCHIEDENEN Paketen je
// Folgenummer, Longpath 240 mit einem. Das ist die doppelte Datenmenge,
// und die Vermutung ist, dass die zweite Paketsorte der zweite Empfaenger
// ist (0x07 kommt im Mitschnitt mit sub 0 UND sub 1).
//
// Pruefen laesst sich das ohne jede Codeaenderung: die rund zwanzig
// Steuerrahmen, die ExpertSDR2 beim Verbinden schickt, gehen ueber
// LONGPATH_SUNSDR_PRE/_EXTRA (siehe sendBenchFrames) mit hinaus, und
// danach wird einfach gezaehlt, was hereinkommt.
//
// Er laeuft NUR, wenn LONGPATH_SUNSDR_MESSLAUF gesetzt ist -- in der CI
// gibt es kein Funkgeraet, und ein Messlauf gehoert ohnehin nicht in eine
// Testsuite, die gruen sein muss.
//
//     LONGPATH_SUNSDR_MESSLAUF=192.168.16.200:50001 \
//     LONGPATH_SUNSDR_FIXED_PORTS=1 \
//     LONGPATH_CONFIG_DIR=<Sandkasten> \
//     LONGPATH_SUNSDR_SEKUNDEN=70 \
//     QT_QPA_PLATFORM=offscreen ./build/tests/tst_sunsdr_messlauf
//
// Es wird NICHTS gesendet, was HF erzeugt: kein MOX, kein Drive, keine
// PA-Freigabe. Der Treiber schickt im Empfang genau die vier Rahmen, die
// er immer schickt, plus die Rahmen aus PRE/EXTRA, die der Mensch davor
// eintraegt.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-03 — Original fuer Longpath, KI-gestuetzt (Anthropic
//                 Claude), Betreiber Martin Fischer.
// =================================================================

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
    info.name       = QStringLiteral("SunSDR2 QRP (Messlauf)");
    return info;
}

}  // namespace

class TstSunSdrMesslauf : public QObject { Q_OBJECT
private slots:
    void messe()
    {
        const QString ziel = qEnvironmentVariable("LONGPATH_SUNSDR_MESSLAUF");
        if (ziel.isEmpty()) {
            QSKIP("LONGPATH_SUNSDR_MESSLAUF nicht gesetzt — Messlauf, kein CI-Test.");
        }
        const QStringList hp = ziel.split(QLatin1Char(':'));
        const QHostAddress addr(hp.value(0));
        const quint16 port = quint16(hp.value(1, QStringLiteral("50001")).toUInt());
        const int sekunden =
            qEnvironmentVariableIntValue("LONGPATH_SUNSDR_SEKUNDEN") > 0
                ? qEnvironmentVariableIntValue("LONGPATH_SUNSDR_SEKUNDEN")
                : 70;

        SunSdrRadioConnection conn;
        // Das echte Geraet antwortet nur auf Port 50001 (am 2026-08-26 live
        // gelernt), also feste Ports -- und nichts anderes darf sie halten.
        conn.setFixedPortBindingEnabledForTest(
            qEnvironmentVariableIsSet("LONGPATH_SUNSDR_FIXED_PORTS"));
        conn.init();
        conn.setSingleChannelHoldMsForTest(0);

        QSignalSpy iq(&conn, &RadioConnection::iqDataReceived);
        QSignalSpy verloren(&conn, &RadioConnection::iqPacketLoss);
        QSignalSpy failed(&conn, &RadioConnection::connectFailed);

        QElapsedTimer uhr;
        uhr.start();
        conn.connectToRadio(qrpAt(addr, port));
        QTRY_VERIFY_WITH_TIMEOUT(conn.state() == ConnectionState::Connected
                                 || failed.count() > 0, 15000);
        if (failed.count() > 0) {
            QFAIL(qPrintable(QStringLiteral(
                "Der Treiber kam nicht durch (Grund %1) — ist die QRP an "
                "und auf %2 erreichbar?")
                .arg(int(failed.first().at(0).value<ConnectFailure>()))
                .arg(ziel)));
        }
        qInfo().noquote() << QStringLiteral("verbunden nach %1 ms")
                                 .arg(uhr.elapsed());

        // Frequenz setzen, BEVOR gemessen wird. Ohne das geht der
        // 0x07-Rahmen nie hinaus, und die QRP bleibt im Einkanal-Zustand
        // (Q = 0, Seitenbaender uebereinander) -- gemessen am 2026-09-25.
        // Ein Messlauf in diesem Zustand misst nicht den Betrieb.
        // Mit LONGPATH_SUNSDR_KEINE_FREQ bleibt der Frequenzrahmen aus --
        // damit laesst sich der EINSCHALTZUSTAND messen (am 2026-09-25
        // eingegrenzt: nach dem Einschalten liefert die QRP nur einen
        // reellen Kanal, Q = 0, die Seitenbaender liegen uebereinander).
        if (!qEnvironmentVariableIsSet("LONGPATH_SUNSDR_KEINE_FREQ")) {
            const quint64 freqHz =
                qEnvironmentVariableIsSet("LONGPATH_SUNSDR_FREQ")
                    ? qEnvironmentVariable("LONGPATH_SUNSDR_FREQ").toULongLong()
                    : 7100000ULL;
            conn.setReceiverFrequency(0, freqHz);
            qInfo().noquote() << QStringLiteral("Frequenz gesetzt: %1 Hz").arg(freqHz);
            QTest::qWait(1500);
        } else {
            qInfo().noquote() << QStringLiteral(
                "KEIN Frequenzrahmen -- Einschaltzustand wird gemessen");
            QTest::qWait(1500);
        }

        qInfo().noquote() << QStringLiteral(
            "Q ungleich null: %1 %  (0 % = nur ein reeller Kanal, "
            "Seitenbaender uebereinander)")
            .arg(conn.qNonZeroPercentForTest(), 0, 'f', 1);

        // Abfragen ZUR LAUFZEIT, nachdem die Frequenz steht. Damit laesst
        // sich dieselbe Abfrage auf zwei Baendern stellen und vergleichen.
        if (qEnvironmentVariableIsSet("LONGPATH_SUNSDR_NACHFRAGE")) {
            conn.sendBenchFramesForTest(QStringLiteral("LONGPATH_SUNSDR_NACHFRAGE"));
            QTest::qWait(1200);
        }
        if (qEnvironmentVariableIsSet("LONGPATH_SUNSDR_FREQ2")) {
            const quint64 f2 =
                qEnvironmentVariable("LONGPATH_SUNSDR_FREQ2").toULongLong();
            conn.setReceiverFrequency(0, f2);
            qInfo().noquote() << QStringLiteral("zweite Frequenz: %1 Hz").arg(f2);
            QTest::qWait(1500);
            if (qEnvironmentVariableIsSet("LONGPATH_SUNSDR_NACHFRAGE")) {
                conn.sendBenchFramesForTest(QStringLiteral("LONGPATH_SUNSDR_NACHFRAGE"));
                QTest::qWait(1200);
            }
            qInfo().noquote() << QStringLiteral(
                "Q ungleich null nach Bandwechsel: %1 %")
                .arg(conn.qNonZeroPercentForTest(), 0, 'f', 1);
        }

        // Viele Frequenzwechsel, um die Verlustrate von STEUERRAHMEN zu
        // messen: jeder Wechsel schickt zwei Rahmen (DDC 0x07 und VFO 0x08)
        // und muss zwei Quittungen bekommen. Am 2026-10-03 war EINER von
        // etwa fuenfzehn Laeufen unquittiert -- diese Messung sagt, wie oft
        // das wirklich vorkommt, und das ist die Zahl, an der die
        // Entscheidung ueber das Nachschicken haengt.
        const int wechsel = qEnvironmentVariableIntValue("LONGPATH_SUNSDR_WECHSEL");
        if (wechsel > 0) {
            quint64 f = 7000000;
            for (int i = 0; i < wechsel; ++i) {
                f += 1000;                       // 1 kHz weiter, im Band bleiben
                if (f > 7200000) { f = 7000000; }
                conn.setReceiverFrequency(0, f);
                QTest::qWait(60);                // Quittung kommt in 15-50 ms
            }
            qInfo().noquote() << QStringLiteral(
                "%1 Frequenzwechsel geschickt (= %2 Steuerrahmen)")
                .arg(wechsel).arg(wechsel * 2);
            QTest::qWait(1500);
        }

        // Alle Bedienelemente durchschalten, die beim QRP ueberhaupt etwas
        // schicken, und mitschreiben, was zurueckkommt. Das ist die Frage,
        // fuer die das Mithoeren gebaut wurde: aendert sich im Betrieb eine
        // Nutzlast, ist es ein Messwert; bleibt alles still, meldet das
        // Geraet nichts. Dazwischen jeweils die Abfrage 0x0c -- wenn ihre
        // 320 Byte den Geraetezustand tragen, muessen sie sich hier
        // bewegen.
        if (qEnvironmentVariableIsSet("LONGPATH_SUNSDR_BEDIENEN")) {
            const QByteArray abfrage =
                QByteArray::fromHex("03ff0c000000000000000100000037f7affe");
            const auto abfragen = [&]() {
                qputenv("LONGPATH_SUNSDR_ABFRAGE", abfrage.toHex());
                conn.sendBenchFramesForTest(QStringLiteral("LONGPATH_SUNSDR_ABFRAGE"));
                qunsetenv("LONGPATH_SUNSDR_ABFRAGE");
                QTest::qWait(400);
            };

            abfragen();
            for (const int stufe : {0, 2, 1, 7}) {   // -20, -10, 0, +10 dB
                conn.setPreampModeIndex(stufe);
                QTest::qWait(300);
                abfragen();
            }
            for (const int dB : {0, -20}) {
                conn.setAttenuator(dB);
                QTest::qWait(300);
                abfragen();
            }
            conn.setActiveReceiverCount(2);
            conn.setSampleRate(96000);
            QTest::qWait(500);
            abfragen();
            qInfo().noquote() << QStringLiteral(
                "Bedienung durchgeschaltet: 4 Vorverstaerkerstufen, "
                "2 Daempfungswerte, Empfaengerzahl, Abtastrate -- je mit "
                "Abfrage 0x0c dazwischen");
        }

        // Rate zur Laufzeit umstellen -- der Weg, den spaeter die
        // Oberflaeche nimmt. LONGPATH_SUNSDR_RATE=96000 schaltet nach dem
        // Verbinden um.
        if (qEnvironmentVariableIsSet("LONGPATH_SUNSDR_RATE")) {
            const int r = qEnvironmentVariableIntValue("LONGPATH_SUNSDR_RATE");
            conn.setSampleRate(r);
            qInfo().noquote() << QStringLiteral("setSampleRate(%1) gerufen").arg(r);
            QTest::qWait(2000);
        }

        const int bloeckeVorher = iq.count();
        QElapsedTimer fenster;
        fenster.start();
        while (fenster.elapsed() < sekunden * 1000) {
            // 20 ms, nicht 500: mit groben Bloecken laeuft die
            // Ereignisschleife zu selten, die Blockantworten gehen
            // verspaetet hinaus, und das Geraet WIEDERHOLT -- am
            // 2026-10-03 gemessen 1,20 Kopien je Nummer im Messlauf gegen
            // 1,00 im echten Betrieb des Betreibers. Der Pruefstand hat
            // also gemessen, was er selbst verursacht hat. Genau der
            // Fehler, vor dem feedback-messung-schlaegt-nicht-das-geraet
            // warnt, nur umgekehrt: hier war nicht das Geraet schuld,
            // sondern das Messgeraet.
            QTest::qWait(20);
        }
        const double secs = double(fenster.elapsed()) / 1000.0;
        const int bloecke = iq.count() - bloeckeVorher;

        qInfo().noquote() << QStringLiteral(
            "MESSLAUF ueber %1 s: %2 IQ-Bloecke nach oben (%3/s), "
            "%4 Verlustmeldungen")
            .arg(secs, 0, 'f', 1).arg(bloecke)
            .arg(double(bloecke) / secs, 0, 'f', 1).arg(verloren.count());
        for (const QList<QVariant>& e : verloren) {
            qInfo().noquote() << QStringLiteral(
                "  Verlust: %1 %, verloren %2, angenommen %3")
                .arg(e.at(0).toDouble(), 0, 'f', 2)
                .arg(e.at(1).toUInt()).arg(e.at(2).toUInt());
        }

        // Das Inventar ist die eigentliche Ausbeute: welche Rahmensorten das
        // Geraet geschickt hat, und ob sich eine Nutzlast geaendert hat.
        qInfo().noquote() << QStringLiteral(
            "Quittungen: %1 gesehen, %2 Rahmen unbeantwortet, %3 noch offen")
            .arg(conn.quittungenGesehenForTest())
            .arg(conn.rahmenOhneQuittungForTest())
            .arg(conn.offeneRahmenForTest());
        qInfo().noquote() << QStringLiteral(
            "Uebersteuerung: %1 Proben am Anschlag, %2 Meldungen")
            .arg(conn.anschlagProbenForTest())
            .arg(conn.anschlagMeldungenForTest());
        qInfo().noquote() << QStringLiteral(
            "PTT vom Geraet: %1 Flanken, Geraet sendet jetzt: %2")
            .arg(conn.mikrofonPttFlankenForTest())
            .arg(conn.geraetSendetForTest() ? QStringLiteral("ja")
                                            : QStringLiteral("nein"));
        for (int k = 0; k < 4; ++k) {
            const quint64 n = conn.kanalPaketeForTest(k);
            if (n == 0) { continue; }
            qInfo().noquote() << QStringLiteral(
                "Kanal %1: %2 Pakete (%3/s), %4 Fortsetzungen, %5 verworfen")
                .arg(k).arg(n).arg(double(n)/secs, 0, 'f', 0)
                .arg(conn.kanalFortsetzungenForTest(k))
                .arg(conn.kanalVerworfenForTest(k));
        }
        qInfo().noquote() << conn.frameInventoryReport();
        qInfo().noquote() << conn.seqDeltaReport();

        conn.disconnect();
        QTRY_VERIFY_WITH_TIMEOUT(conn.state() != ConnectionState::Connected, 5000);
    }
};

QTEST_MAIN(TstSunSdrMesslauf)
#include "tst_sunsdr_messlauf.moc"
