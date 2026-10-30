// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Verriegelung der Flex — vom Draht bis ins Modell (2026-10-30).
//
// Die Auswertung selbst ist in tst_flex_senden geprueft, aber mit
// HANDGESCHRIEBENEN Schluesselpaaren. Was dort nie gefahren wurde: die
// Zeile, wie die Maschine sie wirklich schickt.
//
// Anlass ist das Protokoll vom 2026-10-30 (longpath-20261030-182157.log).
// Dreiundzwanzig Mal stand dort
//
//   WRN: Flex: getastet, OHNE je eine Verriegelungsmeldung gehoert zu
//        haben — die Maschine entscheidet selbst
//
// und zwar bei JEDER FT8-Sendung, vierzig Minuten lang. Longpath hat
// die Lage der Maschine kein einziges Mal gelesen und getastet, ohne
// ihr Nein lesen zu koennen -- bei einer Endstufe in der Leitung die
// falsche Richtung.
//
// Am Geraet nachgemessen (FLEX-8400, 2026-10-30, nur gelesen, als
// GUI-Client angemeldet wie Longpath, dieselben drei Bestellungen):
// die Lage KOMMT, 0,20 s nach dem Anmelden, und sie kommt mehrfach.
//
//   S7B4CB680|interlock tx_client_handle=0x00000000 state=READY
//             reason= source= tx_allowed=1 amplifier=
//   S7B4CB680|interlock tx_client_handle=0x00000000 state=NOT_READY
//             reason=OUT_OF_PA_RANGE source= tx_allowed=0 amplifier=
//   S0|interlock tx_client_handle=0x00000000 state=READY ...
//
// Daneben schickt sie Zeilen OHNE Lage -- die Einstellungen der
// Sendewege und, das ist der Haken, eine Zeile JE BAND:
//
//   S7B4CB680|interlock acc_txreq_enable=0 ... tx_delay=0
//   S7B4CB680|interlock band 5 band_name=40 acc_txreq_enable=0 ...
//
// Dieser Lauf haelt beide Sorten am Draht fest.

#include <QtTest>
#include <QSignalSpy>

