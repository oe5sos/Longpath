// no-port-check: Longpath-eigener Prüfstand für die TCI-Kommandos `drive:`,
// `tune_drive:` und das eingehende `tune:`.
//
// Vorgeschichte: alle drei fehlten bis 2026-09-30 vollständig. Das fiel nicht
// auf, weil TciProtocol unbekannte Kommandonamen ohne Antwort verwirft (die
// "silent error invariant", tst_tci_silent_error_invariant.cpp) — der
// Leistungsregler einer Fernbedienung schien also zu funktionieren, wirkte
// aber nie. Gefunden beim Abgleich gegen "TCI Remote" von ON7OFF, die `drive`
// in ihrer Pflichtliste führt; betrifft genauso N1MM+ und Log4OM.
//
// Quelle für das Verhalten: Thetis TCIServer.cs [v2.10.3.13] —
//   handleDrive / handleTuneDrive   4138-4190  (1 Arg = Abfrage, 2 = Setzen)
//   sendDrivePower / sendTunePower  2328-2341  (Bereich 0..100)
//   handleTune                                 (setzt nur bei echter Änderung)
//   Set-Switch-Fälle                5372, 5420, 5423
//
// Was hier NICHT geprüft werden kann: der Setz-Pfad von `tune:` bis ins Gerät.
// RadioModel::setTune() ist aus einem Unit-Test nicht erreichbar — es verlangt
// eine lebende Verbindung UND eine Audio-Engine (PowerOn-Guard, siehe
// RadioModel.h:1587-1589). Geprüft wird darum, was ohne Gerät entscheidbar
// ist: Abfragepfad, Argumentprüfung und dass der Handler nicht aus Versehen
// eine Zustandsmeldung erfindet, bevor der Träger wirklich steht.

#include <QtTest/QtTest>

#include "core/TciProtocol.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace Longpath;

class TestTciDriveTuneDrive : public QObject {
    Q_OBJECT

private:
    // Alle Meldungen abholen, die ein Kommando in die Warteschlange gelegt hat.
    static QStringList drain(TciProtocol& p) {
        QStringList out;
        while (p.hasPendingNotification()) {
            out << p.takePendingNotification();
        }
        return out;
    }

private slots:
    // ── drive: ──────────────────────────────────────────────────────────────

    void drive_set_writes_transmit_power() {
        RadioModel radio;
        TciProtocol p(&radio);

        // Gegenprobe zum Ausgangszustand: vor dem Fix lief dieses Kommando in
        // den Default-Zweig und änderte gar nichts.
        const QString echo = p.handleCommand(QStringLiteral("drive:0,42;"));
        QCOMPARE(echo, QString());  // Setzen antwortet nicht direkt …
        QCOMPARE(radio.transmitModel().power(), 42);
        // … sondern über die Warteschlange, damit ALLE Clients es sehen.
        QVERIFY(drain(p).contains(QStringLiteral("drive:0,42;")));
    }

    void drive_query_reports_current_power() {
        RadioModel radio;
        radio.transmitModel().setPower(73);
        TciProtocol p(&radio);

        QCOMPARE(p.handleCommand(QStringLiteral("drive:0;")),
                 QStringLiteral("drive:0,73;"));
    }

    void drive_clamps_to_thetis_range() {
        RadioModel radio;
        TciProtocol p(&radio);

        // Thetis prüft 0..100 erst beim SENDEN (sendDrivePower, TCIServer.cs:2335)
        // und schweigt bei Ausreißern. Longpath klemmt statt dessen schon beim
        // Setzen — ein fremder Client soll keine unmögliche Leistung setzen
        // können und dann auf eine Meldung warten, die nie kommt.
        p.handleCommand(QStringLiteral("drive:0,150;"));
        QCOMPARE(radio.transmitModel().power(), 100);
        QVERIFY(drain(p).contains(QStringLiteral("drive:0,100;")));

        p.handleCommand(QStringLiteral("drive:0,-5;"));
        QCOMPARE(radio.transmitModel().power(), 0);
        QVERIFY(drain(p).contains(QStringLiteral("drive:0,0;")));
    }

    void drive_rejects_bad_receiver_index() {
        RadioModel radio;
        radio.transmitModel().setPower(55);
        TciProtocol p(&radio);

        QCOMPARE(p.handleCommand(QStringLiteral("drive:7,10;")), QString());
        QCOMPARE(radio.transmitModel().power(), 55);  // unverändert
        QVERIFY(!p.hasPendingNotification());

        QCOMPARE(p.handleCommand(QStringLiteral("drive:abc,10;")), QString());
        QCOMPARE(radio.transmitModel().power(), 55);
    }

