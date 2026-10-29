// =================================================================
// tests/tst_flex_senden.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigener Pruefstand.
//
// Stufe 2 des Flex-Treibers: senden. Bis zum 2026-10-23 stand im Kopf
// von FlexRadioConnection.h ausdruecklich „Was Stufe 1 trotzdem NICHT
// tut: senden". Der Betreiber hat danach gefragt.
//
// Dieser Stand prueft, was sich OHNE Funkgeraet pruefen laesst — also
// den Wortlaut der Befehle und den Bau der Tonpakete. Was ein Geraet
// braucht (tastet die Maschine wirklich? kommt der Ton an?), steht
// nicht hier, sondern im Pruefplan; es wird mit dem Betreiber am Geraet
// gefahren und nie von einem Pruefstand ausgeloest.
//
// Die drei Fallen, an denen es am Geraet scheitern wuerde und die
// darum hier festgenagelt sind:
//
//   1. Ohne Sendescheibe weist die Maschine `xmit` ab.
//   2. Ohne `transmit set dax=1` nimmt sie das Mikrofon an ihrer
//      eigenen Buchse und wirft jedes Tonpaket weg.
//   3. Der Sendekopf ist NICHT der Empfangskopf: hinaus geht
//      Pakettyp 1 (IFDataWithStream) mit sieben Woertern, herein kommt
//      Pakettyp 3 (ExtDataWithStream) mit 28 Byte.
// =================================================================

#include <QtTest/QtTest>
#include <QtEndian>

#include "core/FlexRadioConnection.h"
#include "core/FlexVitaReader.h"
#include "models/RadioModel.h"

#include <cmath>
#include <cstring>

using namespace Longpath;
using namespace Longpath::FlexCommandWire;

class TstFlexSenden : public QObject {
    Q_OBJECT

private slots:

    // ── Der Wortlaut der Befehle ────────────────────────────────────
    void dasTastenSelbst()
    {
        QCOMPARE(xmitCommand(true),  QStringLiteral("xmit 1"));
        QCOMPARE(xmitCommand(false), QStringLiteral("xmit 0"));
    }

    void dieSendescheibe()
    {
        QCOMPARE(setSliceTxCommand(QStringLiteral("0"), true),
                 QStringLiteral("slice set 0 tx=1"));
        QCOMPARE(setSliceTxCommand(QStringLiteral("2"), false),
                 QStringLiteral("slice set 2 tx=0"));
        // Ohne Kennung lieber nichts schicken als die falsche Scheibe
        // zum Sender machen.
        QVERIFY(setSliceTxCommand(QString(), true).isEmpty());
    }

    void derDaxZweig()
    {
        QCOMPARE(setDaxRouteCommand(true),
                 QStringLiteral("transmit set dax=1"));
        QCOMPARE(setDaxRouteCommand(false),
                 QStringLiteral("transmit set dax=0"));
    }

    void dieLeistungWirdGeklemmt()
    {
        QCOMPARE(setRfPowerCommand(50),
                 QStringLiteral("transmit set rfpower=50"));
        QCOMPARE(setTunePowerCommand(10),
                 QStringLiteral("transmit set tunepower=10"));
        QCOMPARE(setRfPowerCommand(0),
                 QStringLiteral("transmit set rfpower=0"));
        QCOMPARE(setRfPowerCommand(100),
                 QStringLiteral("transmit set rfpower=100"));
        // Ausserhalb: GAR NICHTS. Ein abgewiesener Befehl laesst die
        // Leistung auf dem alten Wert stehen, und das ist schlimmer
        // als nichts zu schicken — der Betreiber glaubt dann, er habe
        // heruntergeregelt.
        QVERIFY(setRfPowerCommand(101).isEmpty());
        QVERIFY(setRfPowerCommand(-1).isEmpty());
        QVERIFY(setTunePowerCommand(255).isEmpty());
    }

    void derAbstimmtraeger()
    {
        QCOMPARE(tuneCommand(true),  QStringLiteral("transmit tune 1"));
        QCOMPARE(tuneCommand(false), QStringLiteral("transmit tune 0"));
    }

    void derSendetonstrom()
    {
        QCOMPARE(createDaxTxStreamCommand(),
                 QStringLiteral("stream create type=dax_tx"));
    }

