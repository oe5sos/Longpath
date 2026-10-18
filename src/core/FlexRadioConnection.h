#pragma once

// =================================================================
// src/core/FlexRadioConnection.h  (Longpath)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/core/CommandParser.{h,cpp} at d58e2b8a
//     — die ganze Satzlehre: V/H/C/R/S/M, der Aufbau von
//       R<seq>|<code>|<rumpf>, die Objektgrenze im S-Satz, die
//       Schwere in Bit 24-25 des M-Satzes, parseKVs, buildCommand.
//   src/core/RadioConnection.{h,cpp} at d58e2b8a
//     — Zeilenaufsammlung aus dem Strom (onReadyRead), processLine
//       und die Zustandsfolge daran (Handle → Connected).
//   src/models/RadioModel.cpp:6523-6596 at d58e2b8a
//     — die Reihenfolge des Anmeldens (`client program` vor
//       `client gui` vor den `sub`-Befehlen).
//   src/core/backends/flex/FlexBackend.cpp:724-745 at d58e2b8a
//     — welcher Schluessel im Scheibenzustand die Frequenz traegt
//       (`RF_frequency`, in MHz) und die „nur wenn gemeldet, und nur
//       wenn lesbar"-Regel beim Uebernehmen.
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 section 5.
//
// =================================================================
// Stufe 1 — was hier BEWUSST fehlt
// =================================================================
//
// Erkennung und Befehlskanal, keine Stroeme. Es gibt hier keinen
// VITA-49-Empfang, keinen Ton, keinen Panadapter. Der Zweck ist eine
// Antwort auf eine Frage, nicht ein halbes Geraet: traegt der Entwurf,
// bevor die 2279 Zeilen VITA-49 angefasst werden.
//
// **`client gui` wird NICHT gesendet.** AetherSDR sendet es
// (RadioModel.cpp:6540 [@d58e2b8a]) und muss das auch — es ist der
// Hauptbediener der Maschine. Ein GUI-Client uebernimmt die Maschine:
// er bekommt die Scheibenverwaltung, und bei Bedarf wirft die Maschine
// dafuer einen anderen hinaus (AetherSDR fuehrt dafuer eigens eine
// „reclaim-eviction guard", RadioModel.cpp:6580). Zum LESEN einer
// Frequenz braucht es das nicht: `sub slice all` genuegt, und Martins
// Maschine darf dabei weiter an SmartSDR oder am Maestro haengen. Der
// Schritt vom Mitleser zum Bediener gehoert in die Stufe, die auch
// senden kann — nicht in die, die nur nachsieht.
//
// Was daraus folgt: diese Klasse ist KEINE RadioConnection-Unterklasse.
// Die Basis dort ist durch und durch OpenHPSDR (Alex-Antennenwege,
// ADC-Zahlen, Metis-Rahmen, C&C-Register) und erwartet, dass der Client
// rechnet. Bei Flex rechnet das Radio. Dieselbe Trennung wie bei
// TciClient und dem KiwiSDR-Weg: eine eigene Klasse daneben, nicht eine
// Unterklasse, die neun Zehntel ihrer Basis nicht benutzt.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 Longpath. Satzlehre unveraendert uebernommen;
//                 Anmeldefolge ohne `client gui` (Begruendung oben);
//                 Demo-/Synthetik-Zweige, Multi-Flex-Verwaltung,
//                 Keepalive-Ping und RTT-Messung nicht uebernommen —
//                 Stufe 1 liest, sie fuehrt nicht.
// =================================================================

#include <QByteArray>
#include <QHostAddress>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTcpSocket>

#include <optional>

