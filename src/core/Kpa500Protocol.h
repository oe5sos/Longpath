// no-port-check: Longpath-eigene Datei. Kein Port -- gebaut aus Elecrafts
// eigener, oeffentlich veroeffentlichter Programmierreferenz. Siehe den
// Kopf unten; die `// From Elecraft ...`-Verweise nennen Fundstellen in
// jenem Dokument, keinen fremden Quelltext.
#pragma once

// =================================================================
// src/core/Kpa500Protocol.h  (Longpath-eigen)
// =================================================================
//
// Elecraft KPA500: die Fernsteuerschnittstelle.
//
// HERKUNFT, und diesmal eine gute: "ELECRAFT KPA500 PROGRAMMERʼS
// REFERENCE", Rev. A2 (7.7.2011), Elecraft Inc. Das Dokument ist frei
// herunterladbar und liegt bei Elecraft selbst:
//
//   https://ftp.elecraft.com/KPA/Manuals%20Downloads/KPA500%20Programmers%20Ref.pdf
//   (verzeichnet auf https://elecraft.com/pages/programmers-reference-manuals)
//
// Es ist am 2026-10-09 beschafft und GELESEN worden; jede Zahl und jedes
// Format unten traegt die Fundstelle daneben. Das ist der Unterschied zu
// SPE (core/SpeProtocol.h): dort stammt alles aus AetherSDRs Lesung einer
// Anleitung, die oeffentlich nicht auffindbar ist. Hier steht das
// Dokument selbst dahinter.
//
// KEIN PORT. Von den vier Verstaerkern der Zeus-Liste hat AetherSDR fuer
// den KPA500 keine Vorlage; diese Datei ist von Hand gegen das Dokument
// geschrieben. Entsprechend gibt es nichts zu attribuieren ausser dem
// Dokument -- und das ist eine Beschreibung, kein Quelltext.
//
// DAS PROTOKOLL ist viel einfacher als das des SPE: reiner ASCII-Text,
// keine Rahmen, keine Pruefsummen.
//
//   Rechner -> Verstaerker:  ^XX[Daten];      (SET)
//                            ^XX;             (GET)
//   Verstaerker -> Rechner:  ^XX[Daten];      (RSP)
//
// Zwei oder drei Buchstaben, dann die Daten, dann ein Semikolon. Befehle
// mit falschem Aufbau oder Werten ausserhalb des Bereichs werden
// STILLSCHWEIGEND verworfen -- es gibt keine Fehlermeldung, nur das
// Ausbleiben einer Antwort. Gross- und Kleinschreibung ist dem Geraet
// gleich, es antwortet immer in Grossbuchstaben. Ein einzelnes ";"
// schickt das Geraet als ";" zurueck und ist damit die billigste Probe,
// ob ueberhaupt eine Verbindung steht.
//
// Reine Funktion, kein QObject, kein Netzwerk, kein Zeitgeber -- damit
// vollstaendig am Schreibtisch pruefbar (tst_kpa500_protokoll).
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 -- Neu, Zeus-Punkt 7. Martin Fischer, AI-assisted via
//                 Anthropic Claude (Claude Code).
// =================================================================

