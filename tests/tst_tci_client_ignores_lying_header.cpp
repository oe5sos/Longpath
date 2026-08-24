// SPDX-License-Identifier: GPL-3.0-or-later
//
// TciClient darf dem Ratenfeld im Binaerkopf NICHT trauen.
//
// Anlass, 2026-08-24: gemessen an ExpertSDR2/SunSDR2 QRP (drei
// Durchlaeufe, docs/TCI-SunSDR-gemessen.md) — nach
// "iq_samplerate:192000;" bleibt das Ratenfeld im Binaerkopf bei
// 48000 stehen, waehrend tatsaechlich 192 kHz fliessen. Wer den Kopf
// glaubt, bekommt ein Viertel der Spanne im Panadapter und Ton in
// einem Viertel der Geschwindigkeit — und es saehe plausibel aus.
//
// Der vorige Test (tst_tci_client_against_live_device.cpp) prueft das
// GLUECKSFALL-Szenario: bei der Vorgabe 48000 Hz stimmen Kopf und
// Textkanal ohnehin ueberein, ein Fehler dort waere unsichtbar. Diese
// Pruefung stellt die Luege GEZIELT nach, unabhaengig davon, ob
// ExpertSDR2 gerade laeuft: ein selbstgebauter TCI-Sender behauptet im
// Textkanal 192000, schickt aber einen Rahmen, dessen Kopf weiterhin
// 48000 traegt — byteidentisch mit dem, was am echten Geraet gemessen
// wurde. Bestehen kann die Pruefung nur, wenn TciClient tatsaechlich
// den Textkanal liest statt den Kopf.

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocketServer>
#include <QWebSocket>

#include "core/TciBinaryFrame.h"
#include "core/TciClient.h"

using namespace Longpath;

namespace {
constexpr int kWaitMs = 5000;
} // namespace

class TstTciClientIgnoresLyingHeader : public QObject
{
    Q_OBJECT

private slots:
    void kopfluegeWirdIgnoriertTextkanalGewinnt()
    {
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");

        QWebSocket* remote = nullptr;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            remote = server.nextPendingConnection();
            QVERIFY(remote);

            // Selbstauskunft wie ExpertSDR2: Geraetename, Rate 48000,
            // "ready". So beginnt jede echte Sitzung auch.
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;iq_samplerate:48000;"
                "audio_samplerate:48000;ready;"));

            // Erster Rahmen bei der Ausgangsrate — Kopf und Wahrheit
            // stimmen hier ueberein, wie am Anfang jeder echten Sitzung.
            QVector<float> pcmA(64 * 2, 0.1f);
            remote->sendBinaryMessage(TciBinaryFrame::buildStreamPayload(
                0, 48000, int(TciSampleType::Float32), pcmA.size(),
                int(TciStreamType::IqStream), 2, pcmA.constData()));

            // Jetzt die Umstellung -- und DANACH der Rahmen, dessen Kopf
            // bewusst die gemessene Luege traegt: sampleRate=48000, obwohl
            // die Nutzlast der neuen, vierfachen Rate entspricht. Exakt das
            // Bitmuster, das am echten Geraet nach iq_samplerate:192000
            // im Kopf steht.
            remote->sendTextMessage(QStringLiteral("iq_samplerate:192000;"));
            QVector<float> pcmB(256 * 2, 0.2f);   // 4x soviele Werte wie pcmA
            remote->sendBinaryMessage(TciBinaryFrame::buildStreamPayload(
                0, /* Kopf-Luege: */ 48000, int(TciSampleType::Float32),
                pcmB.size(), int(TciStreamType::IqStream), 2,
                pcmB.constData()));
        });

        TciClient client;
        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);
        QSignalSpy iqSpy(&client, &TciClient::iqFrameReady);

        client.connectToEndpoint(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);
        QVERIFY(remote);

        QTRY_VERIFY_WITH_TIMEOUT(iqSpy.count() >= 2, kWaitMs);

        // Rahmen 1: Ausgangsrate, keine Luege im Spiel -- muss stimmen,
        // sonst waere der Vergleichsfall selbst schon falsch.
        const QList<QVariant> first = iqSpy.at(0);
        QCOMPARE(first.at(1).toInt(), 48000);

        // Rahmen 2: der Kopf sagt 48000. Vertraut TciClient dem Kopf,
        // meldet dieser Test 48000 -- und faellt durch. Vertraut es dem
        // Textkanal, wie TciClient.h/.cpp es vorschreiben, steht hier
        // 192000.
        const QList<QVariant> second = iqSpy.at(1);
        QCOMPARE(second.at(1).toInt(), 192000);
        QCOMPARE(client.iqSampleRate(), 192000);

        qInfo() << "Rahmen 1 (keine Luege):" << first.at(1).toInt() << "Hz —"
                << "Rahmen 2 (Kopf luegt, sagt 48000):" << second.at(1).toInt()
                << "Hz uebernommen";
    }

    void kanalfeldWirdIgnoriertStromartEntscheidet()
    {
        // Zweiter Messbefund, ebenso gezielt nachgestellt: das
        // Kanalfeld im Kopf war am echten Geraet 0, 1229 oder ein
        // Fliesskomma-Bitmuster -- niemals verlaesslich 2. TciClient
        // leitet die Kanalzahl daher aus der Stromart ab, nicht aus
        // dem Kopf. Hier baut der Testsender einen Rahmen mit einem
        // eindeutig falschen Kanalfeld und prueft, dass trotzdem 2
        // Kanaele beim Aufrufer ankommen.
        QWebSocketServer server(QStringLiteral("Testgeraet"),
                                 QWebSocketServer::NonSecureMode);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0),
                 "Testserver konnte nicht aufmachen");

        QWebSocket* remote = nullptr;
        connect(&server, &QWebSocketServer::newConnection, this, [&]() {
            remote = server.nextPendingConnection();
            QVERIFY(remote);
            remote->sendTextMessage(QStringLiteral(
                "device:Testgeraet;iq_samplerate:48000;"
                "audio_samplerate:48000;ready;"));

            QVector<float> pcm(64 * 2, 0.1f);
            QByteArray frame = TciBinaryFrame::buildStreamPayload(
                0, 48000, int(TciSampleType::Float32), pcm.size(),
                int(TciStreamType::IqStream),
                /* Kopf-Luege im Kanalfeld: */ 1229, pcm.constData());
            remote->sendBinaryMessage(frame);
        });

        TciClient client;
        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);
        QSignalSpy iqSpy(&client, &TciClient::iqFrameReady);
        client.connectToEndpoint(QStringLiteral("127.0.0.1"), server.serverPort());
        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(iqSpy.count() >= 1, kWaitMs);

        QCOMPARE(iqSpy.constFirst().at(2).toInt(), 2);
    }
};

QTEST_MAIN(TstTciClientIgnoresLyingHeader)
#include "tst_tci_client_ignores_lying_header.moc"