namespace Longpath {

// From AetherSDR CommandParser.h:13-24 [@d58e2b8a] — die Satzarten des
// SmartSDR-Textprotokolls:
//   V<version>             – Fassungsansage der Maschine
//   H<handle>              – zugeteilte Clientkennung (hex)
//   C<seq>|<befehl>        – Befehl (Client → Maschine)
//   R<seq>|<code>|<rumpf>  – Antwort (Maschine → Client)
//   S<handle>|<zustand>    – ungefragter Zustandsschub
//   M<8 hex>|<text>        – Meldung; Bit 24-25 der Zahl tragen die
//                            Schwere
enum class FlexLineType {
    Unknown,
    Version,
    Handle,
    Response,
    Status,
    Message
};

// From AetherSDR RadioMessageTypes.h [@d58e2b8a] — MessageSeverity.
// Bit 24-25 der M-Zahl, per FlexLib Radio.cs:4498-4516.
enum class FlexSeverity {
    Info    = 0,
    Warning = 1,
    Error   = 2,
    Fatal   = 3
};

// From AetherSDR CommandParser.h:26-36 [@d58e2b8a] — ParsedMessage.
struct FlexLine {
    FlexLineType type{FlexLineType::Unknown};
    quint32      sequence{0};    // bei R
    quint32      handle{0};      // bei H/S/M
    quint32      resultCode{0};  // bei R
    QString      object;         // z. B. "slice 0"
    QString      raw;
    QMap<QString, QString> kvs;
    FlexSeverity severity{FlexSeverity::Info};
};

// Zustandslose Satzlehre. Getrennt von der Verbindung, damit sie am
// Schreibtisch pruefbar ist — ohne Netz, ohne Maschine.
// From AetherSDR CommandParser [@d58e2b8a].
namespace FlexCommandWire {

constexpr quint16 kPort = 4992;

FlexLine   parseLine(const QString& rawLine);
QByteArray buildCommand(quint32 seq, const QString& command);
QMap<QString, QString> parseKVs(const QString& body);

// „slice 3" → 3; alles andere → -1.
int sliceIdFromObject(const QString& object);

// Liest `RF_frequency` (MHz) aus einem Scheibenzustand und gibt Hz.
// Fehlt der Schluessel oder ist er unlesbar, kommt nichts zurueck —
// NICHT 0. From AetherSDR FlexBackend.cpp:731-742 [@d58e2b8a]: eine
// verstuemmelte RF_frequency wuerde sonst auf 0 Hz stimmen, und
// FlexLib selbst faellt an dieser Stelle zu (TryParse + continue).
std::optional<double> frequencyHzFromSliceKvs(const QMap<QString, QString>& kvs);

}  // namespace FlexCommandWire

// Der Befehlskanal zu einer FlexRadio-Maschine: TCP 4992, Textsaetze,
// zeilenweise. Stufe 1 — liest mit, fuehrt nicht (siehe Kopf).
class FlexRadioConnection : public QObject {
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,    // Handle zugeteilt, Anmeldung durch
        Error
    };

    explicit FlexRadioConnection(QObject* parent = nullptr);
    ~FlexRadioConnection() override;

    State  state() const { return m_state; }
    bool   isConnected() const { return m_state == State::Connected; }
    quint32 clientHandle() const { return m_handle; }
    QString radioVersion() const { return m_version; }

    // Letzte gelesene Frequenz je Scheibe, in Hz. Leer, solange die
    // Maschine nichts gemeldet hat.
    QMap<int, double> sliceFrequenciesHz() const { return m_sliceFreqHz; }

    void connectToRadio(const QHostAddress& address, quint16 port = FlexCommandWire::kPort);
    void disconnectFromRadio();

    // Schickt einen Befehl und gibt die verwendete Folgenummer zurueck.
    // 0 heisst „nicht gesendet" (keine Verbindung).
    quint32 sendCommand(const QString& command);

    // Nur fuer Pruefstaende: schiebt eine Zeile durch denselben Weg,
    // den der Socket nimmt.
    void ingestLineForTesting(const QString& line) { processLine(line); }

signals:
    void stateChanged(Longpath::FlexRadioConnection::State state);
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
    void versionReceived(const QString& version);
    void handleAssigned(quint32 handle);
    void lineReceived(const Longpath::FlexLine& line);
    void statusReceived(const QString& object, const QMap<QString, QString>& kvs);
    void commandResponse(quint32 sequence, quint32 resultCode, const QString& body);
    void messageFromRadio(Longpath::FlexSeverity severity, const QString& text);

    // Das Ziel der Stufe 1.
    void sliceFrequencyChanged(int sliceId, double frequencyHz);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketErrorOccurred(QAbstractSocket::SocketError error);
    void onReadyRead();

private:
    void processLine(const QString& line);
    void setState(State s);
    void sendRegistrationSequence();

    QTcpSocket m_socket;
    QByteArray m_readBuffer;
    State      m_state{State::Disconnected};
    quint32    m_handle{0};
    quint32    m_nextSeq{1};
    QString    m_version;
    QMap<int, double> m_sliceFreqHz;
};

}  // namespace Longpath

Q_DECLARE_METATYPE(Longpath::FlexLine)
