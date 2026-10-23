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
// Was Stufe 1 NICHT tat: senden. Kein `xmit`, keine TX-Scheibe, keine
// Leistung; die Interlock-Meldung der Maschine (`state=RECEIVE
// tx_allowed=0`) wurde gelesen, nie benutzt.
//
// ── Stufe 2: senden (2026-10-23) ────────────────────────────────────
//
// Der Betreiber hat danach gefragt. Jetzt gibt es: `xmit`, die
// Sendescheibe (`slice set <n> tx=1`), Sende- und Abstimmleistung,
// den Abstimmtraeger, den Sendetonstrom (`stream create type=dax_tx`)
// samt Paketbau, und die Verriegelung als TOR davor statt als blosze
// Meldung.
//
// Drei Dinge, die man dabei nicht raten darf und die am Geraet Zeit
// kosten, wenn man sie uebersieht:
//
//   1. Ohne Sendescheibe weist die Maschine `xmit` ab.
//   2. Ohne `transmit set dax=1` nimmt sie das Mikrofon an ihrer
//      eigenen Buchse und wirft jedes unserer Tonpakete weg — getastet
//      wird, gesendet wird Stille.
//   3. Der Sendeton geht an UDP-Port 4991, nicht an 4992; 4992 ist nur
//      die Anmeldung, aus der die Maschine unsere Adresse lernt.
//
// Was Stufe 2 weiterhin nicht tut: CW tasten, PureSignal, Mithoeren
// ueber die Maschine. Und sie tastet nie von selbst — jede Taste
// kommt vom Betreiber.
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

// ── Betriebsart und Filterbreite (2026-10-22) ────────────────────────
//
// Bis heute war `slice tune` der EINZIGE Befehl, den Longpath an eine
// Flex-Scheibe schicken konnte. Betriebsart und Filterflanken gingen
// nie hinaus: die Bedienung zeigte LSB und 2,9 kHz, die Maschine blieb
// auf dem, was beim Verbinden gesetzt wurde. Der Betreiber am
// 2026-10-22, nachdem das Abstimmen endlich durchkam: „aber reine
// stimmen hoere ich nicht", „als waere es eine andere modulation",
// „und der bandfilter zeigt auch zu wenig an". Beides derselbe Grund.
//
// Am Geraet gemessen war der Ton dabei makellos — 0–2,75 kHz,
// scharfe Kante bei 3 kHz, ueber 4 kHz nur −79 dB am Lautsprecher.
// Richtig demoduliert, nur in der falschen Seitenbandlage.
//
// Namen der Betriebsarten aus der Maschine selbst (`mode_list` im
// Scheibenzustand): LSB, USB, AM, CW, DIGL, DIGU, SAM, FM, NFM, DFM,
// RTTY. Eine Betriebsart, die nicht darin steht, gibt einen leeren
// Befehl — lieber nichts schicken als die Scheibe verstellen.
QString setSliceModeCommand(const QString& sliceId, const QString& mode);

// From AetherSDR SliceModel.cpp:231-232 [@d58e2b8a]:
//   // FlexAPI: "filt <id> <low_hz> <high_hz>"
//   sendCommand(QString("filt %1 %2 %3").arg(m_id).arg(low).arg(high));
// Flanken in Hertz, bezogen auf die Traegerfrequenz, wie die Maschine
// sie in `filter_lo`/`filter_hi` zurueckmeldet.
QString setSliceFilterCommand(const QString& sliceId, int lowHz, int highHz);

// ── Den Panadapter der Maschine mitnehmen (2026-10-22) ───────────────
//
// `slice tune` traegt `autopan=0`, damit die Maschine das Bild bei
// jedem Dreh nicht neu zentriert — unser Ausschnitt haengt an der
// Wasserfallkachel und wuerde sonst springen. Der Preis: stimmt der
// Betreiber WEIT ab, laeuft die Scheibe aus dem Ausschnitt heraus.
//
// Gemessen am Geraet, 2026-10-22, 21:22: die Scheibe stand auf
// 14.382279 MHz, der Panadapter zeigte 230 kHz um 14.0999 MHz, also
// 13,985–14,215. Der Betreiber hoerte an einer Stelle und sah eine
// andere, 170 kHz daneben: „aber keine signale".
//
// From AetherSDR FlexBackend.cpp:420 [@d58e2b8a] —
//   sendSlice("display pan set %1 center=%2")
// Megahertz mit sechs Nachkommastellen, wie bei `slice tune`.
QString setPanCenterCommand(const QString& panId, double hz);