    // Die Antwort auf `stream create` traegt die Kennung in drei
    // Formen — alle drei kommen am Geraet vor.
    void dieStromkennungAusDerAntwort()
    {
        QCOMPARE(streamIdFromCreateReply(QStringLiteral("stream_id=0x84000001")),
                 0x84000001u);
        QCOMPARE(streamIdFromCreateReply(QStringLiteral("84000001")),
                 0x84000001u);
        QCOMPARE(streamIdFromCreateReply(QStringLiteral("0x84000001,foo=1")),
                 0x84000001u);
        // Nichts Lesbares -> 0, und 0 ist keine gueltige Kennung.
        QCOMPARE(streamIdFromCreateReply(QString()), 0u);
        QCOMPARE(streamIdFromCreateReply(QStringLiteral("quark")), 0u);
    }

    // ── Der Bau der Tonpakete ───────────────────────────────────────
    //
    // Feld fuer Feld gegen AetherSDR AudioEngine.cpp:8770-8826
    // [@d58e2b8a] — ein falsches Bit hier heisst am Geraet: getastet,
    // aber nichts zu hoeren, und das sieht man keinem Protokoll an.
    void derSendekopfStimmtFeldFuerFeld()
    {
        QVector<float> ton(FlexVita::kTxFramesProPaket * 2, 0.0f);
        quint8 zaehler = 0;
        const QByteArray p = FlexVita::buildTxAudioPacket(
            0x84000001u, ton.constData(), FlexVita::kTxFramesProPaket, zaehler);

        const int sollWorte = FlexVita::kTxFramesProPaket * 2
                              + FlexVita::kTxHeaderWords;
        QCOMPARE(p.size(), sollWorte * 4);

        const quint32* w = reinterpret_cast<const quint32*>(p.constData());
        const quint32 kopf = qFromBigEndian(w[0]);
        QCOMPARE((kopf >> 28) & 0xF, 0x1u);   // IFDataWithStream, NICHT 3
        QCOMPARE((kopf >> 27) & 0x1, 0x1u);   // C = 1
        QCOMPARE((kopf >> 26) & 0x1, 0x0u);   // T = 0
        QCOMPARE((kopf >> 22) & 0x3, 0x3u);   // TSI = Other
        QCOMPARE((kopf >> 20) & 0x3, 0x1u);   // TSF = SampleCount
        QCOMPARE((kopf >> 16) & 0xF, 0x0u);   // Zaehler, erstes Paket
        QCOMPARE(kopf & 0xFFFF, static_cast<quint32>(sollWorte));

        QCOMPARE(qFromBigEndian(w[1]), 0x84000001u);
        QCOMPARE(qFromBigEndian(w[2]), FlexVita::kFlexOui);
        QCOMPARE(qFromBigEndian(w[3]),
                 (static_cast<quint32>(FlexVita::kInfoClassSl) << 16)
                     | FlexVita::kPacketClassIfNarrow);
        QCOMPARE(qFromBigEndian(w[4]), 0u);
        QCOMPARE(qFromBigEndian(w[5]), 0u);
        QCOMPARE(qFromBigEndian(w[6]), 0u);
    }

    void derTonKommtUnveraendertAn()
    {
        // Ein Sinus hinein, derselbe Sinus im Rumpf wieder heraus —
        // Big-Endian, Fliesskomma, Stereo verschraenkt.
        QVector<float> ton(FlexVita::kTxFramesProPaket * 2);
        for (int i = 0; i < ton.size(); ++i) {
            ton[i] = 0.5f * std::sin(0.11f * i);
        }
        quint8 zaehler = 0;
        const QByteArray p = FlexVita::buildTxAudioPacket(
            0x84000001u, ton.constData(), FlexVita::kTxFramesProPaket, zaehler);

        const quint32* rumpf =
            reinterpret_cast<const quint32*>(p.constData())
            + FlexVita::kTxHeaderWords;
        for (int i = 0; i < ton.size(); ++i) {
            const quint32 roh = qFromBigEndian(rumpf[i]);
            float wieder = 0.0f;
            std::memcpy(&wieder, &roh, sizeof(float));
            QCOMPARE(wieder, ton[i]);
        }
    }

