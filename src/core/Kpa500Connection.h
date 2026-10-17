// no-port-check: Longpath-eigene Datei, kein Port. Herkunft des
// Protokolls: Elecrafts oeffentliche Programmierreferenz, siehe
// core/Kpa500Protocol.h.
#pragma once

// =================================================================
// src/core/Kpa500Connection.h  (Longpath-eigen)
// =================================================================
//
// Elecraft KPA500: der Transport.
//
// Schritt 2 nach der Protokollschicht (core/Kpa500Protocol.h). Hier
// kommt dazu, was ein Geraet am anderen Ende noetig macht: der
// Abfragetakt, das Erkennen von Stille, das Wiederverbinden -- und EIN
// Zustand, den der SPE nicht hat und der diese Klasse von
// SpeConnection unterscheidet.
//
// ── DER BOOT-ZUSTAND, und warum er wichtig ist ──────────────────────
//
// Der KPA500 kennt zwei Betriebsarten (Elecraft Rev A2, §BootLoader
// Command Reference):
//
//   * Hauptsoftware laeuft -> die `^XX;`-Befehle gelten
//   * BOOT-Zustand        -> nur EINZELNE Grossbuchstaben gelten
//
// Der Boot-Zustand ist der, in dem das Geraet steht, wenn der Schalter
// hinten auf ON steht, die Hauptsoftware aber nicht laeuft -- also:
// ausgeschaltet an der Front, aber am Draht erreichbar. In diesem
// Zustand antwortet er auf `^ON;` GAR NICHT (§^ON: „No response if
// off."), aber auf 'I' mit „KPA500".
//
// Daraus ergibt sich etwas, das es beim SPE nicht gibt: wir koennen
// „ausgeschaltet, aber da" von „nicht da" UNTERSCHEIDEN, statt beides
// als Stille zu melden. Bleiben die regulaeren Abfragen unbeantwortet,
// schickt diese Klasse ein 'I'; kommt „KPA500" zurueck, steht der
// Verstaerker im Boot-Zustand und der Einschaltknopf hat einen Sinn.
// Kommt auch darauf nichts, ist wirklich niemand da.
//
// Und das Einschalten ist hier keine Bastelei mit Steuerleitungen wie
// beim SPE, sondern ein dokumentierter Befehl: 'P' (§BootLoader, 'P'
// Power On: „This is the command to use to remotely power up the
// KPA500.").
//
// ── Die Datenrate ist NICHT zu erraten ──────────────────────────────
//
// Der SPE passt sich an (115200 und abwaerts). Der KPA500 nicht: seine
// Datenrate steht in ^BRP und kennt vier Werte (4800/9600/19200/38400,
// §^BRP). Welcher eingestellt ist, sagt das Dokument nicht, und an der
// falschen Rate kommt NICHTS durch. Darum ist sie hier eine Angabe des
// Betreibers mit 38400 als Vorgabe (die hoechste; wer es nie umgestellt
// hat, probiert sie zuerst) -- nicht eine Zahl, die dieses Programm
// stillschweigend annimmt.
//
// Longpath hat keinen KPA500 am Kabel. Der Pruefstand
// (tst_kpa500_verbindung) stellt einen eigenen QTcpServer auf 127.0.0.1
// hin, der die Antworten des Verstaerkers nachspricht.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 -- Neu, Zeus-Punkt 7. Martin Fischer, AI-assisted via
//                 Anthropic Claude (Claude Code).
// =================================================================

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

#ifdef HAVE_SERIALPORT
#include <QSerialPort>
#endif

#include "core/Kpa500Protocol.h"

namespace Longpath {

class Kpa500Connection : public QObject {
    Q_OBJECT

public:
    explicit Kpa500Connection(QObject* parent = nullptr);
    // Wie bei SpeConnection: ohne Destruktor ruft ~QTcpSocket ueber
    // disconnected() noch in schon abgebaute Zeitgeber. Siehe die
    // Begruendung dort und in der Erinnerung
    // „Abbau greift in tote Mitglieder".
    ~Kpa500Connection() override;

    bool    isConnected() const { return m_connected; }
    QString description() const;
    QString sourceLabel() const;   // "SERIAL" / "NETWORK" / "—"

    // Was der Verstaerker gerade ist, soweit wir es wissen.
    enum class Liveness {
        Unknown,    // noch keine Antwort gesehen
        Running,    // Hauptsoftware antwortet
        BootMode,   // 'I' antwortet "KPA500" -- aus, aber am Draht erreichbar
        Silent,     // nichts antwortet, auch nicht 'I'
    };
    Liveness liveness() const { return m_liveness; }

#ifdef HAVE_SERIALPORT
    // 8N1, kein Handschlag. Die Datenrate MUSS mitgegeben werden -- sie
    // laesst sich nicht erraten (siehe Kopf).
    void connectSerial(const QString& portName, int baudRate);
#endif
    void connectNetwork(const QString& host, quint16 port);
    void disconnect();