    void drive_rejects_non_numeric_value() {
        RadioModel radio;
        radio.transmitModel().setPower(55);
        TciProtocol p(&radio);

        QCOMPARE(p.handleCommand(QStringLiteral("drive:0,viel;")), QString());
        QCOMPARE(radio.transmitModel().power(), 55);
        QVERIFY(!p.hasPendingNotification());
    }

    // ── tune_drive: ─────────────────────────────────────────────────────────

    void tune_drive_set_and_query_roundtrip() {
        RadioModel radio;
        TciProtocol p(&radio);

        p.handleCommand(QStringLiteral("tune_drive:0,35;"));
        QCOMPARE(radio.transmitModel().tunePower(), 35);
        QVERIFY(drain(p).contains(QStringLiteral("tune_drive:0,35;")));

        QCOMPARE(p.handleCommand(QStringLiteral("tune_drive:0;")),
                 QStringLiteral("tune_drive:0,35;"));
    }

    void tune_drive_reports_the_value_that_was_actually_applied() {
        RadioModel radio;
        TciProtocol p(&radio);

        // TransmitModel::setTunePower klemmt selbst und modellabhängig
        // (HERMESLITE 0..99, sonst 0..100 — TransmitModel.cpp,
        // setTunePowerForBand). Die Rückmeldung muss deshalb den TATSÄCHLICH
        // gesetzten Wert nennen, nicht den gewünschten: sonst behauptet die
        // Meldung an einem HL2 eine 100, die nie ankam.
        p.handleCommand(QStringLiteral("tune_drive:0,500;"));
        const int applied = radio.transmitModel().tunePower();
        QVERIFY2(applied <= 100, "Modell muss den Ausreißer geklemmt haben");
        QVERIFY(drain(p).contains(
            QStringLiteral("tune_drive:0,%1;").arg(applied)));
    }

    // ── tune: (eingehend) ───────────────────────────────────────────────────

    void tune_query_reports_state() {
        RadioModel radio;
        TciProtocol p(&radio);

        // Ohne Gerät steht der Abstimmträger aus — das ist der Zustand, den
        // eine frisch verbundene Fernbedienung abfragt.
        QCOMPARE(p.handleCommand(QStringLiteral("tune:0;")),
                 QStringLiteral("tune:0,false;"));
    }

    void tune_rejects_non_boolean() {
        RadioModel radio;
        TciProtocol p(&radio);

        QCOMPARE(p.handleCommand(QStringLiteral("tune:0,vielleicht;")), QString());
        QVERIFY(!p.hasPendingNotification());
        QCOMPARE(p.handleCommand(QStringLiteral("tune:0,1;")), QString());
        QVERIFY(!p.hasPendingNotification());
    }

    void tune_set_does_not_invent_a_state_message() {
        RadioModel radio;
        TciProtocol p(&radio);

        // Der `tune:`-Broadcast gehört TciServer, der an TransmitModel::
        // tuneChanged hängt (TciServer.cpp) und erst meldet, wenn der Träger
        // WIRKLICH steht. Würde der Handler hier selbst eine Meldung
        // einreihen, bekäme die Fernbedienung ein Versprechen statt einer
        // Tatsache — an einem Gerät ohne Strom bliebe ihre Anzeige auf "TUN
        // an" stehen, während nichts sendet.
        p.handleCommand(QStringLiteral("tune:0,true;"));
        QVERIFY2(!p.hasPendingNotification(),
                 "tune: darf keine eigene Zustandsmeldung erzeugen");
    }

    // ── Gegenprobe zur Ausgangslage ─────────────────────────────────────────

    void all_three_are_routed_not_silently_dropped() {
        RadioModel radio;
        TciProtocol p(&radio);

        // Das ist der eigentliche Fehler von vorher: die Namen liefen in den
        // Default-Zweig. Der Dispatch-Zähler beweist, dass sie jetzt einen
        // eigenen Zweig haben — und der Zustand beweist, dass er wirkt.
        p.resetDispatchCounters();
        p.handleCommand(QStringLiteral("drive:0,10;"));
        p.handleCommand(QStringLiteral("tune_drive:0,20;"));
        p.handleCommand(QStringLiteral("tune:0;"));
        QCOMPARE(p.setDispatchCount() + p.queryDispatchCount(), 3);
        QCOMPARE(radio.transmitModel().power(), 10);
        QCOMPARE(radio.transmitModel().tunePower(), 20);
    }
};

QTEST_MAIN(TestTciDriveTuneDrive)
#include "tst_tci_drive_tune_drive.moc"