#include <functional>
#include <optional>

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace Longpath {
namespace Kpa500 {

// From Elecraft KPA500 Programmer's Reference Rev A2, §Command Format:
// „SET commands use 2 or 3 characters, optional data fields, and a
// terminating semicolon (;)."
constexpr char kPrefix     = '^';
constexpr char kTerminator = ';';

// ── Was der Verstaerker meldet ───────────────────────────────────────

// Eine gelesene Antwort. `verb` ist der Befehl OHNE das fuehrende '^'
// und ohne das Semikolon ("WS", "VI", "OS", …), `data` der Rest.
//
// Der Boot-Lader antwortet anders: auf 'I' kommt "KPA500" ohne Praefix
// und ohne Semikolon (§BootLoader Command Reference). Darum gibt es
// dafuer ein eigenes Feld statt eines erfundenen Verbs.
struct Reply {
    QString verb;
    QString data;
    bool    isBootIdentify{false};   // die nackte Antwort "KPA500"
    bool    isBareSemicolon{false};  // das zurueckgeworfene ";"
};

// Stromorientierter Leser. Fuettert man ihn mit rohen Bytes von
// irgendeinem Transport (serieller Anschluss oder TCP), ruft er je
// vollstaendiger Antwort zurueck. Alles vor einem '^' ist Rauschen und
// wird verworfen -- ausser der Boot-Antwort "KPA500" und dem nackten
// ";", die beide ohne Praefix kommen.
class ReplyParser {
public:
    void setReplyCallback(std::function<void(const Reply&)> cb) { m_onReply = std::move(cb); }
    void feed(const QByteArray& bytes);
    void reset() { m_buf.clear(); }

    // Pruefstand-Naht: wie viel noch unverarbeitet liegt. Ohne sie laesst
    // sich nicht belegen, dass ein Geraet, das nur Rauschen schickt und
    // nie ein Semikolon, den Speicher NICHT volllaufen laesst -- die
    // Antworten kommen naemlich trotzdem richtig an, der Puffer waechst
    // nur still mit. Genau daran ist der erste Anlauf dieses Falls
    // gruen durchgelaufen.
    int bufferedForTesting() const { return static_cast<int>(m_buf.size()); }

private:
    QByteArray m_buf;
    std::function<void(const Reply&)> m_onReply;
};

// ── Was der Rechner schickt ──────────────────────────────────────────

// Eine Abfrage: die Befehlsbuchstaben und ein Semikolon, nichts dazwischen.
// From Elecraft Rev A2, §Command Reference: „the GET format is just the
// 2 or 3 letters of the command followed by a semicolon."
QByteArray buildGet(const QString& verb);
// Eine Einstellung: Buchstaben, Daten, Semikolon.
QByteArray buildSet(const QString& verb, const QString& data);

// Die billigste Probe: ein nacktes Semikolon, das der Verstaerker
// zurueckwirft. From Elecraft Rev A2, §Command Format: „The KPA500 will
// respond to a null command, containing only a ';' by echoing the ';'
// character."
QByteArray buildPing();

// Die drei Befehle des Boot-Laders -- EINZELNE Grossbuchstaben, ohne
// Semikolon (§BootLoader Command Reference). Nur zwei davon gibt es hier:
//
//   'I'  Identify   -> der Verstaerker antwortet "KPA500"
//   'P'  Power On   -> fuehrt die Pruefungen aus und startet die
//                      Hauptsoftware; KEINE Antwort
//
// Der dritte ist 'D' (Firmware laden). Den gibt es hier NICHT und soll es
// nicht geben: das Dokument sagt dazu „This command is for Elecraft
// internal use only. Accidentally issuing a D command may require a rear
// panel power off for recovery."
QByteArray buildBootIdentify();
QByteArray buildBootPowerOn();

// ── Die Befehle, die dieses Programm benutzt ─────────────────────────
//
// Das Dokument listet 21 Befehle. Hier stehen die, die im Betrieb
// gebraucht werden; die uebrigen (Datenraten der beiden
// RS232-Anschluesse, Demo-Betrieb, INHIBIT-Eingang, Lautsprecher,
// T/R-Verzoegerung, Abschwaecher-Freigabezeit, ALC-Schwelle,
// Leistungskorrektur, STBY-bei-Bandwechsel, Luefter-Mindestdrehzahl,
// Funkgeraete-Schnittstelle) sind Einrichtungssachen und gehoeren an die
// Front des Geraets oder in eine Einstellseite, nicht in den Abfragetakt.
namespace Verb {
inline constexpr auto kPowerSwr    = "WS";   // GET only  -- ^WSppp sss;
inline constexpr auto kVoltsAmps   = "VI";   // GET only  -- ^VIvvv iii;
inline constexpr auto kTemperature = "TM";   // GET only  -- ^TMnnn;
inline constexpr auto kOperate     = "OS";   // GET/SET   -- ^OSn;
inline constexpr auto kBand        = "BN";   // GET/SET   -- ^BNnn;
inline constexpr auto kFault       = "FL";   // GET/CLEAR -- ^FLnn; / ^FLC;
inline constexpr auto kPowerState  = "ON";   // GET/SET   -- ^ONn; / ^ON0;
inline constexpr auto kFirmware    = "RVM";  // GET only  -- ^RVMnn.nn;
inline constexpr auto kSerial      = "SN";   // GET only  -- ^SNnnnnn;
}  // namespace Verb

QByteArray buildOperateSet(bool operate);   // ^OS1; / ^OS0;
QByteArray buildBandSet(int bandIndex);     // ^BNnn;   nn = 00..10
QByteArray buildFaultClear();               // ^FLC;
QByteArray buildPowerOff();                 // ^ON0;

// ── Entschluesseln ───────────────────────────────────────────────────

// ^WSppp sss; -- Ausgangsleistung in Watt (0..999) und Stehwelle. Die
// Stehwelle hat ein GEDACHTES Komma hinter der zweiten Stelle: "015"
// heisst 1,5. Im Empfang meldet das Geraet 000 (nicht 1,0) -- From
// Elecraft Rev A2, §^WS: „sss will return as 000 when not transmitting."
// Dann kommt hier 0.0f heraus, und der Aufrufer entscheidet, was er
// anzeigt; eine 1,0 hineinzuerfinden waere eine Messung, die es nicht
// gab.
struct PowerSwr {
    int   watts{0};
    float swr{0.0f};
};
std::optional<PowerSwr> parsePowerSwr(const QString& data);

// ^VIvvv iii; -- Spannung und Strom, beide mit gedachtem Komma hinter
// der zweiten Stelle ("475" = 47,5). From Elecraft Rev A2, §^VI: „Note
// the implied decimal point after the second digit for both values."
//
// Gelesen wird MIT Trenner („475 320") und OHNE („475320"): das Dokument
// schreibt einen Zwischenraum, ob der auf dem Draht steht oder nur im
// Satz des PDF, laesst sich daran nicht entscheiden, und hier haengt
// kein Geraet zum Nachsehen. Gilt genauso fuer ^WS.
struct VoltsAmps {
    float volts{0.0f};
    float amps{0.0f};
};
std::optional<VoltsAmps> parseVoltsAmps(const QString& data);

// ^TMnnn; -- 0..150 Grad C. Hier IST die Einheit bekannt (anders als
// beim SPE, der die Einheit seines eigenen Displays nicht mitteilt).
std::optional<int> parseTemperature(const QString& data);

// ^BNnn; -- 00 = 160 m bis 10 = 6 m. Der KPA500 kennt KEIN 4-m-Band
// (der SPE hat dort eine 11). Unbekannte Werte geben "?m".
QString bandName(int index);
std::optional<int> parseBand(const QString& data);

// ^OSn; -- 0 = Standby, 1 = Operate.
std::optional<bool> parseOperate(const QString& data);

// ^ONn; -- n = 1, wenn er laeuft. Ist er aus, kommt GAR KEINE Antwort
// (§^ON: „No response if off.") -- das Ausbleiben ist die Aussage.
std::optional<bool> parsePowerState(const QString& data);

// ^FLnn; -- 00 heisst "kein Fehler". JEDE ANDERE ZAHL BLEIBT EINE ZAHL.
//
// Das Dokument gibt die Zuordnung Nummer -> Fehlername NICHT her; es
// sagt nur „nn = current fault identifier". Einen Namen dazuzuerfinden
// waere geraten, und geraten wird hier nicht -- angezeigt wird
// "Fehler 03". Wer die Zuordnung sauber beschaffen will, loest die
// Fehler am Geraet einzeln aus und liest die Namen an der Front ab.
// (Dieselbe Enthaltsamkeit, aus demselben Grund, die AetherSDR beim
// VK3AMP ueben musste -- dort stammte die Tabelle aus einem
// dekompilierten Herstellerprogramm und durfte nicht mit.)
struct Fault {
    int code{0};
    bool isFault() const { return code != 0; }
};
std::optional<Fault> parseFault(const QString& data);

// ── Anzeigeskala ─────────────────────────────────────────────────────
//
// Der KPA500 ist mit 500 W angegeben, und das Dokument nennt fuer ^WS
// einen Bereich bis 999 W. Daraus ABGELEITET -- nicht gemessen, es haengt
// hier kein KPA500 am Kabel -- dieselbe Form, die beim SPE aus einer
// Messung an echter Hardware stammt: gelb ab nominal-50, rot ab nominal,
// Skalenende nominal+100.
constexpr float kNominalPowerW = 500.0f;
constexpr float kWarnPowerW    = 450.0f;
constexpr float kMaxPowerW     = 600.0f;

// Temperatur: das Dokument gibt den BEREICH (0..150 °C), aber keine
// Warn- oder Abschaltschwelle. Die Zahlen unten sind darum eine
// Vermutung in der Form, die Longpath schon fuer den RF2K-S benutzt
// (Rf2ksApplet: gelb 55, rot 80) -- ausdruecklich geraten, nicht
// abgeleitet, und als erstes zu berichtigen, wenn jemand ein Geraet hat.
constexpr int kTempWarnC = 55;
constexpr int kTempRedC  = 80;

}  // namespace Kpa500
}  // namespace Longpath

Q_DECLARE_METATYPE(Longpath::Kpa500::PowerSwr)
Q_DECLARE_METATYPE(Longpath::Kpa500::VoltsAmps)
