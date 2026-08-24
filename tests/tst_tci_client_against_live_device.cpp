// SPDX-License-Identifier: GPL-3.0-or-later
//
// TciClient gegen das ECHTE Geraet: den SunSDR2 QRP des Betreibers,
// ueber ExpertSDR2 (TCI 1.4) auf ws://127.0.0.1:40001.
//
// Anlass, 2026-08-24: "wie gehts beim sun sdr qrp weiter" — Schritt 1
// des vierstufigen Plans ist die Verbindung selbst. Gemessen mit
// tools/tci_probe.cpp und in docs/TCI-SunSDR-gemessen.md festgehalten:
// das Ratenfeld im Binaerkopf luegt bei einer Umstellung, das
// Kanalfeld ist durchgaengig unbrauchbar. TciClient ist bewusst SO
// gebaut, dass es beiden Feldern misstraut (siehe TciClient.h/.cpp).
// Diese Pruefung ist der Beweis, nicht nur die Behauptung: sie
// verlangt die WAHRE, gemessene Rate im emittierten Rahmen, nicht
// irgendeine plausible Zahl.
//
// Ohne laufendes ExpertSDR2 auf diesem Rechner wird uebersprungen,
// nicht als Fehler gemeldet — dieselbe Unterscheidung, die die Sonde
// bereits treffen musste (siehe deren Kommentare zu "Betriebsbereit,
// aber leer").

#include <QtTest>
#include <QSignalSpy>
#include <QTcpSocket>

#include "core/TciClient.h"

using namespace Longpath;

namespace {

constexpr const char* kHost = "127.0.0.1";
constexpr quint16     kPort = 40001;
constexpr int         kWaitMs = 8000;

bool tciPortIsOpen()
{
    QTcpSocket probe;
    probe.connectToHost(QString::fromLatin1(kHost), kPort);
    const bool up = probe.waitForConnected(1000);
    probe.close();
    return up;
}

} // namespace

class TstTciClientAgainstLiveDevice : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!tciPortIsOpen()) {
            QSKIP("Kein TCI-Anbieter auf 127.0.0.1:40001 (ExpertSDR2 nicht "
                  "erreichbar) — nichts zu pruefen.");
        }
    }

    void verbindungWirdVollstaendigBeschrieben()
    {
        TciClient client;
        QSignalSpy stateSpy(&client, &TciClient::stateChanged);
        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);

        client.connectToEndpoint(QString::fromLatin1(kHost), kPort);

        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);
        QCOMPARE(client.state(), TciClient::State::Connected);

        // Die Raten muessen jetzt bekannt sein — vor "ready" duerfen
        // Stroeme gar nicht erst angefordert werden (siehe
        // startIqStream), und nach "ready" nennt die Selbstauskunft sie
        // immer (gemessen, docs/TCI-SunSDR-gemessen.md).
        QVERIFY2(client.iqSampleRate() > 0,
                 "IQ-Rate nach Ende der Selbstauskunft noch unbekannt");
        QVERIFY2(client.audioSampleRate() > 0,
                 "Ton-Rate nach Ende der Selbstauskunft noch unbekannt");
        qInfo() << "verbunden mit" << client.deviceName()
                << "IQ-Rate" << client.iqSampleRate()
                << "Ton-Rate" << client.audioSampleRate()
                << "Empfaenger laeuft:" << client.isReceiverRunning();
    }

    void iqUndTonStromLiefernDieWahreRateNichtDenKopfwert()
    {
        TciClient client;
        QSignalSpy describedSpy(&client, &TciClient::deviceDescribed);
        client.connectToEndpoint(QString::fromLatin1(kHost), kPort);
        QTRY_VERIFY_WITH_TIMEOUT(describedSpy.count() >= 1, kWaitMs);

        if (client.isReceiverStateKnown() && !client.isReceiverRunning()) {
            QSKIP("Der Empfaenger steht (TCI meldet 'stop') — ohne "
                  "laufenden Empfaenger kommt kein einziger Rahmen, "
                  "siehe docs/TCI-SunSDR-gemessen.md. In ExpertSDR2 den "
                  "Start-Knopf druecken und diese Pruefung wiederholen.");
        }

        QSignalSpy iqSpy(&client, &TciClient::iqFrameReady);
        QSignalSpy audioSpy(&client, &TciClient::audioFrameReady);

        const int kIqReceiver = 0;
        client.startIqStream(kIqReceiver);
        client.startAudioStream(kIqReceiver);

        QTRY_VERIFY_WITH_TIMEOUT(iqSpy.count() >= 1, kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(audioSpy.count() >= 1, kWaitMs);

        const QList<QVariant> iqArgs = iqSpy.constFirst();
        const int iqReceiver   = iqArgs.at(0).toInt();
        const int iqRate       = iqArgs.at(1).toInt();
        const int iqChannels   = iqArgs.at(2).toInt();
        const auto iqSamples   = iqArgs.at(3).value<std::vector<float>>();

        QCOMPARE(iqReceiver, kIqReceiver);
        // Der entscheidende Punkt: die emittierte Rate muss die WAHRE,
        // aus dem Textkanal gelernte Rate sein -- client.iqSampleRate()
        // ist die einzige Quelle, der TciClient traut. Waere hier
        // stattdessen der (bei einer Umstellung falsche) Kopfwert
        // durchgereicht, wuerde dieser Vergleich es nicht zwingend
        // aufdecken, solange 48000 die Vorgabe ist -- deshalb testet
        // eine zweite Pruefung unten zusaetzlich bei 192 kHz.
        QCOMPARE(iqRate, client.iqSampleRate());
        QCOMPARE(iqChannels, 2);
        QVERIFY2(!iqSamples.empty(), "IQ-Rahmen ohne Werte");
        QVERIFY2(iqSamples.size() % 2 == 0,
                 "IQ-Werte sind nicht paarweise (I/Q verschraenkt)");

        const QList<QVariant> audioArgs = audioSpy.constFirst();
        QCOMPARE(audioArgs.at(1).toInt(), client.audioSampleRate());
        QCOMPARE(audioArgs.at(2).toInt(), 2);
        const auto audioSamples = audioArgs.at(3).value<std::vector<float>>();
        QVERIFY2(!audioSamples.empty(), "Ton-Rahmen ohne Werte");

        qInfo() << "IQ:" << iqSpy.count() << "Rahmen bei" << iqRate << "Hz,"
                << "Ton:" << audioSpy.count() << "Rahmen bei"
                << client.audioSampleRate() << "Hz";
    }
};

QTEST_MAIN(TstTciClientAgainstLiveDevice)
#include "tst_tci_client_against_live_device.moc"
