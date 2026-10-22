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
// **Longpath meldet sich als GUI-Client an** (`client gui <uuid>`).
// Das war in der ersten Fassung dieser Datei anders — da blieb Longpath
// bewusst Mitleser. Die Messung am echten Gerät hat die Frage
// entschieden: `slice list` kam LEER zurück. Die Maschine hat ohne
// GUI-Client gar keine Scheiben, also auch keine Frequenz zu lesen.
// `max_slices=2` ist die lizenzierte Zahl, nicht die vorhandene.
//
// Mitlesen wäre eine Sackgasse gewesen: Ton und Panadapter hängen
// später an genau den Scheiben und Panadaptern, die ein GUI-Client
// anlegt und besitzt. Ein Weg, der nur funktioniert, solange SmartSDR
// nebenher läuft, führt zu keiner Stufe 2.
//
// Was ein GUI-Client bedeutet, ausdrücklich: er übernimmt die
// Scheibenverwaltung, und bei Bedarf wirft die Maschine dafür einen
// anderen Client hinaus (AetherSDR führt dafür eigens eine
// „reclaim-eviction guard", RadioModel.cpp:6580 [@d58e2b8a] — hier
// nicht portiert, weil Stufe 1 sich nicht wiederverbindet).
//
// Was Stufe 1 trotzdem NICHT tut: senden. Kein `xmit`, keine
// TX-Scheibe, keine Leistung. Die Interlock-Meldung der Maschine
// (`state=RECEIVE tx_allowed=0`) wird gelesen, nie geschrieben.
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
//                 Demo-/Synthetik-Zweige, Multi-Flex-Verwaltung,
//                 Keepalive-Ping und RTT-Messung nicht uebernommen.
//   2026-10-18 — Zweiter Durchgang, nach der Messung am Gerät: GUI-
//                 Anmeldung, `slice list`, und das Anlegen einer
//                 Vorgabescheibe, wenn keine da ist (portiert aus
//                 RadioModel::createPanadapter / createDefaultSliceOnPan
//                 und SliceRecreatePolicy, alle [@d58e2b8a]).
// =================================================================

#include <QByteArray>
#include <QHostAddress>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTcpSocket>

#include <functional>
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

// From AetherSDR RadioStatusOwnership.h:27-41 [@d58e2b8a] —
// normalizedFlexId(). Flex-Kennungen kommen als Hex, mal mit „0x"
// davor, mal ohne, mal nicht auf acht Stellen gefuellt. Zurueck kommt
// immer die achtstellige Kleinschreibung; unlesbar ⇒ leer.
QString normalizedFlexId(QString text);

// Der Befehl, der eine Scheibe abstimmt.
//
// From AetherSDR SliceModel.cpp:124 [@d58e2b8a] — Form und das
// `autopan=0`: „SmartSDR pcap confirms: scroll-wheel uses
// 'slice tune <id> <freq> autopan=0'". Ohne `autopan=0` zentriert die
// Maschine den Panadapter neu; unser Bild haengt aber am Ausschnitt,
// den die Wasserfallkachel meldet, und der springt dann mit.
//
// Hertz hinein, Megahertz mit sechs Nachkommastellen hinaus — dieselbe
// Schreibweise, in der die Maschine Frequenzen meldet.
QString tuneSliceCommand(const QString& sliceId, double hz);

// From AetherSDR RadioStatusOwnership.h:48-60 [@d58e2b8a] —
// parsePanafallCreatePanId(). Die Antwort auf „display panafall
// create" traegt die Kennung mal als `pan=`, mal als `id=`, mal nackt
// als erstes Glied einer Kommaliste. Alle drei Formen werden gelesen.
QString panIdFromCreateReply(const QString& body);