    void derZaehlerLaeuftUndRollt()
    {
        QVector<float> ton(FlexVita::kTxFramesProPaket * 2, 0.0f);
        quint8 zaehler = 0;
        for (int n = 0; n < 18; ++n) {
            const QByteArray p = FlexVita::buildTxAudioPacket(
                0x84000001u, ton.constData(),
                FlexVita::kTxFramesProPaket, zaehler);
            const quint32 kopf =
                qFromBigEndian(*reinterpret_cast<const quint32*>(p.constData()));
            QCOMPARE((kopf >> 16) & 0xF, static_cast<quint32>(n & 0xF));
        }
        // Nach 18 Paketen steht er auf 2 — er rollt bei 16 um.
        QCOMPARE(zaehler, quint8(2));
    }

    void unbrauchbaresGibtKeinPaket()
    {
        QVector<float> ton(FlexVita::kTxFramesProPaket * 2, 0.0f);
        quint8 zaehler = 0;
        // Kennung 0 ist keine: ein Paket darauf ginge ins Leere und
        // die Maschine wuesste nicht, wohin damit.
        QVERIFY(FlexVita::buildTxAudioPacket(0, ton.constData(),
                                             FlexVita::kTxFramesProPaket,
                                             zaehler).isEmpty());
        QVERIFY(FlexVita::buildTxAudioPacket(0x84000001u, nullptr, 128,
                                             zaehler).isEmpty());
        QVERIFY(FlexVita::buildTxAudioPacket(0x84000001u, ton.constData(), 0,
                                             zaehler).isEmpty());
        // Und der Zaehler darf dabei NICHT weiterlaufen, sonst reiszt
        // ein verworfenes Paket eine Luecke in die Folge.
        QCOMPARE(zaehler, quint8(0));
    }

    // Gegenprobe: der Sendekopf unterscheidet sich wirklich vom
    // Empfangskopf. Waere er gleich, haette ich beim Bauen einfach den
    // Leser spiegeln koennen — er ist es nicht.
    void sendekopfUndEmpfangskopfSindVerschieden()
    {
        QVector<float> ton(FlexVita::kTxFramesProPaket * 2, 0.0f);
        quint8 zaehler = 0;
        const QByteArray p = FlexVita::buildTxAudioPacket(
            0x84000001u, ton.constData(), FlexVita::kTxFramesProPaket, zaehler);
        // Der Leser erwartet 28 Byte Kopf und Pakettyp 3; unser
        // Sendepaket hat 28 Byte Kopf NICHT und Pakettyp 1.
        const FlexVita::Header h = FlexVita::parseHeader(p);
        QVERIFY2(!h.valid || h.packetType != 3,
                 "der Sendekopf darf nicht als Empfangskopf durchgehen");
    }

    // ── Das Tor vor dem Tasten ──────────────────────────────────────
    //
    // Die Verriegelung der Maschine war bis heute eine blosze Meldung.
    // Jetzt ist sie das Tor: ohne ihre Freigabe geht kein `xmit`
    // hinaus. Hier wird NICHTS getastet — es gibt keine Verbindung,
    // und genau das ist auch der erste geprüfte Fall.
    void ohneVerbindungWirdNichtGetastet()
    {
        RadioModel radio;
        QCOMPARE(radio.flexDarfSenden(), false);   // Vorgabe: gesperrt
        QCOMPARE(radio.flexSendetGerade(), false);
        QVERIFY2(!radio.flexTasten(true),
                 "ohne Verbindung darf kein xmit hinausgehen");
        QVERIFY2(!radio.flexAbstimmen(true),
                 "ohne Verbindung darf kein Abstimmtraeger hinausgehen");
    }

