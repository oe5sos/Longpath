// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath als TCI-CLIENT — Gegenstueck zu TciServer.h, das Longpath als
// TCI-Server spielt. Hier meldet sich Longpath bei einem FREMDEN
// TCI-Anbieter an: ExpertSDR2 am SunSDR2 QRP des Betreibers.
//
// Anlass, 2026-08-24: Der Betreiber hat einen SunSDR2 QRP, dessen
// Netzprotokoll nicht offenliegt ("yuri gibt keinen code raus!"). TCI
// 1.4 ist der einzige gangbare Weg — gemessen mit tools/tci_probe.cpp,
// festgehalten in docs/TCI-SunSDR-gemessen.md. Diese Klasse ist die
// erste Stufe: Verbindung, Selbstauskunft, Stroeme anfordern und
// entpacken. Das Einspeisen in ReceiverManager::feedIqData folgt als
// eigener Schritt.
//
// WICHTIG — zwei Faellen, die die Messung aufgedeckt hat und die diese
// Klasse deshalb NICHT wie ein naiver Client behandeln darf:
//
//   1. Das Ratenfeld im Binaerkopf luegt. Nach iq_samplerate:192000
//      bleibt es bei 48000 stehen, waehrend tatsaechlich 192 kHz
//      fliessen. Die wahre Rate kommt NUR aus dem Textkanal.
//   2. Das Kanalfeld im Binaerkopf ist unbrauchbar (0, oder ein
//      Fliesskomma-Bitmuster). Die Kanalzahl ergibt sich aus der
//      Stromart (I/Q und RX-Ton sind beide 2) und wird gegen die
//      Wertzahl gegengeprueft.
//
// Beide Regeln stehen bereits an TciBinaryFrame.h/.cpp — dort liegt
// der gemeinsame Kopf-Leser fuer Client UND Server. Diese Klasse ruft
// ihn nur auf.
//
// =================================================================
// src/core/TciClient.h  (Longpath)
// =================================================================

#pragma once
#ifdef HAVE_WEBSOCKETS

#include "core/TciBinaryFrame.h"

#include <QAbstractSocket>
#include <QMap>
#include <QObject>
#include <QString>

#include <vector>

class QWebSocket;

namespace Longpath {

class TciClient : public QObject {
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        // Verbindung steht, aber die Selbstauskunft ist noch nicht zu
        // Ende (kein "ready;" gesehen). Vor diesem Punkt duerfen keine
        // Stroeme angefordert werden — startIqStream/startAudioStream
        // weisen das mit einer Warnung zurueck.
        Connecting,
        // "ready;" gesehen. Raten aus der Selbstauskunft sind bekannt,
        // Stroeme koennen angefordert werden.
        Connected,
        Error,
    };
    Q_ENUM(State)

    explicit TciClient(QObject* parent = nullptr);
    ~TciClient() override;

    TciClient(const TciClient&) = delete;
    TciClient& operator=(const TciClient&) = delete;

    // port folgt der Vorgabe von ExpertSDR2/Thetis (40001). Ein
    // bestehender Aufbau wird zuerst sauber abgebaut.
    void connectToEndpoint(const QString& host, quint16 port = 40001);
    void disconnectFromEndpoint();

    State state() const { return m_state; }
    QString lastError() const { return m_lastError; }
    QString deviceName() const { return m_deviceName; }

    // Ströme an-/abmelden. receiver ist der GERAETESEITIGE Index
    // (0, 1, ...), nicht Longpaths eigene Slice-Nummer — die
    // Zuordnung trifft der Aufrufer.
    void startIqStream(int receiver);
    void stopIqStream(int receiver);
    void startAudioStream(int receiver);
    void stopAudioStream(int receiver);

    // Die WAHREN Raten, aus dem Textkanal gelernt (siehe Klassenkopf).
    // 0, solange das Geraet sie noch nicht genannt hat — das ist vor
    // Connected der Normalfall, danach ein Fehlerzeichen.
    int iqSampleRate() const { return m_iqRateFromText; }
    int audioSampleRate() const { return m_audioRateFromText; }

    // TCI meldet den Laufzustand des Empfaengers als NACKTE Zeile
    // "start"/"stop", ohne Geraeteindex — global, nicht je Strom. Ohne
    // laufenden Empfaenger kommt aus keinem angeforderten Strom auch
    // nur ein Rahmen (docs/TCI-SunSDR-gemessen.md, "Betriebsbereit,
    // aber leer" war der zweite falsche Befund der Sonde).
    bool isReceiverRunning() const { return m_trxRunning; }
    bool isReceiverStateKnown() const { return m_sawRunState; }

    // Die tatsaechliche Mittenfrequenz des I/Q-Stroms, aus dem Textkanal
    // gelernt ("dds:<receiver>,<hz>", gemessen 2026-08-24 gegen
    // ExpertSDR2). NICHT dasselbe wie die VFO-Anzeige("vfo:R,V,hz"):
    // vfo ist die Stimme des Betreibers innerhalb der ZF-Durchlassbreite,
    // dds ist die Mitte, um die der I/Q-Strom selbst liegt -- genau das,
    // was ein Panadapter als Mittenfrequenz braucht. 0, solange das
    // Geraet sie noch nicht genannt hat.
    qint64 ddcCenterHz(int receiver) const
    { return m_ddcCenterHz.value(receiver, 0); }

signals:
    void stateChanged(Longpath::TciClient::State state, const QString& detail);

    // Ein entpackter Rahmen, interleaved. sampleRate ist die WAHRE Rate
    // (Textkanal, niemals der Kopf); channels ist aus der Stromart
    // abgeleitet und gegen die Wertzahl gegengeprueft — niemals aus
    // Kopf-Feld 28 uebernommen (siehe Klassenkopf).
    void iqFrameReady(int receiver, int sampleRate, int channels,
                       const std::vector<float>& interleaved);
    void audioFrameReady(int receiver, int sampleRate, int channels,
                          const std::vector<float>& interleaved);

    void receiverRunStateChanged(bool running);
    // Siehe ddcCenterHz() -- feuert bei jeder "dds:"-Zeile, auch nach
    // dem Verbindungsaufbau, falls der Betreiber in ExpertSDR2 retunt.
    void ddcCenterChanged(int receiver, qint64 hz);
    // Einmalig, wenn die Selbstauskunft zu Ende ist (Uebergang nach
    // Connected). deviceName kann leer sein, wenn das Geraet sich
    // nicht per "device:" vorstellt.
    void deviceDescribed(const QString& deviceName);
    void errorOccurred(const QString& message);

private:
    void handleTextMessage(const QString& message);
    void handleTextLine(const QString& line);
    void handleBinaryMessage(const QByteArray& frame);
    void handleSocketError(QAbstractSocket::SocketError error);
    void setState(State state, const QString& detail = QString());
    void sendCommand(const QString& command);
    void resetSessionState();

    QWebSocket* m_socket = nullptr;
    State       m_state  = State::Disconnected;
    QString     m_lastError;
    QString     m_deviceName;

    int  m_iqRateFromText    = 0;
    int  m_audioRateFromText = 0;
    bool m_trxRunning  = false;
    bool m_sawRunState = false;
    bool m_sawReady    = false;

    QMap<int, qint64> m_ddcCenterHz;   // Empfaenger -> letzte "dds:"-Mitte
};

} // namespace Longpath

#endif // HAVE_WEBSOCKETS