// ── Der Wasserfallstrom wird getrennt eingerichtet (2026-10-23) ──────
//
// Er ist nicht der Panadapter: der Panadapter-Zustand nennt ihn als
// `waterfall=0x42000000` neben `panadapter=0x40000000`, und er hat
// eigene Einstellungen. Longpath hat sie nie angefasst — am Geraet
// stand in ALLEN fuenf Statusmeldungen `auto_black=1`. Die Maschine
// rechnet dann ihren eigenen Schwarzpunkt in die Kachel, gezielt auf
// SmartSDRs Darstellung, waehrend wir obendrauf noch einmal schaetzen.
//
// From AetherSDR RadioModel::configureWaterfall [@d58e2b8a]:
//   display panafall set <wfId> auto_black=0 black_level=15 color_gain=50
// mit Rueckfall auf `display waterfall set` fuer Fassungen ohne
// Panafall-Adressierung. `black_level=0` statt 15, weil es bei
// abgeschaltetem `auto_black` der Handwert der Maschine ist und wir
// ohnehin selbst faerben.
//
// ── `line_duration` ist KEINE Millisekundenzahl ──────────────────────
//
// From AetherSDR WaterfallRate.h [@d58e2b8a]: es ist eine RATE von 1
// bis 100, KLEIN ist LANGSAM und GROSZ ist SCHNELL — an echter
// Flex-Hardware gemessen (AetherSDR #3070/#3104). FlexLib tippt das
// Feld als Millisekunden, und wer das woertlich nimmt, dreht den
// Regler um.
//
// Martins Maschine stand auf 80 und lieferte 12,5 Zeilen je Sekunde;
// bei einem Wasserfall von rund 150 Bildzeilen sind das ZWOELF
// Sekunden, bis er voll ist. Der Betreiber am 2026-10-23: „dauert
// extrem lange bis bis kommt mit wasserfall". Mit 100 sind es 25
// Zeilen je Sekunde, also sechs Sekunden — am Geraet nachgemessen.
//
// Nur ueber die Panafall-Adressierung: `display waterfall set …
// line_duration=…` weist die Maschine ab (Code 50000016, gemessen).
QString setWaterfallSetupCommand(const QString& waterfallId, bool rueckfall);

// From AetherSDR RadioStatusOwnership.h:48-60 [@d58e2b8a] —
// parsePanafallCreatePanId(). Die Antwort auf „display panafall
// create" traegt die Kennung mal als `pan=`, mal als `id=`, mal nackt
// als erstes Glied einer Kommaliste. Alle drei Formen werden gelesen.
// ── Senden (2026-10-23, Stufe 2) ─────────────────────────────────────
//
// Bis heute stand im Kopf dieser Datei: „Was Stufe 1 trotzdem NICHT
// tut: senden. Kein `xmit`, keine TX-Scheibe, keine Leistung." Der
// Betreiber hat danach gefragt, und hier ist der Draht dazu.
//
// Alle Befehle sind reine Zeichenketten — sie gehen nirgendwohin, bis
// jemand sie durch `sendCommand` schickt. Das ist Absicht: so laesst
// sich der Wortlaut pruefen, ohne eine Maschine zu tasten.

// From AetherSDR FlexBackend.cpp:438 [@d58e2b8a] —
//   send(QStringLiteral("xmit %1").arg(key ? 1 : 0));
// Das Tasten selbst. Die Entscheidung, OB getastet werden darf, faellt
// darueber (Verriegelung, Betriebsart, Eigentuemer der Sendescheibe);
// dieser Befehl uebersetzt sie nur.
QString xmitCommand(bool tasten);

// From AetherSDR RadioModel.cpp:3077 [@d58e2b8a] —
//   QStringLiteral("slice set %1 tx=1").arg(restoreSliceId)
// Welche Scheibe sendet. Die Maschine fuehrt genau eine; `tx=0` nimmt
// ihr das Senden, ohne eine andere zu bestimmen.
QString setSliceTxCommand(const QString& sliceId, bool sendet);

// Sendeleistung und Abstimmleistung in Prozent, wie die Maschine sie
// im `transmit`-Zustand meldet (`rfpower=`, `tunepower=`).
//
// From AetherSDR FlexBackend.cpp:853-854 [@d58e2b8a] — dort werden
// beide auf 0..100 geklemmt. Hier ebenso: ausserhalb liegende Werte
// weist die Maschine ab, und ein abgewiesener Befehl laesst die
// Leistung auf dem alten Wert stehen, was schlimmer ist als nichts.
QString setRfPowerCommand(int prozent);
QString setTunePowerCommand(int prozent);

// Der Abstimmtraeger. `transmit tune 1` taktet mit `tunepower`, nicht
// mit `rfpower`.
//
// From AetherSDR RadioModel.cpp:2307 [@d58e2b8a] —
//   if (cmd == "transmit tune 1" || cmd == "atu start")
QString tuneCommand(bool an);

// ── Der Sendeton braucht einen eigenen Strom ─────────────────────────
//
// Die Flex rechnet selbst: es geht kein fertiges Sendesignal hinaus,
// sondern Mikrofonton. Der laeuft als VITA-49-Strom vom Typ `dax_tx`
// zur Maschine — erst anlegen, dann traegt die Antwort die Kennung,
// die in jedes Paket gehoert.
//
// From AetherSDR RadioModel.cpp:11374 [@d58e2b8a] —
//   sendCmd("stream create type=dax_tx", ...)
// ── Ohne `dax=1` verfaellt jedes Tonpaket ────────────────────────────
//
// Die Maschine nimmt den Sendeton NUR dann aus unserem Strom, wenn der
// Senderzweig auf DAX steht. Sonst benutzt sie das Mikrofon an ihrer
// eigenen Buchse und wirft jedes unserer Pakete weg — getastet wird,
// gesendet wird Stille.
//
// From AetherSDR AudioEngine.cpp:8954 [@d58e2b8a]:
//   „Do NOT emit daxRouteRequested(0) here — dax=0 tells the radio to
//    use the physical mic and discard every dax_tx packet, producing
//    no TX waveform."
// und TransmitModel.cpp:570 [@d58e2b8a] fuer den Wortlaut.
QString setDaxRouteCommand(bool ueberDax);

QString createDaxTxStreamCommand();

// Die Antwort auf `stream create` traegt die Stromkennung — mal als
// `stream_id=`, mal nackt, mal mit `0x` davor. Null heisst „nicht
// gelesen"; eine Null ist auch keine gueltige Kennung.
//
// From AetherSDR RadioStatusOwnership::parseCreateResponseStreamId
// [@d58e2b8a].
quint32 streamIdFromCreateReply(const QString& body);

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