    // ── Abwesenheit ist keine Aussage (2026-10-27) ──────────────────
    //
    // Der Fehler, der den ersten Sendeversuch des Betreibers zweimal
    // verschluckt hat: das Tasttor las den VORGABEWERT von
    // `flexDarfSenden()` als Verbot. Die Maschine hatte aber gar
    // nichts gesagt — ihr `interlock`-Zustand kam nie an (wir hatten
    // `sub tx all` nie bestellt).
    //
    // Jetzt trennt `flexVerriegelungGehoert()` die beiden Faelle:
    // ein gehoertes Nein sperrt, Schweigen nicht.
    void schweigenIstKeinVerbot()
    {
        RadioModel radio;
        QCOMPARE(radio.flexVerriegelungGehoert(), false);
        QCOMPARE(radio.flexDarfSenden(), false);

        // Nach einer Meldung ist es ein echter Befund — und RECEIVE
        // ist KEIN Verbot, siehe dieVerriegelungWirdGelesen().
        radio.verarbeiteFlexVerriegelung({
            { QStringLiteral("state"), QStringLiteral("NOT_READY") },
            { QStringLiteral("reason"), QStringLiteral("OUT_OF_BAND") },
            { QStringLiteral("tx_allowed"), QStringLiteral("0") },
        });
        QCOMPARE(radio.flexVerriegelungGehoert(), true);
        QCOMPARE(radio.flexDarfSenden(), false);

        // ── Und eine Ebene tiefer derselbe Fehler (2026-10-27) ──────
        //
        // Hier stand: eine Meldung OHNE `tx_allowed` zaehlt auch als
        // gehoert. Am Geraet kam genau so eine — weder `state` noch
        // `tx_allowed` darin — und das Tor las wieder den Vorgabewert
        // false als Verbot:
        //
        //   Verriegelung gehoert true, darf senden false
        //   nicht abgestimmt — die Maschine sperrt das Senden "" ""
        //
        // „Gehoert" heisst: sie hat ueber DIE LAGE gesprochen. Eine
        // Zeile ohne `state` ist Schweigen, und Schweigen ist kein
        // Verbot. (Bis zum 2026-10-29 haengte das an `tx_allowed` —
        // siehe dieVerriegelungWirdGelesen(), warum das falsch war.)
        RadioModel zweite;
        zweite.verarbeiteFlexVerriegelung({
            { QStringLiteral("tx_allowed"), QStringLiteral("0") },
        });
        QCOMPARE(zweite.flexVerriegelungGehoert(), false);

        // Erst mit der Lage.
        zweite.verarbeiteFlexVerriegelung({
            { QStringLiteral("state"), QStringLiteral("RECEIVE") },
        });
        QCOMPARE(zweite.flexVerriegelungGehoert(), true);
        QCOMPARE(zweite.flexDarfSenden(), true);
    }