#include "core/FlexRadioConnection.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TestFlexVerriegelungAmDraht : public QObject
{
    Q_OBJECT

private slots:
    // Die gemessene Lage-Zeile muss als Objekt „interlock" ankommen,
    // mit `state` darin. Genau daran haengt das Tor im Modell
    // (RadioModel::verarbeiteFlexVerriegelung prueft kv.contains("state")).
    void dieGemesseneLageKommtAlsInterlockAn()
    {
        FlexRadioConnection conn;
        conn.ingestLineForTesting(QStringLiteral("H7B4CB680"));
        QSignalSpy status(&conn, &FlexRadioConnection::statusReceived);

        conn.ingestLineForTesting(QStringLiteral(
            "S7B4CB680|interlock tx_client_handle=0x00000000 state=READY "
            "reason= source= tx_allowed=1 amplifier="));

        QCOMPARE(status.count(), 1);
        const QString objekt = status.at(0).at(0).toString();
        const auto kv = status.at(0).at(1).value<QMap<QString, QString>>();

        QCOMPARE(objekt, QStringLiteral("interlock"));
        QVERIFY2(kv.contains(QStringLiteral("state")),
                 "ohne `state` sieht das Modell Schweigen");
        QCOMPARE(kv.value(QStringLiteral("state")), QStringLiteral("READY"));
        QCOMPARE(kv.value(QStringLiteral("tx_allowed")), QStringLiteral("1"));
    }

    // Dieselbe Zeile mit Kennung 0 — so schickt sie die Maschine an
    // Clients, die sich nicht als GUI angemeldet haben, und einmal
    // auch danach (0,42 s, gemessen).
    void auchMitKennungNull()
    {
        FlexRadioConnection conn;
        conn.ingestLineForTesting(QStringLiteral("H7B4CB680"));
        QSignalSpy status(&conn, &FlexRadioConnection::statusReceived);

        conn.ingestLineForTesting(QStringLiteral(
            "S0|interlock tx_client_handle=0x00000000 state=NOT_READY "
            "reason=OUT_OF_PA_RANGE source= tx_allowed=0 amplifier="));

        QCOMPARE(status.count(), 1);
        QCOMPARE(status.at(0).at(0).toString(), QStringLiteral("interlock"));
        const auto kv = status.at(0).at(1).value<QMap<QString, QString>>();
        QCOMPARE(kv.value(QStringLiteral("state")), QStringLiteral("NOT_READY"));
        QCOMPARE(kv.value(QStringLiteral("reason")),
                 QStringLiteral("OUT_OF_PA_RANGE"));
    }

    // Die Zeilen ohne Lage. Sie duerfen nicht als „interlock" mit
    // leerer Lage durchgehen -- „gehoert" heisst: sie hat ueber die
    // LAGE gesprochen (siehe tst_flex_senden::schweigenIstKeinVerbot).
    void dieZeilenOhneLageSindSchweigen()
    {
        FlexRadioConnection conn;
        conn.ingestLineForTesting(QStringLiteral("H7B4CB680"));
        QSignalSpy status(&conn, &FlexRadioConnection::statusReceived);

        conn.ingestLineForTesting(QStringLiteral(
            "S7B4CB680|interlock acc_txreq_enable=0 rca_txreq_enable=0 "
            "acc_tx_enabled=0 tx1_enabled=0 tx_delay=0 timeout=0"));
        QCOMPARE(status.count(), 1);
        QCOMPARE(status.at(0).at(0).toString(), QStringLiteral("interlock"));
        const auto ohneLage =
            status.at(0).at(1).value<QMap<QString, QString>>();
        QVERIFY(!ohneLage.contains(QStringLiteral("state")));

        // Und die Zeile je Band: ihr Objektname ist „interlock band N",
        // nicht „interlock" -- sie darf das Tor nicht anfassen.
        conn.ingestLineForTesting(QStringLiteral(
            "S7B4CB680|interlock band 5 band_name=40 acc_txreq_enable=0 "
            "rca_txreq_enable=0 acc_tx_enabled=0"));
        QCOMPARE(status.count(), 2);
        QCOMPARE(status.at(1).at(0).toString(),
                 QStringLiteral("interlock band 5"));
    }

    // ── Und der Weg, an dem es wirklich hing (2026-10-30) ───────────
    //
    // Im Modell lag die Weiche auf `interlock` HINTER der Zeile
    //
    //     if (!m_flexAudioWanted) { return; }
    //
    // also hinter dem Tonschalter des Betreibers. Ohne Flex-Ton wurde
    // jede Verriegelungsmeldung verworfen — und `flexTasten()` tastete
    // mit dem Vorgabewert „nie gehoert", bei einer Endstufe in der
    // Leitung die falsche Richtung. Der Ton ist ein Wunsch, die Lage
    // der Maschine eine Sicherheitsfrage; sie duerfen nicht am selben
    // Schalter haengen.
    //
    // Hier wird KEIN Ton verlangt. Die Verbindung geht auf einen Port
    // auf 127.0.0.1, auf dem niemand lauscht — es geht nur um die
    // Verdrahtung, die `connectToRadio` legt.
    void ohneTonWirdDieLageTrotzdemGelesen()
    {
        RadioModel model;
        QVERIFY(model.flexConnection() == nullptr);

        RadioInfo info;
        info.protocol   = ProtocolVersion::Flex;
        info.boardType  = HPSDRHW::Flex8400;
        info.name       = QStringLiteral("FLEX-8400 (Pruefstand)");
        info.macAddress = QStringLiteral("00:1C:2D:05:34:28");
        info.address    = QHostAddress(QHostAddress::LocalHost);
        info.port       = 1;   // niemand lauscht hier

        model.connectToRadio(info);
        QVERIFY(model.flexConnection() != nullptr);
        QCOMPARE(model.flexVerriegelungGehoert(), false);

        // Genau die gemessene Zeile.
        model.flexConnection()->ingestLineForTesting(QStringLiteral(
            "S7B4CB680|interlock tx_client_handle=0x00000000 state=READY "
            "reason= source= tx_allowed=1 amplifier="));

        QVERIFY2(model.flexVerriegelungGehoert(),
                 "die Lage muss auch ohne Flex-Ton ankommen");
        QCOMPARE(model.flexVerriegelungsLage(), QStringLiteral("READY"));
        QVERIFY(model.flexDarfSenden());

        // Ein echtes Nein sperrt dann auch ohne Ton.
        model.flexConnection()->ingestLineForTesting(QStringLiteral(
            "S7B4CB680|interlock tx_client_handle=0x00000000 state=NOT_READY "
            "reason=OUT_OF_PA_RANGE source= tx_allowed=0 amplifier="));
        QCOMPARE(model.flexVerriegelungsLage(), QStringLiteral("NOT_READY"));
        QVERIFY2(!model.flexDarfSenden(),
                 "ein gehoertes Nein sperrt — auch ohne Ton");
    }
};

QTEST_MAIN(TestFlexVerriegelungAmDraht)
#include "tst_flex_verriegelung_am_draht.moc"