// Die Antwort auf „slice list" ist eine Kommaliste von Nummern, und
// LEER, wenn die Maschine keine Scheiben hat — das ist der Normalfall
// ohne GUI-Client, nicht ein Fehler.
QList<int> sliceListFromReply(const QString& body);

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

    // Die Kennung, unter der Longpath sich als GUI-Client anmeldet.
    // Wird von aussen gesetzt (RadioModel kennt AppSettings, diese
    // Klasse soll es nicht), weil die Maschine Scheiben und Panadapter
    // je Kennung ZURUECKLEGT: eine neue Kennung bei jedem Start heisst
    // jedes Mal eine neue Vorgabescheibe auf der Maschine.
    // Ungesetzt ⇒ es wird eine erzeugt und per
    // guiClientIdChosen() gemeldet, damit der Aufrufer sie behalten
    // kann. Form wie AetherSDR: UUID ohne Klammern, gross
    // (AppSettings.cpp:1256 [@d58e2b8a]).
    void setGuiClientId(const QString& id);
    QString guiClientId() const { return m_guiClientId; }

    // Der Stationsname, den die Maschine in ihrer Clientliste zeigt.
    void setStationName(const QString& name) { m_stationName = name; }

    // Darf Longpath eine Vorgabescheibe anlegen, wenn die Maschine
    // keine hat? Vorgabe ja — ohne Scheibe gibt es nichts zu lesen.
    // Aus heisst: nur nachsehen, nichts auf der Maschine aendern.
    void setMayCreateDefaultSlice(bool may) { m_mayCreateSlice = may; }

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

    // Dasselbe mit Rueckruf auf die Antwort dieses Befehls. Der
    // Rueckruf laeuft genau einmal. Ohne Verbindung laeuft er nicht.
    //
    // AetherSDR fuehrt diese Zuordnung eine Ebene hoeher, in
    // RadioModel (RadioConnection.cpp:339 + „Callbacks removed —
    // responses emitted via commandResponse signal (#502)"), weil die
    // Verbindung dort auf einem eigenen Faden liegt. Hier liegt sie auf
    // dem Hauptfaden, und die Anmeldefolge besteht gerade aus
    // „warte auf die Antwort, dann der naechste Schritt" — deshalb
    // sitzt die Zuordnung hier.
    using ResponseCallback = std::function<void(quint32 code, const QString& body)>;
    quint32 sendCommand(const QString& command, ResponseCallback onResponse);

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

    // Die Anmeldung ist durch: ab hier fuehrt Longpath die Maschine.
    void guiClientRegistered(const QString& guiClientId);
    // Die Anmeldung wurde abgelehnt — mit dem Code der Maschine.
    void guiClientRejected(quint32 resultCode, const QString& body);
    // Die verwendete Kennung, damit der Aufrufer sie behalten kann.
    // Kommt auch, wenn die Maschine eine eigene zurueckmeldet.
    void guiClientIdChosen(const QString& guiClientId);
    // Was „slice list" geantwortet hat. Leere Liste ist ein Ergebnis,
    // kein Fehler.
    void sliceListRead(const QList<int>& sliceIds);
    // Eine Vorgabescheibe wurde angelegt (oder es ist misslungen).
    void defaultSliceCreated(const QString& panId);
    void defaultSliceFailed(quint32 resultCode, const QString& body);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketErrorOccurred(QAbstractSocket::SocketError error);
    void onReadyRead();

private:
    void processLine(const QString& line);
    void setState(State s);
    void sendRegistrationSequence();
    void onGuiClientAccepted(const QString& body);
    void queryAndRecoverSlices();
    void createDefaultSlice();
    void createSliceOnPan(const QString& panId);

    QTcpSocket m_socket;
    QByteArray m_readBuffer;
    State      m_state{State::Disconnected};
    quint32    m_handle{0};
    quint32    m_nextSeq{1};
    QString    m_version;
    QMap<int, double> m_sliceFreqHz;

    QString m_guiClientId;
    QString m_stationName{QStringLiteral("Longpath")};
    bool    m_mayCreateSlice{true};
    bool    m_guiRegistered{false};
    QMap<quint32, ResponseCallback> m_pending;
};

}  // namespace Longpath

Q_DECLARE_METATYPE(Longpath::FlexLine)