    void dieVerriegelungWirdGelesen()
    {
        RadioModel radio;
        QSignalSpy freigabe(&radio, &RadioModel::flexSendefreigabeGeaendert);
        QSignalSpy sendet(&radio, &RadioModel::flexSendetGeaendert);

        // ── `tx_allowed` ist nicht die Erlaubnis (2026-10-29) ───────
        //
        // Am Geraet mitgelesen, mit einer zweiten Verbindung neben
        // Longpath:
        //
        //   19:52:31  state=NOT_READY  reason=OUT_OF_BAND  tx_allowed=0
        //   19:56:12  state=RECEIVE    reason=             tx_allowed=0
        //
        // Die Scheibe lag beim Verbinden ausserhalb des Bandes — das
        // erste Nein war richtig. Danach stand sie auf 7,177 MHz, die
        // Maschine ging auf RECEIVE und schickte weiter `tx_allowed=0`.
        // Im Ruhezustand ist das Feld IMMER 0; es sagt „es wird gerade
        // nicht gesendet", nicht „du darfst nicht".
        //
        // Der Betreiber bekam daraufhin weder TUNE noch MOX, und zwar
        // dauerhaft — und davor ging beides nur deshalb, weil
        // ueberhaupt keine Verriegelungsmeldung ankam.
        //
        // Deshalb entscheidet die LAGE.
        radio.verarbeiteFlexVerriegelung({
            { QStringLiteral("state"), QStringLiteral("NOT_READY") },
            { QStringLiteral("reason"), QStringLiteral("OUT_OF_BAND") },
            { QStringLiteral("tx_allowed"), QStringLiteral("0") },
        });
        QCOMPARE(radio.flexDarfSenden(), false);
        QCOMPARE(radio.flexVerriegelungsLage(), QStringLiteral("NOT_READY"));
        QCOMPARE(radio.flexVerriegelungsGrund(), QStringLiteral("OUT_OF_BAND"));
        QCOMPARE(freigabe.count(), 0);   // war schon gesperrt

        // Der Ruhezustand der Maschine: RECEIVE mit tx_allowed=0.
        // Genau die Zeile, die das Tor bis zum 2026-10-29 zugehalten
        // hat — sie ist eine Freigabe.
        radio.verarbeiteFlexVerriegelung({
            { QStringLiteral("state"), QStringLiteral("RECEIVE") },
            { QStringLiteral("tx_allowed"), QStringLiteral("0") },
        });
        QCOMPARE(radio.flexDarfSenden(), true);
        QCOMPARE(freigabe.count(), 1);
        QCOMPARE(freigabe.takeFirst().at(0).toBool(), true);

        // Und so, wenn eine Tastanfrage ansteht.
        radio.verarbeiteFlexVerriegelung({
            { QStringLiteral("state"), QStringLiteral("READY") },
            { QStringLiteral("tx_allowed"), QStringLiteral("1") },
        });
        QCOMPARE(radio.flexDarfSenden(), true);
        QCOMPARE(freigabe.count(), 0);   // war schon frei

        // Sie sendet.
        radio.verarbeiteFlexVerriegelung({
            { QStringLiteral("state"), QStringLiteral("TRANSMITTING") },
            { QStringLiteral("tx_allowed"), QStringLiteral("1") },
            { QStringLiteral("source"), QStringLiteral("MIC") },
        });
        QCOMPARE(radio.flexSendetGerade(), true);
        QCOMPARE(sendet.count(), 1);
        QCOMPARE(sendet.takeFirst().at(0).toBool(), true);
        // Die Quelle sagt, WER getastet hat — MIC ist die Taste am
        // Geraet, nicht unser `xmit` (das meldet SW).
        QCOMPARE(radio.flexVerriegelungsQuelle(), QStringLiteral("MIC"));

        // Und zurueck.
        radio.verarbeiteFlexVerriegelung({
            { QStringLiteral("state"), QStringLiteral("RECEIVE") },
        });
        QCOMPARE(radio.flexSendetGerade(), false);
        QCOMPARE(sendet.count(), 1);
        QCOMPARE(sendet.takeFirst().at(0).toBool(), false);
    }

    // ── dBm in Watt, fuer die Balken (2026-10-27) ───────────────────
    //
    // Betreiber: „tune funktioniert, zeigt aber keinen wert an". Die
    // Flex misst selbst und schickt FWDPWR/REFPWR im Messwertstrom —
    // in dBm. Die Balken wollen Watt. Die Umrechnung steht in
    // RadioModel; hier wird sie an den Zahlen festgenagelt, die am
    // Geraet vorkommen.
    void dbmInWatt()
    {
        auto watt = [](double dbm) { return std::pow(10.0, (dbm - 30.0) / 10.0); };
        // Die Zahl aus Martins Protokoll bei Ruhe.
        QVERIFY(watt(0.0) < 0.002);            // 0 dBm = 1 mW
        // 1 W, 10 W, 100 W.
        QVERIFY(std::abs(watt(30.0) - 1.0)   < 0.001);
        QVERIFY(std::abs(watt(40.0) - 10.0)  < 0.01);
        QVERIFY(std::abs(watt(50.0) - 100.0) < 0.1);
    }

    void derGrundWirdMitgenommen()
    {
        // Die Maschine nennt bei gesperrtem Senden oft einen Grund.
        // Ohne ihn steht im Protokoll „es sendet nicht" und sonst
        // nichts — das hat am 2026-10-22 einen Abend gekostet.
        RadioModel radio;
        radio.verarbeiteFlexVerriegelung({
            { QStringLiteral("state"), QStringLiteral("PTT_REQUESTED") },
            { QStringLiteral("tx_allowed"), QStringLiteral("0") },
            { QStringLiteral("reason"), QStringLiteral("AMP:TG") },
        });
        QCOMPARE(radio.flexVerriegelungsGrund(), QStringLiteral("AMP:TG"));
    }
};

QTEST_MAIN(TstFlexSenden)
#include "tst_flex_senden.moc"