    void setAutoReconnect(bool on) { m_autoReconnect = on; }

    // Befehle.
    void setOperate(bool operate);
    void clearFault();
    void powerOff();      // ^ON0;
    void powerOn();       // 'P' im Boot-Zustand
    void setBand(int bandIndex);

    // Letzter Stand, soweit gemeldet.
    Kpa500::PowerSwr  lastPowerSwr()  const { return m_powerSwr; }
    Kpa500::VoltsAmps lastVoltsAmps() const { return m_voltsAmps; }
    int      lastTemperature() const { return m_tempC; }
    bool     isOperate()       const { return m_operate; }
    int      lastBandIndex()   const { return m_bandIndex; }
    int      lastFaultCode()   const { return m_faultCode; }
    QString  firmware()        const { return m_firmware; }
    QString  serialNumber()    const { return m_serial; }

    // Pruefstand-Naht, wie bei SpeConnection und aus demselben Grund:
    // ohne gestauchte Zeiten ist die Stilleerkennung ohne Geraet nicht
    // pruefbar. Im Betrieb ruft sie niemand; wer sie ruft, VOR connect().
    int  pollIntervalMs() const { return m_pollIntervalMs; }
    void setPollIntervalMs(int ms);
    int  silentPollLimit() const { return m_silentPollLimit; }
    void setSilentPollLimit(int polls);
    void setReconnectIntervalMs(int ms);

signals:
    void connected();
    void disconnected();
    void connectionFailed(const QString& errorString);
    // Ein vollstaendiger Satz Messwerte ist eingetroffen (nach jedem
    // Takt, in dem mindestens eine Antwort kam).
    void telemetryUpdated();
    void livenessChanged(Longpath::Kpa500Connection::Liveness state);
    void faultChanged(int code);
    void operateChanged(bool operate);

private slots:
    void onReadyRead();

private:
    enum class Mode { None, Serial, Network };

    void onTransportUp();
    void onTransportDown();
    void onTransportError(const QString& errorString);
    void onReply(const Kpa500::Reply& r);
    void teardownDevice();
    void sendRaw(const QByteArray& bytes);
    void armReconnect();
    void pollTick();
    void setLiveness(Liveness s);

    QIODevice* m_device{nullptr};
    QTcpSocket m_socket;
#ifdef HAVE_SERIALPORT
    QSerialPort* m_serialPort{nullptr};
    int m_lastBaud{38400};
#endif

    Kpa500::ReplyParser m_parser;

    Mode    m_mode{Mode::None};
    QString m_lastSerialPort;
    QString m_lastHost;
    quint16 m_lastPort{0};

    bool m_connected{false};
    bool m_autoReconnect{false};
    bool m_deliberateDisconnect{false};

    QTimer m_reconnectTimer;
    QTimer m_pollTimer;
    // Der KPA500 antwortet nur auf Nachfrage. 250 ms ist langsamer als
    // beim SPE (100 ms): dort kommt EIN Rahmen je Abfrage, hier sind es
    // sechs Abfragen je Takt, und die Anzeige lebt mit 4 Hz gut.
    static constexpr int kDefaultPollIntervalMs = 250;
    int m_pollIntervalMs{kDefaultPollIntervalMs};
    // Rund drei Sekunden bei 250 ms -- dieselbe Groessenordnung wie beim
    // SPE.
    static constexpr int kDefaultSilentPollLimit = 12;
    int m_silentPollLimit{kDefaultSilentPollLimit};

    bool m_replySeenSinceTick{false};
    int  m_silentPolls{0};
    Liveness m_liveness{Liveness::Unknown};
    // Im Boot-Zustand wird nicht weiter mit `^XX;` gefragt -- das
    // beantwortet er nicht --, sondern nur noch mit 'I' nachgehorcht,
    // damit das Einschalten bemerkt wird.
    bool m_probingBoot{false};
    // Wechselt je Takt: einmal 'I', einmal die regulaeren Abfragen.
    // Warum abgewechselt und nicht beides zugleich: siehe pollTick().
    bool m_bootProbeToggle{false};

    Kpa500::PowerSwr  m_powerSwr;
    Kpa500::VoltsAmps m_voltsAmps;
    int     m_tempC{0};
    bool    m_operate{false};
    int     m_bandIndex{-1};
    int     m_faultCode{0};
    QString m_firmware;
    QString m_serial;
};

}  // namespace Longpath
